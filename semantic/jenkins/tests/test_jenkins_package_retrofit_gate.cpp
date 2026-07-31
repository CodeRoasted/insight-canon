// NOLINTBEGIN — integration gate: literals and printed diagnostics are intended.
// test_jenkins_package_retrofit_gate.cpp — the Jenkins RECOGNIZER RETROFIT over jenkins-markers/v2
// (bibles/jenkins_dialect.md; the GitLab package-proof precedent, §4 of
// technical_docs/history/architecture-v1/corpus_backed_gates.md).
//
// ═══ WHAT IS BEING CLAIMED, IN THE WORDS §2 REQUIRES — three oracles, three claim words ═══
//
//   * L-T  — FIDELITY OF TRANSCRIPTION, permanently (declared limitation 1): the oracle is the
//     pinned spike's per-trace output (console stage names, step count, `Finished:` token), and
//     the spike authored the shipped rows' intent, so agreement here can never become independent
//     validation. SUT==ORACLE is inherent and IS the claim: exactly the link the audit was
//     missing — nothing in the tree scored the SHIPPED rows against the corpus.
//   * L-S1/L-S2 — AGREEMENT WITH THE PLATFORM'S OWN STRUCTURAL RECORD (the wfapi stage tree), a
//     producer this codebase never authored. Axis: the 64 stage-bearing WorkflowJob trees / 442
//     stages / 2 145 steps. NEVER averaged with the 113 (different populations).
//   * L-O  — RECOVERY OF THE PLATFORM'S RECORDED VERDICT from console bytes alone, on all 113
//     (the REST `result` rules the run grain; wfapi's run status is a corroborate surface —
//     §2's pre-registered authority map). Unstable is FIRST-CLASS and pinned 4/4 by name.
//   * The frozen external Jenkins depth/outcome claim does NOT widen on a green — a green makes
//     the existing words AUDITED, which is the entire deliverable (§5).
//
// ═══ THE POPULATION AND THE THREE COMMITTED ORACLE FILES (clauses 1/4; §3) ═══
//   * `RETRO-v2.trace-sidecar.tsv` — the committed projection of the 113 corpus.jsonl records
//     (sorted by path), emitted by `emit_retrofit_sidecars.py`, which IMPORTS the pinned spike's
//     `parse` and `depth_type_of` — one classifier, one owner; a C++ re-derivation inside this
//     gate would agree with itself. Per-row `sha256` is the ATTESTED digest from the frozen
//     content-anchor manifest (REGISTRY pin ae5f19f1…); bytes are VERIFIED here (clause 4).
//   * `RETRO-v2.console-stages.tsv` — the frozen instrument's transcription oracle (524 stage
//     names, console order). `RETRO-v2.wfapi-stages.tsv` — the platform's structural oracle
//     (442 rows, names + statuses; 12 UNSTABLE stages counted there and asserted NOWHERE —
//     stage-grain verdicts are not a product surface, declared limitation 4).
//   * The corpora-side `projection(manifest) == sidecar` equality is the generator's own
//     `--check` mode (corpora-repo governance) — this binary cannot see the JSON and does not try.
//
// ═══ THE INGEST ASSEMBLY THE MARKER LEGS SCORE (clause 8 — the PURIFIED chain, G-T5-RETRO) ═══
// Since T5 5.2 the legs re-score through the DECLARED chain, expectations frozen byte-for-byte at
// the pre-cut figures (the migration-gate shape, ADR-8). Per `\n`-split line (binary read,
// `\r` NEVER trimmed — clause 6): the DECLARED transport peel — the stack comes from the
// sidecar's frozen `stamp_class` column, never from inspection (whole-stream ⇒
// `bracket-rfc3339-line-prefix`, payload-stamped/bare ⇒ degenerate; parse order transport →
// logformat → intent, ADR-23; a stamp-only line peels blank and DROPS, bundled #4
// catalogue-side) → stage 1 `normalize` (skip an all-escape line — the LogParser discipline;
// measured on this corpus: 7 582 ESC-bearing lines, ZERO of them marker/`Finished:`-bearing, so
// the seam is declared and inert on these bytes) → the SHIPPED `recognize()` over the composed
// rows (there is no strategy any more — the rows plus canon's walkers ARE the parser). The
// outcome leg is the SHIPPED `scan_run_outcome` + `resolve_run_outcome` over the PEELED lines —
// public, driving the real LogParser (clause 8 satisfied outright).
//
// ═══ THE §4.4 NAMED DELTAS, so a red is attributed rather than hand-waved ═══
// Two spike-vs-walker semantic deltas are pre-named: (1) exclusion token boundary — spike
// `body.split()[0]` (any whitespace) vs walker `' '` only; (2) bare `[Pipeline] //` (no trailing
// space) — spike counts a step, walker excludes. Neither has a known corpus instance; the L-T
// leg turns "unknown" into "counted". An L-T mismatch NOT attributable to a named delta is the
// TRANSCRIPTION-DIVERGED event: stop, report to the Founder — pre-named claim retraction, never
// a bug row.
//
// ═══ CLAUSE MAP (corpus_backed_gates.md §2) ═══
//   1 committed population, sorted, uncapped     → the trace sidecar
//   2 UNSET ⇒ skip; SET-BUT-BROKEN ⇒ hard fail   → SetUp()
//   3 population SIZE selects the pins (113 asserted, not observed); unrecognized ⇒ FAIL
//   4 bytes verified (sha256 + size) against the ATTESTED digests
//   5 partitions CLOSE — depth cells sum to 113; wfapi ⟺ WorkflowJob exactly; outcome cells sum
//     per class; elided (12) segregated in every structural cell, never filtered
//   6 binary reads, `\n`-split only, `\r` is content
//   7 red-capability OBSERVED and recorded below
//   8 the SUT is the shipped symbols — `TransportStack::peel_raw`, `recognize()`,
//     `scan_run_outcome`, `resolve_run_outcome`, `map_outcome_token`, `normalize`
//   9 registered as RUN in `scripts/run_corpus_gates.sh` in the same commit as this file
//
// ═══ PIN PROVENANCE — two strengths, labelled (the G1-PEEL / GitLab discipline) ═══
//   CORROBORATED    — the pinned spike's recorded v2 figures (g1_jenkins_v2.py on the frozen
//                     corpus, 2026-07-30 run banked; TABLE 1/2b/4), reproduced independently
//                     from the committed TSVs at freeze time AND by this gate's first run.
//   CHARACTERIZATION — measured here first (2026-07-30), pinned so it cannot move silently
//                     (the per-cell elided splits; the 2-trace cross-surface cell).
//
// ═══ FALSIFIABILITY — OBSERVED 2026-07-30, then recorded (clause 7); every mutation reverted ═══
//   M-A  `"//"` dropped from kStepExcludes (jenkins.cppm) — the walker counts block-close
//        annotations as steps: RED — L-T step-count mismatches on 65/113 traces (first row:
//        builds_apache_org/job_Aries_job_website_build__29.log engine 19 vs oracle 13) and every
//        L-S2 engine-denominator cell inflated (declarative non-elided 1 481 vs pinned 944).
//        L-S1 per-trace 0, L-O and the population leg stayed green — the mutation is
//        step-axis-local, and the legs partition exactly as designed.
//   M-B  the UNSTABLE token row dropped (jenkins.cppm kOutcomeTokens) — RED on exactly the 4
//        named traces, all inside L-O: agree cells 71/28/0/9 (Unstable 0 of pinned 4),
//        unstable_recovered 0/4, each of the four traces printed as a named VOCABULARY error
//        (`result 'UNSTABLE' does not map in the composed vocabulary`), and the sum-to-113
//        outcome partition red. L-T/L-S1/L-S2/population all stayed green — nothing else moved.
//   M-C  the timestamper peel width broken by one byte — SHORT side (`return close`), because
//        the +1-long side is masked by the strategy's own post-stamp whitespace strip; the `]`
//        leaks into content: RED — L-T on exactly the 12 whole-stream (ci.jenkins.io) traces,
//        L-S1 collapses where the skeleton rides the stamp (matrix-pipe non-elided hits 0/64,
//        elided 27/131; per-trace rows name acceptance-test-harness engine 0 vs oracle 23), and
//        L-O absent-console grows to 12 (pinned 0) with agree 60/28/3/9 — the epilogue behind a
//        stamp is unreachable. Three legs red, each naming its axis; population green.
//   M-D  the STAGE extractor swapped to `RemainderAfterPrefix` (jenkins.cppm kMarkers[0]) —
//        stage names storm (the G-GL-P5 class from the other side): RED — L-T stage-SEQUENCE
//        divergences on 64/113 (counts equal, names differ — the `{ (Build)`-shaped remainder),
//        L-S1 name hits collapse to 0 in all six cells (0/442) with 64 per-trace mismatches.
//        L-S2/L-O/population green — the axis partition again.
//
// Determinism: byte-only — committed-order population, integer counts, no RNG, no clock, no
// float, no threads. The sha256 is FIPS 180-4 over bytes; pure integer.

