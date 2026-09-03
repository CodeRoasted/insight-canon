module;
#include "utils/log_macros.hpp" // textual macro layer (ADR-3.D4)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// HealthAppStrategy — parses pipe-delimited HealthApp logs:
//   "20171223-22:15:29:606|Step_LSC|30002312|onStandStepChanged 3579"
//
// Hand-written scanner: zero RE2, zero string copies.

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

    // Format: "YYYYMMDD-HH:MM:SS:mmm|component|process_id|message"
    // The three takes are total by construction: is_health_app_prefix proves all three
    // separators exist (DN-43.D16 — arity is grammar), so none of them can run off the end
    // and swallow a neighbouring field. There is no post-take field guard: a decline here
    // would DELETE the line rather than demote it, and an empty `component` is a positive
    // statement that the record declares no functional source (ADR-16.D5 fail-safe KEEP),
    // not a parse failure.
    std::string_view rest{line};
    const std::string_view ts_str{sv_take_until(rest, '|')};
    const std::string_view component{sv_take_until(rest, '|')};
    (void)sv_take_until(rest, '|'); // skip process_id
    // rest = message

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
    // The shortest line the grammar can accept, derived rather than guessed: 8 date digits + '-'
    // + a 1-digit hour + ':' + a 1-digit minute + ':' + a 1-digit second + ':' + 1 millisecond
    // digit = 16, then the record's three separators = 19. It read 22 while the predicate
    // demanded a 2-digit minute and 3-digit milliseconds; DN-43.O5 widened both, and a stale
    // early-out is a silent false negative — the predicate below would accept a 19-byte line
    // that this guard never lets it see.
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
