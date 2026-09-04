// test_gitlab_package_corpus_proof_gate.cpp — the GitLab PACKAGE PROOF over marker_corpus_v1.
// The property: does the SHIPPED `insight.semantic.gitlab` see on real GitLab traces what the
// frozen Python instrument that authored its rows saw? That is one component over bytes it does
// not author and needs no seam, so it homes in the package's own suite.
//
// ═══ WHAT IS BEING CLAIMED, IN THESE EXACT WORDS ═══
//
//   * MARKER AXIS — FIDELITY OF TRANSCRIPTION, never independent validation. The package's rows
//     were written by reading the frozen instrument's recognizers, so oracle and SUT share an
//     author's intent; what this gate proves is that the shipped C++ package carries what the
//     frozen instrument measured, per trace and per leg, on the frozen corpus. That is the honest
//     word and it is stated rather than left implicit — a transcription from Python regexes to
//     composed C++ rows across 430 real traces is exactly where a faithful-looking port silently
//     narrows. The external GitLab depth claim may not exceed that word.
//   * OUTCOME AXIS — RECOVERY OF THE PLATFORM'S RECORDED VERDICT from console bytes alone, on a
//     declared leg: the oracle is GitLab's own REST `job_status`, a producer this codebase never
//     authored. Stronger than transcription; still NOT external validity — one corpus, one roster.
//   * It does NOT re-earn, widen, or re-open the depth claim — that claim is the PO's and rests on
//     the corpus, not on this gate; it says NOTHING about prevalence beyond the corpus's declared
//     residuals, and NOTHING about the old leg in either direction.
//
// ═══ AXIS VOCABULARY — THREE AXES, NEVER A BARE "MODERN LEG" ═══
// "Modern leg" has denoted three different populations in this project's record, so every cell
// here names its axis:
//   * BANNER leg — the `Running with gitlab-runner …` banner version, ≥ 18.9 vs < 18.9, read from
//     the trace BYTES by the frozen scorer's `leg_of` and carried in the committed sidecar's `leg`
//     column (banner-modern 445 · banner-old 174 · void 8). A C++ re-derivation of that classifier
//     here would be the mirror-of-source defect this gate exists to refuse — the leg is READ, never
//     re-derived.
//   * STAMPED axis — the 32-byte runner transport prefix is present (482 traces). Derived IN-BAND
//     by the SHIPPED strategy (an engaged `ParsedLine::timestamp`), then cross-checked against the
//     committed oracle's own `stamped` column. The frozen study's recall legs are cut on THIS axis.
//   * ran-job — the exogenous never-ran predicate (15 traces), segregated and counted. Not needed
//     by any cell below; named so nobody reads the 445 as a ran-job count (ran-job banner-modern
//     is 430).
// The mixed stamped-axis loss rate (the headline percentage) is NEVER asserted here: 7.7 % of the
// stamped traces carry 17.7 % of the loss, so the mixed figure is not attributable to any
// assertion leg. The hard rule for this gate: no reported figure may span two legs, and every one
// carries its leg in the assertion's own name.
//
// ═══ THE POPULATION AND THE TWO COMMITTED ORACLE FILES (clauses 1/4) ═══
//   * `PROBE-v1.trace-sidecar.tsv` — the committed projection of the 627 `kind == "trace"` rows of
//     `PROBE-v1.manifest.json` (627 = 445 banner-modern + 174 banner-old + 8 void, unclassified 0),
//     emitted by `emit_trace_sidecar.py`, which IMPORTS `g0_gitlab.leg_of`. Read sorted as
//     committed (manifest order), uncapped, never a directory walk. Per-row `sha256` and `bytes`
//     are VERIFIED against the trace bytes (clause 4) — a right count over wrong bytes is the
//     fabricated-pass shape, and counts cannot see it.
//   * `PROBE-v1.recognition-delta.tsv` — the standing harness `emit_recognition_delta.py`'s
//     committed emission: per-trace `study_starts` (the FROZEN instrument's own section-start
//     count, via its frozen `P_SECTION`) and `markers_canon` (the harness's model of the shipped
//     ingest path). Its licence to be read as the engine's numbers is its reproduction check —
//     nine recorded figures exactly, per class — and THIS gate closes the loop from the other
//     side: the SHIPPED symbols must agree with it PER TRACE. An aggregate ("coverage 100 % on
//     430 traces") is satisfiable by a recognizer wrong per trace in compensating directions;
//     per-trace agreement is what closes that. The eidos pre/post-seam columns in that file are
//     NOT asserted here: they belong to the two-path agreement gate in `insight_sift_tests` — the
//     only binary that links both paths — and ride the emission for the join's completeness.
//   * The corpora-side check that the sidecar equals `projection(PROBE-v1.manifest.json)` is that
//     repo's governance, not this binary's — this gate cannot see the JSON and does not try to.
//     Without it, "pure function of a committed manifest" quietly becomes "of a committed copy
//     nobody reconciles".
//
// ═══ CLAUSE MAP ═════════
//   1 committed population, sorted, uncapped     → the sidecar, above
//   2 UNSET ⇒ skip; SET-BUT-BROKEN ⇒ hard fail   → SetUp()
//   3 population SIZE selects the pins; an unrecognized population FAILS, never skips
//   4 bytes verified (sha256 + size), not assumed
//   5 partitions CLOSE — legs sum to 627, outcome cells sum to each leg, void is a counted cell
//   6 binary reads, `\n`-split only, `\r` NEVER trimmed (it is content on this corpus family)
//   7 red-capability OBSERVED and recorded below
//   8 the SUT is the shipped symbols — `make_strategy()`, `recognize()`, `scan_run_outcome`,
//     `resolve_run_outcome`, `map_outcome_token`, with stage 1 = the public
//     `normalize` (the marker leg discharges the type-borne precondition, then scores the
//     package's rows on content normalized as canon normalizes it — stated in those words; the
//     outcome leg is the shipped `scan_run_outcome` outright, no assembly)
//   9 registered as RUN in `scripts/run_corpus_gates.sh` in the same pass that writes this file
//
// ═══ THE INGEST ASSEMBLY THE MARKER LEG SCORES ═══
// Per `\n`-split line: stage 1 `normalize` (skip a line that was all escape bytes,
// the LogParser discipline) → the SHIPPED strategy's `parse` (peels the 32-byte transport prefix
// where present; claims the bare marker/verdict shapes; declines everything else, which then
// scores verbatim — the RawText fall-through) → the SHIPPED `recognize()` over the composed rows.
// The marker axis has no public parser-driven entry point in any binary — which is precisely why
// every marker consumer hand-assembles the ingest, and why two of three assembled it wrong — so
// this assembly IS the consumer path, and the anti-phantom guard is specified against exactly this
// input. Per-trace agreement is at COUNT grain; name-grain agreement within a trace is carried by
// the package's byte-form fixtures and by the two-path keyed multiset, not re-checked here
// (declared, not smuggled).
//
// ═══ PIN PROVENANCE — two strengths, labelled (the G1-PEEL discipline) ═══
//   CORROBORATED    — recorded closed measurements (Kleio 2026-07-29), reproduced independently by
//                     the standing harness AND by this gate's first run.
//   CHARACTERIZATION — measured HERE first (2026-07-29), pinned so it cannot move silently. It
//                     guards regression; it confirms no prediction.
//
// ═══ FALSIFIABILITY — OBSERVED 2026-07-29, then recorded (clause 7); both mutations reverted ═══
//   M-A  the strategy's transport peel disabled (`has_transport_prefix` forced false — the shipped
//        stamped-line claim never fires, so every marker on a stamped trace sits behind 32 bytes
//        of prefix the anchor cannot cross): RED —
//        stamped traces 0 (pinned 482), banner-modern markers 0 (pinned 2 963), stamped∧banner-old
//        markers 0 (pinned 230), stamped-axis total 0 (pinned 3 193), 482 per-trace marker
//        disagreements + 482 stamped-axis disagreements, each naming its trace, banner leg and
//        expected count (first row: `…/job_2728336.log`, banner-modern, engine 0 vs oracle 7).
//        `markers, unstamped` stayed 294 — the bare-marker shapes on unstamped traces never
//        needed the peel. This is the ingest-assembly defect class that cost 1 077 markers when a
//        consumer peeled without normalizing, seen by every cell that must see it.
//   M-B  the outcome scan's `\r`-anchor reverted to `\n`-only (`find('\r')` → npos, i.e. the
//        pre-c6a1e84 engine): RED — banner-old collapsed to Success 19 (pinned 111) / Failure 20
//        (52) / Aborted 0 (6) / none 135 (5) while banner-modern stood at 329/93/11/12 (its
//        verdicts are `\n`-delimited); recovered/agree/diverge 472/471/1 (pinned 602/599/3);
//        Aborted total 11 (pinned 17, the console ceiling). Summed with the 8 void, the mutated
//        engine reads 348 S · 113 F · 11 A · 155 none — the RECORDED pre-anchor engine, per class
//        — so the mutation reproduces the historical defect exactly, and the gate is red on eight
//        named cells. This is the `\r`-blind recognizer defect class that once manufactured a
//        score on this corpus family; a gate that cannot see it has learned nothing from the
//        incident that produced its corpus.
//
// Determinism: byte-only — committed-order population, integer counts, no RNG, no clock, no float,
// no threads. The sha256 is FIPS 180-4 over bytes; pure integer.

