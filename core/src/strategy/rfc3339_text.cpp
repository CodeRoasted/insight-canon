module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// invariant: the leading-RFC-3339 LAYOUT — a stamp token followed by free text.
// invariant: zero-copy — the line is arena-stable when parse is invoked, so the content IS it:
// nothing is removed, and the remainder the level is inferred from is a tail subview.
// note: the contract and the disjointness argument are on the class declaration.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
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
    // invariant: the stamp is READ, never REMOVED — a stamp followed by an unconstrained
    // remainder is not a header.
    // invariant: the declared transport row peels exactly this shape, so the stamp's LAYER is
    // undecidable from the line's own bytes and only a declaration settles it.
    // invariant: removing it here would assert that these bytes are not this record's, on no
    // authority — an over-claim at the projection grain.
    // refs: ADR-23.D5, DN-43.D12
    parsed_line.content = line;
    // invariant: the level is scanned from the POST-STAMP remainder and never from the content, and
    // the strategy knows where the field ends so the bytes still stay in the content.
    // invariant: stage 1's budget is a TOKEN count, so the stamp costs it ONE token; what the stamp
    // still shifts is stage 2's raw-BYTE cue head, which is the surviving reason to scan late.
    // invariant: bound the scan, never the claim.
    // refs: ADR-16.D7, ADR-20.D3
    parsed_line.level = utils::infer_leading_log_level(rest);
    // invariant: the component is deliberately EMPTY because the layout names no functional source,
    // and the host likewise because it carries no node identity.
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
    // invariant: the SAME value the syslog strategy's RFC-3339 arm scored before the split, so
    // every routing comparison against a THIRD strategy stays byte-identical.
    // invariant: the only decision the split moves is the one between the two disjoint predicates.
    static constexpr double kRfc3339TextConfidence{0.80};
    static constexpr double kNoConfidence{0.0};

    if (!is_rfc3339_prefix(line))
        return kNoConfidence;
    return scan_syslog_header(line) ? kNoConfidence : kRfc3339TextConfidence;
}

} // namespace insight::tokenization
