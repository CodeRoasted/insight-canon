module;
#include "insight/utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.detail;
import insight.canon.internal;
import insight.canon.api;

// src/1_tokenization/strategies/health_app.cpp
//
// HealthAppStrategy — parses pipe-delimited HealthApp logs:
//   "20171223-22:15:29:606|Step_LSC|30002312|onStandStepChanged 3579"
//
// Hand-written scanner: zero RE2, zero string copies.




namespace insight::tokenization
{

namespace
{
    constexpr std::string_view::size_type kMinimumCandidateLength{22};
    constexpr double kNoConfidence{0.0};
    constexpr double kHealthAppConfidence{0.92};

} // namespace

std::expected<ParsedLine, std::string> HealthAppStrategy::parse(std::string_view line,
                                                                ArenaAllocator& /*arena*/) const
{
    if (!detail::is_health_app_prefix(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=HealthApp parse miss");
        return std::unexpected(
            std::string("HealthAppStrategy: line does not match HealthApp format"));
    }

    // Format: "YYYYMMDD-HH:MM:SS:mmm|component|process_id|message"
    std::string_view rest{line};
    const std::string_view ts_str{detail::sv_take_until(rest, '|')};
    const std::string_view component{detail::sv_take_until(rest, '|')};
    (void)detail::sv_take_until(rest, '|'); // skip process_id
    // rest = message

    if (ts_str.empty() || component.empty())
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=HealthApp parse miss (bad fields)");
        return std::unexpected(
            std::string("HealthAppStrategy: line does not match HealthApp format"));
    }

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = utils::parse_health_app_ts(ts_str);
    parsed_line.level = LogLevel::Unknown;
    parsed_line.component = component;
    parsed_line.content = rest;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=HealthApp parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat HealthAppStrategy::format() const noexcept
{
    return LogFormat::HealthApp;
}

double HealthAppStrategy::confidence(std::string_view line) const noexcept
{
    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (detail::is_health_app_prefix(line))
        return kHealthAppConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
