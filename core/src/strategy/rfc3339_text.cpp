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
// Zero-copy: `line` is arena-stable when parse() is invoked, so `content` is a tail subview of it.

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
    // TOTAL by construction: every byte past the stamp is content. Nothing is named, so nothing may
    // be dropped (DN-43.D6).
    parsed_line.content = rest;
    parsed_line.level = utils::infer_leading_log_level(parsed_line.content);
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
