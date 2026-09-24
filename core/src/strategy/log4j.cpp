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

std::expected<ParsedLine, std::string> Log4jStrategy::parse(std::string_view line,
                                                            ArenaAllocator& /*arena*/) const
{
    const std::optional<Log4jStamp> stamp{find_log4j_stamp(line)};
    if (!stamp.has_value())
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=Log4j parse miss (no ts)");
        return std::unexpected(
            std::string("Log4jStrategy: line does not match any Log4j/Python logging format"));
    }

    const std::string_view ts_str{line.substr(stamp->ts_start, stamp->ts_end - stamp->ts_start)};
    std::string_view rest{line.substr(stamp->ts_end)};

    // invariant: the three layouts share one entry rather than three predicates — the locator names
    // the prefixed one, and the token after the timestamp tells the dash variant from the standard.
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
        // invariant: the thread field is BALANCED — its interior may hold balanced pairs, so
        // the close that ends it is the one at depth 0, never the first `]`.
        // invariant: an unbalanced bracket DECLINES the field, so bytes no predicate
        // validated stay in content rather than being swallowed with the message.
        // refs: ADR-16.D11
        const std::string_view thread_name{sv_take_balanced_bracketed_or_none(rest)};
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

    // invariant: the PREFIXED layout is the locator's verdict, never the stamp's offset — leading
    // whitespace moves the offset without a prefix token.
    // invariant: the locator admits a prefix only when a process id follows the stamp, so the first
    // token here is always that id and the level is always the token after it.
    if (stamp->prefixed)
    {
        (void)sv_take_token(rest);
        const std::string_view level_sv{sv_take_token(rest)};
        const std::string_view component{sv_take_token(rest)};
        // invariant: the request-id section is a FLAT optional skip reaching no field, so an
        // unclosed one keeps its bytes rather than emptying content.
        // refs: DN-43.D11
        if (!rest.empty() && rest[0] == '[')
            (void)sv_take_bracketed_or_none(rest);

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
    // invariant: the SAME balanced thread field as the dash layout, discarded here rather than
    // named — a first-`]` take leaves a stray `]` at the head of the component.
    // refs: ADR-16.D11
    (void)sv_take_balanced_bracketed_or_none(rest);
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
    if (find_log4j_stamp(line).has_value())
        return kLog4jConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
