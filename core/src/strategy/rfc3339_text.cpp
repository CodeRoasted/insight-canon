module;
#include "utils/log_macros.hpp" // textual macro layer (ADR-3.D4)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// Rfc3339TextStrategy — the leading-RFC-3339 LAYOUT: a stamp token followed by free text.
// The contract, the disjointness argument and the empty-component ruling live on the class
// declaration in canon.detail.strategy.cppm.
//
// Zero-copy: `line` is arena-stable when parse() is invoked, so `content` IS it — nothing is
// removed, and the post-stamp remainder the level is inferred from is a tail subview.

namespace insight::tokenization
{

std::expected<ParsedLine, std::string> Rfc3339TextStrategy::parse(std::string_view line,
                                                                  ArenaAllocator& /*arena*/) const
{
    if (!is_rfc3339_prefix(line) || scan_syslog_header(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=Rfc3339Text parse miss");
        return std::unexpected(
            std::string("Rfc3339TextStrategy: line is not a bare RFC3339-prefixed layout"));
    }

    std::string_view rest{line};
    const std::string_view raw_ts{sv_take_token(rest)};

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = EventTime::parsed(utils::parse_iso8601(raw_ts));
    // NAMING TOTALITY (DN-43.D12): the stamp is READ, never REMOVED. `<stamp><unconstrained
    // remainder>` is not a header — the declared transport row `api-rfc3339-line-prefix` peels
    // exactly this shape, so the stamp's LAYER (record or delivery envelope) is undecidable from
    // the line's own bytes and only a declaration settles it. Removing it here would assert "these
    // bytes are not this record's" on no authority — an over-claim at the projection grain, and the
    // content-side workaround for an absent declaration ADR-23.D5 forbids in terms.
    parsed_line.content = line;
    // Scanned from the POST-STAMP remainder, never from `content`: infer_leading_log_level's
    // leading head is a RAW-BYTE budget and an RFC-3339 stamp spends most of it, so scanning from
    // byte 0 would push the level word out of the head — the exact proxy-over-presentation-bytes
    // defect ADR-20's "bound the scan, never the claim" was learned on. The strategy knows where
    // the field ends, so it scans from there while the bytes stay in `content`.
    parsed_line.level = utils::infer_leading_log_level(rest);
    // `component` is deliberately left empty — see the class declaration: the layout names no
    // functional source. `host` likewise: this layout carries no node identity.
    INSIGHT_LOG_DEBUG(logging::strategy_logger(), "strategy=Rfc3339Text parsed level={} has_ts={}",
                      to_string(parsed_line.level.value()), parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat Rfc3339TextStrategy::format() const noexcept
{
    return LogFormat::Rfc3339Text;
}

double Rfc3339TextStrategy::confidence(std::string_view line) const noexcept
{
    // The SAME value SyslogStrategy's RFC-3339 arm scored before the split, and that is deliberate:
    // it leaves every routing comparison against a THIRD strategy byte-identical, so the only
    // decision this change moves is the one between the two disjoint predicates.
    static constexpr double kRfc3339TextConfidence{0.80};
    static constexpr double kNoConfidence{0.0};

    if (!is_rfc3339_prefix(line))
        return kNoConfidence;
    return scan_syslog_header(line) ? kNoConfidence : kRfc3339TextConfidence;
}

} // namespace insight::tokenization