#include <gtest/gtest.h>

import std;
import insight.canon;           // recognize / scan_run_outcome / normalize / compose
import insight.semantic.gitlab; // kManifest + make_strategy (via export import spi: IFormatStrategy)

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
constexpr const char* kCorpusVar{"CORPUS_GITLAB_MARKERS_DIR"};
constexpr std::string_view kSidecarFile{"PROBE-v1.trace-sidecar.tsv"};
constexpr std::string_view kOracleFile{"PROBE-v1.recognition-delta.tsv"};
constexpr std::string_view kBytesRoot{"data/v1"};

// The BANNER-leg vocabulary — the sidecar's `leg` column values, verbatim from `leg_of` through
// `emit_trace_sidecar.py`. The identifiers name the axis; the strings are the file's.
constexpr std::string_view kLegBannerModern{"modern"};
constexpr std::string_view kLegBannerOld{"old"};
constexpr std::string_view kLegVoid{"void"};
constexpr std::string_view kLegUnclassified{"unclassified"};

// How many disagreeing rows to print before truncating; totals are always printed in full.
constexpr std::size_t kMaxReportedRows{10};

// ── The pins ──────────────────────────────────────────────────────────────────────────────────
// CORROBORATED — population (reproduced by the sidecar emission 2026-07-29):
constexpr std::size_t kTraceRows{627};
constexpr std::size_t kBannerModernTraces{445};
constexpr std::size_t kBannerOldTraces{174};
constexpr std::size_t kVoidTraces{8};
constexpr std::size_t kUnclassifiedTraces{0};
// CORROBORATED — the producer-authored verdict distribution, 458 / 144 / 25 over the 627:
constexpr std::size_t kApiSuccess{458};
constexpr std::size_t kApiFailed{144};
constexpr std::size_t kApiCanceled{25};
// CORROBORATED — the STAMPED axis crossed with the BANNER axis (482 = 445 + 37):
constexpr std::size_t kStampedTraces{482};
constexpr std::size_t kStampedBannerModern{445}; // every banner-modern trace is stamped
constexpr std::size_t kStampedBannerOld{37};     // the contaminant cell — scored, never averaged in
// CORROBORATED — markers through the shipped ingest assembly, as crossed cells never a subtraction:
constexpr std::size_t kMarkersBannerModern{2963};    // banner-modern (all stamped)
constexpr std::size_t kMarkersStampedBannerOld{230}; // stamped ∧ banner-old
constexpr std::size_t kMarkersUnstamped{294};        // unstamped (all banner-old) — the study's 294
constexpr std::size_t kMarkersStampedAxisTotal{
    3193}; // the STAMPED-axis total, the study's numerator
// CORROBORATED — the frozen instrument's own per-leg numerators (the oracle file's integrity pins):
constexpr std::size_t kStudyStartsBannerModern{3001};
constexpr std::size_t kStudyStartsStampedAxis{3231};
constexpr std::size_t kStudyStartsUnstamped{1054};
// CORROBORATED — the outcome cells, POST `\r`-aware anchor, cut by BANNER leg (a verdict cell
// names its leg — these are never summed into one figure):
struct OutcomeLegPins
{
    std::size_t success;
    std::size_t failure;
    std::size_t aborted;
    std::size_t none;
};
constexpr OutcomeLegPins kOutcomeBannerModern{
    .success = 329, .failure = 93, .aborted = 11, .none = 12};
