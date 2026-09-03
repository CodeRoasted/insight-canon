// test_jenkins_bare_null_gate.cpp — G-T5-BARE: the 82 bare Jenkins traces' whole surface, frozen
// and change-detected against a committed baseline.
//
// ═══ THE CLAIM THIS GATE MAKES — NARROWED BY RULING ON 2026-08-26; READ BEFORE THE REST ═══
// This gate was born as the T5 purification's ABORT WIRE. Its baseline file,
// `BARE-v2.baseline.tsv`, was emitted from the SHIPPED PRE-CUT chain (JenkinsStrategy live)
// at insight-canon e6f5494 on 2026-07-30, so a green then meant a genuine before/after null:
//     "the T5 purification moved NOTHING on the 82 bare traces."
//
// IT NO LONGER MEANS THAT, AND THE OLD MEANING CANNOT BE RE-ARMED. Seven of the 82 rows were
// RE-EMITTED on 2026-08-26 from the CURRENT purified chain, on the Founder's explicit ruling. The
// "pre" side of those seven is not recomputable — JenkinsStrategy is deleted — so a re-emission
// can never restore the comparison; it can only re-freeze the current surface as a new baseline.
// What a green means from that date is exactly and only:
//     "nothing has moved on the 82 bare traces since 2026-08-26."
// The purification null itself is now HISTORY: it is witnessed by the attribution record below and
// by the baseline file's own provenance header, and by nothing else in this file. A later reader
// must not read the old promise off the new baseline — which is why the narrowing is written here
// in the instrument rather than left in a plan document.
//
// WHAT THIS GATE STILL PROVES, stated positively so the narrowing is not read as a retreat: the
// bare-class surface is a byte-exact regression fence over the SHIPPED tokenizer, the SHIPPED
// recognize()/scan_run_outcome assembly and the SHIPPED jenkins manifest, on 82 real third-party
// traces, at LINE grain, with a per-trace whole-surface sha256 that is strictly stronger than the
// legible columns (B-B2 below measured that). Nothing about the DESIGN of the T5 cut is claimed
// by it any more.
//
// WHY THE RE-EMISSION WAS LEGITIMATE, since this file's own standing rule is "never re-pin". The
// rule is intact and unchanged: an UNATTRIBUTED movement is the abort wire, never a re-pin. The
// 2026-08-26 movement — 7 of 82 traces — was attributed CLOSED by causal experiment (three
// ship-leg builds each reverting one candidate's hunks in the worktree only: both reverted =>
// 0/82 and exit 0, so there is no third mechanism and no residue; fb2bfd5 alone => 6/82;
// f6169f0 alone => 2/82; 6 + 2 − 1 shared = 7) to two insight-canon commits, and BOTH were judged
// CORRECT defect repairs:
//   f6169f0 (2026-08-19) — 6 traces. `sv_take_until` -> `sv_take_until_or_none` at three sibling
//           callers. Before it, a colon-less remainder collapsed the message body onto
//           `component` and left `content` EMPTY, which templated to the sha256 prefix of the
//           empty string: an empty-content collision bucket published on the wire as an ordinary
//           identity, with an unmasked message body on the cube's WHERE axis.
//   fb2bfd5 (2026-08-18) — 2 traces, one of them shared with the above. `TokenShape::has_separator`
//           gained the wrapper bytes, so a hex run >= 16 wrapped in ( { < " ' now reaches
//           `embedded_identity` instead of being kept whole. Before it, a raw third-party 64-hex
//           container id stayed literal in a template — the same leak class that once put real
//           addresses into a published render.
// Neither may be reverted to satisfy a frozen file, so the gate could NOT be made green by fixing
// code. Attribution CLOSED **plus** every attributed movement judged CORRECT is the condition —
// and the only condition — under which this baseline moves.
//
// ═══ THE RE-BASELINE PROCEDURE — DECLARED, so a third occurrence needs no fresh ruling ═══
// This gate pins a frozen surface across an OPEN-ENDED code window, so every future CORRECT canon
// masking or projection repair reds it. It did so twice in one month. An instrument that reds on
// correct work and has no ruled way to move is a fixture-decay generator, so the way to move it is
// written down rather than improvised each time:
//   1. ATTRIBUTE the movement to named commits by CAUSAL EXPERIMENT — revert candidates in the
//      worktree only — until reverting the named set yields 0 of 82. A partial attribution is the
//      abort wire: stop there, the pass does not stand.
//   2. JUDGE each attributed movement: correct repair, or defect? A defect is an engine bug and is
//      fixed in the engine — never re-baselined. This step is the Founder's or the architect's;
//      a lane may prepare it and may not conclude it.
//   3. RE-RUN. On a red the gate prints one `REBASELINE-ROW\t…` line per moved trace, in the
//      baseline's own tab-separated column order. Those lines ARE the patch: replace exactly those
//      rows in the baseline and touch nothing else, so `git diff` shows precisely which traces
//      moved and no more. There is no in-place rewrite mode and there must not be one — a
//      change-detector that can silently overwrite its own reference is not an instrument.
//   4. APPEND a dated `# re-emitted:` line to the baseline's provenance header naming the commits,
//      the verdict and the row count. A baseline that moved must show FROM THE FILE ALONE that it
//      moved by ruling and not by drift.
//   5. RE-RUN: 0 of 82 moved. Then prove the gate is still RED-CAPABLE and record the observation
//      in the falsifiability block below — a baseline that can only agree with itself is not a
//      gate.
//
// THE TEST WAS RENAMED WITH THE CLAIM, 2026-08-26: `ThePurifiedChainMovesNothingOnTheBareClass`
// -> `TheBareSurfaceIsByteIdenticalToTheCommittedBaseline`. The old name asserted the discharged
// before/after null in the one string a CI log shows a reader who opens nothing else, which is
// precisely where a narrowed claim must not survive under its old wording. The SUITE name
// (`JenkinsBareNullGate`) is unchanged — it is what `scripts/run_corpus_gates.sh` matches.
//
// ═══ THE BASELINE IS THE COMMITTED FILE ═══
// `BARE-v2.baseline.tsv`, beside this TU. 75 of its 82 rows are still the pre-cut emission of
// 2026-07-30 (by the since-deleted JenkinsBareNullPrecutOracleEmitter, 2-run byte-identical at
// emit time); 7 are the 2026-08-26 re-emission. The file's own header carries both provenances,
// and the filename keeps its `precut` token because most of the file still IS that emission —
// the rename is deferred to the planning drain, where the plan documents citing it can be
// repointed in the same pass.
//
// ═══ WHAT IS COMPARED, PER TRACE (82/82 — whole surface, line grain) ═══
// The gate recomputes the emitter's EXACT serialization through the purified chain and compares
// byte-for-byte (digest + every legible column):
//   section T: one record per `\n`-split segment (EVERY segment, empties included): the
//              Tokenizer's produced template (escaped \t, \r, \\) or `<declined>` — tokenizer
//              UNDECLARED over compose({kManifest}): pre-cut that was the shipped detection
//              world (the strategy claimed `[Pipeline] `/epilogue lines with content unmoved);
//              post-cut it is the RawText floor. Byte-identity here IS the design's assertion:
//              "the strategy's bare-line parse really was RawText-equivalent".
//   section Q: `stages:<'\x1f'-joined>` + `steps:` + `finished:` through the purified assembly
//              under the DECLARED dialect view (for_stream(jenkins, {}); the bare class declares
//              the DEGENERATE transport stack — a bare stream has no envelope to peel).
//   digest  := sha256(section T + '\n' + section Q) — the whole-surface byte compare; the
//              per-column counts exist so a red names its axis without hexdump archaeology.
//
// ═══ CLAUSE MAP ═══════════════════════════════
//   1 population = the committed baseline rows ∩ the sidecar's frozen `stamp_class` labels,
//     closure asserted BOTH ways (a baseline row without a bare sidecar row, or the converse, is
//     red)
//   2 UNSET ⇒ skip; SET-BUT-BROKEN ⇒ hard fail   3 population SIZE pinned (82)
//   4 bytes verified against the sidecar's attested sha256 — a drifted or unreadable trace is
//     reported and SKIPPED rather than aborting the run, so one bad trace cannot hide the other 81
//     and the `compared == 82` arm has something real to guard (see COVERAGE below)
//   5 the 82 = the bare cell exactly
//   6 binary reads, `\n`-split only, `\r` is content   7 observed below   8 SUT = the shipped
//   Tokenizer / recognize / scan_run_outcome   9 RUN-registered in run_corpus_gates.sh, same
//   commit as this file
//
// ═══ COVERAGE — the `compared == kBare` arm, and why it is not a tautology ═══
// It was one until 2026-08-26: every in-loop failure path was a fatal `ASSERT` that returned from
// the test body, so `compared` could only ever equal the already-pinned population size and the
// arm could not fail. It is now real. A trace that cannot be READ, whose BYTE SIZE disagrees with
// the sidecar, or whose sha256 disagrees with the attestation is reported by name and SKIPPED —
// the loop continues — so `compared` genuinely counts traces whose surface was recomputed and
// compared. The arm therefore asserts the thing its name claims: the whole declared population was
// actually measured, not merely enumerated. The change also strictly improves the diagnosis of a
// corpus drift: every drifted trace is named on one run instead of only the first.
//
// ═══ FALSIFIABILITY — OBSERVED, then recorded (clause 7); every mutation reverted ═══
//   B-A   [2026-07-30] the `[Pipeline] ` STEP row un-gated (dialect_gate → kAnyDialect, BOTH
//         projections so DialectIntent still compiles): G-T5-BARE stayed GREEN (section Q runs
//         under the jenkins declaration, where the row fires either way; section T never consumes
//         marker rows), and so did G-T5-RETRO and the conformance kit (its marker legs SKIP
//         kAnyDialect rows by design). The catcher was the package's own dialect-gating unit test —
//         `JenkinsMarkers.DialectGatedToTheDeclaringStream` RED (an undeclared stream recovered
//         Step structure) — plus `JenkinsOutcome` collaterally. SCOPE NOTE, recorded so nobody
//         later "simplifies" that unit test believing the corpus gates cover gating width: they
//         run UNDER the declaration and structurally cannot.
//   B-B   [2026-07-30] the status-KEEP helper narrowed (single-digit tokens escape
//   `is_all_digits`):
//         GREEN — the status-keyword + single-digit class has zero instances in the 82 bare traces'
//         templated surface; recorded as a fact about these bytes, not about the gate.
//   B-B2  [2026-07-30] the digit-leading whole-token mask disabled (SRC-D-TID-12 #5 trigger forced
//         false): RED — 74/82 traces DIGEST MOVED, and on every reported row the legible counts
//         were IDENTICAL (e.g. Accumulo_2_1__496: produced 4114/4114, distinct 2630/2630, digest
//         moved) — observed proof that the whole-surface digest is STRICTLY stronger than the
//         count columns, which exist only so a red names its axis.
//   B-C   [2026-08-26, ON THE RE-EMITTED BASELINE — step 5 of the procedure above, and the reason
//         step 5 exists]. A baseline re-frozen from the current chain agrees with itself by
//         construction, so it must be shown to disagree with something before its green is worth
//         anything. Three mutations, all on the ship leg (linux-gcc16-release), all reverted, and
//         each leg REBUILT (a stale binary fakes a red, a stale tree fakes a green):
//   B-C1  SUT-side, manifest grain — `kOutcomeMarkers[0].prefix` in
//         `insight-canon/semantic/jenkins/src/jenkins.cppm`, "Finished: " -> "Finshed: ": RED,
//         exit 1, **82 of 82** traces moved, 0 byte-identical, every reported row showing
//         `finished ''/'<verdict>'` and DIGEST MOVED. The re-emitted baseline detects a one-byte
//         change in the shipped declaration on every trace in the population.
//   B-C1a SUT-side, NULL result, and it is a SCOPE fact rather than a gate defect (same species as
//         B-B). `kOutcomeTokens[0].token`, "SUCCESS" -> "SUCCES": GREEN, 82/82, 0 moved. Section
//         Q's `finished:` field carries `RunOutcomeScan::token`, which is the REMAINDER after an
//         outcome MARKER prefix (core/src/compose/outcome.cpp `improve_match`) — the raw verdict
//         word off the line. The token -> RunOutcome MAPPING table is therefore NOT in this gate's
//         surface at all, and no amount of corpus would put it there. Recorded so nobody later
//         reads this gate as covering the outcome vocabulary: `JenkinsOutcome` and
//         `JenkinsRecognizerRetrofitGate`'s L-O leg are what cover that.
//   B-C2  COVERAGE-side — the `compared == kBare` arm, which was a can't-FAIL arm until this
//         commit. One trace's bytes perturbed (one byte XOR 0x20, size preserved) under a
//         SYMLINK-FARM copy of the corpus so the real corpus is never written:
//         `ci_codemc_io/job_4drian3d_job_AuthMeVelocity__96.log`. RED, exit 1, the attestation
//         mismatch reported with both sha256 values, the loop CONTINUING, `compared` = **81** vs
//         the declared 82, the coverage arm firing and the banner taking its COVERAGE form
//         ("0 of 81 compared traces moved ... 1 skipped"). Under the pre-2026-08-26 code this run
//         aborted at the first bad trace and the arm was never reached.
#include <gtest/gtest.h>

