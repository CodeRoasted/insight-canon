module;
#include "utils/log_macros.hpp" // textual macro layer (§11.9) — the kept trace-level divergence log

module insight.canon;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;          // OutcomeTokenRow / OutcomeMarkerRow
import insight.canon.compose;      // ComposedSemantics
import insight.canon.detail.parse; // LogParser — the scan is a parse-only pass (no masking)

// outcome.cpp — the run-outcome ALGORITHMS over the composed grammar-2 vocabulary (ADR 0025 /
// insight_run_outcome_model.md §3–§4). Canon owns the format-gated token map, the console-tail
// scan, and the D-OUT-RUN-1 precedence resolver; the semantic packages own only the rows. Homed as
// a facade impl unit (module insight.canon, the semantic_walkers.cpp precedent) because it consumes
// ComposedSemantics and drives the sealed LogParser.
//
// Determinism (F5): byte-exact ASCII token/prefix compare + integer line index; no float, no
// wall-clock; the LAST console-tail match wins (line-order deterministic — a run has one terminal
// verdict). The scan arena is reset per line (the build_line_index discipline).

namespace insight
{

namespace
{
    // The scan's per-line arena: parse-only (raw-line copy + strategy scalar fields), reset per
    // line, so a small block suffices and never grows unbounded.
    constexpr std::size_t kScanArenaCapacity{64U * 1024U};

    // A row's gate matches the line's routed format when equal, or when the gate is the kAnyFormat
    // sentinel (mirrors semantic_walkers gate_matches — a line routed Unknown must NOT trigger a
    // concretely-gated dialect row).
    [[nodiscard]] constexpr bool gate_matches(LogFormat row_gate, LogFormat line_format) noexcept
    {
        return row_gate == insight::semantic::kAnyFormat || row_gate == line_format;
    }

    // The console-tail remainder must be a single native verdict word — `Finished: SUCCESS`, never
    // `Finished: SUCCESS (took 3s)` (studies/006: `^Finished: (\w+)$`, ASCII \w, strict).
    [[nodiscard]] constexpr bool is_verdict_word(std::string_view token) noexcept
    {
        if (token.empty())
            return false;
        for (const char chr : token)
        {
            const bool word{(chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') ||
                            (chr >= '0' && chr <= '9') || chr == '_'};
            if (!word)
                return false;
        }
        return true;
    }

    // Does any composed OutcomeTokenRow declare this format as its gate? (The dialect latch: the
    // side-input resolves under the log's first outcome-bearing routed format.)
    [[nodiscard]] bool
    format_bears_outcomes(LogFormat format,
                          const insight::semantic::ComposedSemantics& composed) noexcept
    {
        if (format == LogFormat::Unknown)
            return false;
        for (const insight::semantic::OutcomeTokenRow& row : composed.outcome_tokens())
            if (row.format_gate == format)
                return true;
        return false;
    }
} // namespace

std::optional<RunOutcome>
map_outcome_token(std::string_view token, LogFormat format,
                  const insight::semantic::ComposedSemantics& composed) noexcept
{
    for (const insight::semantic::OutcomeTokenRow& row : composed.outcome_tokens())
        if (row.token == token && gate_matches(row.format_gate, format))
            return row.outcome;
    return std::nullopt;
}

RunOutcomeScan scan_run_outcome(std::span<const std::string> lines,
                                const insight::semantic::ComposedSemantics& composed)
{
    RunOutcomeScan scan;
    if (composed.outcome_tokens().empty() && composed.outcome_markers().empty())
        return scan; // no outcome vocabulary composed — nothing to scan for

    tokenization::ArenaAllocator arena{kScanArenaCapacity};
    tokenization::LogParser parser{arena, composed};
    for (const std::string& line : lines)
    {
        if (line.empty())
            continue;
        const auto parsed{parser.parse_line(line)};
        if (parsed.has_value())
        {
            const LogFormat format{parser.routed_format()};
            if (scan.dialect == LogFormat::Unknown && format_bears_outcomes(format, composed))
                scan.dialect = format;
            for (const insight::semantic::OutcomeMarkerRow& row : composed.outcome_markers())
                if (gate_matches(row.format_gate, format) &&
                    parsed->content.starts_with(row.prefix))
                {
                    const std::string_view token{parsed->content.substr(row.prefix.size())};
                    if (is_verdict_word(token))
                    {
                        // LAST match wins (a run has one terminal verdict; deterministic).
                        scan.marker_present = true;
                        scan.marker_format = format;
                        scan.token.assign(token); // copied before the per-line arena reset
                    }
                }
        }
        arena.reset();
    }
    return scan;
}

RunOutcomeResolution resolve_run_outcome(std::string_view side_input_token,
                                         const RunOutcomeScan& scan,
                                         const insight::semantic::ComposedSemantics& composed)
{
    RunOutcomeResolution resolution;

    // The console-tail candidate's mapped value (rung 2 / the divergence check), gated on ITS
    // line's routed format.
    std::optional<RunOutcome> console_mapped;
    if (scan.marker_present)
    {
        console_mapped = map_outcome_token(scan.token, scan.marker_format, composed);
        if (console_mapped)
            resolution.console = *console_mapped;
        else
            resolution.note = "run-outcome: console verdict '" + scan.token +
                              "' is not in the composed outcome vocabulary (fail-closed: Unknown)";
    }

    // Rung 1 — the authoritative side-input, if provided AND it maps in the detected dialect.
    if (!side_input_token.empty())
    {
        if (const std::optional<RunOutcome> mapped{
                map_outcome_token(side_input_token, scan.dialect, composed)})
        {
            resolution.outcome = *mapped;
            resolution.authoritative = true;
            if (console_mapped && *console_mapped != *mapped)
            {
                // Present-but-divergent (Accumulo #498): the console tail reflects a local /
                // nested / caught outcome — never a competing whole-run verdict. Made legible via
                // a KEPT trace-level log (free under level-gating), never a tiebreak.
                resolution.divergent = true;
                INSIGHT_LOG_TRACE(logging::parser_logger(),
                                  "run_outcome: authoritative={} console={} -> {} (divergent "
                                  "console tail not consulted, D-OUT-RUN-1)",
                                  to_string(*mapped), to_string(*console_mapped),
                                  to_string(resolution.outcome));
            }
            return resolution;
        }
        // Provided but unmapped — surfaced, never silent (fail-closed); the ladder continues.
        resolution.note =
            "run-outcome: side-input '" + std::string{side_input_token} +
            (scan.dialect == LogFormat::Unknown
                 ? "' cannot resolve: no outcome-bearing dialect detected in the log"
                 : std::string{"' is not in the '"} + std::string{to_string(scan.dialect)} +
                       "' outcome vocabulary (fail-closed: falling back)");
    }

    // Rung 2 — the console-tail marker's last match, if present AND it maps.
    if (console_mapped)
    {
        resolution.outcome = *console_mapped;
        return resolution;
    }

    // Rung 3 — Unknown (the pre-outcome default; absence carries no verdict framing).
    return resolution;
}

} // namespace insight