constexpr OutcomeLegPins kOutcomeBannerOld{.success = 111, .failure = 52, .aborted = 6, .none = 5};
// The cancel cell is pinned WITH ITS ANCHOR NAMED: 17 is the console CEILING (the producer put a
// cancel line on 17 of the 25 cancelled jobs), reached exactly because the scan anchors after a
// lone `\r` as well as after `\n` (insight-canon c6a1e84). The number is a property of where a row
// may MATCH; pinning 25 would be red for a property of GitLab's producer, not of our rows.
constexpr std::size_t kAbortedConsoleCeiling{17};
// CORROBORATED — agreement against the producer's verdict, over the 602 recovered:
constexpr std::size_t kRecoveredTraces{602};
constexpr std::size_t kAgreements{599};
// All 3 disagreements are one class: GitLab's console and GitLab's API disagreeing with each
// other, our rows faithfully reporting the console — the console's declared subordination to the
// authoritative API result, as a counted cell and never a row defect. 2 × `canceled`-but-`Job
// succeeded`, 1 × `canceled`-but-exit-code-128.
constexpr std::size_t kDisagreements{3};
constexpr std::size_t kDisagreementsConsoleSuccess{2};
constexpr std::size_t kDisagreementsConsoleFailure{1};
// CHARACTERIZATION (measured here first, 2026-07-29) — `section_start:`-shaped content at OFFSET 0
// of normalized+peeled `\n`-line content that the row grammar DECLINES: measured ZERO. The
// wireshark-class malformed stamps are still a counted reality — 60 `%s`/`$(date +%s)`
// occurrences across 21 traces at `\n`-line grain, verified 2026-07-29 — but every one sits behind
// the runner's SGR-wrapped `$ printf "\e[0K` COMMAND ECHO, so it is rejected by POSITION (the
// anti-phantom anchor) before the shape rule is ever consulted, and none reaches offset 0. The
// shape-rule decline itself (a `%s` stamp at a genuine line start) is guarded by the package's
// unit fixture (`TheMalformedProducerMarkerIsDeclinedNotMisParsed`); THIS cell pins that no
// producer shape reaches a line start unrecognized on this corpus. A NONZERO value is a new
// producer shape at offset 0 — find out what it is before re-pinning.
constexpr std::size_t kSectionShapedDeclined{0};

// ── SHA-256 (FIPS 180-4) — frozen, pure integer, single-shot ──────────────────────────────────
// Verified against the manifest's 627 attested digests on every run: a wrong constant here cannot
// pass silently, it mismatches everything.
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
    // Padded message: bytes + 0x80 + zeros + 64-bit big-endian bit length, to a 64-byte multiple.
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
                static_cast<std::uint32_t>(
                    static_cast<unsigned char>(padded[block + 4U * word + 3U]));
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

// ── The committed-file readers — targeted TSV, fail-closed on any shape drift ─────────────────
struct SidecarRow
{
    std::string path;
    std::string sha256;
    std::uint64_t bytes{0};
    std::uint64_t job_id{0};
    std::string job_status;
    std::string leg; // the BANNER axis, from `g0_gitlab.leg_of` through the generator
};

struct OracleRow
{
    std::string leg;
    bool stamped{false};
    std::uint64_t study_starts{0};  // the frozen instrument's own count (g0 P_SECTION)
    std::uint64_t markers_canon{0}; // the standing harness's model of the shipped ingest path
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

[[nodiscard]] std::uint64_t parse_count(std::string_view field, bool& parse_ok)
{
    std::uint64_t value{0};
    const auto [ptr, err]{std::from_chars(field.data(), field.data() + field.size(), value)};
    if (err != std::errc{} || ptr != field.data() + field.size())
        parse_ok = false;
    return value;
}

// ── One trace's engine result ─────────────────────────────────────────────────────────────────
struct TraceResult
{
    bool stamped{false};             // in-band: the SHIPPED strategy engaged a timestamp
    std::size_t markers{0};          // shipped recognize() over the shipped ingest assembly
    std::size_t section_declined{0}; // `section_start:`-shaped content the row grammar declined
    bool outcome_recovered{false};
    RunOutcome outcome{RunOutcome::Unknown}; // meaningful only when outcome_recovered
};

[[nodiscard]] const char* outcome_name(RunOutcome outcome)
{
    switch (outcome)
    {
    case RunOutcome::Success:
        return "Success";
    case RunOutcome::Failure:
        return "Failure";
    case RunOutcome::Aborted:
        return "Aborted";
    case RunOutcome::Unstable:
        return "Unstable";
    case RunOutcome::Unknown:
        return "Unknown";
    }
    return "?";
}

// ── The corpus-wide score, computed ONCE and shared by the three tests ────────────────────────
struct OutcomeLegCells
{
    std::size_t success{0};
    std::size_t failure{0};
    std::size_t aborted{0};
    std::size_t none{0};
};

struct CorpusScore
{
    // population
    std::size_t rows{0};
    std::size_t banner_modern{0};
    std::size_t banner_old{0};
    std::size_t void_rows{0};
    std::size_t unclassified{0};
    std::size_t api_success{0};
    std::size_t api_failed{0};
    std::size_t api_canceled{0};
    std::vector<std::string> integrity_errors; // sha/size/join/read failures — must be EMPTY

    // marker leg
    std::size_t stamped_traces{0};
    std::size_t stamped_banner_modern{0};
    std::size_t stamped_banner_old{0};
    std::size_t unstamped_banner_modern{0}; // must stay 0 — every banner-modern trace is stamped
    std::size_t markers_banner_modern{0};
    std::size_t markers_stamped_banner_old{0};
    std::size_t markers_unstamped{0};
    std::size_t markers_stamped_axis{0};
    std::size_t section_shaped_declined{0};
    std::size_t oracle_study_banner_modern{0};
    std::size_t oracle_study_stamped_axis{0};
    std::size_t oracle_study_unstamped{0};
    std::size_t per_trace_marker_mismatches{0};
    std::size_t per_trace_stamped_mismatches{0};
    std::vector<std::string> reported; // capped per-trace diagnostics