#include <gtest/gtest.h>

import std;
import insight.canon;             // recognize / scan_run_outcome / resolve_run_outcome / normalize
import insight.canon.conformance; // marker_probe_for — the L-C tripwire observes the kit's probe
import insight.semantic.jenkins;  // kManifest + kDialect (the code tier is empty since T5 5.2)

using insight::map_outcome_token;
using insight::resolve_run_outcome;
using insight::RunOutcome;
using insight::RunOutcomeScan;
using insight::scan_run_outcome;
using insight::semantic::ComposedSemantics;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

namespace
{

// A `const char*`, not a string_view: it is handed to getenv, and string_view::data() carries no
// null-termination guarantee.
constexpr const char* kCorpusVar{"CORPUS_JENKINS_MARKERS_DIR"};
constexpr std::string_view kTraceSidecar{"RETRO-v2.trace-sidecar.tsv"};
// The frozen column order (positional reader). Namespace-scope because the pre-cut oracle emitter
// below reads the same sidecar through the same header check.
constexpr std::string_view kTraceHeader{
    "path\tsha256\tbytes\tjob_class\tdepth_type\telided\tresult\twfapi_status"
    "\twfapi_stage_count\twfapi_step_count\tconsole_stage_count\tconsole_step_count"
    "\tconsole_finished\tstamp_class"};
constexpr std::string_view kWfapiStages{"RETRO-v2.wfapi-stages.tsv"};
constexpr std::string_view kConsoleStages{"RETRO-v2.console-stages.tsv"};
constexpr std::string_view kBytesRoot{"data/v2"};

constexpr std::size_t kMaxReportedRows{10};

// ── The pins ──────────────────────────────────────────────────────────────────────────────────
// CORROBORATED — population (corpus.jsonl + the authoritative recheck labels; the 39/11 split in
// an earlier design draft was the PRE-recheck labelling — the spike consumes the recheck, so the
// gate pins the recheck: 40 declarative / 10 scripted, the one TensorFlow flip):
constexpr std::size_t kTraces{113};
constexpr std::size_t kDeclarative{40};
constexpr std::size_t kScripted{10};
constexpr std::size_t kMatrixPipe{17};
constexpr std::size_t kMatrixClassic{19};
constexpr std::size_t kFreestyle{27};
constexpr std::size_t kWorkflowJobs{67}; // wfapi ⟺ WorkflowJob exactly — absence IS the floor
constexpr std::size_t kStageBearingTrees{64};
constexpr std::size_t kWfapiStageRows{442};
constexpr std::size_t kWfapiStepSum{2145};
constexpr std::size_t kUnstableWfapiStages{12}; // counted HERE, asserted nowhere else (§9.4)
constexpr std::size_t kElided{12};
constexpr std::size_t kResultSuccess{72};
constexpr std::size_t kResultFailure{28};
constexpr std::size_t kResultAborted{9};
constexpr std::size_t kResultUnstable{4};
// CORROBORATED — the stamp-class partition (studies/010 §6.2, ADR-23 Part 2), now a GENERATED
// sidecar column (T5 §6: one classifier, one owner — `t0_transport.triage` imported by the
// generator; declarations in the T5 gates come from these frozen labels, never from inspection).
// A second partition on a second axis: NEVER cross-quoted with the depth cells.
constexpr std::size_t kWholeStream{12};
constexpr std::size_t kPayloadStamped{19};
constexpr std::size_t kBare{82};
// CHARACTERIZATION — the result-vs-wfapi cross-surface cell, counted under result-rules-run-grain
// (§2's authority map). The design doc's §2 says ONE such disagreement; the committed oracle
// carries TWO (remoting_3_10_x_backup__1 and Nem_controller_PR_826__53) — pinned at the measured
// value, and the doc's count is reported as the suspect (the oracle outranks the prose).
constexpr std::size_t kResultVsWfapiCrossSurface{2};
// CORROBORATED — L-T: the shipped chain equals the frozen instrument per trace, all 113.
constexpr std::size_t kTranscriptionMismatches{0};
constexpr std::size_t kConsoleStageRows{524};
constexpr std::size_t kConsoleFinishedAbsent{0};
// CORROBORATED — L-S1 name-level cells (hits / wfapi names / console names), by depth_type ×
// elided; the depth sums reproduce the spike's TABLE 2b exactly (220/220 · 27/27 · 160/195).
// Elided is segregated (one-signed confound, §9.3); the elided cells are CHARACTERIZATION.
struct StructuralCell
{
    std::size_t hits;
    std::size_t wfapi;
    std::size_t console;
};
constexpr StructuralCell kS1DeclarativePlain{216, 216, 234};
constexpr StructuralCell kS1DeclarativeElided{4, 4, 4};
constexpr StructuralCell kS1ScriptedPlain{23, 23, 23};
constexpr StructuralCell kS1ScriptedElided{4, 4, 4};
constexpr StructuralCell kS1MatrixPipePlain{64, 64, 86};
constexpr StructuralCell kS1MatrixPipeElided{96, 131, 173};
// CORROBORATED — L-S2 step-count cells (Σ min(engine, wfapi) / Σ wfapi / Σ engine); the depth
// sums reproduce TABLE 1's stpRec exactly (declarative+scripted 100%, matrix-pipe 96.3% =
// (558+833)/(558+887)); the per-elided split is CHARACTERIZATION. Axis discipline: the cells sum
// over the 64 STAGE-BEARING trees only — the 3 zero-stage wfapi trees (remoting_3_10_x_backup__1
// with 69 console steps against a 0-step tree, and the two Nem `#49`s with 1 each) are counted
// in the population, never scored (declared limitation 2); a pin derived over the 67-wfapi axis
// instead reads 1 292/91 and is WRONG — caught on this gate's own first run.
constexpr StructuralCell kS2DeclarativePlain{622, 622, 944};
constexpr StructuralCell kS2DeclarativeElided{4, 4, 14};
constexpr StructuralCell kS2ScriptedPlain{61, 61, 89};
constexpr StructuralCell kS2ScriptedElided{13, 13, 20};
constexpr StructuralCell kS2MatrixPipePlain{558, 558, 823};
constexpr StructuralCell kS2MatrixPipeElided{833, 887, 1223};
// CORROBORATED — L-O (spike TABLE 4, reproduced by the engine's own scan): per API class, the
// count the console scan recovers IN AGREEMENT; absent-console pinned 0 (v2 true-tail capture);
// exactly ONE console-vs-API divergence — the Accumulo-#498 class's own trace, API SUCCESS with
// console `Finished: ABORTED`, our rows faithfully reporting the console (ADR-17 D-OUT-RUN-1's
// declared subordination as a counted cell, never a row defect).
constexpr std::size_t kAgreeSuccess{71};
constexpr std::size_t kAgreeFailure{28};
constexpr std::size_t kAgreeUnstable{4}; // 4/4 by name — RunOutcome::Unstable is FIRST-CLASS
constexpr std::size_t kAgreeAborted{9};
constexpr std::size_t kConsoleAbsent{0};
constexpr std::size_t kDivergent{1};
constexpr std::string_view kDivergentTrace{"builds_apache_org/job_Accumulo_job_2_1__498.log"};

// ── SHA-256 (FIPS 180-4) — frozen, pure integer, single-shot (the GitLab-gate block) ──────────
constexpr std::array<std::uint32_t, 64> kSha256RoundConstants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U,
    0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU,
    0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU,
    0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
    0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
    0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U,
    0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U,
    0xc67178f2U};

