module;
#include "utils/log_macros.hpp"
#include <cstring>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: an IIS W3C extended access record, in either the full or the short field order.
// invariant: the content is the method, the URI and the remaining fields, assembled in the arena;
// the status is the first three-digit HTTP code in that remainder.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{
    constexpr std::size_t kTimestampPrefixLen{19};

    constexpr bool is_http_method(std::string_view tok) noexcept
    {
        return tok == "GET" || tok == "POST" || tok == "PUT" || tok == "DELETE" || tok == "HEAD" ||
               tok == "OPTIONS" || tok == "PATCH" || tok == "PROPFIND" || tok == "MKCOL";
    }

    // post: the level implied by the first three-digit HTTP status code in the remainder.
    LogLevel status_to_level(std::string_view rest) noexcept
    {
        std::string_view scan{rest};
        while (!scan.empty())
        {
            const std::string_view tok{sv_take_token(scan)};
            if (tok.size() == 3U &&
                (tok[0] == '2' || tok[0] == '3' || tok[0] == '4' || tok[0] == '5') &&
                is_digit(tok[1]) && is_digit(tok[2]))
            {
                if (tok[0] == '5')
                    return LogLevel::Error;
                if (tok[0] == '4')
                    return LogLevel::Warn;
                return LogLevel::Info;
            }
        }
        return LogLevel::Info;
    }

    // post: the joined content in arena storage, returned as a view over it.
    std::string_view build_content(ArenaAllocator& arena, std::string_view method,
                                   std::string_view uri, std::string_view rest) noexcept
    {
        const std::size_t total{method.size() + 1U + uri.size() +
                                (rest.empty() ? 0U : 1U + rest.size())};
        auto* buf{static_cast<char*>(arena.allocate(total, 1U))};
        const std::span<char> buf_span{buf, total};
        std::size_t off{0};
        std::memcpy(buf_span.data(), method.data(), method.size());
        off += method.size();
        buf_span[off++] = ' ';
        std::memcpy(buf_span.subspan(off).data(), uri.data(), uri.size());
        off += uri.size();
        if (!rest.empty())
        {
            buf_span[off++] = ' ';
            std::memcpy(buf_span.subspan(off).data(), rest.data(), rest.size());
        }
        return {buf, total};
    }

} // namespace

std::expected<ParsedLine, std::string> IISW3CStrategy::parse(std::string_view line,
                                                             ArenaAllocator& arena) const
{
    // invariant: a directive line is skipped rather than parsed — this format interleaves its own
    // header comments with records.
    if (!line.empty() && line[0] == '#')
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=IISW3C comment_or_directive");
        return std::unexpected(std::string("IISW3CStrategy: comment/directive line"));
    }

    if (line.size() < kTimestampPrefixLen + 1U)
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=IISW3C parse miss (short)");
        return std::unexpected(std::string("IISW3CStrategy: line does not match IIS W3C format"));
    }

    const std::string_view ts_str{line.substr(0, kTimestampPrefixLen)};
    std::string_view rest{line.substr(kTimestampPrefixLen)};
    sv_skip_ws(rest);

    // invariant: the full and short field orders are told apart by whether the next token is an
    // HTTP method, so no separate declaration is needed.
    std::string_view component{"IIS"};

    const std::string_view tok1{sv_take_token(rest)};
    if (!is_http_method(tok1))
    {
        const std::string_view server{sv_take_token(rest)};
        component = server.empty() ? tok1 : server;
        const std::string_view method{sv_take_token(rest)};
        const std::string_view uri{sv_take_token(rest)};
        if (method.empty() || uri.empty() || !is_http_method(method))
        {
            INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=IISW3C parse miss (no method)");
            return std::unexpected(
                std::string("IISW3CStrategy: line does not match IIS W3C format"));
        }
        ParsedLine parsed;
        parsed.raw_line = line;
        parsed.timestamp = EventTime::parsed(utils::parse_iso8601(ts_str));
        parsed.level = EventLevel::declared(status_to_level(rest));
        parsed.component = component;
        parsed.content = build_content(arena, method, uri, rest);
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=IISW3C parsed component={} level={} has_timestamp={}",
                          parsed.component, to_string(parsed.level.value()),
                          parsed.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed};
    }

    const std::string_view uri{sv_take_token(rest)};
    ParsedLine parsed;
    parsed.raw_line = line;
    parsed.timestamp = EventTime::parsed(utils::parse_iso8601(ts_str));
    parsed.level = EventLevel::declared(status_to_level(rest));
    parsed.component = "IIS";
    parsed.content = build_content(arena, tok1, uri, rest);
    INSIGHT_LOG_DEBUG(
        logging::strategy_logger(), "strategy=IISW3C parsed component={} level={} has_timestamp={}",
        parsed.component, to_string(parsed.level.value()), parsed.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed};
}

LogFormat IISW3CStrategy::format() const noexcept
{
    return LogFormat::IISW3C;
}

double IISW3CStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::string_view::size_type kMinimumCandidateLength{22};
    static constexpr double kIisW3cConfidence{0.87};
    static constexpr double kNoConfidence{0.0};

    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (!line.empty() && line[0] == '#')
        return kNoConfidence;
    if (!is_iso_datetime_space_prefix(line, /*require_fraction=*/false))
        return kNoConfidence;
    if (line.size() <= kTimestampPrefixLen || !is_space(line[kTimestampPrefixLen]))
        return kNoConfidence;
    return kIisW3cConfidence;
}

} // namespace insight::tokenization
