module;
#include "utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// src/1_tokenization/strategies/log4j.cpp
//
// Log4jStrategy — parses Java Log4j / Python logging formats.
//
// Standard Log4j (Hadoop):
//   "2015-10-18 18:01:47,978 INFO [main] org.apache.hadoop.mapreduce: msg"
//
// Dash-variant (Zookeeper):
//   "2015-07-29 17:41:44,747 - INFO  [QuorumPeer[myid=1]] - Notification time out"
//
// Python/OpenStack:
//   "nova-api.log.1 2017-05-16 00:00:00.008 25746 INFO nova.osapi [req-id] msg"
//
// Hand-written scanner: zero RE2, zero string copies.

namespace insight::tokenization
{

namespace
{
    // Returns true and sets ts_start to the index where the ISO timestamp begins.
    constexpr bool find_log4j_ts_start(std::string_view line, std::size_t& ts_start) noexcept
    {
        static constexpr std::size_t kIsoTimestampMinLen{20U};

        if (is_iso_datetime_space_prefix(line, /*require_fraction=*/true))
        {
            ts_start = 0;
            return true;
        }
        // OpenStack: optional "prefix " before timestamp
        constexpr std::size_t kScanLimit{96U};
        const std::size_t limit{line.size() < kScanLimit ? line.size() : kScanLimit};
        for (std::size_t i{1U}; i + kIsoTimestampMinLen <= limit; ++i)
        {
            if (!is_space(line[i - 1U]))
                continue;
            if (is_iso_datetime_space_prefix(line.substr(i), /*require_fraction=*/true))
            {
                ts_start = i;
                return true;
            }
        }
        return false;
    }

} // namespace

std::expected<ParsedLine, std::string> Log4jStrategy::parse(std::string_view line,
                                                            ArenaAllocator& /*arena*/) const
{
    std::size_t ts_start{0};
    if (!find_log4j_ts_start(line, ts_start))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=Log4j parse miss (no ts)");
        return std::unexpected(
            std::string("Log4jStrategy: line does not match any Log4j/Python logging format"));
    }

    // ── Extract 23-char timestamp "YYYY-MM-DD HH:MM:SS,mmm" ───────────────
    std::string_view rest{line.substr(ts_start)};
    const std::string_view ts_str{sv_take_n(rest, 23U)};

    // Peek at next token to identify variant.
    sv_skip_ws(rest);

    if (rest.empty())
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=Log4j parse miss (no fields)");
        return std::unexpected(
            std::string("Log4jStrategy: line does not match any Log4j/Python logging format"));
    }

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = utils::parse_log4j_timestamp(ts_str);

    // ── Dash variant: "ts - LEVEL [thread] - msg" ─────────────────────────
    if (rest[0] == '-' && (rest.size() < 2U || is_space(rest[1])))
    {
        (void)sv_take_token(rest); // consume '-'
        const std::string_view level_sv{sv_take_token(rest)};
        const std::string_view thread_name{sv_take_bracketed(rest)};
        sv_skip_ws(rest);
        if (!rest.empty() && rest[0] == '-')
            (void)sv_take_token(rest); // consume trailing '-'

        parsed_line.level = utils::parse_log_level(level_sv);
        parsed_line.component = thread_name;
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=Log4j dash component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level),
                          parsed_line.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed_line};
    }

    // ── OpenStack variant: "prefix ts PID LEVEL component [req-id] msg" ───
    // ts_start > 0 means a prefix token was skipped; next non-ts token is PID.
    if (ts_start > 0U)
    {
        // next token should be a numeric PID
        const std::string_view pid_or_level{sv_take_token(rest)};
        bool is_pid{true};
        for (const char chr : pid_or_level)
            if (!is_digit(chr))
            {
                is_pid = false;
                break;
            }

        std::string_view level_sv;
        if (is_pid)
            level_sv = sv_take_token(rest);
        else
            level_sv = pid_or_level;

        const std::string_view component{sv_take_token(rest)};
        // Skip optional "[req-id ...]"
        if (!rest.empty() && rest[0] == '[')
            (void)sv_take_bracketed(rest);

        parsed_line.level = utils::parse_log_level(level_sv);
        parsed_line.component = component;
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=Log4j openstack component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level),
                          parsed_line.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed_line};
    }

    // ── Standard variant: "ts LEVEL [thread] component: msg" ──────────────
    const std::string_view level_sv{sv_take_token(rest)};
    (void)sv_take_bracketed(rest); // skip [thread]
    sv_skip_ws(rest);

    // Component is until ':' or " -"
    const std::string_view component{sv_take_until(rest, ':')};
    // sv_take_until already consumed ':', rest is now " msg" or "- msg"
    sv_skip_ws(rest);
    if (!rest.empty() && rest[0] == '-')
        (void)sv_take_token(rest);

    parsed_line.level = utils::parse_log_level(level_sv);
    parsed_line.component = component;
    parsed_line.content = rest;
    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=Log4j standard component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat Log4jStrategy::format() const noexcept
{
    return LogFormat::Log4j;
}

double Log4jStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::string_view::size_type kMinimumCandidateLength{23};
    static constexpr double kLog4jConfidence{0.82};
    static constexpr double kNoConfidence{0.0};

    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    std::size_t ts_start{0};
    if (find_log4j_ts_start(line, ts_start))
        return kLog4jConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