[[nodiscard]] constexpr std::uint32_t rotate_right(std::uint32_t value,
                                                   std::uint32_t count) noexcept
{
    return (value >> count) | (value << (32U - count));
}

[[nodiscard]] std::string sha256_hex(std::string_view bytes)
{
    std::array<std::uint32_t, 8> digest{0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
                                        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
    const std::uint64_t bit_length{static_cast<std::uint64_t>(bytes.size()) * 8U};
    std::string padded{bytes};
    padded.push_back(static_cast<char>(0x80));
    while (padded.size() % 64U != 56U)
        padded.push_back('\0');
    for (int shift{56}; shift >= 0; shift -= 8)
        padded.push_back(static_cast<char>((bit_length >> static_cast<unsigned>(shift)) & 0xFFU));

    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t block{0}; block < padded.size(); block += 64U)
    {
        for (std::size_t word{0}; word < 16U; ++word)
            schedule[word] =
                (static_cast<std::uint32_t>(static_cast<unsigned char>(padded[block + 4U * word]))
                 << 24U) |
                (static_cast<std::uint32_t>(
                     static_cast<unsigned char>(padded[block + 4U * word + 1U]))
                 << 16U) |
                (static_cast<std::uint32_t>(
                     static_cast<unsigned char>(padded[block + 4U * word + 2U]))
                 << 8U) |
                static_cast<std::uint32_t>(static_cast<unsigned char>(padded[block + 4U * word + 3U]));
        for (std::size_t word{16}; word < 64U; ++word)
        {
            const std::uint32_t sigma0{rotate_right(schedule[word - 15U], 7U) ^
                                       rotate_right(schedule[word - 15U], 18U) ^
                                       (schedule[word - 15U] >> 3U)};
            const std::uint32_t sigma1{rotate_right(schedule[word - 2U], 17U) ^
                                       rotate_right(schedule[word - 2U], 19U) ^
                                       (schedule[word - 2U] >> 10U)};
            schedule[word] = schedule[word - 16U] + sigma0 + schedule[word - 7U] + sigma1;
        }
        auto [wa, wb, wc, wd, we, wf, wg, wh] = digest;
        for (std::size_t round{0}; round < 64U; ++round)
        {
            const std::uint32_t big_sigma1{rotate_right(we, 6U) ^ rotate_right(we, 11U) ^
                                           rotate_right(we, 25U)};
            const std::uint32_t choose{(we & wf) ^ (~we & wg)};
            const std::uint32_t temp1{wh + big_sigma1 + choose + kSha256RoundConstants[round] +
                                      schedule[round]};
            const std::uint32_t big_sigma0{rotate_right(wa, 2U) ^ rotate_right(wa, 13U) ^
                                           rotate_right(wa, 22U)};
            const std::uint32_t majority{(wa & wb) ^ (wa & wc) ^ (wb & wc)};
            const std::uint32_t temp2{big_sigma0 + majority};
            wh = wg;
            wg = wf;
            wf = we;
            we = wd + temp1;
            wd = wc;
            wc = wb;
            wb = wa;
            wa = temp1 + temp2;
        }
        digest[0] += wa;
        digest[1] += wb;
        digest[2] += wc;
        digest[3] += wd;
        digest[4] += we;
        digest[5] += wf;
        digest[6] += wg;
        digest[7] += wh;
    }

    constexpr std::string_view kHexDigits{"0123456789abcdef"};
    std::string out;
    out.reserve(64U);
    for (const std::uint32_t word : digest)
        for (int shift{28}; shift >= 0; shift -= 4)
            out.push_back(kHexDigits[(word >> static_cast<unsigned>(shift)) & 0xFU]);
    return out;
}

// ── The committed-file rows ───────────────────────────────────────────────────────────────────
struct TraceRow
{
    std::string path;
    std::string sha256;
    std::uint64_t bytes{0};
    std::string job_class;
    std::string depth_type;
    bool elided{false};
    std::string result;
    std::string wfapi_status;               // "" = no wfapi
    std::optional<std::uint64_t> wfapi_stage_count;
    std::optional<std::uint64_t> wfapi_step_count;
    std::uint64_t console_stage_count{0};
    std::uint64_t console_step_count{0};
    std::string console_finished;           // "" = the instrument found no epilogue
    std::string stamp_class;                // studies/010 §6.2: whole-stream | payload-stamped | bare
};

[[nodiscard]] std::vector<std::string_view> split_tabs(std::string_view line)
{
    std::vector<std::string_view> fields;
    std::size_t begin{0};
    while (true)
    {
        const std::size_t tab{line.find('\t', begin)};
        fields.push_back(line.substr(begin, tab - begin));
        if (tab == std::string_view::npos)
            return fields;
        begin = tab + 1U;
    }
}

[[nodiscard]] std::uint64_t parse_count(std::string_view text, bool& parse_ok)
{
    std::uint64_t value{0};
    const auto [ptr, err]{std::from_chars(text.data(), text.data() + text.size(), value)};
    if (err != std::errc{} || ptr != text.data() + text.size())
        parse_ok = false;
    return value;
}

// ── One trace's engine result through the shipped chain ──────────────────────────────────────
struct TraceEngineResult
{
    std::vector<std::string> stage_names; // console order
    std::size_t steps{0};
    bool outcome_recovered{false};
    RunOutcome outcome{RunOutcome::Unknown};
    std::string finished_token; // the scan's RemainderToken word ("" when absent)
};

[[nodiscard]] const char* outcome_name(RunOutcome outcome)
{
    switch (outcome)
    {
    case RunOutcome::Success: return "Success";
    case RunOutcome::Failure: return "Failure";
    case RunOutcome::Aborted: return "Aborted";
    case RunOutcome::Unstable: return "Unstable";
    case RunOutcome::Unknown: return "Unknown";
    }
    return "?";
}

[[nodiscard]] ComposedSemantics jenkins_only()
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::jenkins::kDialect,
                                                            {});
}