import std;
import insight.canon;            // Tokenizer / ArenaAllocator / MaskConfig / compose / recognize
import insight.semantic.jenkins; // kManifest + kDialect

namespace
{
using insight::RunOutcomeScan;
using insight::scan_run_outcome;
using insight::semantic::ComposedSemantics;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::MaskConfig;
using insight::tokenization::recognize;
using insight::tokenization::Tokenizer;

constexpr const char* kCorpusVar{"CORPUS_JENKINS_MARKERS_DIR"};
constexpr std::string_view kTraceSidecar{"RETRO-v2.trace-sidecar.tsv"};
constexpr std::string_view kBytesRoot{"data/v2"};
constexpr std::string_view kOracleFile{"BARE-v2.baseline.tsv"};
constexpr std::size_t kBare{82};
constexpr std::size_t kMaxReportedRows{10};
constexpr std::size_t kArenaBlockBytes{4U * 1024U * 1024U};

// ── SHA-256 (FIPS 180-4) — frozen, pure integer, single-shot (the retrofit-gate block) ────────
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

// The EMITTER's exact field escape (frozen with the baseline: \t, \r, \\).
[[nodiscard]] std::string escape_oracle_field(std::string_view text)
{
    std::string out;
    out.reserve(text.size());
    for (const char chr : text)
    {
        if (chr == '\t')
            out += "\\t";
        else if (chr == '\r')
            out += "\\r";
        else if (chr == '\\')
            out += "\\\\";
        else
            out += chr;
    }
    return out;
}

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

struct OracleRow
{
    std::string path;
    std::uint64_t stages{0};
    std::uint64_t steps{0};
    std::string finished;
    std::uint64_t produced_lines{0};
    std::uint64_t distinct_templates{0};
    std::string surface_sha256;
};

struct RecomputedRow
{
    std::size_t stages{0};
    std::size_t steps{0};
    std::string finished;
    std::size_t produced_lines{0};
    std::size_t distinct_templates{0};
    std::string surface_sha256;
};

// The emitter's serialization, recomputed through the PURIFIED chain (the emitter's own spelling,
// byte for byte — section T undeclared tokenizer, section Q declared-view assembly; the bare
// class's declared transport stack is DEGENERATE, so no peel step appears).
[[nodiscard]] RecomputedRow recompute(const std::string& bytes,
                                      const ComposedSemantics& undeclared_view,
                                      const ComposedSemantics& declared_view)
{
    RecomputedRow row;
    std::string section_t;
    std::set<std::string> distinct_templates;
    {
        ArenaAllocator arena{kArenaBlockBytes};
        Tokenizer tokenizer{arena, MaskConfig{}, undeclared_view};
        for (std::size_t begin{0}; begin < bytes.size();)
        {
            std::size_t end{bytes.find('\n', begin)};
            if (end == std::string::npos)
                end = bytes.size();
            const std::string_view raw_line{bytes.data() + begin, end - begin};
            begin = end + 1U;
            const auto event{tokenizer.process_line(raw_line)};
            if (event.has_value())
            {
                ++row.produced_lines;
                distinct_templates.insert(std::string{event->template_str});
                section_t += escape_oracle_field(event->template_str);
            }
            else
            {
                section_t += "<declined>";
            }
            section_t += '\n';
        }
    }
    row.distinct_templates = distinct_templates.size();

    std::vector<std::string> outcome_lines;
    std::vector<std::string> stage_names;
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
        outcome_lines.emplace_back(raw_line);
        const auto normalized{insight::tokenization::normalize(raw_line, stage1_scratch)};
        if (normalized.bytes().empty())
            continue;
        const auto marker{recognize(normalized.undeclared_suffix(0), declared_view)};
        if (marker.kind == IntentMarkerKind::Job)
            stage_names.emplace_back(marker.name);
        else if (marker.kind == IntentMarkerKind::Step)
            ++row.steps;
    }
    row.stages = stage_names.size();
    const RunOutcomeScan scan{scan_run_outcome(outcome_lines, declared_view)};
    row.finished = scan.token;

