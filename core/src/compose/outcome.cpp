module;
#include "utils/log_macros.hpp" // textual macro layer (ADR-3.D4) — the kept trace-level divergence log

module insight.canon;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;          // OutcomeTokenRow / OutcomeMarkerRow
import insight.canon.compose;      // ComposedSemantics
import insight.canon.detail.parse; // LogParser — the scan is a parse-only pass (no masking)

// outcome.cpp — the run-outcome ALGORITHMS over the composed grammar-2 vocabulary (ADR-17 /
// insight_run_outcome_model.md §3–§4). Canon owns the token map, the console-tail
// scan, and the SRC-D-OUT-RUN-1 precedence resolver; the semantic packages own only the rows. Homed as
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

    /// The best outcome row seen so far within one line, across all of its `\r`-anchored segments.
    /// `token` is OWNED because the parse arena is reset between segments, so the `content` the
    /// match borrowed from does not outlive the segment that produced it.
    struct OutcomeMarkerMatch
    {
        const insight::semantic::OutcomeMarkerRow* row{nullptr};
        std::string token;
    };

    /// Longest VALID prefix wins within the line — "valid" because a row whose own shape
    /// requirement fails must fall through so a shorter row can still claim the line, the same rule
    /// `recognize` applies to its extractors. A segment that produces no winner leaves `best`
    /// untouched, so a mid-log `Finished: SUCCESS (took 3s)` never displaces an earlier real
    /// verdict.
    void improve_match(OutcomeMarkerMatch& best, std::string_view content,
                       const insight::semantic::ComposedSemantics& composed)
    {
        for (const insight::semantic::OutcomeMarkerRow& row : composed.outcome_markers())
        {
            if (!content.starts_with(row.prefix) ||
                (best.row != nullptr && row.prefix.size() <= best.row->prefix.size()))
                continue;
            std::string_view token{content};
            token.remove_prefix(row.prefix.size());
            if (row.shape == insight::semantic::OutcomeMarkerShape::RemainderToken &&
                !is_verdict_word(token))
                continue;
            best.row = &row;
            best.token.assign(token);
        }
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
        // A line start is not only where the caller's splitter put one. Runners built before
        // GitLab 18.9 frame the epilogue with a BARE `\r`, so the terminal verdict of such a trace
        // sits mid-element in any `\n`-split line vector and an at-offset-0 test can never see it —
        // silently, as a missing verdict rather than an error. Anchoring after a lone `\r` as well
        // is the same widening the corpus scorer already carries.
        //
        // It is deliberately done HERE, on where a row may MATCH, and not by teaching the splitter
        // to break on `\r`: the `\r` is CONTENT. In the `after_script` warning shape it is the one
        // byte keeping `after_script` and `WARNING` from fusing into a token that reads as a
        // failure, so a read path that folds or strips it re-manufactures a false positive. This
        // widening rewrites no bytes and changes no caller's segmentation.
        //
        // Each `\r`-separated segment is re-parsed rather than merely offset into, because the
        // per-line prefix a strategy peels (timestamp, ANSI escape run) recurs after the `\r` on
        // exactly these traces — the anchor has to compose with that peel, not bypass it.
        OutcomeMarkerMatch best;
        std::string_view remaining{line};
        while (true)
        {
            const auto carriage_return{remaining.find('\r')};
            if (const std::string_view segment{remaining.substr(0, carriage_return)};
                !segment.empty())
            {
                if (const auto parsed{parser.parse_line(segment)}; parsed.has_value())
                    improve_match(best, parsed->content, composed);
                arena.reset();
            }
            if (carriage_return == std::string_view::npos)
                break;
            remaining.remove_prefix(carriage_return + 1);
        }
        if (best.row != nullptr)
        {
            // LAST matching line wins (a run has one terminal verdict; deterministic).
            scan.marker_present = true;
            if (best.row->shape == insight::semantic::OutcomeMarkerShape::PrefixIsVerdict)
            {
                scan.verdict = best.row->outcome;
                scan.token.clear();
            }
            else
            {
                scan.verdict.reset();
                scan.token = std::move(best.token);
            }
        }
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
        // grammar-5 (ADR-17): a PrefixIsVerdict row carries its verdict on the ROW, so there is
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
                                  "console tail not consulted, SRC-D-OUT-RUN-1)",
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