// The DECLARED stacks, per stamp class (T5 §6: declarations come from the frozen sidecar labels,
// never from inspection). whole-stream ⇒ the bracket row; payload-stamped and bare ⇒ the
// degenerate stack (the payload-stamped class is NOT declarable — ADR-23 — so its stamps
// stay content, the attributed re-baseline).
[[nodiscard]] const insight::transport::TransportStack& stack_for(std::string_view stamp_class)
{
    static const insight::transport::TransportStack degenerate{};
    static const std::array<std::string_view, 1> bracket_names{"bracket-rfc3339-line-prefix"};
    static const insight::transport::TransportStack bracket{insight::transport::
        resolve_transport_stack(insight::transport::IngestDeclaration{.stack = bracket_names})};
    return stamp_class == "whole-stream" ? bracket : degenerate;
}

[[nodiscard]] TraceEngineResult score_trace(const std::string& bytes,
                                            const ComposedSemantics& composed,
                                            const insight::transport::TransportStack& stack)
{
    TraceEngineResult result;
    std::vector<std::string> outcome_lines;
    std::string stage1_scratch;

    for (std::size_t begin{0}; begin < bytes.size();)
    {
        std::size_t end{bytes.find('\n', begin)};
        if (end == std::string::npos)
            end = bytes.size();
        const std::string_view raw_line{bytes.data() + begin, end - begin};
        begin = end + 1U;
        if (raw_line.empty())
            continue;

        // ── the purified chain: DECLARED transport peel → stage 1 → shipped recognize() ──
        // (parse order transport → logformat → intent, ADR-23). A stamp-only line peels to
        // blank and blank means DROP (PeeledLine::is_blank — the strategy's bundled #4, now
        // catalogue-side); a line the row's grammar declines peels to itself (totality is
        // application, not effect — ADR-23).
        const insight::transport::RawPeeledLine peeled{stack.peel_raw(raw_line)};
        if (peeled.content.empty())
            continue;

        // The outcome scan receives the PEELED line: `scan_run_outcome` owns its own stage 1
        // (it drives the real LogParser) and its own `\r` anchoring.
        outcome_lines.emplace_back(peeled.content);

        const auto normalized{insight::tokenization::normalize(peeled.content, stage1_scratch)};
        if (normalized.bytes().empty())
            continue; // the line was all escape bytes — the LogParser discipline
        const auto marker{recognize(normalized.undeclared_suffix(0), composed)};
        if (marker.kind == IntentMarkerKind::Job)
            result.stage_names.emplace_back(marker.name);
        else if (marker.kind == IntentMarkerKind::Step)
            ++result.steps;
    }

    const RunOutcomeScan scan{scan_run_outcome(outcome_lines, composed)};
    result.finished_token = scan.token;
    if (scan.marker_present)
    {
        result.outcome_recovered = true;
        result.outcome = resolve_run_outcome({}, scan, composed).outcome; // console bytes ALONE
    }
    return result;
}

// ── The corpus-wide score, computed ONCE and shared by the leg tests ─────────────────────────
struct CorpusScore
{
    // population
    std::size_t rows{0};
    std::map<std::string, std::size_t> depth_cells;
    std::size_t workflow_jobs{0};
    std::size_t wfapi_traces{0};
    std::size_t stage_bearing{0};
    std::size_t wfapi_stage_rows{0};
    std::size_t wfapi_step_sum{0};
    std::size_t unstable_wfapi_stages{0};
    std::size_t elided{0};
    std::map<std::string, std::size_t> result_cells;
    std::map<std::string, std::size_t> stamp_cells; // studies/010 §6.2 — the second partition
    std::size_t cross_surface{0}; // result ABORTED ∧ wfapi FAILED — counted, result rules
    std::size_t console_stage_rows{0};
    std::size_t console_finished_absent{0};
    std::vector<std::string> integrity_errors;

    // L-T
    std::size_t transcription_mismatches{0};
    std::vector<std::string> transcription_rows;

    // L-S1 / L-S2 — cells keyed (depth_type, elided)
    std::map<std::pair<std::string, bool>, StructuralCell> s1_cells;
    std::map<std::pair<std::string, bool>, StructuralCell> s2_cells;
    std::size_t s1_per_trace_mismatches{0}; // engine hit != oracle-derived hit, per trace
    std::vector<std::string> s1_rows;

    // L-O
    std::size_t agree_success{0};
    std::size_t agree_failure{0};
    std::size_t agree_unstable{0};
    std::size_t agree_aborted{0};
    std::size_t console_absent{0};
    std::size_t divergent{0};
    std::vector<std::string> divergent_rows;
    std::size_t unstable_recovered{0}; // of the 4 named UNSTABLE traces
    std::vector<std::string> unstable_rows;
    std::vector<std::string> vocabulary_errors;
};

[[nodiscard]] std::size_t multiset_hits(const std::vector<std::string>& left,
                                        const std::vector<std::string>& right)
{
    std::map<std::string, std::size_t> counts;
    for (const std::string& name : left)
        ++counts[name];
    std::size_t hits{0};
    for (const std::string& name : right)
        if (auto found{counts.find(name)}; found != counts.end() && found->second > 0)
        {
            --found->second;
            ++hits;
        }
    return hits;
}