    // outcome leg
    OutcomeLegCells outcome_banner_modern;
    OutcomeLegCells outcome_banner_old;
    std::size_t recovered{0};
    std::size_t agreements{0};
    std::size_t disagreements{0};
    std::size_t disagreements_console_success{0};
    std::size_t disagreements_console_failure{0};
    std::size_t disagreements_api_canceled{0};
    std::vector<std::string> disagreement_rows; // always printed in full (there are 3)
    std::vector<std::string>
        vocabulary_errors; // engine verdict outside {S,F,A} / unknown api token
};

[[nodiscard]] ComposedSemantics gitlab_only()
{
    const std::array manifests{insight::semantic::gitlab::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::gitlab::kDialect,
                                                            {});
}

// Score one trace through the SHIPPED symbols (clause 8; the assembly the file header states).
[[nodiscard]] TraceResult score_trace(const std::string& bytes, const ComposedSemantics& composed,
                                      insight::tokenization::IFormatStrategy& strategy,
                                      ArenaAllocator& arena)
{
    TraceResult result;
    std::vector<std::string> outcome_lines;
    std::string stage1_scratch;
    std::string refixpoint_scratch; // never written: strategy content is escape-free (fixed point)

    for (std::size_t begin{0}; begin < bytes.size();)
    {
        std::size_t end{bytes.find('\n', begin)};
        if (end == std::string::npos)
            end = bytes.size();
        const std::string_view raw_line{bytes.data() + begin, end - begin};
        begin = end + 1U;
        if (raw_line.empty())
            continue;

        // The outcome scan receives the raw `\n`-split line: `scan_run_outcome` owns its own
        // stage 1 (it drives the real LogParser) and its own `\r` anchoring. Handing it stripped
        // content would double-normalize — the consumer defect this gate exists to catch, committed
        // inside the gate built to catch it.
        outcome_lines.emplace_back(raw_line);

        // ── marker leg: stage 1 (the typed factory) → shipped strategy → shipped recognize() ──
        const auto normalized{insight::tokenization::normalize(raw_line, stage1_scratch)};
        if (normalized.bytes().empty())
            continue; // the line was all escape bytes — the LogParser discipline
        // A declined line scores verbatim (suffix 0 — the RawText fall-through carries no further
        // peel); a parsed line re-expresses the strategy's content as the SUFFIX it is (the GitLab
        // strategy only ever peels the 32-byte prefix / anchored form, never a rebuild) — the same
        // offset arithmetic the production seam uses.
        auto content{normalized.undeclared_suffix(0)};
        if (const auto parsed{strategy.parse(normalized.bytes(), arena)}; parsed.has_value())
        {
            // The strategy ARENA-STORES its content (gitlab_strategy.cpp: store_string after the
            // peel) — verbatim bytes of an input that was already normalized, so stage 1 is a
            // FIXED POINT on them and the factory is the honest re-attestation available to a
            // test (production's tokenizer holds the parser and uses its mint for exactly this
            // stored-bytes shape; a test neither holds one nor may grow the friend list).
            content = insight::tokenization::normalize(parsed->content, refixpoint_scratch)
                          .undeclared_suffix(0);
            if (parsed->timestamp.has_value())
                result.stamped = true; // the 32-byte transport prefix, derived IN-BAND
        }
        const auto marker{recognize(content, composed)};
        if (marker.kind != IntentMarkerKind::None)
            ++result.markers;
        else if (content.bytes().starts_with("section_start:"))
            ++result.section_declined; // the declared-limitation cell (malformed `%s`, empty-name)
        arena.reset();
    }

    const RunOutcomeScan scan{scan_run_outcome(outcome_lines, composed)};
    if (scan.marker_present)
    {
        result.outcome_recovered = true;
        result.outcome =
            resolve_run_outcome({}, scan, composed, composed).outcome; // console bytes ALONE
    }
    return result;
}

class GitLabPackageCorpusProofGate : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Clause 2 — UNSET vs SET-BUT-BROKEN are different states and must not share a verdict.
        // Empty counts as unset: an undefined `vars.X` expands to "" on a runner.
        const char* const raw{std::getenv(kCorpusVar)};
        if (raw == nullptr || *raw == '\0')
            FAIL() << kCorpusVar
                   << " unset — the private GitLab marker corpus is not present. Point it "
                      "at the tree holding "
                   << kSidecarFile << ", " << kOracleFile
                   << " and data/v1/ (i.e. .../coderoast-corpora/gitlab_corpora/marker_corpus).";

