module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: a Java Log4j or Python logging record in one of three layouts — the standard one, the
// dash variant, and the prefixed one that carries a process id.
// invariant: a hand-written scanner with no regex and, on the SUCCESS path, no string copies — a
// decline builds an error message.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{
    // post: true, with the index where the timestamp begins, when the line carries one.
    constexpr bool find_log4j_ts_start(std::string_view line, std::size_t& ts_start) noexcept
    {
        static constexpr std::size_t kIsoTimestampMinLen{20U};

        if (is_iso_datetime_space_prefix(line, /*require_fraction=*/true))
        {
            ts_start = 0;
            return true;
        }
        // invariant: the search for an optional leading prefix is BOUNDED, so a line that carries
        // no timestamp at all costs a bounded scan rather than a whole-line one.
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

    std::string_view rest{line.substr(ts_start)};
    const std::string_view ts_str{sv_take_n(rest, 23U)};

    // invariant: the variant is identified by peeking at the token after the timestamp, so the
    // three layouts share one entry rather than three predicates.
    sv_skip_ws(rest);

    if (rest.empty())
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=Log4j parse miss (no fields)");
        return std::unexpected(
            std::string("Log4jStrategy: line does not match any Log4j/Python logging format"));
    }

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = EventTime::parsed(utils::parse_log4j_timestamp(ts_str));

    if (rest[0] == '-' && (rest.size() < 2U || is_space(rest[1])))
    {
        (void)sv_take_token(rest);
        const std::string_view level_sv{sv_take_token(rest)};
        const std::string_view thread_name{sv_take_bracketed(rest)};
        sv_skip_ws(rest);
        if (!rest.empty() && rest[0] == '-')
            (void)sv_take_token(rest);

        parsed_line.level = EventLevel::declared(utils::parse_log_level(level_sv));
        parsed_line.component = thread_name;
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=Log4j dash component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level.value()),
                          parsed_line.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed_line};
    }

    // invariant: a non-zero timestamp offset means a prefix token was skipped, which is what
    // identifies the layout that carries a process id.
    if (ts_start > 0U)
    {
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
        if (!rest.empty() && rest[0] == '[')
            (void)sv_take_bracketed(rest);

        parsed_line.level = EventLevel::declared(utils::parse_log_level(level_sv));
        parsed_line.component = component;
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=Log4j openstack component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level.value()),
                          parsed_line.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed_line};
    }

    const std::string_view level_sv{sv_take_token(rest)};
    (void)sv_take_bracketed(rest);
    sv_skip_ws(rest);

    // invariant: the colon TERMINATES the component, so its ABSENCE means this line names no
    // component — not that the component is the rest of the line.
    // invariant: the unbounded form emptied content and put the whole message on the cube's WHERE
    // axis.
    // refs: ADR-16.D9
    const std::string_view component{sv_take_until_or_none(rest, ':')};
    sv_skip_ws(rest);
    if (!rest.empty() && rest[0] == '-')
        (void)sv_take_token(rest);

    parsed_line.level = EventLevel::declared(utils::parse_log_level(level_sv));
    parsed_line.component = component;
    parsed_line.content = rest;
    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=Log4j standard component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level.value()),
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
