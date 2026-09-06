module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: an Nginx error-log record — a slash-separated date and clock, a bracketed level, a
// process and thread id, then the message.
// invariant: a hand-written scanner with no regex and, on the SUCCESS path, no string copies — a
// decline builds an error message.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

std::expected<ParsedLine, std::string> NginxErrorStrategy::parse(std::string_view line,
                                                                 ArenaAllocator& /*arena*/) const
{
    if (!is_nginx_error_prefix(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=NginxError parse miss");
        return std::unexpected(
            std::string("NginxErrorStrategy: line does not match Nginx error format"));
    }

    std::string_view rest{line};
    const std::string_view ts_str{sv_take_n(rest, 19U)};
    sv_skip_ws(rest);

    const std::string_view level_sv{sv_take_bracketed(rest)};
    if (level_sv.empty())
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=NginxError parse miss (no level)");
        return std::unexpected(
            std::string("NginxErrorStrategy: line does not match Nginx error format"));
    }

    (void)sv_take_token(rest);

    ParsedLine parsed;
    parsed.raw_line = line;
    parsed.timestamp = EventTime::parsed(utils::parse_nginx_error_ts(ts_str));
    parsed.level = EventLevel::declared(utils::parse_log_level(level_sv));
    parsed.component = "nginx";
    parsed.content = rest;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=NginxError parsed component={} level={} has_timestamp={}",
                      parsed.component, to_string(parsed.level.value()),
                      parsed.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed};
}

LogFormat NginxErrorStrategy::format() const noexcept
{
    return LogFormat::NginxError;
}

double NginxErrorStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::string_view::size_type kMinimumCandidateLength{25};
    static constexpr double kNginxErrorConfidence{0.89};
    static constexpr double kNoConfidence{0.0};
    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (is_nginx_error_prefix(line))
        return kNginxErrorConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
