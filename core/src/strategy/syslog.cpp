module;
#include "utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// SyslogStrategy — parses BSD syslog and RFC 3339-prefixed syslog lines.
//
// BSD format:   "Jan 15 08:03:22 hostname process[pid]: message"
// RFC 3339:     "2024-01-15T10:30:00Z hostname process[pid]: message"
//
// Hand-written scanner: zero RE2, zero string copies. `line` is already
// arena-stable (copied by LogParser before parse() is called), so every
// substring is a valid zero-copy string_view.

namespace insight::tokenization
{

// extract_syslog_tag now lives in insight.canon.detail.scan (shared with the BGL/Thunderbird
// branch, F3b) — the `[pid]` is stripped (identity), leaving the daemon name.

// ─────────────────────────────────────────────────────────────────────────────
// IFormatStrategy interface
// ─────────────────────────────────────────────────────────────────────────────

std::expected<ParsedLine, std::string> SyslogStrategy::parse(std::string_view line,
                                                             ArenaAllocator& /*arena*/) const
{
    static constexpr std::size_t kBsdTimeLen{8U}; // "HH:MM:SS"

    if (line.size() < kBsdMinLen)
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=Syslog parse miss (too short)");
        return std::unexpected(std::string("SyslogStrategy: line too short"));
    }

    // ── BSD syslog: "Mon DD HH:MM:SS hostname process[pid]: message" ──────
    if (is_bsd_syslog_prefix(line))
    {
        // Walk to the end of "HH:MM:SS".
        std::size_t ts_end{3};
        while (ts_end < line.size() && is_space(line[ts_end]))
            ++ts_end;
        while (ts_end < line.size() && is_digit(line[ts_end]))
            ++ts_end;
        while (ts_end < line.size() && is_space(line[ts_end]))
            ++ts_end;
        ts_end += kBsdTimeLen; // "HH:MM:SS"

        const std::string_view raw_ts{line.substr(0, ts_end)};
        std::string_view rest{line.substr(ts_end)};
        sv_skip_ws(rest);
        (void)sv_take_token(rest); // skip hostname
        const std::string_view tag{extract_syslog_tag(rest)};

        ParsedLine parsed_line;
        parsed_line.raw_line = line;
        parsed_line.timestamp = utils::parse_bsd_syslog_ts(raw_ts);
        parsed_line.level = LogLevel::Unknown;
        parsed_line.component = tag;
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=Syslog parsed component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level),
                          parsed_line.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed_line};
    }

    // ── RFC 3339: "YYYY-MM-DDTHH:MM:SS[Z|±HH:MM] hostname process[pid]: msg"
    if (is_rfc3339_prefix(line))
    {
        std::string_view rest{line};
        const std::string_view raw_ts{sv_take_token(rest)};
        (void)sv_take_token(rest); // skip hostname
        const std::string_view tag{extract_syslog_tag(rest)};

        ParsedLine parsed_line;
        parsed_line.raw_line = line;
        parsed_line.timestamp = utils::parse_iso8601(raw_ts);
        parsed_line.level = LogLevel::Unknown;
        parsed_line.component = tag;
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=Syslog parsed component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level),
                          parsed_line.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed_line};
    }

    INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=Syslog parse miss");
    return std::unexpected(
        std::string("SyslogStrategy: line does not match BSD or RFC3339 syslog format"));
}

LogFormat SyslogStrategy::format() const noexcept
{
    return LogFormat::Syslog;
}

double SyslogStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr double kBsdSyslogConfidence{0.85};
    static constexpr double kRfc3339SyslogConfidence{0.80};
    static constexpr double kNoConfidence{0.0};

    if (line.size() < kBsdMinLen)
        return kNoConfidence;
    if (is_bsd_syslog_prefix(line))
        return kBsdSyslogConfidence;
    if (is_rfc3339_prefix(line))
        return kRfc3339SyslogConfidence;
    return kNoConfidence;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

std::optional<Timestamp> SyslogStrategy::parse_bsd_timestamp(std::string_view timestamp_str)
{
    return utils::parse_bsd_syslog_ts(timestamp_str);
}

std::optional<Timestamp> SyslogStrategy::parse_iso_timestamp(std::string_view timestamp_str)
{
    return utils::parse_iso8601(timestamp_str);
}

} // namespace insight::tokenization
