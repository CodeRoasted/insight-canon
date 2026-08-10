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
// scan, and the SRC-D-OUT-RUN-1 precedence resolver; the semantic packages own only the rows. Homed
// as a facade impl unit (module insight.canon, the semantic_walkers.cpp precedent) because it
// consumes ComposedSemantics and drives the sealed LogParser.
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

    // The composed package list, appended to both wiring fatals below. Shared rather than
    // re-spelled: a diagnostic that lists the legal names is only useful if it cannot drift from
    // the composition it is describing, and the two fatals describe the same one.
    void stream_composed_packages(std::span<const insight::semantic::ComposedPackage> packages)
    {
        if (packages.empty())
        {
            std::cerr << "<none>";
            return;
        }
        for (std::size_t i{0}; i < packages.size(); ++i)
            std::cerr << (i == 0 ? "" : ", ") << '"' << packages[i].name << '"';
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

std::optional<RunOutcome> map_outcome_token_in(std::string_view token, std::string_view vocabulary,
                                               const insight::semantic::ComposedSemantics& composed)
{
    const auto packages{composed.packages()};
    // ── THE HALF-PAIR. A declaration that is STRUCTURALLY INCOMPLETE, and it is a WIRING error ──
    //
    // DN-32.D6 rules a caller-declared verdict is a PAIR — (vocabulary name, native token). A
    // token without its vocabulary is not a weak declaration, it is half of one, and it used to
    // return `nullopt` here "by design": the caller had declared something, the engine resolved
    // nothing, and every rule that reads a verdict was disarmed in silence. That is not a
    // hypothetical. `sift-crawl` shipped this exact shape on EVERY pair it ever produced, and it
    // took 63 identical-commit pairs, and 60 critical/high `regression` rows against a ground
    // truth of silence, to notice.
    //
    // WHY THIS IS FATAL AND THE UNMAPPED-TOKEN CASE BELOW IS NOT, which is the whole distinction:
    // a missing half is a CONFIG error — it is unreachable from any log byte, it does not depend
    // on what was fetched, and it fails identically on the first invocation and the millionth. A
    // token that is simply not in a NAMED vocabulary is a VALUE error: a legitimate runtime state
    // under correct wiring (a platform adds a conclusion string, an adapter forwards an unexpected
    // one), and killing the process for that would delete a working safety mechanism.
    if (vocabulary.empty())
    {
        std::cerr << "FATAL: insight::map_outcome_token_in — a verdict token (\"" << token
                  << "\") was declared with NO outcome vocabulary to interpret it. The composed "
                     "packages are: ";
        stream_composed_packages(packages);
        std::cerr
            << ".\nA caller-declared verdict is a PAIR — (vocabulary, token) — never a bare "
               "string (DN-32.D6): `failure` / `failed` / `FAILURE` mean the same thing on three "
               "platforms and `UNSTABLE` means nothing on two of them, so a token does not "
               "interpret itself. A side-input verdict is interpreted by the vocabulary of whoever "
               "SUPPLIED it, not by the dialect of whoever WROTE the bytes — name it (e.g. "
               "--outcome-vocabulary) rather than declaring a dialect the bytes do not have.\n"
               "This is a WIRING error, not a resolution failure: the half-pair resolved nothing "
               "and silently disarmed every rule that reads the verdict.\n";
        std::terminate();
    }
    // The name is checked HERE, with a message that names the coordinate the caller actually
    // declared. `for_stream` below would also fail closed on an unknown name — but its message says
    // *"unknown dialect"*, and pointing an `--outcome-vocabulary` typo at the DIALECT is exactly
    // the conflation DN-32.D6 exists to end. Two coordinates, two diagnostics; the shared filter
    // stays shared, only the sentence differs.
    if (std::ranges::none_of(packages, [vocabulary](const insight::semantic::ComposedPackage& pkg)
                             { return pkg.name == vocabulary; }))
    {
        std::cerr << "FATAL: insight::map_outcome_token_in — unknown outcome vocabulary \""
                  << vocabulary << "\". The composed packages are: ";
        stream_composed_packages(packages);
        std::cerr
            << ".\nThis is the vocabulary that INTERPRETS a caller-supplied verdict (DN-32.D6) — "
               "who SUPPLIED the verdict, which is not the same question as the stream's dialect "
               "(who WROTE the bytes). An unknown name is a MISTAKE: it would resolve nothing and "
               "silently disarm every rule that reads the verdict. Declare one of the names above, "
               "or none.\n";
        std::terminate();
    }
    // `for_stream` IS the dialect gate's one evaluation point, and it is reused rather than
    // re-spelled here for two reasons that both bite. It re-derives from the UNFILTERED tables, so
    // it sees rows this composition's own view has already dropped — and a FRESH composition is
    // the doubly-Unspecified view, in which every concretely-gated row is ALREADY GONE. Walking
    // `composed.outcome_tokens()` directly therefore matches nothing at all, silently, whatever
    // the gate says. It also fails closed on an unknown NAME with the message that lists the
    // composed packages, so a typo'd vocabulary terminates instead of quietly resolving nothing.
    //
    // Cold path by construction: once per side per diff, never per line, so the ~30-POD-row copy
    // is not on any hot path — and no per-line code gains a gate coordinate, which is the
    // determinism property ADR-22 protects.
    const insight::semantic::ComposedSemantics view{composed.for_stream(vocabulary, {})};
    return map_outcome_token(token, view);
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

RunOutcomeResolution resolve_run_outcome(SideInputVerdict side_input, const RunOutcomeScan& scan,
                                         const insight::semantic::ComposedSemantics& stream_view,
                                         const insight::semantic::ComposedSemantics& vocabularies)
{
    const std::string_view side_input_token{side_input.token};
    // Rung 1 resolves against the vocabulary its DECLARER named, drawn from the full composition —
    // ALWAYS, with no unnamed alternative (DN-32.D6). The old fallback to the stream's own view was
    // the half-pair's hiding place: it made an incomplete declaration look like a resolvable one
    // and then resolved nothing, so `map_outcome_token_in` now refuses the empty name outright and
    // this call is the site that reaches it.
    //
    // ⚠ SCOPE, and it is narrow ON PURPOSE: this governs the CALLER'S SIDE-INPUT declaration only.
    // Canon's own console-tail resolution below still reads `stream_view`, because a verdict found
    // IN the bytes is interpreted by whoever wrote them — that is the authorship test, not a
    // second-class side input, and nothing here may reach it.
    const insight::semantic::ComposedSemantics& composed{stream_view};
    RunOutcomeResolution resolution;

    // The console-tail candidate's mapped value (rung 2 / the divergence check), resolved against
    // this stream's declared vocabulary.
    std::optional<RunOutcome> console_mapped;
    if (scan.marker_present)
    {
        // grammar-5 (ADR-17): a PrefixIsVerdict row carries its verdict on the ROW, so there is
        // nothing to map and nothing that can fail to map. The token path — and with it the
        // fail-closed note — stays exactly as it was for the RemainderToken shape, which is the
        // only shape whose verdict comes off the LINE and can therefore be outside the vocabulary.
        console_mapped = scan.verdict ? scan.verdict : map_outcome_token(scan.token, composed);
        if (console_mapped)
            resolution.console = *console_mapped;
        else
            resolution.note = "run-outcome: console verdict '" + scan.token +
                              "' is not in the composed outcome vocabulary (fail-closed: Unknown)";
    }

    // Rung 1 — the authoritative side-input, if provided AND it maps in the vocabulary its
    // declarer NAMED. An ABSENT declaration (empty token) skips the rung entirely and degrades,
    // which is DN-32.D7's third state and is untouched here; an INCOMPLETE one never gets this
    // far, because `map_outcome_token_in` refuses a missing vocabulary before it can resolve
    // nothing quietly. Absent is a choice; half-declared is a mistake; only the first degrades.
    if (!side_input_token.empty())
    {
        const std::optional<RunOutcome> mapped{
            map_outcome_token_in(side_input_token, side_input.vocabulary, vocabularies)};
        if (mapped)
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
        // ONE cause now, because only one survives: a COMPLETE pair whose token is not in the
        // vocabulary that was named. The two former siblings both described a missing vocabulary,
        // and a missing vocabulary is no longer a resolution outcome that can be noted — it
        // terminates at the wiring boundary, carrying the sentence that used to live here to the
        // one moment it can still be acted on. What remains is the VALUE error, and it stays
        // fail-closed and non-fatal: the ladder continues to the console tail, and the note
        // travels ON THE REPORT (`ChangeReportSummary::*_outcome_note`), in the same surface as
        // the claims it qualifies. A console line does not travel with a report row — that was the
        // second half of how 63 crawl pairs went out unbounded with the diagnosis already computed.
        resolution.note = "run-outcome: side-input '" + std::string{side_input_token} +
                          "' is not in the '" + std::string{side_input.vocabulary} +
                          "' outcome vocabulary (fail-closed: falling back)";
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
