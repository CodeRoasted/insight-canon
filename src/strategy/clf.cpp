module;
#include "utils/log_macros.hpp" // textual macro layer (§11.9)
#include <cstring>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// src/1_tokenization/strategies/clf.cpp
//
// CLFStrategy — parses Apache/Nginx Common Log Format and Combined Log Format.
//
// CLF:      host ident user [timestamp] "method url proto" status bytes
// Combined: … "referer" "user-agent"
//
// The Drain input content is constructed as: "METHOD URL STATUS" — a compact
// form useful for template extraction of HTTP access patterns.
//
// Hand-written scanner: zero RE2, zero string copies except for the
// three-part content construction ("METHOD URL STATUS").

namespace insight::tokenization
{

namespace
{
    constexpr int kDefaultSuccessStatusCode{200};
} // namespace

// Group 1: client IP / hostname
// Group 2: CLF timestamp (content of the [...] brackets)
// Group 3: request string "GET /path HTTP/1.0"  — quotes stripped by capture
// Group 4: HTTP status code (3 digits)
// Group 5: response bytes (number or "-")
// Groups 6,7 (optional): referer, user-agent (Combined Log Format)
//
std::expected<ParsedLine, std::string> CLFStrategy::parse(std::string_view line,
                                                          ArenaAllocator& arena) const
{
    std::string_view rest{line};

    const std::string_view host{sv_take_token(rest)};
    (void)sv_take_token(rest); // skip ident
    (void)sv_take_token(rest); // skip user

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
    (void)sv_take_token(rest); // skip bytes

    if (host.empty() || status_str.size() != 3U)
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=CLF parse miss (bad fields)");
        return std::unexpected(std::string("CLFStrategy: line does not match CLF/Combined format"));
    }

    int status_code{kDefaultSuccessStatusCode};
    // const char* (not begin()/end()): portable across stdlibs (MSVC's sv iterator isn't a pointer).
    std::from_chars(status_str.data(), status_str.data() + status_str.size(), status_code);

    // Build content: "METHOD URL STATUS" — extracted from request string.
    // request already points into the arena-stable `line`.
    std::string_view req{request};
    const std::string_view method{sv_take_token(req)};
    const std::string_view url{sv_take_token(req)};

    // Allocate directly in arena — one bump-pointer advance, no heap.
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
    parsed_line.timestamp = parse_clf_timestamp(raw_ts);
    parsed_line.level = status_code_to_level(status_code);
    parsed_line.component = host;
    parsed_line.content = {buf.data(), clen};

    INSIGHT_LOG_DEBUG(
        logging::strategy_logger(), "strategy=CLF parsed component={} level={} has_timestamp={}",
        parsed_line.component, to_string(parsed_line.level), parsed_line.timestamp.has_value());
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

    // Manual gate: locate "[DD/Mon/YYYY:HH:MM:SS". Strong vs weak distinction
    // (the strong path also requires `"GET / HTTP/1.x`) is left to parse() —
    // the cost difference between strong and weak confidences here was a
    // tie-break only, and parse() will catch malformed lines anyway.
    if (has_clf_timestamp(line))
        return kStrongConfidence;
    return kNoConfidence;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

std::optional<Timestamp> CLFStrategy::parse_clf_timestamp(std::string_view timestamp_str)
{
    return utils::parse_clf_timestamp(timestamp_str);
}

// Map HTTP status codes to LogLevel for downstream anomaly detection.
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
