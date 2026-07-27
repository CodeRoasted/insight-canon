// NOLINTBEGIN — integration gate: literals and printed diagnostics are intended.
// test_transport_peel_equivalence_gate.cpp — G1's CORPUS arm + G1-PEEL (ADR 0044 §9), homed here.
//
// HOMING (Kleio). This is the third of G1's three grains; the other two are unit tests in canon
// core (`core/tests/transport/`, `core/tests/compose/`). This one is a PACKAGE INTEGRATION GATE and
// belongs in this package for one structural reason: it needs BOTH implementations in scope at
// once. The SUT (`insight::transport::TransportStack::peel`, canon core) and the ORACLE
// (`GitHubActionsStrategy::parse`, reached through this package's `make_strategy()`) meet HERE and
// nowhere lower — the dependency arrow runs core → semantic/github and never back.
//   • NOT insight-eidos, which already has the `CORPUS_D11_*` plumbing. Reusing that wiring would
//     home a canon-internal refactor-equivalence claim inside a downstream consumer — homing by
//     convenience past the package that owns the property.
//   • NOT a LogCraft scenario. No writer of ours produced these bytes, and that is the entire
//     value: the oracle is an implementation written years before the SUT, scored on third-party
//     logs neither was tuned against.
//
// WHAT IS BEING CLAIMED, AND WHAT IS NOT (ADR 0044 §9 block-quotes this; it will be tempting to
// overstate, so it is restated at the top of the instrument that produces the number):
//
//     G1-PEEL is REFACTOR-EQUIVALENCE, never external validity. Zero mismatches proves the declared
//     transform is behavior-preserving against the shipped detector. It proves NOTHING about the
//     transport model being right about the world, and no sentence anywhere may cite it as if it
//     did.
//
// THE POPULATION IS A PURE FUNCTION OF THE COMMITTED MANIFEST, and that is load-bearing rather than
// tidy. The first run of this measurement (Heph, 2026-07-27) scored a machine-local, GITIGNORED log
// bank sliced by `std::filesystem::directory_iterator` — UNSPECIFIED order — and capped
// mid-iteration. That population was unnameable BY CONSTRUCTION and unreproducible even on the same
// box once a file landed; its numbers are withdrawn, not reconciled. So this gate never walks a
// directory: it reads `corpus.jsonl`, takes every non-null `log_annotated`, SORTS, and does not cap.
// Same manifest ⇒ same population ⇒ same numbers, on any machine, forever.
//
// THE EQUIVALENCE DEFINITION IS THE WHOLE DESIGN, and the naive one is WRONG. Asserting
// `strategy.parse().content == peel().content` over ALL lines reports thousands of "disagreements"
// that are all artifacts of scoring lines the strategy never claimed. The claim is scored on CELL A
// ONLY — the lines the strategy claims. The other cells are lines the strategy DECLINES, and each
// decline has a different cause that must be counted separately, not summed into a failure rate:
//   A            the strategy claims the line          ⇒ peeled content MUST be byte-identical
//   blank        timestamp-only; peels to empty        ⇒ §8 bundled behavior (3) surviving the move
//   empty-input  the source line was already empty     ⇒ split out from `blank`; see below
//   B            declined solely for a leading BOM     ⇒ a REAL SHIPPED DEFECT (bugs.md 2026-07-27)
//   C            unstamped; the peel is a no-op        ⇒ §2 totality is about APPLICATION not EFFECT
//   violation    declined, yet peel changed the bytes  ⇒ the invariant that must stay 0
//
// WHY B IS NOT "FIXED" HERE. A G1-PEEL that corrected the BOM drop would break the very equivalence
// it asserts — the gate must reproduce the shipped peel INCLUDING its warts. B is its own cell so
// that when the BOM row closes, this gate reports the change loudly instead of absorbing it.
//
// FALSIFIABILITY — OBSERVED, not asserted (§9 makes this a requirement, not a note). Three peel-path
// mutations were run against this gate; each was reverted:
//   D  `strip_leading_space` ignored      ⇒ sample RED: 292 985/292 985 cell-A mismatches, AND
//                                            10 420 decline-side violations. Note WHICH cell caught
//                                            what: the timestamp-only lines stopped peeling to
//                                            empty, so they left the blank cell and landed as
//                                            violations. Cell A structurally could not have seen
//                                            that — the invariant cell is not decoration.
//   E  `is_space` stops accepting TAB     ⇒ sample GREEN, full RED (23 157 mismatches + 16
//                                            violations). See the blindness note below.
//   (the third, on the unit arms in canon core, is recorded with them.)
//
// ⚠ MEASURED BLINDNESS — the two slices are NOT interchangeable, and this is why the pins are
// per-slice rather than the gate taking whatever it is pointed at. Mutation E is a REAL divergence
// class (the GHA separator being a tab rather than a space) that `data/v1/sample` does not contain a
// single instance of: the sample arm stayed fully GREEN under it while the full arm caught 23 157
// lines. So `sample` (0.4 s) is a SMOKE arm and `full` (23.5 s) is the claim. Wiring only the fast
// one buys a green that is measurably weaker than it reads
// ([[synthetic-gate-vacuity-vs-judgment]] — green-BLIND).
//
// Determinism: byte-only. Sorted population, fixed files, no RNG, no clock, no float, no threads.