class JenkinsRecognizerRetrofitGate : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Clause 2 — UNSET vs SET-BUT-BROKEN are different states and must not share a verdict.
        const char* const raw{std::getenv(kCorpusVar)};
        if (raw == nullptr || *raw == '\0')
            GTEST_SKIP() << kCorpusVar
                         << " unset — the §2a-private Jenkins marker corpus is not present. Point "
                            "it at the tree holding the RETRO-v2.*.tsv projections and data/v2/ "
                            "(i.e. .../coderoast-corpora/jenkins_corpora/marker_corpus).";
        root_ = std::filesystem::path{raw};
        ASSERT_TRUE(std::filesystem::is_regular_file(root_ / kTraceSidecar))
            << kCorpusVar << " is set to '" << raw << "' but there is no " << kTraceSidecar
            << " under it. The corpus is declared present, so this is a wiring error — unset the "
               "variable if this machine has no §2a slice.";
        ASSERT_TRUE(std::filesystem::is_regular_file(root_ / kWfapiStages))
            << kCorpusVar << ": missing " << kWfapiStages;
        ASSERT_TRUE(std::filesystem::is_regular_file(root_ / kConsoleStages))
            << kCorpusVar << ": missing " << kConsoleStages;
        ASSERT_TRUE(std::filesystem::is_directory(root_ / kBytesRoot))
            << kCorpusVar << ": no " << kBytesRoot << "/ under it — the corpus bytes ride the "
            << "data/ symlink on the desk box and must be mounted.";
    }

    [[nodiscard]] static const CorpusScore& score()
    {
        static const CorpusScore computed{compute_score()};
        return computed;
    }

    [[nodiscard]] static std::string read_file(const std::filesystem::path& path, bool& read_ok)
    {
        std::ifstream input{path, std::ios::binary};
        if (!input)
        {
            read_ok = false;
            return {};
        }
        std::ostringstream buffer;
        buffer << input.rdbuf();
        return std::move(buffer).str();
    }

    // Read one committed TSV; returns data rows (header validated against `expected_header`).
    [[nodiscard]] static std::vector<std::vector<std::string>>
    read_tsv(const std::filesystem::path& path, std::string_view expected_header,
             std::vector<std::string>& errors)
    {
        std::vector<std::vector<std::string>> rows;
        bool read_ok{true};
        const std::string text{read_file(path, read_ok)};
        if (!read_ok)
        {
            errors.push_back("could not read " + path.string());
            return rows;
        }
        std::size_t line_no{0};
        for (std::size_t begin{0}; begin < text.size();)
        {
            std::size_t end{text.find('\n', begin)};
            if (end == std::string::npos)
                end = text.size();
            const std::string_view line{text.data() + begin, end - begin};
            begin = end + 1U;
            if (line.empty())
                continue;
            ++line_no;
            if (line_no == 1U)
            {
                if (line != expected_header)
                    errors.push_back(path.filename().string() +
                                     ": header drifted from the frozen column order — the reader "
                                     "indexes positionally and must be revisited, not guessed at");
                continue;
            }
            std::vector<std::string> fields;
            for (const std::string_view field : split_tabs(line))
                fields.emplace_back(field);
            rows.push_back(std::move(fields));
        }
        return rows;
    }

    [[nodiscard]] static CorpusScore compute_score()
    {
        CorpusScore corpus;

        constexpr std::string_view kWfapiHeader{"path\tstage_index\tname\tstatus"};
        constexpr std::string_view kConsoleHeader{"path\tstage_index\tname"};

        std::vector<TraceRow> traces;
        for (const auto& fields :
             read_tsv(root_ / kTraceSidecar, kTraceHeader, corpus.integrity_errors))
        {
            bool row_ok{fields.size() == 14U};
            if (!row_ok)
            {
                corpus.integrity_errors.push_back("unparseable trace-sidecar row");
                continue;
            }
            TraceRow row{.path = fields[0],
                         .sha256 = fields[1],
                         .job_class = fields[3],
                         .depth_type = fields[4],
                         .result = fields[6],
                         .wfapi_status = fields[7],
                         .console_finished = fields[12],
                         .stamp_class = fields[13]};
            row.bytes = parse_count(fields[2], row_ok);
            row.elided = fields[5] == "1";
            row_ok = row_ok && (fields[5] == "0" || fields[5] == "1");
            if (!fields[8].empty())
                row.wfapi_stage_count = parse_count(fields[8], row_ok);
            if (!fields[9].empty())
                row.wfapi_step_count = parse_count(fields[9], row_ok);
            row.console_stage_count = parse_count(fields[10], row_ok);
            row.console_step_count = parse_count(fields[11], row_ok);
            if (!row_ok)
            {
                corpus.integrity_errors.push_back("unparseable trace-sidecar row for '" +
                                                  row.path + "'");
                continue;
            }
            traces.push_back(std::move(row));
        }

        std::map<std::string, std::vector<std::string>> wfapi_names;
        for (const auto& fields :
             read_tsv(root_ / kWfapiStages, kWfapiHeader, corpus.integrity_errors))
        {
            if (fields.size() != 4U)
            {
                corpus.integrity_errors.push_back("unparseable wfapi-stages row");
                continue;
            }
            ++corpus.wfapi_stage_rows;
            wfapi_names[fields[0]].push_back(fields[2]);
            if (fields[3] == "UNSTABLE")
                ++corpus.unstable_wfapi_stages;
        }
        std::map<std::string, std::vector<std::string>> console_names;
        for (const auto& fields :
             read_tsv(root_ / kConsoleStages, kConsoleHeader, corpus.integrity_errors))
        {
            if (fields.size() != 3U)
            {
                corpus.integrity_errors.push_back("unparseable console-stages row");
                continue;
            }
            ++corpus.console_stage_rows;
            console_names[fields[0]].push_back(fields[2]);
        }

        const ComposedSemantics composed{jenkins_only()};

        for (const TraceRow& row : traces)
        {
            ++corpus.rows;
            ++corpus.depth_cells[row.depth_type];
            ++corpus.result_cells[row.result];
            ++corpus.stamp_cells[row.stamp_class];
            if (row.job_class == "WorkflowJob")
                ++corpus.workflow_jobs;
            if (row.elided)
                ++corpus.elided;
            if (row.wfapi_stage_count.has_value() != (row.job_class == "WorkflowJob"))
                corpus.integrity_errors.push_back(
                    "trace '" + row.path + "': wfapi presence and WorkflowJob disagree — the "
                    "wfapi ⟺ WorkflowJob biconditional broke");
            if (row.wfapi_stage_count.has_value())
            {
                ++corpus.wfapi_traces;
                if (*row.wfapi_stage_count > 0)
                    ++corpus.stage_bearing;
                corpus.wfapi_step_sum += static_cast<std::size_t>(row.wfapi_step_count.value_or(0));
            }
            if (row.result == "ABORTED" && row.wfapi_status == "FAILED")
                ++corpus.cross_surface;
            if (row.console_finished.empty())
                ++corpus.console_finished_absent;

            // clause 4 — bytes verified against the ATTESTED digest, not assumed.
            bool read_ok{true};
            const std::string bytes{read_file(root_ / kBytesRoot / row.path, read_ok)};
            if (!read_ok)
            {
                corpus.integrity_errors.push_back("trace '" + row.path +
                                                  "' is attested by the sidecar but not readable");
                continue;
            }
            if (bytes.size() != row.bytes)
                corpus.integrity_errors.push_back(
                    "trace '" + row.path + "' size " + std::to_string(bytes.size()) +
                    " != attested " + std::to_string(row.bytes));
            if (sha256_hex(bytes) != row.sha256)
                corpus.integrity_errors.push_back(
                    "trace '" + row.path + "' sha256 differs from the attested digest — wrong "
                    "bytes under a right count is the fabricated-pass shape");

            const TraceEngineResult engine{
                score_trace(bytes, composed, stack_for(row.stamp_class))};

            // ── L-T: the shipped chain equals the frozen instrument, per trace ──
            const std::vector<std::string> empty_names;
            const auto oracle_names_it{console_names.find(row.path)};
            const std::vector<std::string>& oracle_names{
                oracle_names_it != console_names.end() ? oracle_names_it->second : empty_names};
            const bool names_equal{engine.stage_names == oracle_names};
            const bool steps_equal{engine.steps == row.console_step_count};
            const bool finished_equal{engine.finished_token == row.console_finished};
            if (!names_equal || !steps_equal || !finished_equal)
            {
                ++corpus.transcription_mismatches;
                if (corpus.transcription_rows.size() < kMaxReportedRows)
                {
                    std::string detail{"L-T DIVERGENCE " + row.path + ":"};
                    if (!names_equal)
                        detail += " stage-seq engine " + std::to_string(engine.stage_names.size()) +
                                  " vs oracle " + std::to_string(oracle_names.size());
                    if (!steps_equal)
                        detail += " steps engine " + std::to_string(engine.steps) + " vs oracle " +
                                  std::to_string(row.console_step_count);
                    if (!finished_equal)
                        detail += " finished engine '" + engine.finished_token + "' vs oracle '" +
                                  row.console_finished + "'";
                    corpus.transcription_rows.push_back(std::move(detail));
                }
            }
            if (engine.stage_names.size() != row.console_stage_count && names_equal)
                corpus.integrity_errors.push_back(
                    "trace '" + row.path + "': the two committed oracle files disagree on the "
                    "console stage count — generator drift");

            // ── L-S1 / L-S2 over the stage-bearing wfapi axis ──
            if (row.wfapi_stage_count.has_value() && *row.wfapi_stage_count > 0)
            {
                const std::vector<std::string>& tree{wfapi_names[row.path]};
                const std::size_t engine_hits{multiset_hits(engine.stage_names, tree)};
                const std::size_t oracle_hits{multiset_hits(oracle_names, tree)};
                if (engine_hits != oracle_hits)
                {
                    ++corpus.s1_per_trace_mismatches;
                    if (corpus.s1_rows.size() < kMaxReportedRows)
                        corpus.s1_rows.push_back("L-S1 per-trace divergence " + row.path +
                                                 ": engine hits " + std::to_string(engine_hits) +
                                                 " vs oracle hits " + std::to_string(oracle_hits));
                }
                StructuralCell& s1{corpus.s1_cells[{row.depth_type, row.elided}]};
                s1.hits += engine_hits;
                s1.wfapi += tree.size();
                s1.console += engine.stage_names.size();
                StructuralCell& s2{corpus.s2_cells[{row.depth_type, row.elided}]};
                const std::size_t wfapi_steps{
                    static_cast<std::size_t>(row.wfapi_step_count.value_or(0))};
                s2.hits += std::min(engine.steps, wfapi_steps);
                s2.wfapi += wfapi_steps;
                s2.console += engine.steps;
            }

            // ── L-O over all 113: recover the platform verdict from console bytes alone ──
            const auto api_mapped{map_outcome_token(row.result, composed)};
            if (!api_mapped.has_value())
            {
                corpus.vocabulary_errors.push_back("trace '" + row.path + "': result '" +
                                                   row.result +
                                                   "' does not map in the composed vocabulary");
                continue;
            }
            if (!engine.outcome_recovered)
            {
                ++corpus.console_absent;
                continue;
            }
            if (engine.outcome == *api_mapped)
            {
                if (row.result == "SUCCESS")
                    ++corpus.agree_success;
                else if (row.result == "FAILURE")
                    ++corpus.agree_failure;
                else if (row.result == "UNSTABLE")
                    ++corpus.agree_unstable;
                else if (row.result == "ABORTED")
                    ++corpus.agree_aborted;
                else
                    corpus.vocabulary_errors.push_back("trace '" + row.path +
                                                       "': result class outside the recorded "
                                                       "vocabulary: " + row.result);
            }
            else
            {
                ++corpus.divergent;
                corpus.divergent_rows.push_back(
                    "console-vs-API divergence: " + row.path + " — API '" + row.result +
                    "', console " + outcome_name(engine.outcome) +
                    " (the console's declared subordination to the API result, ADR-17 "
                    "D-OUT-RUN-1 — a counted cell, never a row defect)");
            }
            if (row.result == "UNSTABLE")
            {
                if (engine.outcome_recovered && engine.outcome == RunOutcome::Unstable)
                    ++corpus.unstable_recovered;
                else
                    corpus.unstable_rows.push_back(
                        "UNSTABLE trace '" + row.path + "' recovered as " +
                        (engine.outcome_recovered ? outcome_name(engine.outcome) : "<absent>") +
                        " — Unstable is FIRST-CLASS and must never fold");
            }
        }
        return corpus;
    }

    static std::filesystem::path root_;
};

