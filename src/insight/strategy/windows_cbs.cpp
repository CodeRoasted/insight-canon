module;
#include "insight/utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// src/1_tokenization/strategies/windows_cbs.cpp
//
// WindowsCBSStrategy — parses Windows CBS/CSI log format:
//   "2016-09-28 04:30:30, Info                  CBS    Loaded Servicing Stack v6.1..."
//
// Distinctive anchor: "YYYY-MM-DD HH:MM:SS, Level  Component  message"
// (comma-space after timestamp, level word, then component word).
//
// Hand-written scanner: zero RE2, zero string copies.




namespace insight::tokenization
{

namespace
{
    constexpr std::string_view::size_type kMinimumCandidateLength{25};
    constexpr double kNoConfidence{0.0};
    constexpr double kWindowsCbsConfidence{0.88};
    constexpr std::size_t kTimestampLen{19U}; // "YYYY-MM-DD HH:MM:SS"
    constexpr std::size_t kRestOffset{20U};   // past timestamp + comma
    constexpr std::size_t kMinLineLen{21U};   // timestamp + ", "

} // namespace

std::expected<ParsedLine, std::string> WindowsCBSStrategy::parse(std::string_view line,
                                                                 ArenaAllocator& /*arena*/) const
{
    // Confidence already validates "YYYY-MM-DD HH:MM:SS, Level".
    // Minimum: 19 chars timestamp + comma + space + level.
    if (line.size() < kMinLineLen || line[kTimestampLen] != ',' ||
        !is_space(line[kRestOffset]))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=WindowsCBS parse miss");
        return std::unexpected(
            std::string("WindowsCBSStrategy: line does not match Windows CBS/CSI format"));
    }

    // Timestamp is first 19 chars: "YYYY-MM-DD HH:MM:SS"
    const std::string_view ts_str{line.substr(0, kTimestampLen)};
    std::string_view rest{line.substr(kRestOffset)}; // skip ','
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
    parsed_line.timestamp = utils::parse_iso8601(ts_str);
    parsed_line.level = utils::parse_log_level(level_sv);
    parsed_line.component = component;
    parsed_line.content = rest;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=WindowsCBS parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat WindowsCBSStrategy::format() const noexcept
{
    return LogFormat::WindowsCBS;
}

double WindowsCBSStrategy::confidence(std::string_view line) const noexcept
{
    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (!is_iso_datetime_space_prefix(line, /*require_fraction=*/false))
        return kNoConfidence;
    if (line.size() <= kRestOffset || line[kTimestampLen] != ',' ||
        !is_space(line[kRestOffset]))
        return kNoConfidence;
    return kWindowsCbsConfidence;
}

} // namespace insight::tokenization
