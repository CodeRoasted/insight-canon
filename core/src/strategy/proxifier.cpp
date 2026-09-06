module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: a Proxifier network record — a bracketed day and clock, a process name, an optional
// separator, then the connection event.
// invariant: a hand-written scanner with no regex and, on the SUCCESS path, no string copies — a
// decline builds an error message.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

std::expected<ParsedLine, std::string> ProxifierStrategy::parse(std::string_view line,
                                                                ArenaAllocator& /*arena*/) const
{
    if (!is_proxifier_prefix(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=Proxifier parse miss");
        return std::unexpected(
            std::string("ProxifierStrategy: line does not match Proxifier format"));
    }

    std::string_view rest{line};
    (void)sv_take_bracketed(rest);
    sv_skip_ws(rest);
    const std::string_view process{sv_take_token(rest)};

    if (!rest.empty() && (rest[0] == '-' || rest[0] == '*'))
        (void)sv_take_token(rest);

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    // invariant: the timestamp is PARSED as absent rather than declared: the prefix carries a
    // month-day pair and a clock, but NO YEAR, so no instant can be built without inventing one.
    parsed_line.timestamp = EventTime::parsed(std::nullopt);
    parsed_line.level = EventLevel{};
    parsed_line.component = process;
    parsed_line.content = rest;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=Proxifier parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level.value()),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat ProxifierStrategy::format() const noexcept
{
    return LogFormat::Proxifier;
}

double ProxifierStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::string_view::size_type kMinimumCandidateLength{18};
    static constexpr double kProxifierConfidence{0.90};
    static constexpr double kNoConfidence{0.0};
    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (is_proxifier_prefix(line))
        return kProxifierConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
