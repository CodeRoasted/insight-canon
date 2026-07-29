module;
#include "utils/log_macros.hpp" // textual macro layer (§11.9) — the kept trace-level divergence log

module insight.canon;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;          // OutcomeTokenRow / OutcomeMarkerRow
import insight.canon.compose;      // ComposedSemantics
import insight.canon.detail.parse; // LogParser — the scan is a parse-only pass (no masking)

// outcome.cpp — the run-outcome ALGORITHMS over the composed grammar-2 vocabulary (ADR 0025 /
// insight_run_outcome_model.md §3–§4). Canon owns the token map, the console-tail
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
    constexpr std::size_t kScanArenaCapacity{std::size_t{64U} * 1024U}; // 64 KiB, widened operand

    // The console-tail remainder must be a single native verdict word — `Finished: SUCCESS`, never
    // `Finished: SUCCESS (took 3s)` (studies/006: `^Finished: (\w+)$`, ASCII \w, strict).
    [[nodiscard]] constexpr bool is_verdict_word(std::string_view token) noexcept
    {
        if (token.empty())
            return false;
        return std::ranges::all_of(token,
                                   [](const char chr) noexcept
                                   {
                                       return (chr >= 'a' && chr <= 'z') ||
                                              (chr >= 'A' && chr <= 'Z') ||
                                              (chr >= '0' && chr <= '9') || chr == '_';
                                   });
    }

} // namespace

std::optional<RunOutcome>
map_outcome_token(std::string_view token,
                  const insight::semantic::ComposedSemantics& composed) noexcept
{
    for (const insight::semantic::OutcomeTokenRow& row : composed.outcome_tokens())
        if (row.token == token)
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
            // Longest VALID prefix wins within the line — "valid" because a row whose own shape
            // requirement fails must fall through so a shorter row can still claim the line, the
            // same rule `recognize` applies to its extractors. A line that produces no winner leaves
            // the scan untouched, so a mid-log `Finished: SUCCESS (took 3s)` never displaces an
            // earlier real verdict.
            const insight::semantic::OutcomeMarkerRow* best{nullptr};
            std::string_view best_token;
            for (const insight::semantic::OutcomeMarkerRow& row : composed.outcome_markers())
            {
                if (!parsed->content.starts_with(row.prefix) ||
                    (best != nullptr && row.prefix.size() <= best->prefix.size()))
                    continue;
                std::string_view token{parsed->content};
                token.remove_prefix(row.prefix.size());
                if (row.shape == insight::semantic::OutcomeMarkerShape::RemainderToken &&
                    !is_verdict_word(token))
                    continue;
                best = &row;
                best_token = token;
            }
            if (best != nullptr)
            {
                // LAST matching line wins (a run has one terminal verdict; deterministic).
                scan.marker_present = true;
                if (best->shape == insight::semantic::OutcomeMarkerShape::PrefixIsVerdict)
                {
                    scan.verdict = best->outcome;
                    scan.token.clear();
                }
                else
                {
                    scan.verdict.reset();
                    scan.token.assign(best_token); // copied before the per-line arena reset
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

    // The console-tail candidate's mapped value (rung 2 / the divergence check), resolved against
    // this stream's declared vocabulary.
    std::optional<RunOutcome> console_mapped;
    if (scan.marker_present)
    {
        // grammar-5 (ADR 0069): a PrefixIsVerdict row carries its verdict on the ROW, so there is
        // nothing to map and nothing that can fail to map. The token path — and with it the
        // fail-closed note — stays exactly as it was for the RemainderToken shape, which is the only
        // shape whose verdict comes off the LINE and can therefore be outside the vocabulary.
        console_mapped = scan.verdict ? scan.verdict : map_outcome_token(scan.token, composed);
        if (console_mapped)
            resolution.console = *console_mapped;
        else
            resolution.note = "run-outcome: console verdict '" + scan.token +
                              "' is not in the composed outcome vocabulary (fail-closed: Unknown)";
    }

    // Rung 1 — the authoritative side-input, if provided AND it maps in the DECLARED dialect's
    // vocabulary. "Declared", not "detected": the view was resolved once at stream open, so an
    // undeclared stream carries no concretely-gated outcome row and the token cannot resolve —
    // fail-closed on depth, and the note below says so instead of naming a latched format.
    if (!side_input_token.empty())
    {
        if (const std::optional<RunOutcome> mapped{map_outcome_token(side_input_token, composed)})
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
        // Provided but unmapped — surfaced, never silent (fail-closed); the ladder continues. The
        // two causes are kept apart because their fixes are different: an EMPTY outcome vocabulary
        // means the stream declared no dialect (or one that ships no verdict tokens), which the
        // caller fixes by declaring; a non-empty one that does not carry the token means the token
        // is wrong for this dialect.
        resolution.note =
            "run-outcome: side-input '" + std::string{side_input_token} +
            (composed.outcome_tokens().empty()
                 ? "' cannot resolve: this stream's resolved vocabulary carries no run-outcome "
                   "tokens (declare the stream's dialect to get them)"
                 : std::string{"' is not in the declared dialect's outcome vocabulary "
                               "(fail-closed: falling back)"});
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
