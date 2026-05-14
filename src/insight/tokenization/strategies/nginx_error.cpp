// src/1_tokenization/strategies/nginx_error.cpp
//
// NginxErrorStrategy — parses Nginx error-log format:
//   "2024/03/27 10:15:23 [error] 12345#0: *99 connect() failed"
//
// Hand-written scanner: zero RE2, zero string copies.

#include "insight/tokenization/strategies/nginx_error.hpp"

#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include "insight/tokenization/strategies/detail/fast_gates.hpp"
#include "insight/utils/logger.hpp"
#include <expected>
#include "insight/utils/time_utils.hpp"

namespace insight::tokenization
{

namespace
{
    constexpr std::string_view::size_type kMinimumCandidateLength{25};
    constexpr double kNoConfidence{0.0};
    constexpr double kNginxErrorConfidence{0.89};

} // namespace

std::expected<ParsedLine, std::string> NginxErrorStrategy::parse(std::string_view line,
                                                      ArenaAllocator& /*arena*/) const
{
    if (!detail::is_nginx_error_prefix(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=NginxError parse miss");
        return std::unexpected(std::string("NginxErrorStrategy: line does not match Nginx error format"));
    }

    // "YYYY/MM/DD HH:MM:SS [level] PID#TID: msg"
    // Timestamp = first 19 chars: "YYYY/MM/DD HH:MM:SS"
    std::string_view rest{line};
    const std::string_view ts_str{detail::sv_take_n(rest, 19U)};
    detail::sv_skip_ws(rest);

    const std::string_view level_sv{detail::sv_take_bracketed(rest)};
    if (level_sv.empty())
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=NginxError parse miss (no level)");
        return std::unexpected(std::string("NginxErrorStrategy: line does not match Nginx error format"));
    }

    (void)detail::sv_take_token(rest); // skip "PID#TID:"
    // rest = message (may start with "*CID ")

    ParsedLine parsed;
    parsed.raw_line = line;
    parsed.timestamp = utils::parse_nginx_error_ts(ts_str);
    parsed.level = utils::parse_log_level(level_sv);
    parsed.component = "nginx";
    parsed.content = rest;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=NginxError parsed component={} level={} has_timestamp={}",
                      parsed.component, to_string(parsed.level), parsed.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed};
}

LogFormat NginxErrorStrategy::format() const noexcept
{
    return LogFormat::NginxError;
}

double NginxErrorStrategy::confidence(std::string_view line) const noexcept
{
    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (detail::is_nginx_error_prefix(line))
        return kNginxErrorConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