        root_ = std::filesystem::path{raw};
        // Set-but-broken is a WIRING FAILURE, not an absent corpus: the operator declared the
        // corpus present, so skipping here is how a mis-wired gate greens forever.
        ASSERT_TRUE(std::filesystem::is_regular_file(root_ / kSidecarFile))
            << kCorpusVar << " is set to '" << raw << "' but there is no " << kSidecarFile
            << " under it. The corpus is declared present, so this is a wiring error — unset the "
               "variable if this machine has no local copy of the private corpus.";
        ASSERT_TRUE(std::filesystem::is_regular_file(root_ / kOracleFile))
            << kCorpusVar << " is set to '" << raw << "' but there is no " << kOracleFile
            << " under it.";
        ASSERT_TRUE(std::filesystem::is_directory(root_ / kBytesRoot))
            << kCorpusVar << " is set to '" << raw << "' but there is no " << kBytesRoot
            << "/ under it — the trace bytes are materialized out-of-tree and must be mounted.";
    }

    // Computed once; the three tests assert different legs of the same deterministic pass.
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

    [[nodiscard]] static CorpusScore compute_score()
    {
        CorpusScore corpus;

        // ── the committed population (clause 1) ──
        bool sidecar_ok{true};
        const std::string sidecar_text{read_file(root_ / kSidecarFile, sidecar_ok)};
        if (!sidecar_ok)
        {
            corpus.integrity_errors.push_back("could not read the committed sidecar");
            return corpus;
        }
        std::vector<SidecarRow> sidecar;
        std::unordered_map<std::string, OracleRow> oracle;
        {
            constexpr std::string_view kSidecarHeader{
                "path\tsha256\tbytes\tjob_id\tjob_status\tleg"};
            std::size_t line_no{0};
            for (std::size_t begin{0}; begin < sidecar_text.size();)
            {
                std::size_t end{sidecar_text.find('\n', begin)};
                if (end == std::string::npos)
                    end = sidecar_text.size();
                const std::string_view line{sidecar_text.data() + begin, end - begin};
                begin = end + 1U;
                if (line.empty())
                    continue;
                ++line_no;
                if (line_no == 1U)
                {
                    if (line != kSidecarHeader)
                        corpus.integrity_errors.push_back(
                            "sidecar header drifted from the frozen column order — the reader "
                            "indexes positionally and must be revisited, not guessed at");
                    continue;
                }
                const auto fields{split_tabs(line)};
                bool row_ok{fields.size() == 6U};
                if (row_ok)
                {
                    SidecarRow row{.path = std::string{fields[0]},
                                   .sha256 = std::string{fields[1]},
                                   .job_status = std::string{fields[4]},
                                   .leg = std::string{fields[5]}};
                    row.bytes = parse_count(fields[2], row_ok);
                    row.job_id = parse_count(fields[3], row_ok);
                    if (row_ok)
                        sidecar.push_back(std::move(row));
                }
                if (!row_ok)
                    corpus.integrity_errors.push_back("unparseable sidecar row " +
                                                      std::to_string(line_no));
            }
        }
        {
            bool oracle_ok{true};
            const std::string oracle_text{read_file(root_ / kOracleFile, oracle_ok)};
            if (!oracle_ok)
            {
                corpus.integrity_errors.push_back(
                    "could not read the committed recognition oracle");
                return corpus;
            }
            constexpr std::string_view kOracleHeader{
                "path\tleg\tstamped\tstudy_starts\tmarkers_canon\tmarkers_eidos_pre"
                "\tmarkers_eidos_post\tlost_pre\tgained_pre\tlost_post\tgained_post"};
            std::size_t line_no{0};
            for (std::size_t begin{0}; begin < oracle_text.size();)
            {
                std::size_t end{oracle_text.find('\n', begin)};
                if (end == std::string::npos)
                    end = oracle_text.size();
                const std::string_view line{oracle_text.data() + begin, end - begin};
                begin = end + 1U;
                if (line.empty())
                    continue;
                ++line_no;
                if (line_no == 1U)
                {
                    if (line != kOracleHeader)
                        corpus.integrity_errors.push_back(
                            "recognition-oracle header drifted from the frozen column order");
                    continue;
                }
                const auto fields{split_tabs(line)};
                bool row_ok{fields.size() == 11U};
                if (row_ok)
                {
                    OracleRow row{.leg = std::string{fields[1]}};
                    row.stamped = fields[2] == "1";
                    row_ok = row.stamped || fields[2] == "0";
                    row.study_starts = parse_count(fields[3], row_ok);
                    row.markers_canon = parse_count(fields[4], row_ok);
                    if (row_ok)
                        oracle.emplace(std::string{fields[0]}, std::move(row));
                }
                if (!row_ok)
                    corpus.integrity_errors.push_back("unparseable recognition-oracle row " +
                                                      std::to_string(line_no));
            }
        }

        // ── the shipped composition + strategy, once ──
        const ComposedSemantics composed{gitlab_only()};
        const std::unique_ptr<insight::tokenization::IFormatStrategy> strategy{
            insight::semantic::gitlab::make_strategy()};
        constexpr std::size_t kArenaBlockBytes{64U * 1024U};
        ArenaAllocator arena{kArenaBlockBytes};

        for (const SidecarRow& row : sidecar)
        {
            ++corpus.rows;
            if (row.leg == kLegBannerModern)
                ++corpus.banner_modern;
            else if (row.leg == kLegBannerOld)
                ++corpus.banner_old;
            else if (row.leg == kLegVoid)
                ++corpus.void_rows;
            else if (row.leg == kLegUnclassified)
                ++corpus.unclassified;
            else
                corpus.integrity_errors.push_back("sidecar row '" + row.path +
                                                  "' carries an unknown leg '" + row.leg + "'");
            if (row.job_status == "success")
                ++corpus.api_success;
            else if (row.job_status == "failed")
                ++corpus.api_failed;
            else if (row.job_status == "canceled")
                ++corpus.api_canceled;
            else
                corpus.vocabulary_errors.push_back("trace '" + row.path + "': job_status '" +
                                                   row.job_status +
                                                   "' is outside the recorded vocabulary");

            // clause 4 — bytes verified, not assumed.
            bool read_ok{true};
            const std::string bytes{read_file(root_ / kBytesRoot / row.path, read_ok)};
            if (!read_ok)
            {
                corpus.integrity_errors.push_back("trace '" + row.path +
                                                  "' is attested by the sidecar but not readable");
                continue;
            }
            if (bytes.size() != row.bytes)
                corpus.integrity_errors.push_back("trace '" + row.path + "' size " +
                                                  std::to_string(bytes.size()) + " != attested " +
                                                  std::to_string(row.bytes));
            if (sha256_hex(bytes) != row.sha256)
                corpus.integrity_errors.push_back("trace '" + row.path +
                                                  "' sha256 differs from the attested digest — "
                                                  "wrong bytes under a right count is the "
                                                  "fabricated-pass shape");

            const auto oracle_it{oracle.find(row.path)};
            if (oracle_it == oracle.end())
            {
                corpus.integrity_errors.push_back("trace '" + row.path +
                                                  "' has no recognition-oracle row");
                continue;
            }
            const OracleRow& expect{oracle_it->second};
            if (expect.leg != row.leg)
                corpus.integrity_errors.push_back(
                    "trace '" + row.path +
                    "': the two committed files disagree on the banner leg ('" + row.leg +
                    "' vs '" + expect.leg +
                    "') — one classifier, one owner, so this can "
                    "only be generator drift");

            // clause 5 — void is a counted cell, never scored; and it must BE the zero-byte cell.
            if (row.leg == kLegVoid || bytes.empty())
            {
                if ((row.leg == kLegVoid) != bytes.empty())
                    corpus.integrity_errors.push_back(
                        "trace '" + row.path + "': void-leg and zero-byte disagree (leg '" +
                        row.leg + "', " + std::to_string(bytes.size()) + " bytes)");
                continue;
            }

            const TraceResult result{score_trace(bytes, composed, *strategy, arena)};

            // ── marker cells, on the crossed axes ──
            corpus.section_shaped_declined += result.section_declined;
            if (result.stamped)
            {
                ++corpus.stamped_traces;
                corpus.markers_stamped_axis += result.markers;
                if (row.leg == kLegBannerModern)
                {
                    ++corpus.stamped_banner_modern;
                    corpus.markers_banner_modern += result.markers;
                }
                else
                {
                    ++corpus.stamped_banner_old;
                    corpus.markers_stamped_banner_old += result.markers;
                }
            }
            else
            {
                if (row.leg == kLegBannerModern)
                    ++corpus.unstamped_banner_modern;
                corpus.markers_unstamped += result.markers;
            }
            if (row.leg == kLegBannerModern)
                corpus.oracle_study_banner_modern += expect.study_starts;
            if (expect.stamped)
                corpus.oracle_study_stamped_axis += expect.study_starts;
            else
                corpus.oracle_study_unstamped += expect.study_starts;

            // ── per-trace agreement with the committed oracle — the anti-compensation leg: an
            // aggregate is satisfiable by per-trace errors that cancel; this is not ──
            if (result.markers != expect.markers_canon)
            {
                ++corpus.per_trace_marker_mismatches;
                if (corpus.reported.size() < kMaxReportedRows)
                    corpus.reported.push_back(
                        "PER-TRACE MARKER DISAGREEMENT " + row.path + " (banner leg '" + row.leg +
                        "', stamped " + (result.stamped ? "yes" : "no") + "): shipped engine " +
                        std::to_string(result.markers) + " vs committed oracle " +
                        std::to_string(expect.markers_canon));
            }
            if (result.stamped != expect.stamped)
            {
                ++corpus.per_trace_stamped_mismatches;
                if (corpus.reported.size() < kMaxReportedRows)
                    corpus.reported.push_back(
                        "STAMPED-AXIS DISAGREEMENT " + row.path + ": shipped strategy says " +
                        (result.stamped ? "stamped" : "unstamped") + ", committed oracle says " +
                        (expect.stamped ? "stamped" : "unstamped"));
            }

            // ── outcome cells, cut by BANNER leg before anything is asserted (P5) ──
            OutcomeLegCells& cells{row.leg == kLegBannerModern ? corpus.outcome_banner_modern
                                                               : corpus.outcome_banner_old};
            if (!result.outcome_recovered)
            {
                ++cells.none;
            }
            else
            {
                ++corpus.recovered;
                switch (result.outcome)
                {
                case RunOutcome::Success:
                    ++cells.success;
                    break;
                case RunOutcome::Failure:
                    ++cells.failure;
                    break;
                case RunOutcome::Aborted:
                    ++cells.aborted;
                    break;
                default:
                    corpus.vocabulary_errors.push_back(
                        "trace '" + row.path + "': the console scan resolved to " +
                        outcome_name(result.outcome) +
                        ", outside this package's three PrefixIsVerdict verdicts");
                    break;
                }
                const auto api_mapped{map_outcome_token(row.job_status, composed)};
                if (!api_mapped.has_value())
                {
                    corpus.vocabulary_errors.push_back("trace '" + row.path + "': job_status '" +
                                                       row.job_status +
                                                       "' does not map in the composed vocabulary");
                }
                else if (*api_mapped == result.outcome)
                {
                    ++corpus.agreements;
                }
                else
                {
                    ++corpus.disagreements;
                    if (result.outcome == RunOutcome::Success)
                        ++corpus.disagreements_console_success;
                    if (result.outcome == RunOutcome::Failure)
                        ++corpus.disagreements_console_failure;
                    if (row.job_status == "canceled")
                        ++corpus.disagreements_api_canceled;
                    corpus.disagreement_rows.push_back(
                        "console-vs-API divergence: " + row.path + " (job " +
                        std::to_string(row.job_id) + ", banner leg '" + row.leg + "'): API '" +
                        row.job_status + "', console " + outcome_name(result.outcome) +
                        " — the console's declared subordination to the API result "
                        "(SRC-D-OUT-RUN-1), a counted cell, never a row defect");
                }
            }
        }
        return corpus;
    }

    static std::filesystem::path root_;
};

