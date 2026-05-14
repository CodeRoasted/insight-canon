// src/1_tokenization/strategies/syslog.cpp
//
// SyslogStrategy — parses BSD syslog and RFC 3339-prefixed syslog lines.
//
// BSD format:   "Jan 15 08:03:22 hostname process[pid]: message"
// RFC 3339:     "2024-01-15T10:30:00Z hostname process[pid]: message"
//
// Hand-written scanner: zero RE2, zero string copies. `line` is already
// arena-stable (copied by LogParser before parse() is called), so every
// substring is a valid zero-copy string_view.

#include "insight/tokenization/strategies/syslog.hpp"

#include <optional>
#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include "insight/tokenization/strategies/detail/fast_gates.hpp"
#include "insight/utils/logger.hpp"
#include "insight/utils/time_utils.hpp"
#include <expected>

namespace insight::tokenization
{

namespace
{
    constexpr double kNoConfidence{0.0};
    constexpr double kBsdSyslogConfidence{0.85};
    constexpr double kRfc3339SyslogConfidence{0.80};
    constexpr std::size_t kBsdMinLen{15U};
    constexpr std::size_t kBsdTimeLen{8U}; // "HH:MM:SS"

    // Parse "process[pid]:" tag section. Advances `rest` past ':' and
    // any trailing whitespace. Returns the process name (before '[' or ':').
    [[nodiscard]] std::string_view extract_syslog_tag(std::string_view& rest) noexcept
    {
        const auto delim = rest.find_first_of("[:");
        std::string_view tag;
        if (delim == std::string_view::npos)
        {
            tag = rest;
            rest = {};
        }
        else
        {
            tag = rest.substr(0, delim);
            while (!tag.empty() && detail::is_space(tag.back()))
                tag.remove_suffix(1U);
            rest = rest.substr(delim);
            if (!rest.empty() && rest[0] == '[')
            {
                const auto rb_pos = rest.find(']');
                rest = rest.substr(rb_pos != std::string_view::npos ? rb_pos + 1U : 1U);
            }
            if (!rest.empty() && rest[0] == ':')
                rest.remove_prefix(1U);
            detail::sv_skip_ws(rest);
        }
        return tag;
    }

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// IFormatStrategy interface
// ─────────────────────────────────────────────────────────────────────────────

std::expected<ParsedLine, std::string> SyslogStrategy::parse(std::string_view line,
                                                             ArenaAllocator& /*arena*/) const
{
    if (line.size() < kBsdMinLen)
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=Syslog parse miss (too short)");
        return std::unexpected(std::string("SyslogStrategy: line too short"));
    }

    // ── BSD syslog: "Mon DD HH:MM:SS hostname process[pid]: message" ──────
    if (detail::is_bsd_syslog_prefix(line))
    {
        // Walk to the end of "HH:MM:SS".
        std::size_t ts_end{3};
        while (ts_end < line.size() && detail::is_space(line[ts_end]))
            ++ts_end;
        while (ts_end < line.size() && detail::is_digit(line[ts_end]))
            ++ts_end;
        while (ts_end < line.size() && detail::is_space(line[ts_end]))
            ++ts_end;
        ts_end += kBsdTimeLen; // "HH:MM:SS"

        const std::string_view raw_ts{line.substr(0, ts_end)};
        std::string_view rest{line.substr(ts_end)};
        detail::sv_skip_ws(rest);
        (void)detail::sv_take_token(rest); // skip hostname
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
    if (detail::is_rfc3339_prefix(line))
    {
        std::string_view rest{line};
        const std::string_view raw_ts{detail::sv_take_token(rest)};
        (void)detail::sv_take_token(rest); // skip hostname
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
    if (line.size() < kBsdMinLen)
        return kNoConfidence;
    if (detail::is_bsd_syslog_prefix(line))
        return kBsdSyslogConfidence;
    if (detail::is_rfc3339_prefix(line))
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
