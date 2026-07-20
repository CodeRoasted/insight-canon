module;
#include "utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.detail.parse;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;              // ProvenanceHook
import insight.canon.compose;         // ComposedSemantics (echoed-source hooks)
import insight.canon.detail.strategy; // IFormatStrategy, ParsedLine

// LogParser: orchestrates arena allocation, format detection, and strategy
// dispatch for a stream of raw log lines.
//
// Ownership model:
//   - The arena is owned externally; LogParser holds a reference.
//   - Each detected raw line is copied into the arena before strategy parsing
//     so string_views inside the returned ParsedLine are stable for the arena's
//     lifetime.

namespace insight::tokenization
{

namespace
{

    constexpr std::size_t kWarnEveryNFailures{100};

} // namespace

LogParser::LogParser(ArenaAllocator& arena, const insight::semantic::ComposedSemantics& composed)
    : arena_(arena), composed_(composed), detector_(composed)
{
}

// D-PROV-1 echoed-source register: the GHA command-echo SGR wrapper is destroyed by
// strip_escape_sequences before any strategy sees the line, so the composed provenance hooks classify
// the RAW (ANSI-bearing) line — the only place it survives. Strategy-INDEPENDENT (a wrapped line is
// echoed source whatever it routed to), reproducing the pre-split is_echoed_source_line exactly. In
// 1.7.5 the single hook is the GitHub-Actions one from insight_semantic_github.
[[nodiscard]] static bool is_echoed_source(std::string_view raw_line,
                                           const insight::semantic::ComposedSemantics& composed) noexcept
{
    for (const insight::semantic::ProvenanceHook hook : composed.provenance_hooks())
        if (hook(raw_line))
            return true;
    return false;
}

// O(1) fast path: tries sticky strategy first; falls back to full detect.
// Updates sticky_strategy_ / active_strategy_ as a side-effect.
IFormatStrategy* LogParser::select_strategy(std::string_view line)
{
    if (auto_detect_ && sticky_strategy_ != nullptr && sticky_strategy_->confidence(line) > 0.0)
        return sticky_strategy_;

    IFormatStrategy* found = detector_.detect(line);
    if (found != nullptr)
    {
        if (auto_detect_)
            sticky_strategy_ = found;
        else
            active_strategy_ = found;
    }
    return found;
}

// Force a specific format strategy; deactivates per-line auto-detection.
// If no strategy matches the requested format, active_strategy_ remains null
// and auto-detection is re-enabled on the next call.
void LogParser::set_format(LogFormat fmt)
{
    active_strategy_ = nullptr;
    sticky_strategy_ = nullptr; // reset sticky on explicit format override
    for (const auto& strategy_candidate : detector_.strategies())
    {
        if (strategy_candidate->format() == fmt)
        {
            active_strategy_ = strategy_candidate.get();
            auto_detect_ = false;
            INSIGHT_LOG_INFO(logging::parser_logger(), "format set: {}", to_string(fmt));
            return;
        }
    }
    // Requested format not registered — fall back to auto-detect.
    auto_detect_ = true;
    INSIGHT_LOG_INFO(logging::parser_logger(), "format {} not found, falling back to auto-detect",
                     to_string(fmt));
}

void LogParser::set_auto_detect(bool enabled)
{
    auto_detect_ = enabled;
    if (enabled)
    {
        active_strategy_ = nullptr;
        sticky_strategy_ = nullptr; // reset sticky when switching modes
    }
    INSIGHT_LOG_INFO(logging::parser_logger(), "auto-detect {}", enabled ? "enabled" : "disabled");
}

// Successful parses are O(line.size()) for the arena copy + strategy parsing.
// Rejected lines skip the arena copy.
std::expected<ParsedLine, std::string> LogParser::parse_line(std::string_view raw_line)
{
    if (raw_line.empty())
    {
        ++failed_count_;
        INSIGHT_LOG_TRACE(logging::parser_logger(), "parse: empty line skipped");
        return std::unexpected(std::string("LogParser: empty line"));
    }

    // D-TID-11: strip ANSI/CSI/SGR/OSC escape sequences as a content normalization at canon
    // ingest — BEFORE strategy detection AND tokenization, so the format prefix-match, the level
    // token-scan, and the `component` extraction all see colour-free content (colour is
    // presentation, never content; the escapes interleave within/between tokens so a per-token
    // mask cannot reach them). Pure byte state machine → cross-stdlib bit-identical.
    //
    // Fast path: the strip is a no-op for any line carrying no ESC byte — strip_escape_sequences
    // copies such a line through verbatim, so its output is byte-identical to the input (a fixed
    // point). The vast majority of real log lines have no ANSI, so gate the strip (and the
    // escape_scratch_ copy) behind a memchr-backed ESC pre-scan (string_view::find is the SIMD
    // any-ESC scan); a clean line then flows straight to the single arena copy below — no strip,
    // no scratch copy. Identity-preserving by construction, so the canonical digest is unchanged.
    std::string_view line;
    if (raw_line.find(static_cast<char>(kEsc)) != std::string_view::npos)
    {
        strip_escape_sequences(raw_line, escape_scratch_);
        if (escape_scratch_.empty()) // the line was all escape bytes
        {
            ++failed_count_;
            INSIGHT_LOG_TRACE(logging::parser_logger(), "parse: line was all escape bytes, skipped");
            return std::unexpected(std::string("LogParser: empty line"));
        }
        line = escape_scratch_;
    }
    else
    {
        line = raw_line; // no ANSI → zero-copy; the stripped content equals the input
    }

    IFormatStrategy* strategy = active_strategy_;

    if (auto_detect_ || strategy == nullptr)
        strategy = select_strategy(line);

    if (strategy == nullptr)
    {
        ++failed_count_;
        // Rate-limited warning: first failure + every 100th thereafter.
        if (failed_count_ == 1 || failed_count_ % kWarnEveryNFailures == 0)
        {
            INSIGHT_LOG_WARN(logging::parser_logger(), "no strategy matched (total failures={})",
                             failed_count_);
        }
        return std::unexpected(std::string("LogParser: no strategy matched the line format"));
    }

    // Persist raw bytes only after a strategy is known. Failed detection
    // should not consume arena capacity in long-running streams.
    const std::string_view stable{arena_.store_string(line)};

    auto result{strategy->parse(stable, arena_)};
    if (result.has_value())
    {
        ++parsed_count_;
        last_format_ = strategy->format(); // the routed winner for this event (per-line observability)
        // D-PROV-1 (echoed-source register): the GHA command-echo SGR wrapper was destroyed
        // by strip_escape_sequences above, so detect it on the RAW (ANSI-bearing) line — the
        // only place it survives. An echoed-source line is run-step SCRIPT text, not an
        // observed runtime event: demote its level to Unknown so a failure WORD in echoed
        // shell source ("echo \"Download failed …\"") confers NO alerting level. Single root —
        // the level demotion transitively suppresses NewErrorPattern across all eidos channels.
        if (is_echoed_source(raw_line, composed_))
        {
            result->echoed_source = true;
            result->level = LogLevel::Unknown;
        }
        INSIGHT_LOG_TRACE(logging::parser_logger(), "parse ok: strategy={} echoed_source={}",
                          to_string(strategy->format()), result->echoed_source);
    }
    else
    {
        ++failed_count_;
        // Rate-limited warning: first failure + every 100th thereafter.
        if (failed_count_ == 1 || failed_count_ % kWarnEveryNFailures == 0)
        {
            INSIGHT_LOG_WARN(logging::parser_logger(),
                             "parse failed: strategy={} total_failures={}",
                             to_string(strategy->format()), failed_count_);
        }
    }

    // canon-internal DEBUG gate (see the note in tokenizer_engine.cpp). SPDLOG_ACTIVE_LEVEL is
    // canon's PRIVATE build-type compile def, via the textual log_macros.hpp include — kept off the
    // public insight.canon.api surface so the level macro never leaks to consumers.
    constexpr bool kDebugLogsEnabled{SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG};
    if constexpr (kDebugLogsEnabled)
    {
        static constexpr std::size_t kStatsEveryNLines{1000};
        // Periodic stats every 1000 lines.
        const auto total{parsed_count_ + failed_count_};
        if (total > 0 && total % kStatsEveryNLines == 0)
        {
            INSIGHT_LOG_DEBUG(
                logging::parser_logger(), "stats: parsed={} failed={} failure_rate={:.1f}%",
                parsed_count_, failed_count_,
                (static_cast<double>(failed_count_) / static_cast<double>(total)) * 100.0);
        }
    }
    return result;
}

// Fast variant: skips arena store_string(). Caller guarantees stable_line is
// valid for the arena's lifetime. All string_views returned from parse() will
// alias stable_line's storage directly (no extra copy).
std::expected<ParsedLine, std::string> LogParser::parse_stable(std::string_view stable_line)
{
    if (stable_line.empty())
    {
        ++failed_count_;
        INSIGHT_LOG_TRACE(logging::parser_logger(), "parse_stable: empty line skipped");
        return std::unexpected(std::string("LogParser: empty line"));
    }

    IFormatStrategy* strategy = active_strategy_;

    if (auto_detect_ || strategy == nullptr)
        strategy = select_strategy(stable_line);

    if (strategy == nullptr)
    {
        ++failed_count_;
        if (failed_count_ == 1 || failed_count_ % kWarnEveryNFailures == 0)
        {
            INSIGHT_LOG_WARN(logging::parser_logger(), "no strategy matched (total failures={})",
                             failed_count_);
        }
        return std::unexpected(std::string("LogParser: no strategy matched the line format"));
    }

    // Caller guarantees stable_line is arena-stable: no store_string() needed.
    auto result{strategy->parse(stable_line, arena_)};
    if (result.has_value())
    {
        ++parsed_count_;
        last_format_ = strategy->format(); // the routed winner for this event (per-line observability)
        // D-PROV-1: echoed-source demotion (see parse_line). Detect on the supplied line; a
        // pre-ANSI-stripped stable line carries no wrapper, so this is a no-op there.
        if (is_echoed_source(stable_line, composed_))
        {
            result->echoed_source = true;
            result->level = LogLevel::Unknown;
        }
        INSIGHT_LOG_TRACE(logging::parser_logger(), "parse_stable ok: strategy={} echoed_source={}",
                          to_string(strategy->format()), result->echoed_source);
    }
    else
    {
        ++failed_count_;
        if (failed_count_ == 1 || failed_count_ % kWarnEveryNFailures == 0)
        {
            INSIGHT_LOG_WARN(logging::parser_logger(),
                             "parse_stable failed: strategy={} total_failures={}",
                             to_string(strategy->format()), failed_count_);
        }
    }
    return result;
}

std::vector<std::expected<ParsedLine, std::string>>
LogParser::parse_batch(std::span<const std::string_view> lines)
{
    std::vector<std::expected<ParsedLine, std::string>> out;
    out.reserve(lines.size());
    for (auto line : lines)
        out.push_back(parse_line(line));
    return out;
}

std::size_t LogParser::lines_parsed() const noexcept
{
    return parsed_count_;
}
std::size_t LogParser::lines_failed() const noexcept
{
    return failed_count_;
}

LogFormat LogParser::detected_format() const noexcept
{
    return (active_strategy_ != nullptr) ? active_strategy_->format() : LogFormat::Unknown;
}

LogFormat LogParser::routed_format() const noexcept
{
    return last_format_;
}

} // namespace insight::tokenization
