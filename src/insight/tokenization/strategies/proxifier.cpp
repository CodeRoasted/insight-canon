// src/1_tokenization/strategies/proxifier.cpp
//
// ProxifierStrategy — parses Proxifier network log format:
//   "[10.30 16:49:06] chrome.exe - proxy.cse.cuhk.edu.hk:5070 open through proxy"
//   "[10.30 16:49:07] chrome.exe *64 close, 0 bytes sent, ..."
//
// Hand-written scanner: zero RE2, zero string copies.

#include "insight/tokenization/strategies/proxifier.hpp"

#include <optional>
#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include "insight/tokenization/strategies/detail/fast_gates.hpp"
#include "insight/utils/logger.hpp"
#include <expected>

namespace insight::tokenization
{

namespace
{
    constexpr std::string_view::size_type kMinimumCandidateLength{18};
    constexpr double kNoConfidence{0.0};
    constexpr double kProxifierConfidence{0.90};

} // namespace

std::expected<ParsedLine, std::string> ProxifierStrategy::parse(std::string_view line,
                                                     ArenaAllocator& /*arena*/) const
{
    if (!detail::is_proxifier_prefix(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=Proxifier parse miss");
        return std::unexpected(std::string("ProxifierStrategy: line does not match Proxifier format"));
    }

    // "[DD.MM HH:MM:SS] process [-|*N] message"
    std::string_view rest{line};
    (void)detail::sv_take_bracketed(rest); // skip timestamp bracket (not stored)
    detail::sv_skip_ws(rest);
    const std::string_view process{detail::sv_take_token(rest)};

    // Skip optional separator: "-" or "*N"
    if (!rest.empty() && (rest[0] == '-' || rest[0] == '*'))
        (void)detail::sv_take_token(rest);

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = std::nullopt; // Proxifier ts lacks full date
    parsed_line.level = LogLevel::Unknown;
    parsed_line.component = process;
    parsed_line.content = rest;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=Proxifier parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat ProxifierStrategy::format() const noexcept
{
    return LogFormat::Proxifier;
}

double ProxifierStrategy::confidence(std::string_view line) const noexcept
{
    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (detail::is_proxifier_prefix(line))
        return kProxifierConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
