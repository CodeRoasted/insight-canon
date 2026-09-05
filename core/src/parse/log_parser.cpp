module;
// refs: ADR-3.D4
#include "utils/log_macros.hpp"

module insight.canon.detail.parse;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;
import insight.canon.compose;
import insight.canon.detail.strategy;

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

// refs: SRC-D-PROV-1
// pre: `raw_line` is the RAW, ANSI-bearing line — stage-1 `normalize()` destroys the SGR wrapper
// the hooks key on.
// invariant: strategy-independent, and core names no hook — every provenance hook arrives from a
// semantic package.
[[nodiscard]] static bool
is_echoed_source(std::string_view raw_line,
                 const insight::semantic::ComposedSemantics& composed) noexcept
{
    return std::ranges::any_of(composed.provenance_hooks(),
                               [raw_line](insight::semantic::ProvenanceHook hook)
                               { return hook(raw_line); });
}

// refs: ADR-22, ADR-22.D3, ADR-22.D6, DN-32.D3
// pre: runs AFTER the strategy, whose `content` the lift keys on, and BEFORE the echoed-source
// demotion, which must outrank it.
// invariant: Unknown from the walk is the ABSENCE of a declared row, never a level, so it must not
// overwrite the strategy's inference.
// invariant: a lifted level is DECLARED — the value and its provenance are one assignment because
// they are one fact.
// note: an undeclared stream's view carries no gated row, so the walk is empty.
static void apply_level_lift(ParsedLine& parsed,
                             const insight::semantic::ComposedSemantics& composed) noexcept
{
    if (const LogLevel lifted{lift_level(parsed.content, composed)}; lifted != LogLevel::Unknown)
        parsed.level = EventLevel::declared(lifted);
}

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

void LogParser::set_format(LogFormat fmt)
{
    active_strategy_ = nullptr;
    sticky_strategy_ = nullptr;
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
        sticky_strategy_ = nullptr;
    }
    INSIGHT_LOG_INFO(logging::parser_logger(), "auto-detect {}", enabled ? "enabled" : "disabled");
}

std::expected<ParsedLine, std::string> LogParser::parse_line(std::string_view raw_line)
{
    // invariant: an empty line is ordinary input, not a failure — counted as skipped, and still
    // returned as `unexpected` so the caller learns it produced no event.
    if (raw_line.empty())
    {
        ++skipped_count_;
        INSIGHT_LOG_TRACE(logging::parser_logger(), "parse: empty line skipped");
        return std::unexpected(std::string("LogParser: empty line"));
    }

    // refs: SRC-D-TID-11, ADR-21.D4
    // invariant: THE one named site where this parser performs stage 1 unconditionally;
    // `parse_stable` performs none.
    const NormalizedLine normalized{normalize(raw_line, escape_scratch_)};
    // note: a non-empty line normalizing to nothing was all escape bytes — no event.
    if (normalized.bytes().empty())
    {
        ++skipped_count_;
        INSIGHT_LOG_TRACE(logging::parser_logger(), "parse: line was all escape bytes, skipped");
        return std::unexpected(std::string("LogParser: empty line"));
    }
    const std::string_view line{normalized.bytes()};

    IFormatStrategy* strategy = active_strategy_;

    if (auto_detect_ || strategy == nullptr)
        strategy = select_strategy(line);

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

    // invariant: the arena copy happens only after a strategy is known — a failed detection must
    // not consume arena capacity in a long-running stream.
    const std::string_view stable{arena_.store_string(line)};

    auto result{strategy->parse(stable, arena_)};
    if (result.has_value())
    {
        ++parsed_count_;
        last_format_ = strategy->format();
        apply_level_lift(*result, composed_);
        // refs: SRC-D-PROV-1
        // invariant: an echoed-source line is run-step SCRIPT text, not an observed event, so its
        // level is driven to absence.
        // note: a failure word in echoed shell source must confer no alerting level.
        if (is_echoed_source(raw_line, composed_))
        {
            result->echoed_source = true;
            result->level = EventLevel{};
        }
        INSIGHT_LOG_TRACE(logging::parser_logger(), "parse ok: strategy={} echoed_source={}",
                          to_string(strategy->format()), result->echoed_source);
    }
    else
    {
        ++failed_count_;
        // invariant: the ONE rate-limited record of a strategy's failure reason — the facade's
        // own reprint is DEBUG level and unbounded.
        if (failed_count_ == 1 || failed_count_ % kWarnEveryNFailures == 0)
        {
            INSIGHT_LOG_WARN(logging::parser_logger(),
                             "parse failed: strategy={} reason=\"{}\" total_failures={}",
                             to_string(strategy->format()), result.error(), failed_count_);
        }
    }

    // invariant: SPDLOG_ACTIVE_LEVEL is canon's PRIVATE build definition, kept off the public api
    // surface so the level macro never leaks onto a consumer's command line.
    constexpr bool kDebugLogsEnabled{SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG};
    if constexpr (kDebugLogsEnabled)
    {
        // invariant: the denominator is parsed + failed only, so a line counted as skipped cannot
        // move the failure rate; `seen` includes skips and gates the cadence.
        static constexpr std::size_t kStatsEveryNLines{1000};
        const auto seen{parsed_count_ + failed_count_ + skipped_count_};
        const auto attempted{parsed_count_ + failed_count_};
        if (seen > 0 && seen % kStatsEveryNLines == 0 && attempted > 0)
        {
            INSIGHT_LOG_DEBUG(
                logging::parser_logger(),
                "stats: parsed={} failed={} skipped={} failure_rate={:.1f}%", parsed_count_,
                failed_count_, skipped_count_,
                (static_cast<double>(failed_count_) / static_cast<double>(attempted)) * 100.0);
        }
    }
    return result;
}

std::expected<ParsedLine, std::string> LogParser::parse_stable(std::string_view stable_line)
{
    if (stable_line.empty())
    {
        ++skipped_count_;
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

    auto result{strategy->parse(stable_line, arena_)};
    if (result.has_value())
    {
        ++parsed_count_;
        last_format_ = strategy->format();
        apply_level_lift(*result, composed_);
        // refs: SRC-D-PROV-1
        // note: a caller that already stripped ANSI hands no wrapper here, so this is a no-op.
        if (is_echoed_source(stable_line, composed_))
        {
            result->echoed_source = true;
            result->level = EventLevel{};
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
                             "parse_stable failed: strategy={} reason=\"{}\" total_failures={}",
                             to_string(strategy->format()), result.error(), failed_count_);
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
