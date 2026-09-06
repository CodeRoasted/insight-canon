module;
#include "utils/log_macros.hpp"
#include <cstring>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: an Apache or Nginx Common or Combined Log Format access record.
// invariant: the masker input is assembled as method, url and status — a compact form for
// templating HTTP access patterns.
// invariant: a hand-written scanner with no regex; the only copies on the SUCCESS path are that
// three-part content construction, and a decline builds an error message.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{
    constexpr int kDefaultSuccessStatusCode{200};
} // namespace

std::expected<ParsedLine, std::string> CLFStrategy::parse(std::string_view line,
                                                          ArenaAllocator& arena) const
{
    std::string_view rest{line};

    const std::string_view host{sv_take_token(rest)};
    (void)sv_take_token(rest);
    (void)sv_take_token(rest);

    sv_skip_ws(rest);
    const std::string_view raw_ts{sv_take_bracketed(rest)};
    if (raw_ts.empty())
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=CLF parse miss (no timestamp)");
        return std::unexpected(std::string("CLFStrategy: line does not match CLF/Combined format"));
    }

    sv_skip_ws(rest);
    if (rest.empty() || rest[0] != '"')
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=CLF parse miss (no request)");
        return std::unexpected(std::string("CLFStrategy: line does not match CLF/Combined format"));
    }
    const std::string_view request{sv_take_quoted(rest)};
    const std::string_view status_str{sv_take_token(rest)};
    (void)sv_take_token(rest);

    if (host.empty() || status_str.size() != 3U)
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=CLF parse miss (bad fields)");
        return std::unexpected(std::string("CLFStrategy: line does not match CLF/Combined format"));
    }

    int status_code{kDefaultSuccessStatusCode};
    // invariant: a raw pointer pair rather than iterators, because a string view's iterator is not
    // a pointer on every standard library.
    std::from_chars(status_str.data(), status_str.data() + status_str.size(), status_code);

    // invariant: the request already points into the arena-stable line, so the three parts are
    // views and only the joined content is built.
    std::string_view req{request};
    const std::string_view method{sv_take_token(req)};
    const std::string_view url{sv_take_token(req)};

    // invariant: allocated directly in the arena — one bump-pointer advance and no heap.
    const std::size_t clen{method.size() + 1U + url.size() + 1U + status_str.size()};
    auto* const cbuf{static_cast<char*>(arena.allocate(clen, 1U))};
    const auto buf{std::span<char>{cbuf, clen}};
    std::size_t off{0};
    std::memcpy(buf.data(), method.data(), method.size());
    off += method.size();
    buf[off] = ' ';
    off += 1U;
    std::memcpy(&buf[off], url.data(), url.size());
    off += url.size();
    buf[off] = ' ';
    off += 1U;
    std::memcpy(&buf[off], status_str.data(), status_str.size());

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = EventTime::parsed(parse_clf_timestamp(raw_ts));
    parsed_line.level = EventLevel::declared(status_code_to_level(status_code));
    // invariant: the leading field is a NODE IDENTITY — high-card, and the REQUESTER rather than
    // the producer of the line — so it belongs in host and NOT on the cube's WHERE axis.
    // invariant: it sat in component until the field contract ruled otherwise: component is copied
    // unmasked, so the same octets were masked in content and published raw as a dimension.
    // invariant: component stays EMPTY and that is a POSITIVE statement — this layout declares no
    // functional source.
    // invariant: a constant would be sound only for a grammar that is one server's, and this one is
    // emitted by every web server and load balancer, so a constant would be a fabricated fact.
    // refs: ADR-19.D4, DN-43.D8
    parsed_line.host = host;
    parsed_line.content = {buf.data(), clen};

    INSIGHT_LOG_DEBUG(logging::strategy_logger(), "strategy=CLF parsed host={} level={} has_ts={}",
                      parsed_line.host, to_string(parsed_line.level.value()),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat CLFStrategy::format() const noexcept
{
    return LogFormat::CLF;
}

double CLFStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr double kStrongConfidence{0.95};
    static constexpr double kNoConfidence{0.0};

    // invariant: the gate locates the bracketed timestamp only; the strong-versus-weak distinction
    // was a tie-break, and the parse catches a malformed line anyway.
    if (has_clf_timestamp(line))
        return kStrongConfidence;
    return kNoConfidence;
}

std::optional<Timestamp> CLFStrategy::parse_clf_timestamp(std::string_view timestamp_str)
{
    return utils::parse_clf_timestamp(timestamp_str);
}

// post: an HTTP status code mapped to a level, for downstream anomaly detection.
LogLevel CLFStrategy::status_code_to_level(int status)
{
    static constexpr int kServerErrorStatusStart{500};
    static constexpr int kClientErrorStatusStart{400};
    static constexpr int kRedirectStatusStart{300};

    if (status >= kServerErrorStatusStart)
        return LogLevel::Error;
    if (status >= kClientErrorStatusStart)
        return LogLevel::Warn;
    if (status >= kRedirectStatusStart)
        return LogLevel::Info;
    if (status >= kDefaultSuccessStatusCode)
        return LogLevel::Info;
    return LogLevel::Unknown;
}

} // namespace insight::tokenization
