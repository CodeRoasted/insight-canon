// NOLINTBEGIN — integration gate: literals and printed diagnostics are intended.
// test_transport_peel_equivalence_gate.cpp — G1's CORPUS arm + G1-PEEL, homed here.
//
// ⚠ THE ORACLE IS FROZEN, AND THIS GATE HAS CHANGED KIND. Read this before
// citing anything below.
//
//   * THE ORACLE IS A FROZEN COPY, taken VERBATIM from `semantic/github/src/github_strategy.cpp` at
//     insight-canon commit `ac94aff` — the last commit before T4 deleted that detection from
//     production. Its provenance lives HERE, in the gate, and not only in the `git log` of a
//     deleted file. It is FROZEN: an "improvement" to it is a DEFECT, not maintenance.
//   * ONLY THE DECISION FUNCTION `line ↦ (claimed?, content)` TRAVELLED. The level lift,
//     `parse_iso8601`, `confidence()`, `format()`, the echoed-source SGR machinery and the arena
//     did NOT — every function a frozen oracle carries must be reached by an assertion, so the
//     gate reads `has_value()` and `content` and nothing else; a frozen oracle carrying limbs no
//     assertion exercises is the dormant-code smell reproduced
//     inside a test. The SIGNATURE changed (`std::optional<std::string_view>` instead of
//     `std::expected<ParsedLine, std::string>`, which existed only to serve `IFormatStrategy`); the
//     BYTE LOGIC did not, and may not.
//   * THIS GATE CERTIFIED A MIGRATION; IT NOW PINS A CHARACTERIZATION. Before T4 it proved the
//     declared transform behavior-preserving against a SHIPPED detector. After T4
//     `TransportStack::peel` is the SOLE implementation of the GHA peel, and this frozen,
//     independently-authored oracle over 22 490 937 lines is what catches an unintended change to
//     a sole implementation — the one thing a self-consistent codebase cannot catch about itself.
//   * "WRITTEN YEARS BEFORE THE SUT" IS A FACT ABOUT THE ORIGINAL CERTIFICATION, NEVER AN ONGOING
//     INDEPENDENCE CLAIM. Two independently authored implementations agreed on 22 490 937 lines of
//     third-party logs; that event happened, and it is a fact of git history, not of this file's
//     location. Copying bytes cannot manufacture provenance. What relocation preserves is the
//     ability to RE-RUN the same comparison — nothing more, and no document may read it as more.
//
// HOMING (Kleio's). `core/tests/transport/`, 1:1 with
// `core/src/transport/` under the per-domain mirror, beside the sibling G1 grain
// `test_transport_declaration.cpp` (the third grain is `core/tests/compose/test_transport_identity.cpp`).
// The SUT is `insight::transport::TransportStack::peel` — core's; the oracle is inline; the corpus
// arrives by env var. The original home (`semantic/github/tests/`) rested on a premise T4 retired:
// the gate once needed BOTH implementations in scope at once, and the dependency arrow runs
// core → semantic/github and never back. With the oracle frozen inline this file imports only
// `insight.canon`, and leaving it in the package compiled the whole `insight.semantic.github`
// module into a binary that never referenced it while making a core-owned characterization pin
// look like a dialect-package obligation (the misreading the frozen-oracle discipline guards
// against).
//   • NOT insight-eidos, which already has the `CORPUS_D11_*` plumbing. Reusing that wiring would
//     home a canon-internal refactor-equivalence claim inside a downstream consumer — homing by
//     convenience past the package that owns the property.
//   • NOT a LogCraft scenario. No writer of ours produced these bytes, and that is the entire
//     value: the oracle is an implementation written years before the SUT, scored on third-party
//     logs neither was tuned against.
//
// WHAT IS BEING CLAIMED, AND WHAT IS NOT (it will be tempting to overstate, so it is restated at
// the top of the instrument that produces the number):
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
// `oracle_claim() == peel().content` over ALL lines reports thousands of "disagreements"
// that are all artifacts of scoring lines the oracle never claimed. The claim is scored on CELL A
// ONLY — the lines the oracle claims. The other cells are lines the oracle DECLINES, and each
// decline has a different cause that must be counted separately, not summed into a failure rate:
//   A            the oracle claims the line            ⇒ peeled content MUST be byte-identical
//   blank        timestamp-only; peels to empty        ⇒ bundled behavior (3) surviving the move
//   empty-input  the source line was already empty     ⇒ split out from `blank`; see below
//   B            declined solely for a leading BOM     ⇒ a REAL SHIPPED DEFECT, still open
//   C            unstamped; the peel is a no-op        ⇒ totality is about APPLICATION not EFFECT
//   violation    declined, yet peel changed the bytes  ⇒ the invariant that must stay 0
//
// WHY B IS NOT "FIXED" HERE. A G1-PEEL that corrected the BOM drop would break the very equivalence
// it asserts — the gate must reproduce the shipped peel INCLUDING its warts. B is its own cell so
// that the BOM population is NAMED and counted rather than dissolved into "unstamped".
//
// ⚠ AND THIS FILE'S FIRST ARM IS INSENSITIVE TO THE BOM FIX, BY CONSTRUCTION. It declares
// `kDeclaredGha` — the ONE shipped GHA row — and that declaration is FROZEN, exactly as the oracle
// is: the arm's subject is "the declared GHA peel vs. the shipped GHA detector", and adding a second
// row to its stack would change the subject. So when `utf8-bom-line-prefix` lands, cell B stays at
// 511 / 17 487 and this arm stays GREEN. (An earlier revision of this comment claimed the opposite —
// that the fix would turn cell B red here. It would not, and believing it would have left the
// un-drop measured by nothing.) The un-drop is measured by the SECOND arm below, which declares the
// two-row stack over the SAME manifest population.
//
// ── G-BOM-3, the second arm (DN-25.D5) — THE UN-DROP, COUNTED ──────────────────────────────────
// `BomRowUndropsExactlyTheBomDeclinedLinesAndNothingElse` scores the same population under three
// declarations. It is homed HERE, in this file, rather than in a sibling: the counterfactual it
// needs — "what would the shipped detector have claimed on the BOM-free twin?" — is `oracle_claim`
// itself, and copying a FROZEN oracle into a second file to answer it would create the second
// spelling the freezing discipline exists to prevent. It also inherits the manifest population
// reader, so both arms score a population that is the same pure function of the same manifest.
//
// FALSIFIABILITY — OBSERVED, not asserted (red-capability is a requirement, not a note). Three
// peel-path mutations were run against this gate; each was reverted:
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
// one buys a green that is measurably weaker than it reads — green-BLIND.
//
// Determinism: byte-only. Sorted population, fixed files, no RNG, no clock, no float, no threads.