std::filesystem::path JenkinsRecognizerRetrofitGate::root_{};

// The shared diagnostic block — printed on ANY failure (verbose-on-failure, § Observability).
[[nodiscard]] std::string report(const CorpusScore& corpus)
{
    const auto cell{[&](const char* name, const std::map<std::string, std::size_t>& cells) {
        std::string out{name};
        for (const auto& [key, count] : cells)
            out += " " + key + "=" + std::to_string(count);
        return out;
    }};
    std::ostringstream out;
    out << "\n  population rows           : " << corpus.rows << " (pinned " << kTraces << ")"
        << "\n  " << cell("depth cells              :", corpus.depth_cells)
        << "\n  " << cell("result cells             :", corpus.result_cells)
        << "\n  WorkflowJob / wfapi       : " << corpus.workflow_jobs << " / "
        << corpus.wfapi_traces << " (pinned " << kWorkflowJobs << " — biconditional)"
        << "\n  stage-bearing trees       : " << corpus.stage_bearing << " (pinned "
        << kStageBearingTrees << ")"
        << "\n  wfapi stages / steps      : " << corpus.wfapi_stage_rows << " / "
        << corpus.wfapi_step_sum << " (pinned " << kWfapiStageRows << " / " << kWfapiStepSum
        << ")"
        << "\n  UNSTABLE wfapi stages     : " << corpus.unstable_wfapi_stages << " (pinned "
        << kUnstableWfapiStages << " — counted, asserted nowhere else)"
        << "\n  elided                    : " << corpus.elided << " (pinned " << kElided << ")"
        << "\n  cross-surface cell        : " << corpus.cross_surface << " (pinned "
        << kResultVsWfapiCrossSurface << " — result rules the run grain)"
        << "\n  console stage rows        : " << corpus.console_stage_rows << " (pinned "
        << kConsoleStageRows << ")"
        << "\n  integrity errors          : " << corpus.integrity_errors.size() << " (pinned 0)"
        << "\n  L-T mismatches            : " << corpus.transcription_mismatches << " of "
        << kTraces << " (pinned " << kTranscriptionMismatches
        << ")   <<< THE TRANSCRIPTION CLAIM"
        << "\n  L-S1 per-trace mismatches : " << corpus.s1_per_trace_mismatches << " (pinned 0)"
        << "\n  L-O agree S/F/U/A         : " << corpus.agree_success << "/" << corpus.agree_failure
        << "/" << corpus.agree_unstable << "/" << corpus.agree_aborted << " (pinned "
        << kAgreeSuccess << "/" << kAgreeFailure << "/" << kAgreeUnstable << "/" << kAgreeAborted
        << ")"
        << "\n  L-O absent / divergent    : " << corpus.console_absent << " / " << corpus.divergent
        << " (pinned " << kConsoleAbsent << " / " << kDivergent << ")";
    for (const auto& [key, value] : corpus.s1_cells)
        out << "\n  L-S1 [" << key.first << (key.second ? ", elided" : "") << "] hits/wfapi/console: "
            << value.hits << "/" << value.wfapi << "/" << value.console;
    for (const auto& [key, value] : corpus.s2_cells)
        out << "\n  L-S2 [" << key.first << (key.second ? ", elided" : "") << "] min/wfapi/engine : "
            << value.hits << "/" << value.wfapi << "/" << value.console;
    for (const std::string& row : corpus.integrity_errors)
        out << "\n  INTEGRITY: " << row;
    for (const std::string& row : corpus.vocabulary_errors)
        out << "\n  VOCABULARY: " << row;
    for (const std::string& row : corpus.transcription_rows)
        out << "\n  " << row;
    for (const std::string& row : corpus.s1_rows)
        out << "\n  " << row;
    for (const std::string& row : corpus.divergent_rows)
        out << "\n  " << row;
    for (const std::string& row : corpus.unstable_rows)
        out << "\n  " << row;
    return std::move(out).str();
}

[[nodiscard]] StructuralCell cell_of(const std::map<std::pair<std::string, bool>, StructuralCell>&
                                         cells,
                                     const char* depth, bool elided)
{
    const auto found{cells.find({std::string{depth}, elided})};
    return found == cells.end() ? StructuralCell{0, 0, 0} : found->second;
}

void expect_cell(const StructuralCell& actual, const StructuralCell& pinned, const char* leg,
                 const char* depth, bool elided, const CorpusScore& corpus)
{
    EXPECT_EQ(actual.hits, pinned.hits) << leg << " [" << depth << (elided ? ", elided" : "")
                                        << "] numerator moved" << report(corpus);
    EXPECT_EQ(actual.wfapi, pinned.wfapi) << leg << " [" << depth << (elided ? ", elided" : "")
                                          << "] oracle denominator moved" << report(corpus);
    EXPECT_EQ(actual.console, pinned.console)
        << leg << " [" << depth << (elided ? ", elided" : "") << "] engine denominator moved"
        << report(corpus);
}

