module;
#include "utils/log_macros.hpp"
#include <cstring>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: an HPC event record — a record id, a node, a facility, an event type, an epoch and a
// flag, then the message.
// invariant: the joined facility and event type is the ONLY constructed string, and it is built
// directly in the arena.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

std::expected<ParsedLine, std::string> HPCStrategy::parse(std::string_view line,
                                                          ArenaAllocator& arena) const
{
    if (!is_hpc_prefix(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=HPC parse miss");
        return std::unexpected(std::string("HPCStrategy: line does not match HPC format"));
    }

    std::string_view rest{line};
    (void)sv_take_token(rest);
    (void)sv_take_token(rest);
    const std::string_view facility{sv_take_token(rest)};
    const std::string_view event_type{sv_take_token(rest)};
    const std::string_view epoch{sv_take_token(rest)};
    (void)sv_take_token(rest);

    const std::size_t clen{facility.size() + 1U + event_type.size()};
    auto* cbuf{static_cast<char*>(arena.allocate(clen, 1U))};
    const std::span<char> buf_span{cbuf, clen};
    std::memcpy(buf_span.data(), facility.data(), facility.size());
    buf_span[facility.size()] = '.';
    std::memcpy(buf_span.subspan(facility.size() + 1U).data(), event_type.data(),
                event_type.size());

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = EventTime::parsed(utils::parse_epoch_timestamp(epoch));
    parsed_line.level = EventLevel{};
    parsed_line.component = {cbuf, clen};
    parsed_line.content = rest;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=HPC parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level.value()),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat HPCStrategy::format() const noexcept
{
    return LogFormat::HPC;
}

double HPCStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::string_view::size_type kMinimumCandidateLength{30};
    static constexpr double kHpcConfidence{0.80};
    static constexpr double kNoConfidence{0.0};
    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (is_hpc_prefix(line))
        return kHpcConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