#include <gtest/gtest.h>

import std;
import insight.canon; // insight::transport::* — the SUT. The ORACLE is frozen below.

using insight::transport::IngestDeclaration;
using insight::transport::RawPeeledLine;
using insight::transport::TransportStack;

namespace
{

// A `const char*`, not a string_view: it is handed to getenv, and string_view::data() carries no
// null-termination guarantee.
constexpr const char* kSliceDirVar{"CORPUS_D11_SLICE_DIR"};
constexpr std::string_view kGhaTransform{"api-rfc3339-line-prefix"};
constexpr std::array<std::string_view, 1> kDeclaredGha{{kGhaTransform}};
constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF"};

// ── G-BOM-3's declarations (DN-25) ────────────────────────────────────────────────────────────
// OUTSIDE-IN (ADR-23.D4): the bytes are `<BOM><stamp><content>`, so the BOM is the outer delivery
// layer and comes off first. The reversed spelling is kept as a FIRST-CLASS fixture, not as a
// comment — it is the counted red arm for DN-25.D4's ordering claim.
constexpr std::string_view kBomTransform{"utf8-bom-line-prefix"};
constexpr std::array<std::string_view, 2> kDeclaredBomThenGha{{kBomTransform, kGhaTransform}};
constexpr std::array<std::string_view, 2> kDeclaredGhaThenBom{{kGhaTransform, kBomTransform}};

// ══════════════════════════════════════════════════════════════════════════════════════════════
// THE FROZEN ORACLE — `GitHubActionsStrategy`'s peel decision, byte-for-byte
// as it stood in `semantic/github/src/github_strategy.cpp` at insight-canon `ac94aff`, the last
// commit before T4 removed that detection from production.
//
// ⚠ FROZEN. Do not tidy, do not modernize, do not "fix" the BOM wart (cell B below exists precisely
// so the wart is measured rather than absorbed). A change here is a defect: it silently redefines
// the very thing this gate certifies. The one thing that legitimately changed on relocation is the
// SIGNATURE — `std::optional<std::string_view>` in place of `std::expected<ParsedLine,
// std::string>`, because the latter existed only to satisfy `IFormatStrategy`, whose only
// implementor on this path is gone, and the gate never reads the error string, the timestamp, the
// level, the component or the raw line. Dropping the arena is safe by inspection and was checked:
// the production `parse` copied the content into an arena and returned a view OF THE COPY, so
// content EQUALITY is unaffected; this returns a view into the input line instead.
// ══════════════════════════════════════════════════════════════════════════════════════════════

[[nodiscard]] constexpr bool oracle_is_digit(char chr) noexcept
{
    return static_cast<unsigned>(chr) - '0' < 10U;
}
[[nodiscard]] constexpr bool oracle_is_space(char chr) noexcept
{
    return chr == ' ' || chr == '\t';
}

constexpr std::size_t kOracleGhaPrefixLen{28U}; // "YYYY-MM-DDTHH:MM:SS.fffffffZ"

// "YYYY-MM-DD" at offset `pos`.
[[nodiscard]] constexpr bool oracle_match_iso_date_at(std::string_view str, std::size_t pos) noexcept
{
    if (pos + 10U > str.size())
        return false;
    return oracle_is_digit(str[pos]) && oracle_is_digit(str[pos + 1]) &&
           oracle_is_digit(str[pos + 2]) && oracle_is_digit(str[pos + 3]) && str[pos + 4] == '-' &&
           oracle_is_digit(str[pos + 5]) && oracle_is_digit(str[pos + 6]) && str[pos + 7] == '-' &&
           oracle_is_digit(str[pos + 8]) && oracle_is_digit(str[pos + 9]);
}

// "HH:MM:SS" at offset `pos`.
[[nodiscard]] constexpr bool oracle_match_time_at(std::string_view str, std::size_t pos) noexcept
{
    if (pos + 8U > str.size())
        return false;
    return oracle_is_digit(str[pos]) && oracle_is_digit(str[pos + 1]) && str[pos + 2] == ':' &&
           oracle_is_digit(str[pos + 3]) && oracle_is_digit(str[pos + 4]) && str[pos + 5] == ':' &&
           oracle_is_digit(str[pos + 6]) && oracle_is_digit(str[pos + 7]);
}

// "YYYY-MM-DDTHH:MM:SS" — RFC 3339 prefix (T separator).
[[nodiscard]] constexpr bool oracle_is_rfc3339_prefix(std::string_view str) noexcept
{
    return oracle_match_iso_date_at(str, 0) && str.size() > 10U && str[10] == 'T' &&
           oracle_match_time_at(str, 11U);
}

// "YYYY-MM-DDTHH:MM:SS.fffffffZ" — GHA / Azure Pipelines line prefix (exactly 7 fractional
// digits + 'Z'). A strict subset of RFC 3339 so the strategy outranked Syslog only on genuine
// GHA lines.
[[nodiscard]] constexpr bool oracle_is_github_actions_prefix(std::string_view str) noexcept
{
    constexpr std::size_t kDotAt{19U};
    constexpr std::size_t kFracAt{20U};
    constexpr std::size_t kFracLen{7U};
    constexpr std::size_t kZAt{27U};
    if (str.size() < kOracleGhaPrefixLen)
        return false;
    if (!oracle_is_rfc3339_prefix(str))
        return false;
    if (str[kDotAt] != '.')
        return false;
    for (std::size_t pos{kFracAt}; pos < kFracAt + kFracLen; ++pos)
        if (!oracle_is_digit(str[pos]))
            return false;
    if (str[kZAt] != 'Z')
        return false;
    return str.size() == kOracleGhaPrefixLen || oracle_is_space(str[kOracleGhaPrefixLen]);
}

// The decision function the gate scores against: does the shipped detector CLAIM this line, and
// with what content? `nullopt` is a decline — the gate partitions declines by cause afterwards and
// never reads a cause string, which is why the `std::expected` error half did not travel.
[[nodiscard]] std::optional<std::string_view> oracle_claim(std::string_view line)
{
    if (!oracle_is_github_actions_prefix(line))
        return std::nullopt; // "GitHubActionsStrategy: missing GHA timestamp prefix"

    // Everything past the fixed-width timestamp; drop the separator space + any GHA indentation.
    std::string_view content{line.substr(kOracleGhaPrefixLen)};
    while (!content.empty() && oracle_is_space(content.front()))
        content.remove_prefix(1U);

    // A timestamp-only line is a blank line: decline it (make_event drops it, never an empty "").
    if (content.empty())
        return std::nullopt; // "GitHubActionsStrategy: blank GHA line (timestamp only)"

    return content;
}

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
    // G-BOM-3 (DN-25). CORROBORATED — counted by an independent second-language replica of this
    // gate's population reader, oracle and peel, which reproduces EVERY pin above exactly.
    //
    // ⚠ IT COINCIDES WITH `bom_declines` ON BOTH SLICES, AND IT IS A DIFFERENT MEASURAND. This
    // counts lines whose first byte is a BOM; `bom_declines` counts the subset the shipped detector
    // would have claimed but for that BOM. They are equal here only because every BOM-at-head line
    // in both slices is also stamped (BOM-with-no-stamp: 0 / 0). A manifest where they diverged
    // would make `moved_by_bom_row == bom_at_head` while `rescued == bom_declines`, which is why
    // the arm pins both rather than one number twice.
    std::size_t bom_at_head;
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
     .empty_input_lines = 1'297,
     .bom_at_head = 511},
    {.label = "data/v1/full",
     .logs = 4'082,
     .lines = 22'490'937,
     .claimed_equal = 21'878'259,
     .blank_and_empty = 523'126,
     .bom_declines = 17'487,
     .unstamped = 72'065,
     .empty_input_lines = 31'822,
     .bom_at_head = 17'487},
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
//
// It stays a SECOND, separately-spelled derivation now that the frozen oracle lives in the same
// file — this pre-existing duplication is the precedent for freezing the
// oracle here at all. Collapsing it into `oracle_is_github_actions_prefix` would answer cell B's
// counterfactual with the very implementation the counterfactual is about.
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
                         << " unset — the private D11 corpus slice is not present. Point it at a "
                            "slice directory holding corpus.jsonl AND log_annotated/, i.e. "
                            ".../github_corpora/revert_corpus/data/v1/sample (fast, 33 MB) or "
                            ".../data/v1/full (the full claim, 2.3 GB).";

        slice_ = std::filesystem::path{raw};
        // Set-but-broken is a WIRING FAILURE, not an absent corpus: the operator declared the slice
        // present, so skipping here is how a mis-wired gate reports green forever.
        ASSERT_TRUE(std::filesystem::is_regular_file(slice_ / "corpus.jsonl"))
            << kSliceDirVar << " is set to '" << raw << "' but there is no corpus.jsonl under it. "
            << "The slice is declared present, so this is a wiring error, not an absent corpus — "
               "unset the variable if this runner has no local copy of the private slice.";
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
            // skip — verify the asset against what the manifest ATTESTS. Silently skipping would
            // shrink the population behind a green.
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
        const TransportStack stack{insight::transport::resolve_transport_stack(
            IngestDeclaration{.stack = kDeclaredGha, .dialect = {}, .channel = {}})};

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

                const std::optional<std::string_view> parsed{oracle_claim(line)};
                const RawPeeledLine peeled{stack.peel_raw(line)};

                if (parsed.has_value())
                {
                    // ── CELL A — the claim ──
                    if (*parsed == peeled.content)
                    {
                        ++score.claimed_equal;
                        continue;
                    }
                    ++score.claimed_mismatch;
                    if (score.reported.size() < kMaxReportedLines)
                        score.reported.push_back(
                            std::string{"CELL-A MISMATCH "} + path.filename().string() + ":" +
                            std::to_string(line_no) + "\n    raw      : \"" + escape(line) +
                            "\"\n    oracle   : \"" + escape(*parsed) + "\"\n    peel     : \"" +
                            escape(peeled.content) + "\"");
                    continue;
                }

                // ── The oracle DECLINED. Partition by CAUSE, most specific first. ──
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

    // Identify the slice by its population SIZE. Shared by both arms so the "an unrecognized
    // population FAILS" rule has ONE spelling — two spellings of a selection rule is how one arm
    // quietly grows a skip the other does not have. Returns nullptr for an unpinned population;
    // every caller must treat that as a failure, never as a skip.
    [[nodiscard]] static const SlicePins* pins_for(std::size_t logs) noexcept
    {
        for (const SlicePins& candidate : kSlices)
            if (candidate.logs == logs)
                return &candidate;
        return nullptr;
    }

    [[nodiscard]] static std::string unpinned_population_message(std::size_t logs)
    {
        std::ostringstream out;
        out << "the manifest yielded " << logs << " logs, which matches no pinned slice ("
            << kSlices[0].label << " = " << kSlices[0].logs << ", " << kSlices[1].label << " = "
            << kSlices[1].logs
            << ").\nThis gate pins per-slice numbers, so an unrecognized population is a FAILURE, "
               "not a skip — either point " << kSliceDirVar
            << " at a pinned slice, or add the new slice's pins deliberately.";
        return out.str();
    }

    std::filesystem::path slice_;
};