std::filesystem::path GitLabPackageCorpusProofGate::root_{};

// The shared diagnostic block — printed on ANY failure so a red is diagnosable without re-running
// under a debugger — verbose on failure, actual-vs-expected with the trace named.
[[nodiscard]] std::string report(const CorpusScore& corpus)
{
    std::ostringstream out;
    out << "\n  population rows            : " << corpus.rows << " (pinned " << kTraceRows << ")"
        << "\n  banner-modern / old / void : " << corpus.banner_modern << " / " << corpus.banner_old
        << " / " << corpus.void_rows << " (pinned " << kBannerModernTraces << " / "
        << kBannerOldTraces << " / " << kVoidTraces << ")"
        << "\n  banner-unclassified        : " << corpus.unclassified << " (pinned "
        << kUnclassifiedTraces << ")"
        << "\n  api success/failed/canceled: " << corpus.api_success << " / " << corpus.api_failed
        << " / " << corpus.api_canceled << " (pinned " << kApiSuccess << " / " << kApiFailed
        << " / " << kApiCanceled << ")"
        << "\n  integrity errors           : " << corpus.integrity_errors.size() << " (pinned 0)"
        << "\n  stamped traces             : " << corpus.stamped_traces << " (pinned "
        << kStampedTraces << " = " << kStampedBannerModern << " banner-modern + "
        << kStampedBannerOld << " banner-old)"
        << "\n  markers, banner-modern     : " << corpus.markers_banner_modern << " (pinned "
        << kMarkersBannerModern << ")"
        << "\n  markers, stamped∧banner-old: " << corpus.markers_stamped_banner_old << " (pinned "
        << kMarkersStampedBannerOld << ")"
        << "\n  markers, unstamped         : " << corpus.markers_unstamped << " (pinned "
        << kMarkersUnstamped << ")"
        << "\n  markers, STAMPED axis      : " << corpus.markers_stamped_axis << " (pinned "
        << kMarkersStampedAxisTotal << " — the recall claim's numerator)"
        << "\n  per-trace marker mismatches: " << corpus.per_trace_marker_mismatches
        << " (pinned 0)   <<< THE TRANSCRIPTION CLAIM"
        << "\n  outcome banner-modern      : " << corpus.outcome_banner_modern.success << " S / "
        << corpus.outcome_banner_modern.failure << " F / " << corpus.outcome_banner_modern.aborted
        << " A / " << corpus.outcome_banner_modern.none << " none (pinned "
        << kOutcomeBannerModern.success << "/" << kOutcomeBannerModern.failure << "/"
        << kOutcomeBannerModern.aborted << "/" << kOutcomeBannerModern.none << ")"
        << "\n  outcome banner-old         : " << corpus.outcome_banner_old.success << " S / "
        << corpus.outcome_banner_old.failure << " F / " << corpus.outcome_banner_old.aborted
        << " A / " << corpus.outcome_banner_old.none << " none (pinned "
        << kOutcomeBannerOld.success << "/" << kOutcomeBannerOld.failure << "/"
        << kOutcomeBannerOld.aborted << "/" << kOutcomeBannerOld.none << ")"
        << "\n  recovered / agree / diverge: " << corpus.recovered << " / " << corpus.agreements
        << " / " << corpus.disagreements << " (pinned " << kRecoveredTraces << " / " << kAgreements
        << " / " << kDisagreements << ")";
    for (const std::string& row : corpus.integrity_errors)
        out << "\n  INTEGRITY: " << row;
    for (const std::string& row : corpus.vocabulary_errors)
        out << "\n  VOCABULARY: " << row;
    for (const std::string& row : corpus.disagreement_rows)
        out << "\n  " << row;
    for (const std::string& row : corpus.reported)
        out << "\n  " << row;
    if (corpus.reported.size() == kMaxReportedRows)
        out << "\n  … per-trace reporting truncated at " << kMaxReportedRows << " rows";
    return std::move(out).str();
}

