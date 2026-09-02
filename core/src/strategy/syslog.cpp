module;
#include "utils/log_macros.hpp" // textual macro layer (ADR-3.D4)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// SyslogStrategy — parses BSD syslog and RFC 3339-prefixed syslog lines.
//
// BSD format:   "Jan 15 08:03:22 hostname process[pid]: message"
// RFC 3339:     "2024-01-15T10:30:00Z hostname process[pid]: message"
//
// Hand-written scanner: zero RE2, zero string copies. `line` is already
// arena-stable (copied by LogParser before parse() is called), so every
// substring is a valid zero-copy string_view.
//
// The HEADER — not the timestamp — is what this strategy claims; `scan_syslog_header` below is that
// claim, and both `confidence()` and `parse()` read it (DN-43.D2). Level is INFERRED from the
// message body on both branches: a PRI-less syslog line declares no severity, so content inference
// is not a fallback for it but the correct layer (ADR-22.D3), and `apply_level_lift` still runs
// afterwards and still outranks it, so a dialect's declared marker keeps precedence.

namespace insight::tokenization
{

// The tag scan lives in insight.canon.detail.scan as `take_bounded_syslog_tag`, shared with the
// BGL/Thunderbird branch (F3b). It is bounded to ONE token: the former whole-remainder search is
// how `cache key=session:1021 hit=true` produced the component `cache key=session` and the content
// `1021 hit=true` — a message body moved onto a cube axis (DN-43.D3). Bounding it made the
// no-delimiter branch unreachable FROM HERE; DN-43.D14 removed the branch itself, because it was
// still reachable from the Thunderbird caller, where it ate 1 309 whole message bodies.

std::optional<SyslogHeader> scan_syslog_header(std::string_view line) noexcept
{
    static constexpr std::size_t kBsdTimeLen{8U};        // "HH:MM:SS"
    static constexpr std::string_view kHostReject{"[:"}; // a tag delimiter inside a host token

    std::string_view stamp{line};
    std::string_view rest{line};
    bool bsd{false};

    if (is_bsd_syslog_prefix(line))
    {
        // Walk to the end of "HH:MM:SS": "Mon DD HH:MM:SS" is three space-separated fields the
        // stamp parser reads as one. is_bsd_syslog_prefix already proved the same walk lands a
        // full time field inside the line, so the two trims below are in range by construction.
        std::size_t ts_end{3};
        while (ts_end < line.size() && is_space(line[ts_end]))
            ++ts_end;
        while (ts_end < line.size() && is_digit(line[ts_end]))
            ++ts_end;
        while (ts_end < line.size() && is_space(line[ts_end]))
            ++ts_end;
        ts_end += kBsdTimeLen;
        stamp.remove_suffix(line.size() - ts_end);
        rest.remove_prefix(ts_end);
        sv_skip_ws(rest);
        bsd = true;
    }
    else if (is_rfc3339_prefix(line))
    {
        stamp = sv_take_token(rest);
    }
    else
    {
        return std::nullopt;
    }

    // ── Clause 1: HOST ───────────────────────────────────────────────────────
    // The next token is a hostname only if it is non-empty, carries no tag delimiter, and does NOT
    // parse as a log level. That last test is the whole first defect: `(void)sv_take_token` used to
    // swallow `INFO`/`ERROR` as a hostname, which is how a window of application lines published as
    // uniformly INFO with the level never read at all. Its cost is a host literally named `ERROR`,
    // which the raw fallback still templates honestly (DN-43.D3 clause 1).
    const std::string_view host{sv_take_token(rest)};
    if (host.empty() || host.find_first_of(kHostReject) != std::string_view::npos)
        return std::nullopt;
    if (utils::parse_log_level(host) != LogLevel::Unknown)
        return std::nullopt;

    // ── Clause 2: TAG, bounded to ONE token ──────────────────────────────────
    // A tag-less line is not syslog: the HEADER is this strategy's claim, and `TIMESTAMP HOST`
    // with an unconstrained remainder is a stamp, not a header (DN-43.D11). So an empty result
    // from the shared bounded scan DECLINES here — the one point where this caller and the
    // Thunderbird branch, which keeps the remainder instead, legitimately differ.
    std::string_view body{rest};
    const std::string_view tag{take_bounded_syslog_tag(body)};
    if (tag.empty())
        return std::nullopt;

    return SyslogHeader{.stamp = stamp, .tag = tag, .body = body, .bsd = bsd};
}

// ─────────────────────────────────────────────────────────────────────────────
// IFormatStrategy interface
// ─────────────────────────────────────────────────────────────────────────────

std::expected<ParsedLine, std::string> SyslogStrategy::parse(std::string_view line,
                                                             ArenaAllocator& /*arena*/) const
{
    const std::optional<SyslogHeader> header{scan_syslog_header(line)};
    if (!header)
    {
        // Reachable only under set_format(): auto-detection routes on the same predicate, so a line
        // that reaches parse() by scoring has already passed it. A wrong declaration stays wrong
        // and the declarer owns it — nothing announces it (ADR-23.D2).
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=Syslog parse miss");
        return std::unexpected(
            std::string("SyslogStrategy: line does not match BSD or RFC3339 syslog format"));
    }

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp =
        EventTime::parsed(header->bsd ? utils::parse_bsd_syslog_ts(header->stamp)
                                      : utils::parse_iso8601(header->stamp));
    parsed_line.level = utils::infer_leading_log_level(header->body);
    parsed_line.component = header->tag;
    parsed_line.content = header->body;
    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=Syslog parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level.value()),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat SyslogStrategy::format() const noexcept
{
    return LogFormat::Syslog;
}

double SyslogStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr double kBsdSyslogConfidence{0.85};
    static constexpr double kRfc3339SyslogConfidence{0.80};
    static constexpr double kNoConfidence{0.0};

    const std::optional<SyslogHeader> header{scan_syslog_header(line)};
    if (!header)
        return kNoConfidence;
    return header->bsd ? kBsdSyslogConfidence : kRfc3339SyslogConfidence;
}

} // namespace insight::tokenization