TEST_F(JenkinsRecognizerRetrofitGate, ThePopulationIsTheCommittedSidecarVerifiedAgainstTheBytes)
{
    const CorpusScore& corpus{score()};

    // Clause 3 — the population SIZE selects the pins; an unrecognized population FAILS.
    ASSERT_EQ(corpus.rows, kTraces)
        << "the committed sidecar yielded " << corpus.rows << " trace rows, not " << kTraces
        << ". A re-sliced or re-versioned corpus must force a DELIBERATE re-pin of every cell, "
           "not silently redefine what this gate measures."
        << report(corpus);
    EXPECT_TRUE(corpus.integrity_errors.empty())
        << corpus.integrity_errors.size()
        << " integrity error(s) — attested digests, sizes, or the oracle joins failed."
        << report(corpus);

    // Clause 5 — the depth partition closes into named cells (authoritative recheck labels).
    const auto depth{[&](const char* name) {
        const auto found{corpus.depth_cells.find(name)};
        return found == corpus.depth_cells.end() ? std::size_t{0} : found->second;
    }};
    EXPECT_EQ(depth("declarative"), kDeclarative) << report(corpus);
    EXPECT_EQ(depth("scripted"), kScripted) << report(corpus);
    EXPECT_EQ(depth("matrix-pipe"), kMatrixPipe) << report(corpus);
    EXPECT_EQ(depth("matrix-classic"), kMatrixClassic) << report(corpus);
    EXPECT_EQ(depth("freestyle"), kFreestyle) << report(corpus);
    EXPECT_EQ(corpus.depth_cells.size(), 5U)
        << "an unrecognized depth label appeared — the partition no longer closes."
        << report(corpus);
    EXPECT_EQ(depth("declarative") + depth("scripted") + depth("matrix-pipe") +
                  depth("matrix-classic") + depth("freestyle"),
              corpus.rows)
        << report(corpus);

    // wfapi ⟺ WorkflowJob exactly — absence IS the freestyle/matrix-classic floor, not a gap.
    EXPECT_EQ(corpus.workflow_jobs, kWorkflowJobs) << report(corpus);
    EXPECT_EQ(corpus.wfapi_traces, kWorkflowJobs) << report(corpus);
    EXPECT_EQ(corpus.stage_bearing, kStageBearingTrees)
        << "64 stage-bearing trees is L-S's denominator — 46 no-wfapi sidecars and 3 zero-stage "
           "trees are counted, never scored (declared limitation 2)."
        << report(corpus);
    EXPECT_EQ(corpus.wfapi_stage_rows, kWfapiStageRows) << report(corpus);
    EXPECT_EQ(corpus.wfapi_step_sum, kWfapiStepSum) << report(corpus);
    EXPECT_EQ(corpus.unstable_wfapi_stages, kUnstableWfapiStages)
        << "the 12 UNSTABLE stages are a counted oracle cell — no SUT surface consumes a "
           "stage-grain verdict, and this count is asserted nowhere else (declared limitation 4)."
        << report(corpus);
    EXPECT_EQ(corpus.elided, kElided) << report(corpus);
    EXPECT_EQ(corpus.console_stage_rows, kConsoleStageRows) << report(corpus);

    const auto result{[&](const char* name) {
        const auto found{corpus.result_cells.find(name)};
        return found == corpus.result_cells.end() ? std::size_t{0} : found->second;
    }};
    EXPECT_EQ(result("SUCCESS"), kResultSuccess) << report(corpus);
    EXPECT_EQ(result("FAILURE"), kResultFailure) << report(corpus);
    EXPECT_EQ(result("ABORTED"), kResultAborted) << report(corpus);
    EXPECT_EQ(result("UNSTABLE"), kResultUnstable) << report(corpus);
    EXPECT_EQ(corpus.cross_surface, kResultVsWfapiCrossSurface)
        << "the result-vs-wfapi cross-surface cell: result rules the run grain (§2's authority "
           "map); the design prose said 1, the committed oracle carries 2 — the oracle is the "
           "record, pinned at the measured value."
        << report(corpus);

    // The stamp-class partition (studies/010 §6.2, a GENERATED sidecar column — one classifier,
    // one owner). A second axis beside the depth partition, never cross-quoted; closes to 113.
    const auto stamp{[&](const char* name) {
        const auto found{corpus.stamp_cells.find(name)};
        return found == corpus.stamp_cells.end() ? std::size_t{0} : found->second;
    }};
    EXPECT_EQ(stamp("whole-stream"), kWholeStream) << report(corpus);
    EXPECT_EQ(stamp("payload-stamped"), kPayloadStamped) << report(corpus);
    EXPECT_EQ(stamp("bare"), kBare) << report(corpus);
    EXPECT_EQ(corpus.stamp_cells.size(), 3U)
        << "an unrecognized stamp-class label appeared — the partition no longer closes."
        << report(corpus);
    EXPECT_EQ(stamp("whole-stream") + stamp("payload-stamped") + stamp("bare"), corpus.rows)
        << report(corpus);
}

TEST_F(JenkinsRecognizerRetrofitGate, LTTranscriptionTheShippedChainEqualsTheFrozenInstrument)
{
    const CorpusScore& corpus{score()};
    ASSERT_EQ(corpus.rows, kTraces) << report(corpus); // clause 3 gates every cell below

    // ═══ THE TRANSCRIPTION CLAIM — per trace, denominator 113, so compensating errors cannot
    // cancel. A nonzero count NOT attributable to a §4.4 named delta is the pre-named
    // TRANSCRIPTION-DIVERGED event: stop, report to the Founder — a claim retraction, never a
    // bug row. ═══
    EXPECT_EQ(corpus.transcription_mismatches, kTranscriptionMismatches)
        << "the shipped chain and the frozen instrument disagree on at least one trace's "
           "(stage sequence, step count, Finished token). Attribute every row below to a §4.4 "
           "named delta or ESCALATE — this is the retraction event, pre-named."
        << report(corpus);
    EXPECT_EQ(corpus.console_finished_absent, kConsoleFinishedAbsent)
        << "the v2 true-tail capture has an epilogue on every trace — an absent oracle token is "
           "an oracle regression, not an engine finding." << report(corpus);
}

TEST_F(JenkinsRecognizerRetrofitGate, LS1StageNamesAgreeWithThePlatformTree)
{
    const CorpusScore& corpus{score()};
    ASSERT_EQ(corpus.rows, kTraces) << report(corpus);

    // Per-trace agreement FIRST (green-BLIND-via-aggregates is the named failure mode): the
    // engine's per-trace name hits against the platform tree must equal the frozen instrument's
    // per-trace hits — a compensating-error pass across traces is structurally excluded.
    EXPECT_EQ(corpus.s1_per_trace_mismatches, 0U) << report(corpus);

    // The cells, by depth_type × elided (elided segregated — one-signed confound, §9.3).
    // Axis: 64 stage-bearing trees / 442 wfapi stages. Declarative is the claim carrier;
    // scripted / matrix-pipe corroborate. Depth sums reproduce TABLE 2b: 220/220 · 27/27 ·
    // 160/195.
    expect_cell(cell_of(corpus.s1_cells, "declarative", false), kS1DeclarativePlain, "L-S1",
                "declarative", false, corpus);
    expect_cell(cell_of(corpus.s1_cells, "declarative", true), kS1DeclarativeElided, "L-S1",
                "declarative", true, corpus);
    expect_cell(cell_of(corpus.s1_cells, "scripted", false), kS1ScriptedPlain, "L-S1", "scripted",
                false, corpus);
    expect_cell(cell_of(corpus.s1_cells, "scripted", true), kS1ScriptedElided, "L-S1", "scripted",
                true, corpus);
    expect_cell(cell_of(corpus.s1_cells, "matrix-pipe", false), kS1MatrixPipePlain, "L-S1",
                "matrix-pipe", false, corpus);
    expect_cell(cell_of(corpus.s1_cells, "matrix-pipe", true), kS1MatrixPipeElided, "L-S1",
                "matrix-pipe", true, corpus);
    EXPECT_EQ(corpus.s1_cells.size(), 6U)
        << "a structural cell appeared outside the six pinned (depth × elided) coordinates."
        << report(corpus);
}