TEST_F(GitLabPackageCorpusProofGate, ThePopulationIsTheCommittedSidecarVerifiedAgainstTheBytes)
{
    const CorpusScore& corpus{score()};

    // Clause 3 — the population SIZE selects the pins; an unrecognized population FAILS.
    ASSERT_EQ(corpus.rows, kTraceRows)
        << "the committed sidecar yielded " << corpus.rows << " trace rows, not " << kTraceRows
        << ". A re-sliced or re-versioned corpus must force a DELIBERATE re-pin of every cell in "
           "this gate, not silently redefine what it measures."
        << report(corpus);

    // Clause 4 — bytes verified, not assumed; and both committed files must join cleanly.
    EXPECT_TRUE(corpus.integrity_errors.empty())
        << corpus.integrity_errors.size()
        << " integrity error(s) — attested digests, sizes, or the sidecar↔oracle join failed."
        << report(corpus);

    // Clause 5 — the BANNER-leg partition closes into named cells.
    EXPECT_EQ(corpus.banner_modern, kBannerModernTraces) << report(corpus);
    EXPECT_EQ(corpus.banner_old, kBannerOldTraces) << report(corpus);
    EXPECT_EQ(corpus.void_rows, kVoidTraces)
        << "void (zero-byte, served HTTP 200) is a COUNTED cell, never a filter and never a parse "
           "failure attributed to the engine."
        << report(corpus);
    EXPECT_EQ(corpus.unclassified, kUnclassifiedTraces)
        << "a banner the frozen classifier could not read is a named cell — the prevalence study "
           "measured 0, so a nonzero count is a corpus finding, not a formatting detail."
        << report(corpus);
    EXPECT_EQ(corpus.banner_modern + corpus.banner_old + corpus.void_rows + corpus.unclassified,
              corpus.rows)
        << "the banner-leg cells do not sum to the population — records vanished into a bucket "
           "nobody counted."
        << report(corpus);

    // The producer-authored verdict distribution — a population property of the committed
    // sidecar, pinned before any engine number is trusted.
    EXPECT_EQ(corpus.api_success, kApiSuccess) << report(corpus);
    EXPECT_EQ(corpus.api_failed, kApiFailed) << report(corpus);
    EXPECT_EQ(corpus.api_canceled, kApiCanceled) << report(corpus);
}

TEST_F(GitLabPackageCorpusProofGate, TheMarkerLegCarriesTheRecordedDepthPerTraceAndPerAxis)
{
    const CorpusScore& corpus{score()};
    ASSERT_EQ(corpus.rows, kTraceRows) << report(corpus); // clause 3 gates every cell below

    // ── THE TRANSCRIPTION CLAIM — per trace, so compensating errors cannot cancel ──
    EXPECT_EQ(corpus.per_trace_marker_mismatches, 0U)
        << "the shipped package and the committed oracle disagree on at least one trace's marker "
           "count. Aggregates may still balance — that is exactly why this cell exists."
        << report(corpus);
    EXPECT_EQ(corpus.per_trace_stamped_mismatches, 0U)
        << "the shipped strategy and the committed oracle disagree on the STAMPED axis for at "
           "least one trace — the in-band derivation and the model have diverged."
        << report(corpus);

    // ── the STAMPED axis, crossed with the BANNER axis — a crossed cell, never a subtraction ──
    EXPECT_EQ(corpus.stamped_traces, kStampedTraces) << report(corpus);
    EXPECT_EQ(corpus.stamped_banner_modern, kStampedBannerModern)
        << "every banner-modern trace is stamped — a drift here re-opens the leg-vocabulary "
           "collision (stamped and banner-modern are two axes, and only their crossing is a cell)."
        << report(corpus);
    EXPECT_EQ(corpus.stamped_banner_old, kStampedBannerOld)
        << "the 37 stamped∧banner-old contaminants are a named, scored, never-averaged cell."
        << report(corpus);
    EXPECT_EQ(corpus.unstamped_banner_modern, 0U) << report(corpus);

    // ── the marker cells, each naming its axis; the mixed-axis rate is deliberately NOT here ──
    EXPECT_EQ(corpus.markers_banner_modern, kMarkersBannerModern)
        << "banner-modern (runner banner >= 18.9; all stamped) — the assertion leg's recognition, "
           "through stage 1 + the shipped strategy peel + the shipped recognize()."
        << report(corpus);
    EXPECT_EQ(corpus.markers_stamped_banner_old, kMarkersStampedBannerOld)
        << "stamped∧banner-old — scored and reported, never gated as depth (the old banner leg's "
           "depth claim was withdrawn); pinned so the contaminant cell cannot drift silently."
        << report(corpus);
    EXPECT_EQ(corpus.markers_unstamped, kMarkersUnstamped)
        << "unstamped (all banner-old) — the old-leg numerator, on the banner axis the prevalence "
           "study actually cut."
        << report(corpus);
    EXPECT_EQ(corpus.markers_stamped_axis, kMarkersStampedAxisTotal)
        << "the STAMPED-axis total — 3193, the measured figure the package header's recall claim "
           "rests on. This cell names the stamped axis: it is NOT a banner-leg number."
        << report(corpus);
    // Clause 5 for markers: the three cells partition every recognized marker.
    EXPECT_EQ(corpus.markers_banner_modern + corpus.markers_stamped_banner_old +
                  corpus.markers_unstamped,
              corpus.markers_stamped_axis + corpus.markers_unstamped)
        << "the marker cells do not close over the crossed axes." << report(corpus);

    // ── the committed oracle's own integrity — the frozen instrument's numerators (clause 3) ──
    EXPECT_EQ(corpus.oracle_study_banner_modern, kStudyStartsBannerModern) << report(corpus);
    EXPECT_EQ(corpus.oracle_study_stamped_axis, kStudyStartsStampedAxis) << report(corpus);
    EXPECT_EQ(corpus.oracle_study_unstamped, kStudyStartsUnstamped) << report(corpus);

    // ── the malformed-stamp declared-limitation cell (CHARACTERIZATION, measured 2026-07-29) ──
    EXPECT_EQ(corpus.section_shaped_declined, kSectionShapedDeclined)
        << "`section_start:`-shaped content at offset 0 was DECLINED by the row grammar — on this "
           "corpus that cell is pinned ZERO (the malformed `%s` class never reaches a line start; "
           "it rides the `$ printf` command echo and is rejected by position). A nonzero count is "
           "a producer shape at a genuine line start the rows do not carry — a corpus finding or "
           "a shape-rule regression, and which one must be established before any re-pin."
        << report(corpus);
}

