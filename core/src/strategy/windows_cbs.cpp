module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: a Windows CBS or CSI record — a full timestamp, a comma, a level word, a component word,
// then the message.
// invariant: the comma immediately after the timestamp is the distinctive anchor.
// invariant: a hand-written scanner with no regex and, on the SUCCESS path, no string copies — a
// decline builds an error message.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{
    constexpr std::size_t kTimestampLen{19U};
    constexpr std::size_t kRestOffset{20U};

} // namespace

std::expected<ParsedLine, std::string> WindowsCBSStrategy::parse(std::string_view line,
                                                                 ArenaAllocator& /*arena*/) const
{
    static constexpr std::size_t kMinLineLen{21U};

    // invariant: NOT redundant with the confidence score — under an explicit format declaration
    // the parse is reachable without the score ever having run.
    // invariant: this is what keeps the two fixed-offset reads below in bounds, and it is a weaker
    // bound than the score's: length plus the separator, where the score wants the whole stamp.
    if (line.size() < kMinLineLen || line[kTimestampLen] != ',' || !is_space(line[kRestOffset]))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=WindowsCBS parse miss");
        return std::unexpected(
            std::string("WindowsCBSStrategy: line does not match Windows CBS/CSI format"));
    }

    const std::string_view ts_str{line.substr(0, kTimestampLen)};
    std::string_view rest{line.substr(kRestOffset)};
    sv_skip_ws(rest);

    const std::string_view level_sv{sv_take_token(rest)};
    const std::string_view component{sv_take_token(rest)};

    if (level_sv.empty() || component.empty())
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(),
                          "strategy=WindowsCBS parse miss (bad fields)");
        return std::unexpected(
            std::string("WindowsCBSStrategy: line does not match Windows CBS/CSI format"));
    }

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = EventTime::parsed(utils::parse_iso8601(ts_str));
    parsed_line.level = EventLevel::declared(utils::parse_log_level(level_sv));
    parsed_line.component = component;
    parsed_line.content = rest;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=WindowsCBS parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level.value()),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat WindowsCBSStrategy::format() const noexcept
{
    return LogFormat::WindowsCBS;
}

double WindowsCBSStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::string_view::size_type kMinimumCandidateLength{25};
    static constexpr double kWindowsCbsConfidence{0.88};
    static constexpr double kNoConfidence{0.0};

    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (!is_iso_datetime_space_prefix(line, /*require_fraction=*/false))
        return kNoConfidence;
    if (line.size() <= kRestOffset || line[kTimestampLen] != ',' || !is_space(line[kRestOffset]))
        return kNoConfidence;
    return kWindowsCbsConfidence;
}

} // namespace insight::tokenization