TEST_F(TransportPeelEquivalenceGate, DeclaredPeelIsByteIdenticalToTheShippedDetectorOnCellA)
{
    const std::vector<std::filesystem::path> paths{population()};
    ASSERT_FALSE(paths.empty()) << "the manifest yielded an EMPTY population — a gate scoring zero "
                                   "lines reports green while measuring nothing";

    // An UNRECOGNIZED population must FAIL, never skip: the pins below do not apply to it, and a
    // silent skip is how a re-sliced corpus turns this gate into decoration.
    const SlicePins* pins{pins_for(paths.size())};
    ASSERT_NE(pins, nullptr) << unpinned_population_message(paths.size());

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
           "(a count that differs from the study number is a FORMULA difference — how the reader "
           "splits lines — until proven a byte one)."
        << report();
    EXPECT_EQ(score.claimed_equal, pins->claimed_equal) << report();
    EXPECT_EQ(score.blank_decline + score.empty_input, pins->blank_and_empty) << report();
    EXPECT_EQ(score.unstamped, pins->unstamped) << report();

    // ── Cell B — the SHIPPED DEFECT, NAMED and counted ──
    // This arm's declaration is FROZEN at the one shipped GHA row, so landing `utf8-bom-line-prefix`
    // does NOT move this number: the subject here is the shipped detector's own behavior, warts
    // included, and a stack that peeled the BOM would be measuring a different function. Cell B is
    // therefore the pre-fix population, held steady, and the sibling arm below is what watches it
    // get rescued. A move HERE means the corpus bytes changed or the stamp acceptor did — never
    // that the BOM row landed.
    EXPECT_EQ(score.bom_decline, pins->bom_declines)
        << "cell B moved. This arm cannot see the BOM row (it declares one transform, frozen), so "
           "this is a corpus or acceptor change, not the DN-25 fix landing."
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

// ══════════════════════════════════════════════════════════════════════════════════════════════
// G-BOM-3 (DN-25.D5) — THE UN-DROP, COUNTED. The VALUE arm of the BOM transport row.
// ══════════════════════════════════════════════════════════════════════════════════════════════
//
// ⚠ RED UNTIL THE ROW LANDS, AND THAT IS THE HANDOFF, NOT A REGRESSION — see the ASSERT below. It
// is an ASSERT rather than a skip because a gate that skips its absent subject is green for the one
// reason that matters: it never looked. It is an ASSERT rather than a resolve because
// `resolve_transport_stack` on an unknown name calls `std::terminate()`, which would take the whole
// test binary down and destroy every sibling suite's verdict.
//
// THE MEASURAND, stated before the numbers. Under the declared two-row stack, a line the shipped
// detector declined SOLELY for a leading BOM must peel to EXACTLY what that detector would have
// claimed on the line's BOM-free twin. `oracle_claim` — the frozen pre-T4 detector, already in this
// file — computes the twin's claim. SUT and ORACLE are named and they differ: the SUT is
// `TransportStack::peel_raw` under a two-row declaration; the oracle is the pre-T4 detector run on
// bytes the SUT never sees in that form.
//
// PRE-REGISTERED, IN BOTH DIRECTIONS (DN-25.D5, verbatim): an inequality either way is a FINDING,
// never a tolerance. A SMALLER rise means the transform under-applies and the diagnosis was
// incomplete; a LARGER one means it is stripping bytes that were not a BOM. Both directions are
// pinned by `moved_by_bom_row`, which counts EVERY line in the slice whose content the added row
// moved — so an over-strip cannot hide inside a cell that only looks at BOM lines.
//
// THE RED ARM IS COUNTED, NOT ASSUMED (DN-25.D4). The same population is scored a third time under
// the REVERSED declaration, where the stamp acceptor meets EF BB BF at offset 0 and declines. Two
// numbers pin it from both sides: `reversed_rescued` must be 0, and `reversed_kept_stamp` must be
// the FULL BOM population — i.e. the reversed order does not merely differ, it reproduces the
// present shipped defect on every one of those lines. This is the transport stack's first genuine
// composition; before this row, ADR-23.D3's ordering semantics had no consumer and were untested by
// construction.
//
// COST: two peels on every line plus a third on the BOM lines only. `sample` stays sub-second;
// `full` adds roughly one peel pass to the corpus job. Deliberate — the over-strip guard requires
// visiting the lines that must NOT move.

struct BomUndropScore
{
    std::size_t lines{0};
    std::size_t bom_at_head{0};         ///< the line's first bytes are EF BB BF
    std::size_t bom_declined{0};        ///< ... and the shipped detector would have claimed the twin
    std::size_t rescued{0};             ///< ... and the two-row peel == the twin's claim, byte-exact
    std::size_t rescue_mismatch{0};     ///< ... and it does not. THE CLAIM: 0.
    std::size_t rescue_blank{0};        ///< ... and the peel emptied the line (it would still drop)
    std::size_t still_bom_prefixed{0};  ///< ... and the BOM survived the peel. 17 487 → 0.
    std::size_t moved_by_bom_row{0};    ///< over ALL lines: two-row content != one-row content
    std::size_t moved_without_bom{0};   ///< ... on a line carrying no leading BOM. OVER-STRIP: 0.
    std::size_t reversed_rescued{0};    ///< the reversed stack rescued a line. THE RED ARM: 0.
    std::size_t reversed_kept_stamp{0}; ///< the reversed stack left a GHA stamp at line head
    std::vector<std::string> reported;
};

TEST_F(TransportPeelEquivalenceGate, BomRowUndropsExactlyTheBomDeclinedLinesAndNothingElse)
{
    ASSERT_NE(insight::transport::find_transform(kBomTransform), nullptr)
        << "the catalogue does not declare \"" << kBomTransform
        << "\". This arm is PRE-REGISTERED (DN-25.D5) and is RED BY DESIGN until the row, its "
           "algorithm and its identity bump land in ONE commit (ADR-2.D7). A handoff, not a "
           "regression — and an ASSERT rather than a skip, because a gate that skips its absent "
           "subject is green for the one reason that matters: it never looked.";

    const std::vector<std::filesystem::path> paths{population()};
    ASSERT_FALSE(paths.empty()) << "the manifest yielded an EMPTY population — a gate scoring zero "
                                   "lines reports green while measuring nothing";
    const SlicePins* pins{pins_for(paths.size())};
    ASSERT_NE(pins, nullptr) << unpinned_population_message(paths.size());

    const TransportStack one_row{insight::transport::resolve_transport_stack(
        IngestDeclaration{.stack = kDeclaredGha, .dialect = {}, .channel = {}})};
    const TransportStack two_row{insight::transport::resolve_transport_stack(
        IngestDeclaration{.stack = kDeclaredBomThenGha, .dialect = {}, .channel = {}})};
    const TransportStack reversed{insight::transport::resolve_transport_stack(
        IngestDeclaration{.stack = kDeclaredGhaThenBom, .dialect = {}, .channel = {}})};
    ASSERT_EQ(two_row.size(), 2U) << "the declared stack must resolve BOTH rows — a one-row stack "
                                     "would satisfy the counts below for the wrong reason";
    ASSERT_EQ(reversed.size(), 2U) << "the reversed stack must resolve BOTH rows, or the red arm is "
                                      "red for a reason that has nothing to do with ORDER";

    BomUndropScore score;
    for (const std::filesystem::path& path : paths)
    {
        std::ifstream input{path, std::ios::binary};
        std::ostringstream buffer;
        buffer << input.rdbuf();
        const std::string bytes{buffer.str()};

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

            const std::string_view before{one_row.peel_raw(line).content};
            const std::string_view after{two_row.peel_raw(line).content};

            // ── The over-strip guard. The ONLY lines the added row may move are BOM lines. ──
            if (after != before)
            {
                ++score.moved_by_bom_row;
                if (!line.starts_with(kUtf8Bom))
                {
                    ++score.moved_without_bom;
                    if (score.reported.size() < kMaxReportedLines)
                        score.reported.push_back(
                            std::string{"OVER-STRIP "} + path.filename().string() + ":" +
                            std::to_string(line_no) + "\n    raw   : \"" + escape(line) +
                            "\"\n    1-row : \"" + escape(before) + "\"\n    2-row : \"" +
                            escape(after) + "\"");
                }
            }

            if (!line.starts_with(kUtf8Bom))
                continue;
            ++score.bom_at_head;

            // The BOM-FREE TWIN and what the frozen detector claims on it. This is the whole
            // counterfactual: the detector cannot answer "would you have claimed this but for the
            // BOM?" about itself, so the twin is constructed here and handed to it.
            const std::string_view twin{line.substr(kUtf8Bom.size())};
            if (!is_gha_stamp(twin))
                continue; // a BOM in front of something the detector never claimed anyway
            ++score.bom_declined;

            if (after.starts_with(kUtf8Bom))
            {
                ++score.still_bom_prefixed;
                if (score.reported.size() < kMaxReportedLines)
                    score.reported.push_back(std::string{"BOM SURVIVED "} +
                                             path.filename().string() + ":" +
                                             std::to_string(line_no) + "\n    2-row : \"" +
                                             escape(after) + "\"");
            }

            const std::optional<std::string_view> claim{oracle_claim(twin)};
            if (!claim.has_value())
                ++score.rescue_blank; // the twin is stamp-only: peels to empty, still drops
            else if (after == *claim)
                ++score.rescued;
            else
            {
                ++score.rescue_mismatch;
                if (score.reported.size() < kMaxReportedLines)
                    score.reported.push_back(
                        std::string{"RESCUE MISMATCH "} + path.filename().string() + ":" +
                        std::to_string(line_no) + "\n    raw        : \"" + escape(line) +
                        "\"\n    twin claim : \"" + escape(*claim) + "\"\n    2-row peel : \"" +
                        escape(after) + "\"");
            }

            // ── The RED ARM, on the real bytes. ──
            const std::string_view wrong_order{reversed.peel_raw(line).content};
            if (claim.has_value() && wrong_order == *claim)
                ++score.reversed_rescued;
            if (is_gha_stamp(wrong_order))
                ++score.reversed_kept_stamp;
        }
    }

    const auto report{[&score, pins]
                      {
                          std::ostringstream out;
                          out << "\n  slice              : " << pins->label
                              << "\n  lines              : " << score.lines << " (pinned "
                              << pins->lines << ")"
                              << "\n  BOM at head        : " << score.bom_at_head << " (pinned "
                              << pins->bom_at_head << ")"
                              << "\n  BOM-declined       : " << score.bom_declined << " (pinned "
                              << pins->bom_declines << ")"
                              << "\n  RESCUED            : " << score.rescued << " (pinned "
                              << pins->bom_declines << ")   <<< THE UN-DROP"
                              << "\n  rescue MISMATCH    : " << score.rescue_mismatch
                              << " (pinned 0)"
                              << "\n  rescue blank       : " << score.rescue_blank << " (pinned 0)"
                              << "\n  BOM survived peel  : " << score.still_bom_prefixed
                              << " (pinned 0)"
                              << "\n  moved by BOM row   : " << score.moved_by_bom_row
                              << " (pinned " << pins->bom_at_head << ")"
                              << "\n  moved WITHOUT BOM  : " << score.moved_without_bom
                              << " (pinned 0)   <<< OVER-STRIP"
                              << "\n  reversed RESCUED   : " << score.reversed_rescued
                              << " (pinned 0)   <<< THE RED ARM"
                              << "\n  reversed kept stamp: " << score.reversed_kept_stamp
                              << " (pinned " << pins->bom_declines << ")";
                          for (const std::string& line : score.reported)
                              out << "\n  " << line;
                          if (score.reported.size() == kMaxReportedLines)
                              out << "\n  … reporting truncated at " << kMaxReportedLines
                                  << " lines";
                          return out.str();
                      }};

    // ── The population must be the SAME one the first arm scored. Without this the counts below
    // could all pass over a slice that quietly shrank. ──
    EXPECT_EQ(score.lines, pins->lines)
        << "the line total moved — this arm and the first are no longer scoring the same bytes."
        << report();
    EXPECT_EQ(score.bom_at_head, pins->bom_at_head)
        << "the BOM population itself moved: the corpus bytes changed." << report();
    EXPECT_EQ(score.bom_declined, pins->bom_declines)
        << "the pre-fix decline population disagrees with the first arm's cell B. The two arms "
           "partition the same lines by the same predicate; a divergence means one of the two "
           "spellings drifted."
        << report();

    // ── THE UN-DROP: exactly N, pre-registered, an inequality either way is a finding ──
    EXPECT_EQ(score.rescued, pins->bom_declines)
        << "G-BOM-3 FAILED. The declared two-row stack did not un-drop exactly the BOM-declined "
           "population.\n  FEWER means the transform UNDER-APPLIES — something else also declines "
           "those lines and the diagnosis was incomplete.\n  MORE is impossible here (the cell is "
           "a subset), so a larger figure means the predicate above changed meaning."
        << report();
    EXPECT_EQ(score.rescue_mismatch, 0U)
        << "a BOM-declined line peeled to content the shipped detector would NOT have produced on "
           "its BOM-free twin. The row is not content-neutral (ADR-23.D6): it is removing the BOM "
           "AND something else, or the stack composed the two rows wrongly."
        << report();
    EXPECT_EQ(score.rescue_blank, 0U)
        << "a BOM-declined line peeled to EMPTY, so it still drops — the un-drop is smaller than "
           "the count claims. Every such line is a twin that is stamp-only; if the corpus really "
           "gained one, this pin is what makes that visible instead of silently shrinking the win."
        << report();
    EXPECT_EQ(score.still_bom_prefixed, 0U)
        << "the BOM survived the declared peel on a line whose stream declares the row. 17 487 → 0 "
           "is the claim; a non-zero figure here is the transform not firing at all."
        << report();

    // ── THE OVER-STRIP GUARD — the other direction of the pre-registration ──
    EXPECT_EQ(score.moved_without_bom, 0U)
        << "the added row moved a line that carries NO leading BOM. Declaring is SUBTRACTIVE: a "
           "stream that gains this row must lose nothing it had. This is the 'stripping something "
           "that was not a BOM' direction, and it is the arm an inspection of the BOM cells alone "
           "structurally cannot catch."
        << report();
    EXPECT_EQ(score.moved_by_bom_row, pins->bom_at_head)
        << "the number of lines whose content the added row moved is not the number of lines that "
           "carry a leading BOM. Under-applying gives a smaller figure; over-stripping a larger "
           "one. Both are findings, never tolerances."
        << report();

    // ── THE RED ARM — DN-25.D4's ordering claim, observed on the corpus rather than assumed ──
    EXPECT_EQ(score.reversed_rescued, 0U)
        << "THE ORDERING CLAIM IS VACUOUS. The REVERSED declaration [\"" << kGhaTransform
        << "\", \"" << kBomTransform
        << "\"] rescued lines it must not: reversed, the stamp acceptor meets EF BB BF at offset 0 "
           "and declines, so nothing may be rescued. A non-zero figure means the peel is not "
           "applying its rows in declaration order — it is sorting, retrying or looping them — and "
           "ADR-23.D4's outside-in semantics are not implemented. Stack ORDER would then still be "
           "untested by construction, exactly as it has been since the catalogue shipped without a "
           "consumer."
        << report();
    EXPECT_EQ(score.reversed_kept_stamp, pins->bom_declines)
        << "the reversed declaration must leave a GHA stamp at the head of EVERY one of these "
           "lines — that is the present shipped defect, reproduced through the declaration. This "
           "pins the red arm POSITIVELY: 'the reversed order differs' is satisfied by a reversed "
           "order that is wrong in some other way, and would prove nothing about ORDER."
        << report();

    GTEST_LOG_(INFO) << "G-BOM-3 green on " << pins->label << report();
}

} // namespace
// NOLINTEND