TEST_F(GitLabPackageCorpusProofGate, TheOutcomeLegRecoversThePlatformVerdictFromConsoleBytesAlone)
{
    const CorpusScore& corpus{score()};
    ASSERT_EQ(corpus.rows, kTraceRows) << report(corpus); // clause 3 gates every cell below

    EXPECT_TRUE(corpus.vocabulary_errors.empty())
        << "a verdict left the recorded vocabulary — every cell below is suspect."
        << report(corpus);

    // ── the cells, cut by BANNER leg BEFORE they are asserted (P5 — a verdict cell names its leg;
    //    the 627-wide totals are corroborating sums, not an assertion population) ──
    EXPECT_EQ(corpus.outcome_banner_modern.success, kOutcomeBannerModern.success) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_modern.failure, kOutcomeBannerModern.failure) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_modern.aborted, kOutcomeBannerModern.aborted) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_modern.none, kOutcomeBannerModern.none)
        << "banner-modern `none` is 12 traces with NO terminal outcome line anywhere in the "
           "console — the engine is CORRECT to report none; there is no evidence to recover."
        << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_old.success, kOutcomeBannerOld.success) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_old.failure, kOutcomeBannerOld.failure) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_old.aborted, kOutcomeBannerOld.aborted) << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_old.none, kOutcomeBannerOld.none) << report(corpus);

    // Clause 5 — each banner leg's outcome cells sum to that leg's trace count.
    EXPECT_EQ(corpus.outcome_banner_modern.success + corpus.outcome_banner_modern.failure +
                  corpus.outcome_banner_modern.aborted + corpus.outcome_banner_modern.none,
              kBannerModernTraces)
        << "the banner-modern outcome partition does not close." << report(corpus);
    EXPECT_EQ(corpus.outcome_banner_old.success + corpus.outcome_banner_old.failure +
                  corpus.outcome_banner_old.aborted + corpus.outcome_banner_old.none,
              kBannerOldTraces)
        << "the banner-old outcome partition does not close." << report(corpus);

    // ── the cancel ceiling, pinned WITH ITS ANCHOR NAMED (17 of 17, never 25) ──
    EXPECT_EQ(corpus.outcome_banner_modern.aborted + corpus.outcome_banner_old.aborted,
              kAbortedConsoleCeiling)
        << "Aborted must equal the console CEILING: GitLab put `ERROR: Job failed: canceled` on "
           "17 of the 25 cancelled jobs, and the `\\r`-aware anchor (insight-canon c6a1e84) is "
           "what makes all 17 reachable (11 banner-modern `\\n`-delimited + 6 banner-old "
           "`\\r`-delimited). 25 is NOT a recall target — the other 8 carry no cancel line at "
           "all, a property of GitLab's producer, not of our rows."
        << report(corpus);

    // ── agreement with the producer-authored verdict, over the recovered set ──
    EXPECT_EQ(corpus.recovered, kRecoveredTraces) << report(corpus);
    EXPECT_EQ(corpus.agreements, kAgreements) << report(corpus);
    EXPECT_EQ(corpus.disagreements, kDisagreements)
        << "the disagreement cell is 3 console-vs-API divergences, ALL DECLARED — GitLab's console "
           "and GitLab's API disagreeing with each other, our rows faithfully reporting the "
           "console. A gate pinning zero here would be pinning an artefact of not seeing — the "
           "pre-anchor zero was exactly that."
        << report(corpus);
    EXPECT_EQ(corpus.disagreements_api_canceled, kDisagreements)
        << "every recorded divergence has API verdict `canceled` — a divergence on any other API "
           "verdict is a NEW class, not a bigger cell."
        << report(corpus);
    EXPECT_EQ(corpus.disagreements_console_success, kDisagreementsConsoleSuccess)
        << "two cancelled jobs end on `Job succeeded` (the package source declares them)."
        << report(corpus);
    EXPECT_EQ(corpus.disagreements_console_failure, kDisagreementsConsoleFailure)
        << "one cancelled job ends on `ERROR: Job failed: exit code 128`." << report(corpus);

    GTEST_LOG_(INFO) << "GitLab package proof green" << report(corpus);
}

} // namespace