    std::string section_q{"stages:"};
    for (std::size_t index{0}; index < stage_names.size(); ++index)
    {
        if (index != 0)
            section_q += '\x1f';
        section_q += escape_oracle_field(stage_names[index]);
    }
    section_q += "\nsteps:" + std::to_string(row.steps);
    section_q += "\nfinished:" + row.finished;
    row.surface_sha256 = sha256_hex(section_t + '\n' + section_q);
    return row;
}

class JenkinsBareNullGate : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        const char* const raw{std::getenv(kCorpusVar)};
        if (raw == nullptr || *raw == '\0')
            GTEST_SKIP() << kCorpusVar
                         << " unset — the private Jenkins marker corpus is not present.";
        root_ = std::filesystem::path{raw};
        ASSERT_TRUE(std::filesystem::is_regular_file(root_ / kTraceSidecar))
            << kCorpusVar << " set but " << kTraceSidecar << " missing — a wiring error.";
        ASSERT_TRUE(std::filesystem::is_directory(root_ / kBytesRoot))
            << kCorpusVar << ": no " << kBytesRoot << "/ under it.";
    }
    static std::filesystem::path root_;
};
std::filesystem::path JenkinsBareNullGate::root_{};

TEST_F(JenkinsBareNullGate, TheBareSurfaceIsByteIdenticalToTheCommittedBaseline)
{
    bool read_ok{true};
    const auto read_file{[&read_ok](const std::filesystem::path& path)
                         {
                             std::ifstream input{path, std::ios::binary};
                             if (!input)
                             {
                                 read_ok = false;
                                 return std::string{};
                             }
                             std::ostringstream buffer;
                             buffer << input.rdbuf();
                             return std::move(buffer).str();
                         }};

    // The committed baseline, found beside this TU (__FILE__-relative — no second env var).
    const std::filesystem::path oracle_path{std::filesystem::path{__FILE__}.parent_path() /
                                            kOracleFile};
    const std::string oracle_text{read_file(oracle_path)};
    // `.string()` is load-bearing, not noise: libstdc++ spells `operator<<(ostream&, const path&)`
    // in terms of `std::quoted`, whose declaration this TU never sees — it takes std through
    // `import std;` while gtest arrives as a header, so the header template instantiates against a
    // std that is not in its scope. Streaming the string sidesteps the module/header seam entirely.
    ASSERT_TRUE(read_ok) << oracle_path.string() << " — the committed baseline is part of this "
                         << "repo; a missing file is a checkout defect, never a skip.";
    std::map<std::string, OracleRow> oracle_rows;
    {
        bool saw_header{false};
        for (std::size_t begin{0}; begin < oracle_text.size();)
        {
            std::size_t end{oracle_text.find('\n', begin)};
            if (end == std::string::npos)
                end = oracle_text.size();
            const std::string_view line{oracle_text.data() + begin, end - begin};
            begin = end + 1U;
            if (line.empty() || line.front() == '#')
                continue;
            if (!saw_header)
            {
                ASSERT_EQ(line, "path\tstages\tsteps\tfinished\tproduced_lines"
                                "\tdistinct_templates\tsurface_sha256")
                    << "the baseline header drifted — the reader indexes positionally.";
                saw_header = true;
                continue;
            }
            const auto fields{split_tabs(line)};
            ASSERT_EQ(fields.size(), 7U) << "unparseable baseline row: " << line;
            OracleRow row{.path = std::string{fields[0]},
                          .finished = std::string{fields[3]},
                          .surface_sha256 = std::string{fields[6]}};
            const auto parse_count{
                [&](std::string_view text, std::uint64_t& out_value)
                {
                    const auto [ptr, err]{
                        std::from_chars(text.data(), text.data() + text.size(), out_value)};
                    ASSERT_TRUE(err == std::errc{} && ptr == text.data() + text.size()) << text;
                }};
            parse_count(fields[1], row.stages);
            parse_count(fields[2], row.steps);
            parse_count(fields[4], row.produced_lines);
            parse_count(fields[5], row.distinct_templates);
            oracle_rows.emplace(row.path, std::move(row));
        }
    }
    ASSERT_EQ(oracle_rows.size(), kBare)
        << "the committed baseline carries " << oracle_rows.size() << " rows, not " << kBare;

    // The sidecar's bare cell (frozen labels) + attested digests — clause 1 closure BOTH ways.
    struct BareTrace
    {
        std::string path;
        std::string sha256;
        std::uint64_t size{0};
    };
    std::vector<BareTrace> traces;
    {
        const std::string sidecar{read_file(root_ / kTraceSidecar)};
        ASSERT_TRUE(read_ok);
        std::size_t line_no{0};
        for (std::size_t begin{0}; begin < sidecar.size();)
        {
            std::size_t end{sidecar.find('\n', begin)};
            if (end == std::string::npos)
                end = sidecar.size();
            const std::string_view line{sidecar.data() + begin, end - begin};
            begin = end + 1U;
            if (line.empty())
                continue;
            ++line_no;
            if (line_no == 1U)
                continue;
            const auto fields{split_tabs(line)};
            ASSERT_EQ(fields.size(), 14U);
            if (fields[13] != "bare")
                continue;
            BareTrace trace{.path = std::string{fields[0]}, .sha256 = std::string{fields[1]}};
            const auto [ptr, err]{
                std::from_chars(fields[2].data(), fields[2].data() + fields[2].size(), trace.size)};
            ASSERT_TRUE(err == std::errc{} && ptr == fields[2].data() + fields[2].size());
            traces.push_back(std::move(trace));
        }
    }
    ASSERT_EQ(traces.size(), kBare) << "the sidecar's bare cell yielded " << traces.size();
    for (const BareTrace& trace : traces)
        ASSERT_TRUE(oracle_rows.contains(trace.path))
            << "sidecar bare trace '" << trace.path
            << "' has no baseline row — the population closure broke (clause 1).";

    const std::array manifests{insight::semantic::jenkins::kManifest};
    const ComposedSemantics undeclared_view{insight::semantic::compose(manifests)};
    const ComposedSemantics declared_view{
        insight::semantic::compose(manifests).for_stream(insight::semantic::jenkins::kDialect, {})};

    std::size_t compared{0};
    std::size_t moved{0};
    std::size_t unreadable{0};
    std::vector<std::string> moved_rows;
    std::vector<std::string> rebaseline_rows;
    for (const BareTrace& trace : traces)
    {
        read_ok = true;
        const std::string bytes{read_file(root_ / kBytesRoot / trace.path)};
        // NON-FATAL BY DESIGN (clause 4, and the COVERAGE block at the head of this file). These
        // three are corpus-integrity failures, not surface movements, and each one used to be a
        // fatal ASSERT that returned from the test body — which both hid every trace after the
        // first bad one AND made the `compared == kBare` arm below unable to fail. Reporting and
        // skipping keeps the run diagnostic and gives that arm a real quantity to guard.
        if (!read_ok)
        {
            ++unreadable;
            ADD_FAILURE() << trace.path << ": unreadable under " << (root_ / kBytesRoot).string()
                          << " — the corpus mount is incomplete; this trace was NOT compared.";
            continue;
        }
        if (bytes.size() != trace.size)
        {
            ++unreadable;
            ADD_FAILURE() << trace.path << ": " << bytes.size() << " bytes on disk, sidecar "
                          << "attests " << trace.size << " — this trace was NOT compared.";
            continue;
        }
        if (const std::string actual_sha{sha256_hex(bytes)}; actual_sha != trace.sha256)
        {
            ++unreadable;
            ADD_FAILURE() << trace.path << ": corpus bytes drifted from the attestation (sha256 "
                          << actual_sha << ", sidecar attests " << trace.sha256
                          << ") — this trace was NOT compared.";
            continue;
        }

        const OracleRow& expected{oracle_rows.at(trace.path)};
        const RecomputedRow actual{recompute(bytes, undeclared_view, declared_view)};
        ++compared;
        const bool equal{actual.surface_sha256 == expected.surface_sha256 &&
                         actual.stages == expected.stages && actual.steps == expected.steps &&
                         actual.finished == expected.finished &&
                         actual.produced_lines == expected.produced_lines &&
                         actual.distinct_templates == expected.distinct_templates};
        if (!equal)
        {
            ++moved;
            if (moved_rows.size() < kMaxReportedRows)
                moved_rows.push_back(
                    trace.path + " — stages " + std::to_string(actual.stages) + "/" +
                    std::to_string(expected.stages) + " steps " + std::to_string(actual.steps) +
                    "/" + std::to_string(expected.steps) + " finished '" + actual.finished + "'/'" +
                    expected.finished + "' produced " + std::to_string(actual.produced_lines) +
                    "/" + std::to_string(expected.produced_lines) + " distinct " +
                    std::to_string(actual.distinct_templates) + "/" +
                    std::to_string(expected.distinct_templates) +
                    (actual.surface_sha256 != expected.surface_sha256 ? " DIGEST MOVED" : ""));
            // THE RE-BASELINE PATCH (procedure step 3 at the head of this file). One line per
            // moved trace, in the baseline's own column order, so a RULED re-emission is a
            // line-for-line replacement a reader can diff — never a wholesale rewrite, and never
            // an in-place one. Emitted for EVERY moved trace, not capped like the human-readable
            // rows above: a patch that silently drops rows is worse than no patch. It carries
            // counts and a digest only — no corpus content ever reaches this log.
            rebaseline_rows.push_back(trace.path + '\t' + std::to_string(actual.stages) + '\t' +
                                      std::to_string(actual.steps) + '\t' + actual.finished + '\t' +
                                      std::to_string(actual.produced_lines) + '\t' +
                                      std::to_string(actual.distinct_templates) + '\t' +
                                      actual.surface_sha256);
        }
    }

    EXPECT_EQ(moved, 0U)
        << "THE ABORT WIRE: the purified chain moved the bare class on " << moved << "/" << compared
        << " traces — the strategy's bare-line parse was NOT RawText-equivalent somewhere, or "
           "the tokenizer path drifted. The pass does not stand as landed; attribute or abort "
           "(actual/expected per axis below).";
    for (const std::string& row : moved_rows)
        ADD_FAILURE() << "  " << row;
    if (!rebaseline_rows.empty())
    {
        GTEST_LOG_(INFO) << "RE-BASELINE PATCH — " << rebaseline_rows.size()
                         << " row(s). Apply ONLY under the ruled procedure at the head of this "
                            "file (attribution CLOSED + every movement judged CORRECT): replace "
                            "exactly these rows in "
                         << kOracleFile << " and append a dated `# re-emitted:` provenance line.";
        for (const std::string& row : rebaseline_rows)
            GTEST_LOG_(INFO) << "REBASELINE-ROW\t" << row;
    }
    // COVERAGE (clause 4). Not a restatement of the population pin above: `compared` counts
    // traces whose surface was actually RECOMPUTED, and the integrity skips above can lower it.
    EXPECT_EQ(compared, kBare)
        << "the declared population is " << kBare << " bare traces but only " << compared
        << " were compared — " << unreadable
        << " trace(s) failed the corpus-integrity checks and were skipped (named above). A "
           "partially measured population cannot support this gate's claim: fix the mount or the "
           "attestation, never the pin.";
    // THE BANNER STATES THE VERDICT IT MEASURED, and this line is the reason the rule is written
    // here rather than assumed. It was emitted unconditionally, after the abort wire's own
    // EXPECT_EQ, off a quantity that counts traces REACHED and never traces EQUAL — so on
    // 2026-08-26 a run that moved 7 of 82 traces printed "G-T5-BARE green: 82/82 ...
    // byte-identical" four lines above `[  FAILED  ]`, wrong on both halves. This banner is a
    // CI-log grep surface: `G-T5-BARE green` must be findable on a green run and absent from a red
    // one.
    if (moved == 0U && compared == kBare)
        GTEST_LOG_(INFO) << "G-T5-BARE green: " << compared << "/" << kBare
                         << " bare traces compared, 0 moved — every one byte-identical against "
                            "the committed baseline.";
    // A COVERAGE red and a MOVEMENT red are different verdicts and must not share a sentence: a
    // run that skipped traces on corpus integrity has measured nothing about movement, and
    // telling its reader to "attribute the movement" sends them after a phenomenon that may not
    // exist. Both counts are always printed; the instruction follows the cause.
    else if (moved != 0U)
        GTEST_LOG_(INFO) << "G-T5-BARE RED (MOVEMENT): " << moved << " of " << compared
                         << " bare traces compared (" << kBare << " expected, " << unreadable
                         << " skipped on corpus integrity) MOVED against the committed baseline; "
                         << (compared - moved)
                         << " byte-identical. ATTRIBUTE the movement and JUDGE it — an "
                            "unattributed movement is the abort wire, never a re-pin. If every "
                            "movement is ruled a correct repair, the RE-BASELINE PATCH above is "
                            "the only sanctioned way to move this file.";
    else
        GTEST_LOG_(INFO) << "G-T5-BARE RED (COVERAGE): 0 of " << compared
                         << " compared traces moved, but the declared population is " << kBare
                         << " and " << unreadable
                         << " trace(s) were SKIPPED on corpus integrity (named above). This run "
                            "says NOTHING about movement on the skipped traces — repair the "
                            "corpus mount or the attestation and re-run; never lower the pin.";
}
} // namespace