#include <gtest/gtest.h>

import std;
import insight.canon;           // insight::transport::* + ArenaAllocator
import insight.semantic.github; // make_strategy (the ORACLE)

using insight::tokenization::ArenaAllocator;
using insight::transport::IngestDeclaration;
using insight::transport::PeeledLine;
using insight::transport::TransportStack;

namespace
{

// A `const char*`, not a string_view: it is handed to getenv, and string_view::data() carries no
// null-termination guarantee.
constexpr const char* kSliceDirVar{"CORPUS_D11_SLICE_DIR"};
constexpr std::string_view kGhaTransform{"api-rfc3339-line-prefix"};
constexpr std::array<std::string_view, 1> kDeclaredGha{{kGhaTransform}};
constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF"};

// Arena block size for the oracle's `store_string`. Reset per line, so this bounds a single line.
constexpr std::size_t kArenaBlockBytes{1U << 16U};

// How many disagreeing lines to print before truncating. Enough to see the SHAPE of a failure
// without burying it; the totals are always printed in full.
constexpr std::size_t kMaxReportedLines{10};

// ── The pinned expectations, per slice ────────────────────────────────────────────────────────
// Two kinds of number live here and they are NOT equally strong. Labelled, because a reader who
// treats a characterization pin as a corroborated one will over-trust it:
//   CORROBORATED — measured independently by Heph (manifest-backed re-run) and by me (a separate
//                  oracle, written without reference to his). Two implementations agreeing is the
//                  cross-check that matters.
//   CHARACTERIZATION — measured HERE first, pinned so it cannot move silently. No independent
//                  corroboration; it guards regression, it does not confirm a prediction.
struct SlicePins
{
    std::string_view label;
    std::size_t logs;              // CORROBORATED
    std::size_t lines;             // CORROBORATED
    std::size_t claimed_equal;     // CORROBORATED (cell A)
    std::size_t blank_and_empty;   // CORROBORATED (Heph's "blank-decline, both sides")
    std::size_t bom_declines;      // CORROBORATED (cell B)
    std::size_t unstamped;         // CORROBORATED (cell C)
    std::size_t empty_input_lines; // CHARACTERIZATION — my split of blank_and_empty
};

// Cell A mismatches and decline-side violations are pinned at ZERO for every slice — they are the
// CLAIM, not a slice property, so they are not a per-slice field.
constexpr std::size_t kExpectedMismatches{0};
constexpr std::size_t kExpectedViolations{0};

// `empty_input_lines` is filled in by the first green run and is deliberately NOT guessed here —
// see the sentinel handling in the assertion. A guessed characterization pin would be a fitted
// number wearing a contract's clothes.
constexpr std::size_t kUnmeasured{std::numeric_limits<std::size_t>::max()};

constexpr std::array<SlicePins, 2> kSlices{{
    {.label = "data/v1/sample",
     .logs = 60,
     .lines = 305'852,
     .claimed_equal = 292'985,
     .blank_and_empty = 11'717,
     .bom_declines = 511,
     .unstamped = 639,
     .empty_input_lines = 1'297},
    {.label = "data/v1/full",
     .logs = 4'082,
     .lines = 22'490'937,
     .claimed_equal = 21'878'259,
     .blank_and_empty = 523'126,
     .bom_declines = 17'487,
     .unstamped = 72'065,
     .empty_input_lines = 31'822},
}};

// ── The manifest reader ───────────────────────────────────────────────────────────────────────
// A TARGETED extraction, not a JSON parser, and fail-closed on anything it does not understand: a
// silently mis-parsed manifest would build a different population and report a confident wrong
// number, which is the failure mode this whole file exists to avoid. Corpus filenames are
// `log_annotated/<owner>__<repo>__pr<N>__run<N>.log` — no escapes — so an escape sequence here means
// the manifest's shape changed and the extraction must be revisited rather than guessed at.
struct ManifestField
{
    bool found{false};
    bool null_value{false};
    std::string value;
    std::string error; // non-empty ⇒ fail loudly
};

[[nodiscard]] ManifestField extract_log_annotated(std::string_view record)
{
    constexpr std::string_view kKey{"\"log_annotated\""};
    const std::size_t key_at{record.find(kKey)};
    if (key_at == std::string_view::npos)
        return {.found = false};

    std::size_t pos{key_at + kKey.size()};
    while (pos < record.size() && (record[pos] == ' ' || record[pos] == ':'))
        ++pos;
    if (pos >= record.size())
        return {.error = "record ends immediately after \"log_annotated\""};

    if (record.compare(pos, 4U, "null") == 0)
        return {.found = true, .null_value = true};

    if (record[pos] != '"')
        return {.error = std::string{"\"log_annotated\" is neither null nor a quoted string; "
                                     "got '"} +
                         record[pos] + "'"};

    ++pos;
    std::string out;
    while (pos < record.size() && record[pos] != '"')
    {
        if (record[pos] == '\\')
            return {.error = "\"log_annotated\" contains a backslash escape; this reader is "
                             "deliberately escape-free and must be revisited, not extended by "
                             "guesswork"};
        out += record[pos];
        ++pos;
    }
    if (pos >= record.size())
        return {.error = "\"log_annotated\" string is unterminated"};
    return {.found = true, .null_value = false, .value = std::move(out)};
}

// ── The independent BOM oracle ────────────────────────────────────────────────────────────────
// Re-derived from `github_strategy.cpp`'s `is_github_actions_prefix`, deliberately NOT by calling
// it: cell B asks "would this line have been claimed but for the BOM?", which the strategy cannot
// answer about itself. Spelled out here so the two never collapse into one implementation.
[[nodiscard]] bool is_gha_stamp(std::string_view str) noexcept
{
    constexpr std::size_t kPrefixLen{28};
    if (str.size() < kPrefixLen)
        return false;
    const auto digit{[&str](std::size_t idx) noexcept
                     { return str[idx] >= '0' && str[idx] <= '9'; }};
    if (!(digit(0) && digit(1) && digit(2) && digit(3) && str[4] == '-' && digit(5) && digit(6) &&
          str[7] == '-' && digit(8) && digit(9) && str[10] == 'T' && digit(11) && digit(12) &&
          str[13] == ':' && digit(14) && digit(15) && str[16] == ':' && digit(17) && digit(18)))
        return false;
    if (str[19] != '.')
        return false;
    for (std::size_t idx{20}; idx < 27U; ++idx)
        if (!digit(idx))
            return false;
    if (str[27] != 'Z')
        return false;
    return str.size() == kPrefixLen || str[kPrefixLen] == ' ' || str[kPrefixLen] == '\t';
}

[[nodiscard]] std::string escape(std::string_view bytes)
{
    constexpr std::size_t kMaxRendered{160};
    std::string out;
    for (const char chr : bytes.substr(0U, std::min(bytes.size(), kMaxRendered)))
    {
        const auto raw{static_cast<unsigned char>(chr)};
        if (raw == '\\')
            out += "\\\\";
        else if (raw >= 0x20U && raw < 0x7FU)
            out += chr;
        else
        {
            constexpr std::string_view kHex{"0123456789ABCDEF"};
            out += "\\x";
            out += kHex[raw >> 4U];
            out += kHex[raw & 0x0FU];
        }
    }
    if (bytes.size() > kMaxRendered)
        out += "…";
    return out;
}

// ── The score ─────────────────────────────────────────────────────────────────────────────────
struct Score
{
    std::size_t logs{0};
    std::size_t lines{0};
    std::size_t claimed_equal{0};
    std::size_t claimed_mismatch{0};
    std::size_t blank_decline{0}; // non-empty input that peels to empty
    std::size_t empty_input{0};   // the source line was already empty
    std::size_t bom_decline{0};
    std::size_t unstamped{0};
    std::size_t violations{0};
    std::vector<std::string> reported; // mismatch + violation diagnostics, capped

    [[nodiscard]] std::size_t partition_total() const noexcept
    {
        return claimed_equal + claimed_mismatch + blank_decline + empty_input + bom_decline +
               unstamped + violations;
    }
};

class TransportPeelEquivalenceGate : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // UNSET vs SET-BUT-BROKEN are different states and must not share a verdict. An EMPTY value
        // counts as unset: an undefined `vars.X` expands to "" on a runner, so a nullptr-only check
        // would sail past it into a confusing failure.
        const char* const raw{std::getenv(kSliceDirVar)};
        if (raw == nullptr || *raw == '\0')
            GTEST_SKIP() << kSliceDirVar
                         << " unset — the §2a-private D11 slice is not present. Point it at a slice "
                            "directory holding corpus.jsonl AND log_annotated/, i.e. "
                            ".../github_corpora/revert_corpus/data/v1/sample (fast, 33 MB) or "
                            ".../data/v1/full (the full claim, 2.3 GB).";

        slice_ = std::filesystem::path{raw};
        // Set-but-broken is a WIRING FAILURE, not an absent corpus: the operator declared the slice
        // present, so skipping here is how a mis-wired gate reports green forever.
        ASSERT_TRUE(std::filesystem::is_regular_file(slice_ / "corpus.jsonl"))
            << kSliceDirVar << " is set to '" << raw << "' but there is no corpus.jsonl under it. "
            << "The slice is declared present, so this is a wiring error, not an absent corpus — "
               "unset the variable if this runner has no §2a slice.";
        ASSERT_TRUE(std::filesystem::is_directory(slice_ / "log_annotated"))
            << kSliceDirVar << " is set to '" << raw << "' but there is no log_annotated/ under it.";
    }

    // The population: every non-null `log_annotated`, SORTED, UNCAPPED. Never a directory walk.
    [[nodiscard]] std::vector<std::filesystem::path> population() const
    {
        std::vector<std::filesystem::path> paths;
        std::ifstream manifest{slice_ / "corpus.jsonl"};
        std::string record;
        std::size_t record_no{0};
        while (std::getline(manifest, record))
        {
            ++record_no;
            if (record.find_first_not_of(" \t\r\n") == std::string::npos)
                continue;
            const ManifestField field{extract_log_annotated(record)};
            EXPECT_TRUE(field.error.empty())
                << "corpus.jsonl record " << record_no << ": " << field.error
                << "\nThe population must be a pure function of the manifest; a record this reader "
                   "cannot parse would silently shrink it.";
            if (!field.error.empty() || !field.found || field.null_value)
                continue;
            std::filesystem::path path{slice_ / field.value};
            // A manifest row naming an absent file is a corpus-integrity failure, not a line to
            // skip ([[corpus-scrub-freeze-byte-fidelity]] — verify the asset against what the
            // manifest ATTESTS). Silently skipping would shrink the population behind a green.
            EXPECT_TRUE(std::filesystem::exists(path))
                << "corpus.jsonl record " << record_no << " attests '" << field.value
                << "' but that file is not present in the slice.";
            paths.push_back(std::move(path));
        }
        std::ranges::sort(paths);
        return paths;
    }

    [[nodiscard]] static Score score_slice(const std::vector<std::filesystem::path>& paths)
    {
        const std::unique_ptr<insight::tokenization::IFormatStrategy> strategy{
            insight::semantic::github::make_strategy()};
        const TransportStack stack{insight::transport::resolve_transport_stack(
            IngestDeclaration{.stack = kDeclaredGha, .dialect = {}, .channel = {}})};
        ArenaAllocator arena{kArenaBlockBytes};

        Score score;
        for (const std::filesystem::path& path : paths)
        {
            std::ifstream input{path, std::ios::binary};
            std::ostringstream buffer;
            buffer << input.rdbuf();
            const std::string bytes{buffer.str()};
            ++score.logs;

            std::size_t line_no{0};
            for (std::size_t begin{0}; begin < bytes.size();)
            {
                std::size_t end{bytes.find('\n', begin)};
                if (end == std::string::npos)
                    end = bytes.size();
                const std::string_view line{bytes.data() + begin, end - begin};
                begin = end + 1U;
                ++line_no;
                ++score.lines;

                arena.reset(); // bound the arena to ONE line
                const auto parsed{strategy->parse(line, arena)};
                const PeeledLine peeled{stack.peel(line)};

                if (parsed.has_value())
                {
                    // ── CELL A — the claim ──
                    if (parsed->content == peeled.content)
                    {
                        ++score.claimed_equal;
                        continue;
                    }
                    ++score.claimed_mismatch;
                    if (score.reported.size() < kMaxReportedLines)
                        score.reported.push_back(
                            std::string{"CELL-A MISMATCH "} + path.filename().string() + ":" +
                            std::to_string(line_no) + "\n    raw      : \"" + escape(line) +
                            "\"\n    strategy : \"" + escape(parsed->content) +
                            "\"\n    peel     : \"" + escape(peeled.content) + "\"");
                    continue;
                }

                // ── The strategy DECLINED. Partition by CAUSE, most specific first. ──
                if (line.empty())
                {
                    ++score.empty_input;
                    continue;
                }
                if (peeled.is_blank())
                {
                    ++score.blank_decline;
                    continue;
                }
                // Cell B must be tested BEFORE cell C: a BOM line is ALSO one the peel leaves
                // alone, so C's "peel did nothing" test would swallow it.
                if (line.starts_with(kUtf8Bom) && is_gha_stamp(line.substr(kUtf8Bom.size())))
                {
                    ++score.bom_decline;
                    continue;
                }
                if (peeled.content == line)
                {
                    ++score.unstamped;
                    continue;
                }
                // ── The invariant: on a decline, the peel either did nothing or blanked the line.
                // It must NEVER have partially peeled it into a different, non-empty content —
                // that would be the two implementations disagreeing on a line neither claims, which
                // no cell above would have caught.
                ++score.violations;
                if (score.reported.size() < kMaxReportedLines)
                    score.reported.push_back(
                        std::string{"DECLINE-SIDE VIOLATION "} + path.filename().string() + ":" +
                        std::to_string(line_no) + "\n    raw  : \"" + escape(line) +
                        "\"\n    peel : \"" + escape(peeled.content) + "\"");
            }
        }
        return score;
    }

    std::filesystem::path slice_;
};

TEST_F(TransportPeelEquivalenceGate, DeclaredPeelIsByteIdenticalToTheShippedDetectorOnCellA)
{
    const std::vector<std::filesystem::path> paths{population()};
    ASSERT_FALSE(paths.empty()) << "the manifest yielded an EMPTY population — a gate scoring zero "
                                   "lines reports green while measuring nothing";

    // Identify the slice by its population size. An UNRECOGNIZED population must FAIL, never skip:
    // the pins below do not apply to it, and a silent skip is how a re-sliced corpus turns this
    // gate into decoration.
    const SlicePins* pins{nullptr};
    for (const SlicePins& candidate : kSlices)
        if (candidate.logs == paths.size())
            pins = &candidate;
    ASSERT_NE(pins, nullptr)
        << "the manifest yielded " << paths.size()
        << " logs, which matches no pinned slice (" << kSlices[0].label << " = " << kSlices[0].logs
        << ", " << kSlices[1].label << " = " << kSlices[1].logs
        << ").\nThis gate pins per-slice numbers, so an unrecognized population is a FAILURE, not a "
           "skip — either point " << kSliceDirVar
        << " at a pinned slice, or add the new slice's pins deliberately.";

    const Score score{score_slice(paths)};

    // ── The diagnostic block. Printed on ANY failure below, so a red is diagnosable without
    // re-running under a debugger. ──
    const auto report{[&score, pins]
                      {
                          std::ostringstream out;
                          out << "\n  slice            : " << pins->label
                              << "\n  logs             : " << score.logs << " (pinned "
                              << pins->logs << ")"
                              << "\n  lines            : " << score.lines << " (pinned "
                              << pins->lines << ")"
                              << "\n  A  equal         : " << score.claimed_equal << " (pinned "
                              << pins->claimed_equal << ")"
                              << "\n  A  MISMATCH      : " << score.claimed_mismatch
                              << " (pinned 0)   <<< THE CLAIM"
                              << "\n  blank-decline    : " << score.blank_decline
                              << "\n  empty-input      : " << score.empty_input
                              << "\n  blank+empty      : "
                              << (score.blank_decline + score.empty_input) << " (pinned "
                              << pins->blank_and_empty << ")"
                              << "\n  B  BOM decline   : " << score.bom_decline << " (pinned "
                              << pins->bom_declines << ")"
                              << "\n  C  unstamped     : " << score.unstamped << " (pinned "
                              << pins->unstamped << ")"
                              << "\n  decline VIOLATN  : " << score.violations << " (pinned 0)"
                              << "\n  partition total  : " << score.partition_total() << " / "
                              << score.lines;
                          for (const std::string& line : score.reported)
                              out << "\n  " << line;
                          if (score.reported.size() == kMaxReportedLines)
                              out << "\n  … reporting truncated at " << kMaxReportedLines
                                  << " lines";
                          return out.str();
                      }};

    // ── THE CLAIM ──
    EXPECT_EQ(score.claimed_mismatch, kExpectedMismatches)
        << "G1-PEEL FAILED: the declared transform and the shipped detector disagree on a line the "
           "detector CLAIMS. This is refactor-equivalence broken — the two implementations are not "
           "the same function."
        << report();

    EXPECT_EQ(score.violations, kExpectedViolations)
        << "DECLINE-SIDE INVARIANT FAILED: on a declined line the peel must either do nothing or "
           "blank it, never partially peel it into a different non-empty content."
        << report();

    // ── The partition must CLOSE. Without this the cell pins could all pass while lines vanished
    // into a bucket nobody counted — the classic way a corpus gate reports on a subset it does not
    // know it is reporting on. ──
    EXPECT_EQ(score.partition_total(), score.lines)
        << "the cells do NOT sum to the line total — the partition is not closed, so some lines are "
           "unaccounted for and every cell number below is suspect."
        << report();

    // ── Population + corroborated cells ──
    EXPECT_EQ(score.logs, pins->logs) << report();
    EXPECT_EQ(score.lines, pins->lines)
        << "the line total moved: the slice's bytes changed, or the line split did "
           "([[corpus-scrub-freeze-byte-fidelity]] — a count that differs from the study number is "
           "a FORMULA difference until proven a byte one)."
        << report();
    EXPECT_EQ(score.claimed_equal, pins->claimed_equal) << report();
    EXPECT_EQ(score.blank_decline + score.empty_input, pins->blank_and_empty) << report();
    EXPECT_EQ(score.unstamped, pins->unstamped) << report();

    // ── Cell B — the SHIPPED DEFECT, pinned so its closure is loud ──
    // When the BOM row (bugs.md 2026-07-27) is fixed, this number goes to 0 and those lines move
    // into cell A. That MUST fail here rather than be absorbed: the gate asserts equivalence with
    // the shipped peel including its warts, so a behavior change has to be a deliberate re-pin.
    EXPECT_EQ(score.bom_decline, pins->bom_declines)
        << "cell B moved. If the BOM defect was just fixed, this gate is CORRECTLY red: re-pin it "
           "deliberately, and do NOT land that fix between this gate's arms (bugs.md's sequencing "
           "MUST — a behavior change mid-capture makes the red uninterpretable)."
        << report();

    // ── The characterization pin, held to a different standard and labelled as such ──
    if (pins->empty_input_lines == kUnmeasured)
        GTEST_LOG_(INFO) << "CHARACTERIZATION unmeasured for " << pins->label
                         << ": empty-input = " << score.empty_input
                         << ", blank-decline = " << score.blank_decline
                         << ". Pin these in kSlices to guard the split.";
    else
        EXPECT_EQ(score.empty_input, pins->empty_input_lines)
            << "the blank/empty SPLIT moved. This is a CHARACTERIZATION pin (measured here first, "
               "not independently corroborated) — it guards regression, it does not confirm a "
               "prediction."
            << report();

    GTEST_LOG_(INFO) << "G1-PEEL green on " << pins->label << report();
}

} // namespace
// NOLINTEND
