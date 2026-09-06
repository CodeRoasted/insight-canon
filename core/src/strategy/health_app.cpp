module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: a pipe-delimited HealthApp record — a compact date and clock, a component, a process id
// and the message.
// invariant: a hand-written scanner with no regex and, on the SUCCESS path, no string copies — a
// decline builds an error message.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

std::expected<ParsedLine, std::string> HealthAppStrategy::parse(std::string_view line,
                                                                ArenaAllocator& /*arena*/) const
{
    if (!is_health_app_prefix(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=HealthApp parse miss");
        return std::unexpected(
            std::string("HealthAppStrategy: line does not match HealthApp format"));
    }

    // invariant: the three takes are TOTAL BY CONSTRUCTION — the claim predicate proved all three
    // separators exist, so none of them can run off the end and swallow a neighbouring field.
    // invariant: there is deliberately NO post-take field guard: a decline here would DELETE the
    // line rather than demote it.
    // invariant: an empty component is a POSITIVE statement that the record declares no functional
    // source, never a parse failure.
    // refs: ADR-16.D5, DN-43.D16
    std::string_view rest{line};
    const std::string_view ts_str{sv_take_until(rest, '|')};
    const std::string_view component{sv_take_until(rest, '|')};
    (void)sv_take_until(rest, '|');

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = EventTime::parsed(utils::parse_health_app_ts(ts_str));
    parsed_line.level = EventLevel{};
    parsed_line.component = component;
    parsed_line.content = rest;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=HealthApp parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level.value()),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat HealthAppStrategy::format() const noexcept
{
    return LogFormat::HealthApp;
}

double HealthAppStrategy::confidence(std::string_view line) const noexcept
{
    // invariant: the shortest acceptable line is DERIVED rather than guessed — eight date digits,
    // a separator, one digit per clock field, one millisecond digit, and the record's three pipes.
    // invariant: it read 22 while the predicate demanded a two-digit minute and three millisecond
    // digits, and widening one without the other is a SILENT false negative.
    // invariant: a stale early-out would reject a 19-byte line the predicate would have accepted.
    // refs: DN-43.O5
    static constexpr std::string_view::size_type kMinimumCandidateLength{19};
    static constexpr double kHealthAppConfidence{0.92};
    static constexpr double kNoConfidence{0.0};
    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (is_health_app_prefix(line))
        return kHealthAppConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
