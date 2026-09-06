module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: an RFC 5424 syslog record — a bracketed priority and version, then the declared field
// order, with a dash standing for any absent field.
// invariant: the priority decomposes as facility times eight plus severity, so the low three bits
// ARE the severity.
// invariant: a hand-written scanner with no regex and, on the SUCCESS path, no string copies — a
// decline builds an error message.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{

    LogLevel severity_to_level(int pri) noexcept
    {
        const int severity{pri & 0x07};
        switch (severity)
        {
        case 0:
            [[fallthrough]];
        case 1:
            [[fallthrough]];
        case 2:
            return LogLevel::Fatal;
        case 3:
            return LogLevel::Error;
        case 4:
            return LogLevel::Warn;
        case 5:
            [[fallthrough]];
        case 6:
            return LogLevel::Info;
        case 7:
            return LogLevel::Debug;
        default:
            return LogLevel::Unknown;
        }
    }

} // namespace

std::expected<ParsedLine, std::string> RFC5424Strategy::parse(std::string_view line,
                                                              ArenaAllocator& /*arena*/) const
{
    if (!is_rfc5424_prefix(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=RFC5424 parse miss");
        return std::unexpected(std::string("RFC5424Strategy: line does not match RFC 5424 format"));
    }

    std::string_view rest{line};
    rest.remove_prefix(1U);
    const std::string_view pri_str{sv_take_until(rest, '>')};

    (void)sv_take_token(rest);
    const std::string_view timestamp_str{sv_take_token(rest)};
    const std::string_view hostname{sv_take_token(rest)};
    const std::string_view appname{sv_take_token(rest)};
    (void)sv_take_token(rest);
    (void)sv_take_token(rest);

    // invariant: the structured-data field is stripped whether it is the absent dash or a bracketed
    // element, because it is metadata rather than message.
    std::string_view msg{rest};
    if (msg.starts_with("- "))
        msg.remove_prefix(2U);
    else if (!msg.empty() && msg[0] == '[')
    {
        const auto close{msg.find("] ")};
        if (close != std::string_view::npos)
            msg.remove_prefix(close + 2U);
    }

    int pri{};
    std::from_chars(pri_str.data(), pri_str.data() + pri_str.size(), pri);

    ParsedLine parsed;
    parsed.raw_line = line;
    parsed.timestamp = EventTime::parsed(utils::parse_iso8601(timestamp_str));
    parsed.level = EventLevel::declared(severity_to_level(pri));
    parsed.component = (appname != "-") ? appname : hostname;
    parsed.content = msg;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=RFC5424 parsed component={} level={} has_timestamp={}",
                      parsed.component, to_string(parsed.level.value()),
                      parsed.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed};
}

LogFormat RFC5424Strategy::format() const noexcept
{
    return LogFormat::RFC5424;
}

double RFC5424Strategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::size_t kMinimumCandidateLength{20};
    static constexpr double kRfc5424Confidence{0.92};
    static constexpr double kNoConfidence{0.0};

    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (is_rfc5424_prefix(line))
        return kRfc5424Confidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
