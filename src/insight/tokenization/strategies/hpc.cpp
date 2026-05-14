// src/1_tokenization/strategies/hpc.cpp
//
// HPCStrategy — parses HPC event logs:
//   "134681 node-246 unix.hw state_change.unavailable 1077804742 1 Component State Change: ..."
//
// Hand-written scanner: zero RE2. Component "facility.event_type" is the
// only constructed string; built directly in the arena.

#include "insight/tokenization/strategies/hpc.hpp"

#include <cstring>
#include <span>
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
    constexpr std::string_view::size_type kMinimumCandidateLength{30};
    constexpr double kNoConfidence{0.0};
    constexpr double kHpcConfidence{0.80};

} // namespace

std::expected<ParsedLine, std::string> HPCStrategy::parse(std::string_view line,
                                                          ArenaAllocator& arena) const
{
    if (!detail::is_hpc_prefix(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=HPC parse miss");
        return std::unexpected(std::string("HPCStrategy: line does not match HPC format"));
    }

    std::string_view rest{line};
    (void)detail::sv_take_token(rest); // skip record_id
    (void)detail::sv_take_token(rest); // skip node (not stored)
    const std::string_view facility{detail::sv_take_token(rest)};
    const std::string_view event_type{detail::sv_take_token(rest)};
    const std::string_view epoch{detail::sv_take_token(rest)};
    (void)detail::sv_take_token(rest); // skip flag

    // Build "facility.event_type" in arena.
    const std::size_t clen{facility.size() + 1U + event_type.size()};
    auto* cbuf{static_cast<char*>(arena.allocate(clen, 1U))};
    const std::span<char> buf_span{cbuf, clen};
    std::memcpy(buf_span.data(), facility.data(), facility.size());
    buf_span[facility.size()] = '.';
    std::memcpy(buf_span.subspan(facility.size() + 1U).data(), event_type.data(),
                event_type.size());

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = utils::parse_epoch_timestamp(epoch);
    parsed_line.level = LogLevel::Unknown;
    parsed_line.component = {cbuf, clen};
    parsed_line.content = rest;

    INSIGHT_LOG_DEBUG(
        logging::strategy_logger(), "strategy=HPC parsed component={} level={} has_timestamp={}",
        parsed_line.component, to_string(parsed_line.level), parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat HPCStrategy::format() const noexcept
{
    return LogFormat::HPC;
}

double HPCStrategy::confidence(std::string_view line) const noexcept
{
    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (detail::is_hpc_prefix(line))
        return kHpcConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
