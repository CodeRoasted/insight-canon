module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: a BSD syslog or RFC-3339-prefixed syslog record — a stamp, a host, a bracketed-pid tag,
// then the message.
// invariant: the line is already arena-stable when parse is called, so every substring is a valid
// zero-copy view.
// invariant: the HEADER — not the timestamp — is what this strategy claims, and one predicate
// below IS that claim, read by both the confidence score and the parse.
// invariant: the level is INFERRED from the message body on BOTH branches: a priority-less line
// declares no severity, so content inference is the correct layer rather than a fallback.
// invariant: the declared-marker lift still runs afterwards and still outranks it, so a dialect's
// announced level keeps precedence.
// refs: ADR-22.D3, DN-43.D2
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

// invariant: the tag scan is SHARED with the supercomputer branch, so that shape has one
// definition.
// invariant: it is bounded to ONE token — the former whole-remainder search is how a
// key-equals-value message body produced a component holding half the message.
// invariant: bounding it made the no-delimiter branch unreachable FROM HERE, and removing the
// branch itself mattered because the other caller still reached it.
// invariant: there it ate 1 309 whole message bodies onto the component field.
// refs: DN-43.D3, DN-43.D14
std::optional<SyslogHeader> scan_syslog_header(std::string_view line) noexcept
{
    static constexpr std::size_t kBsdTimeLen{8U};
    static constexpr std::string_view kHostReject{"[:"};

    std::string_view stamp{line};
    std::string_view rest{line};
    bool bsd{false};

    if (is_bsd_syslog_prefix(line))
    {
        // invariant: the BSD stamp is three space-separated fields the stamp parser reads as one,
        // and the prefix predicate already proved the walk lands a full time field inside the line.
        // invariant: so the two trims below are in range by construction.
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

    // invariant: the next token is a hostname only if it is non-empty, carries no tag delimiter,
    // and does NOT parse as a log level.
    // invariant: that last test is the whole defect — an unconditional take swallowed a level
    // word as a hostname, which published a window of application lines as uniformly one level.
    // invariant: its cost is a host literally named after a level, which the raw fallback still
    // templates honestly.
    // refs: DN-43.D3
    const std::string_view host{sv_take_token(rest)};
    if (host.empty() || host.find_first_of(kHostReject) != std::string_view::npos)
        return std::nullopt;
    if (utils::parse_log_level(host) != LogLevel::Unknown)
        return std::nullopt;

    // invariant: a tag-less line is NOT syslog — the header is this strategy's claim, and a stamp
    // and host with an unconstrained remainder is a stamp, not a header.
    // invariant: so an empty result from the shared bounded scan DECLINES here, which is the one
    // point where this caller and the supercomputer branch legitimately differ.
    // refs: DN-43.D11
    std::string_view body{rest};
    const std::string_view tag{take_bounded_syslog_tag(body)};
    if (tag.empty())
        return std::nullopt;

    return SyslogHeader{.stamp = stamp, .tag = tag, .body = body, .bsd = bsd};
}

std::expected<ParsedLine, std::string> SyslogStrategy::parse(std::string_view line,
                                                             ArenaAllocator& /*arena*/) const
{
    const std::optional<SyslogHeader> header{scan_syslog_header(line)};
    if (!header)
    {
        // invariant: reachable only under an explicit format declaration — auto-detection routes
        // on the same predicate, so a line reaching parse by scoring has already passed it.
        // invariant: a wrong declaration stays wrong and the declarer owns it; nothing announces
        // it.
        // refs: ADR-23.D2
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