TEST_F(JenkinsRecognizerRetrofitGate, LS2StepCountsAgreeWithThePlatformTree)
{
    const CorpusScore& corpus{score()};
    ASSERT_EQ(corpus.rows, kTraces) << report(corpus);

    // Axis: the same 64 trees, 2 145 oracle steps — never averaged with the 113. The elided
    // cells are segregated so the pin punishes the rows, never the capture (the one-signed
    // elision confound is visible as the matrix-pipe elided 833/887 recall shortfall).
    expect_cell(cell_of(corpus.s2_cells, "declarative", false), kS2DeclarativePlain, "L-S2",
                "declarative", false, corpus);
    expect_cell(cell_of(corpus.s2_cells, "declarative", true), kS2DeclarativeElided, "L-S2",
                "declarative", true, corpus);
    expect_cell(cell_of(corpus.s2_cells, "scripted", false), kS2ScriptedPlain, "L-S2", "scripted",
                false, corpus);
    expect_cell(cell_of(corpus.s2_cells, "scripted", true), kS2ScriptedElided, "L-S2", "scripted",
                true, corpus);
    expect_cell(cell_of(corpus.s2_cells, "matrix-pipe", false), kS2MatrixPipePlain, "L-S2",
                "matrix-pipe", false, corpus);
    expect_cell(cell_of(corpus.s2_cells, "matrix-pipe", true), kS2MatrixPipeElided, "L-S2",
                "matrix-pipe", true, corpus);
    EXPECT_EQ(corpus.s2_cells.size(), 6U) << report(corpus);
}

TEST_F(JenkinsRecognizerRetrofitGate, LOTheRunOutcomeRecoversThePlatformVerdict)
{
    const CorpusScore& corpus{score()};
    ASSERT_EQ(corpus.rows, kTraces) << report(corpus);

    EXPECT_TRUE(corpus.vocabulary_errors.empty())
        << "a verdict left the recorded vocabulary — every cell below is suspect."
        << report(corpus);

    // ═══ Unstable is FIRST-CLASS, pinned 4/4 BY NAME — never recovered as Failure/Success ═══
    EXPECT_EQ(corpus.unstable_recovered, kAgreeUnstable)
        << "an UNSTABLE run did not recover as RunOutcome::Unstable — the four-class run-grain "
           "vocabulary folded (ADR-17)."
        << report(corpus);

    // The agreement cells, per API class — denominator 113, cells sum per class (clause 5).
    EXPECT_EQ(corpus.agree_success, kAgreeSuccess) << report(corpus);
    EXPECT_EQ(corpus.agree_failure, kAgreeFailure) << report(corpus);
    EXPECT_EQ(corpus.agree_unstable, kAgreeUnstable) << report(corpus);
    EXPECT_EQ(corpus.agree_aborted, kAgreeAborted) << report(corpus);

    // The absent-console cell is PINNED — growth here is red, so it can never quietly swallow
    // misses (the named can't-FAIL dodge for this leg).
    EXPECT_EQ(corpus.console_absent, kConsoleAbsent) << report(corpus);

    // Exactly one console-vs-API divergence, and it is the Accumulo-#498 class's own trace.
    ASSERT_EQ(corpus.divergent, kDivergent)
        << "the divergence cell moved — a new console-vs-API disagreement class, or a recognizer "
           "regression; establish which before any re-pin."
        << report(corpus);
    EXPECT_NE(corpus.divergent_rows.front().find(kDivergentTrace), std::string::npos)
        << "the single divergence is pinned BY NAME (API SUCCESS, console Finished: ABORTED)."
        << report(corpus);

    // Clause 5 — the outcome cells sum to the population.
    EXPECT_EQ(corpus.agree_success + corpus.agree_failure + corpus.agree_unstable +
                  corpus.agree_aborted + corpus.console_absent + corpus.divergent,
              kTraces)
        << "the outcome partition does not close — a trace vanished into a bucket nobody counted."
        << report(corpus);

    GTEST_LOG_(INFO) << "Jenkins retrofit gate green" << report(corpus);
}

// The pre-cut bare-null oracle EMITTER lived here between the FIRST ACT and the identity cut
// (T5 5.2, ADR-8): it froze the shipped chain's per-trace scores over the 82 bare traces
// into the committed BARE-v2.precut-oracle.tsv (emitted at e6f5494, provenance in the file's
// own header) and was DELETED with the cut, exactly as announced — regenerating the oracle now
// requires re-adding code, which is the loud act the freeze demands. The comparing gate is
// test_jenkins_bare_null_gate.cpp (G-T5-BARE).

} // namespace

// ═══ L-C — the conformance-probe regression tripwire (synthetic, NO corpus, runs everywhere) ═══
// The repaired `marker_probe_for` renders the paired writer row, so the STAGE probe is
// `[Pipeline] { (probe)` and FIRES; the old `prefix + " probe"` form yielded `[Pipeline] { ( probe`
// — a probe that fires NOWHERE, which made the kit's `dialect_gate.marker_leak` leg vacuous
// (asserting "did not fire on a foreign stream" about a probe that could not fire anywhere).
// This leg observes the KIT'S OWN exported probe, so a regression inside the kit is red HERE,
// outside it — vacuity-by-regression, guarded. A separate suite (not the corpus fixture) so it
// runs on every clone and every CI leg, corpus or none.
namespace
{
TEST(JenkinsRetrofitConformanceTripwire, TheKitsOwnStageProbeFires)
{
    const auto& manifest{insight::semantic::jenkins::kManifest};
    const ComposedSemantics composed{jenkins_only()};
    for (const auto& row : manifest.markers)
    {
        const std::string probe{
            insight::semantic::conformance::marker_probe_for(row, manifest.emits)};
        ASSERT_FALSE(probe.empty()) << "row '" << row.prefix << "' has no paired writer — "
                                    << "grammar.unpaired_marker should already be red";
        std::string probe_scratch;
        const auto marker{recognize(
            insight::tokenization::normalize(probe, probe_scratch).undeclared_suffix(0), composed)};
        EXPECT_EQ(marker.kind, row.kind)
            << "the kit's probe \"" << probe << "\" does not fire its own row ('" << row.prefix
            << "') — the marker_leak leg is vacuous again (the defect the render_row repair "
               "closed; a probe that fires nowhere cannot prove a leak's absence)";
        EXPECT_EQ(marker.name, "probe")
            << "the probe's payload must round-trip through the row's extractor, probe=\"" << probe
            << "\"";
    }
}
} // namespace
// NOLINTEND
