module;
#include "utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// RFC5424Strategy — parses IETF RFC 5424 syslog format:
//   "<PRI>VERSION TIMESTAMP HOSTNAME APP-NAME PROCID MSGID [SD] MSG"
//   e.g. "<134>1 2024-01-15T10:30:00.003Z server sshd 1234 ID47 - Accepted password"
//   e.g. "<165>1 2024-01-15T10:30:00Z router - - - Network interface down"
//
// PRI = facility * 8 + severity (RFC 5424 §6.2.1)
// Severity: 0=emerg, 1=alert, 2=crit, 3=error, 4=warn, 5=notice, 6=info, 7=debug
//
// Hand-written scanner: zero RE2, zero string copies.

namespace insight::tokenization
{

namespace
{

    // NOLINTBEGIN(readability-magic-numbers)

    LogLevel severity_to_level(int pri) noexcept
    {
        const int severity{pri & 0x07}; // NOLINT low 3 bits
        switch (severity)
        {
        case 0:
            [[fallthrough]];
        case 1:
            [[fallthrough]];
        case 2:
            return LogLevel::Fatal; // emerg/alert/crit
        case 3:
            return LogLevel::Error; // err
        case 4:
            return LogLevel::Warn; // warning
        case 5:                    // NOLINT(readability-magic-numbers)
            [[fallthrough]];
        case 6:                     // NOLINT(readability-magic-numbers)
            return LogLevel::Info;  // notice/info
        case 7:                     // NOLINT(readability-magic-numbers)
            return LogLevel::Debug; // debug
        default:
            return LogLevel::Unknown;
        }
    }

    // NOLINTEND(readability-magic-numbers)

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
    rest.remove_prefix(1U); // skip '<' (validated by is_rfc5424_prefix)
    const std::string_view pri_str{sv_take_until(rest, '>')};

    (void)sv_take_token(rest); // skip version
    const std::string_view timestamp_str{sv_take_token(rest)};
    const std::string_view hostname{sv_take_token(rest)};
    const std::string_view appname{sv_take_token(rest)};
    (void)sv_take_token(rest); // skip procid
    (void)sv_take_token(rest); // skip msgid

    // Strip structured-data prefix "- " or "[sd-id ...] "
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
    parsed.timestamp = utils::parse_iso8601(timestamp_str);
    parsed.level = severity_to_level(pri);
    parsed.component = (appname != "-") ? appname : hostname;
    parsed.content = msg;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=RFC5424 parsed component={} level={} has_timestamp={}",
                      parsed.component, to_string(parsed.level), parsed.timestamp.has_value());
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
