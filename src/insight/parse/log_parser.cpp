module;
#include "insight/utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.detail.parse;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.strategy; // IFormatStrategy, ParsedLine

// src/1_tokenization/log_parser.cpp
//
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

LogParser::LogParser(ArenaAllocator& arena) : arena_(arena) {}

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
std::expected<ParsedLine, std::string> LogParser::parse_line(std::string_view line)
{
    if (line.empty())
    {
        ++failed_count_;
        INSIGHT_LOG_TRACE(logging::parser_logger(), "parse: empty line skipped");
        return std::unexpected(std::string("LogParser: empty line"));
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
        INSIGHT_LOG_TRACE(logging::parser_logger(), "parse ok: strategy={}",
                          to_string(strategy->format()));
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

    if constexpr (logging::kDebugLogsEnabled)
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
        INSIGHT_LOG_TRACE(logging::parser_logger(), "parse_stable ok: strategy={}",
                          to_string(strategy->format()));
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

} // namespace insight::tokenization
