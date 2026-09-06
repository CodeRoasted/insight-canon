
// invariant: the corpus arm plus the peel-equivalence gate, homed here beside the sibling shape
// grain.
// invariant: THE ORACLE IS A FROZEN COPY taken verbatim from the strategy at the last commit before
// that detection was deleted from production, and its provenance lives HERE.
// invariant: it is FROZEN — an improvement to it is a DEFECT rather than maintenance.
// invariant: only the DECISION FUNCTION travelled; the level lift, the parse, the confidence, the
// format and the arena did not.
// invariant: a frozen oracle carrying limbs no assertion exercises is the dormant-code smell
// reproduced inside a test.
// invariant: the SIGNATURE changed because the old one existed only to serve the strategy
// interface; the BYTE LOGIC did not, and may not.
// invariant: THIS GATE CERTIFIED A MIGRATION AND NOW PINS A CHARACTERIZATION, because the peel is
// now the SOLE implementation of that decision.
// invariant: a frozen, independently-authored oracle is what catches an unintended change to a sole
// implementation.
// invariant: that is the one thing a self-consistent codebase cannot catch about itself.
// invariant: written-years-before-the-subject is a fact about the ORIGINAL certification and never
// an ongoing independence claim — copying bytes cannot manufacture provenance.
// invariant: what relocation preserves is the ability to RE-RUN the comparison, nothing more.
// invariant: THE ORACLE AND THE SUBJECT ARE NO LONGER THE SAME DECISION FUNCTION, and the green
// must not be read as identity.
// invariant: the oracle keeps a trusted-width grammar where the subject now requires a COMPLETE
// datetime of exactly the declared width — the trusted bytes were a measured defect.
// invariant: the two grammars can disagree only on a line whose complete datetime is not that
// width, and this corpus contains NONE — censused before the change.
// invariant: so the zero is an AGREEMENT ON THIS POPULATION, which is what a characterization pin
// measures; it is no longer refactor-equivalence and never again certifies the grammars coincide.
// invariant: the oracle is NOT updated to match — a frozen oracle that tracks its subject stops
// being able to catch it.
// refs: ADR-23.D4, DN-25.D4, DN-25.D5
// invariant: the SUBJECT is core's peel and the oracle is inline, so this file imports only the
// facade.
// invariant: the original home rested on a premise that retired when the two implementations
// stopped needing to be in scope at once.
// invariant: leaving it in the package compiled a whole dialect module into a binary that never
// referenced it, while making a core-owned characterization pin look like a package obligation.
// invariant: NOT homed downstream where the corpus plumbing already exists — reusing that wiring
// would home a canon-internal claim inside a consumer, homing by convenience past the owner.
// invariant: NOT a generated scenario: no writer of ours produced these bytes, and that is the
// entire value.
// invariant: peel equivalence is REFACTOR-EQUIVALENCE and never external validity — zero
// mismatches proves the transform behaviour-preserving against the shipped detector.
// invariant: it proves NOTHING about the transport model being right about the world, and no
// sentence anywhere may cite it as if it did.
// invariant: THE POPULATION IS A PURE FUNCTION OF THE COMMITTED MANIFEST, and that is load-bearing
// rather than tidy.
// invariant: the first run of this measurement scored a machine-local ignored bank walked in
// UNSPECIFIED order and capped mid-iteration — unnameable by construction and unreproducible.
// invariant: those numbers are WITHDRAWN, not reconciled; this gate never walks a directory, it
// reads the manifest, sorts, and does not cap.
// invariant: THE EQUIVALENCE DEFINITION IS THE WHOLE DESIGN and the naive one is WRONG — scoring
// every line reports thousands of disagreements that are artifacts of lines the oracle declined.
// invariant: the claim is scored on the CLAIMED cell only, and each decline cause is counted
// separately rather than summed into a failure rate.
// invariant: the mark-declined cell is a REAL SHIPPED DEFECT, still open, and it is NOT fixed here
// — a gate that corrected it would break the very equivalence it asserts.
// refs: ADR-23.D3
// invariant: it is its own cell so the population is NAMED and counted rather than dissolved into
// unstamped.
// invariant: THIS FILE'S FIRST ARM IS INSENSITIVE TO THE MARK FIX, by construction — it declares
// the ONE shipped row and that declaration is FROZEN exactly as the oracle is.
// invariant: the arm's subject is the declared peel against the shipped detector, and adding a
// second row to its stack would change the subject.
// invariant: an earlier revision of this comment claimed the opposite, and believing it would have
// left the un-drop measured by nothing.
// invariant: the un-drop is measured by the SECOND arm, which declares the two-row stack over the
// SAME population.
// invariant: that arm is homed HERE rather than in a sibling because the counterfactual it needs IS
// the frozen oracle, and copying it would create the second spelling freezing prevents.
// refs: DN-25.D5
// invariant: falsifiability was OBSERVED and not asserted — three peel-path mutations were run
// and each reverted.
// invariant: one of them shows WHICH CELL catches what: ignoring the separator strip moved the
// timestamp-only lines out of the blank cell and into violations.
// invariant: the claimed cell structurally could not have seen that, so the invariant cell is not
// decoration.
// invariant: MEASURED BLINDNESS — a real divergence class left the small slice fully GREEN while
// the full slice caught tens of thousands of lines.
// invariant: so the small slice is a SMOKE arm and the full one is the claim; wiring only the fast
// one buys a green that is measurably weaker than it reads.
// invariant: byte-only determinism — sorted population, fixed files, no randomness, no clock, no
// float, no threads.
#include <gtest/gtest.h>

import std;
import insight.canon;

using insight::transport::IngestDeclaration;
using insight::transport::RawPeeledLine;
using insight::transport::TransportStack;

namespace
{

// invariant: a null-terminated pointer rather than a borrowed view, because it is handed to the
// environment lookup and a view carries no termination guarantee.
constexpr const char* kSliceDirVar{"CORPUS_D11_SLICE_DIR"};
constexpr std::string_view kGhaTransform{"api-rfc3339-line-prefix"};
constexpr std::array<std::string_view, 1> kDeclaredGha{{kGhaTransform}};
constexpr std::string_view kUtf8Bom{"\xEF\xBB\xBF"};

// invariant: OUTSIDE-IN — the mark is the outer delivery layer and comes off first.
// invariant: the reversed spelling is kept as a FIRST-CLASS fixture and not as a comment, because
// it is the counted red arm for the ordering claim.
// refs: ADR-23.D4, DN-25.D4
constexpr std::string_view kBomTransform{"utf8-bom-line-prefix"};
constexpr std::array<std::string_view, 2> kDeclaredBomThenGha{{kBomTransform, kGhaTransform}};
constexpr std::array<std::string_view, 2> kDeclaredGhaThenBom{{kGhaTransform, kBomTransform}};

// invariant: THE FROZEN ORACLE — do not tidy, do not modernize, and do not fix the mark wart,
// which has its own cell precisely so it is measured rather than absorbed.
// invariant: a change here is a defect, because it silently redefines the very thing this gate
// certifies.
// invariant: the one legitimate change on relocation was the SIGNATURE, since the old one existed
// only to satisfy an interface whose only implementor on this path is gone.
// invariant: dropping the arena is safe BY INSPECTION and was checked — the production parse
// copied the content and returned a view OF THE COPY, so content equality is unaffected.
[[nodiscard]] constexpr bool oracle_is_digit(char chr) noexcept
{
    return static_cast<unsigned>(chr) - '0' < 10U;
}
[[nodiscard]] constexpr bool oracle_is_space(char chr) noexcept
{
    return chr == ' ' || chr == '\t';
}

// invariant: the oracle's prefix width is the dialect's own fixed stamp length.
constexpr std::size_t kOracleGhaPrefixLen{28U};

[[nodiscard]] constexpr bool oracle_match_iso_date_at(std::string_view str,
                                                      std::size_t pos) noexcept
{
    if (pos + 10U > str.size())
        return false;
    return oracle_is_digit(str[pos]) && oracle_is_digit(str[pos + 1]) &&
           oracle_is_digit(str[pos + 2]) && oracle_is_digit(str[pos + 3]) && str[pos + 4] == '-' &&
           oracle_is_digit(str[pos + 5]) && oracle_is_digit(str[pos + 6]) && str[pos + 7] == '-' &&
           oracle_is_digit(str[pos + 8]) && oracle_is_digit(str[pos + 9]);
}

[[nodiscard]] constexpr bool oracle_match_time_at(std::string_view str, std::size_t pos) noexcept
{
    if (pos + 8U > str.size())
        return false;
    return oracle_is_digit(str[pos]) && oracle_is_digit(str[pos + 1]) && str[pos + 2] == ':' &&
           oracle_is_digit(str[pos + 3]) && oracle_is_digit(str[pos + 4]) && str[pos + 5] == ':' &&
           oracle_is_digit(str[pos + 6]) && oracle_is_digit(str[pos + 7]);
}

[[nodiscard]] constexpr bool oracle_is_rfc3339_prefix(std::string_view str) noexcept
{
    return oracle_match_iso_date_at(str, 0) && str.size() > 10U && str[10] == 'T' &&
           oracle_match_time_at(str, 11U);
}

// invariant: the dialect prefix is a STRICT SUBSET of the general grammar, which is what let the
// strategy outrank its neighbour only on genuine lines of that dialect.
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

// post: the decision function the gate scores against — whether the shipped detector CLAIMS the
// line, and with what content.
// invariant: a decline carries no cause string — the gate partitions declines by cause afterwards
// and never reads one, which is why the old error half did not travel.
[[nodiscard]] std::optional<std::string_view> oracle_claim(std::string_view line)
{
    if (!oracle_is_github_actions_prefix(line))
        return std::nullopt;

    // invariant: everything past the fixed-width stamp, with the separator and any indentation
    // dropped.
    std::string_view content{line.substr(kOracleGhaPrefixLen)};
    while (!content.empty() && oracle_is_space(content.front()))
        content.remove_prefix(1U);

    // invariant: a timestamp-only line is a BLANK line and is declined, so it is dropped rather
    // than emitted as an empty template.
    if (content.empty())
        return std::nullopt;

    return content;
}

// invariant: the report truncates the per-line detail but never the totals — enough to see the
// SHAPE of a failure without burying it.
constexpr std::size_t kMaxReportedLines{10};

// invariant: two kinds of number live here and they are NOT equally strong, so each is LABELLED —
// a reader who treats a characterization pin as a corroborated one will over-trust it.
// invariant: CORROBORATED means measured independently by two implementations written without
// reference to each other, which is the cross-check that matters.
// invariant: CHARACTERIZATION means measured HERE first and pinned so it cannot move silently —
// it guards regression and does not confirm a prediction.
struct SlicePins
{
    std::string_view label;
    std::size_t logs;
    std::size_t lines;
    std::size_t claimed_equal;
    std::size_t blank_and_empty;
    std::size_t bom_declines;
    std::size_t unstamped;
    std::size_t empty_input_lines;
    // invariant: this count is CORROBORATED by an independent second-language replica of the
    // population reader, oracle and peel, which reproduces every pin above exactly.
    // invariant: it COINCIDES with the mark-decline count on both slices and is a DIFFERENT
    // measurand.
    // invariant: one counts lines whose first byte is a mark, the other the subset the detector
    // would have claimed but for it — equal only because every marked line here is also stamped.
    // invariant: a population where they diverged would separate them, which is why the arm pins
    // BOTH rather than one number twice.
    std::size_t bom_at_head;
};

// invariant: the mismatch and violation counts are pinned at ZERO for every slice — they are the
// CLAIM and not a slice property, so they are not per-slice fields.
constexpr std::size_t kExpectedMismatches{0};
constexpr std::size_t kExpectedViolations{0};

// invariant: the unmeasured pin is filled in by the first green run and deliberately NOT guessed
// — a guessed characterization pin would be a fitted number wearing a contract's clothes.
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

// invariant: a TARGETED extraction rather than a general parser, fail-closed on anything it does
// not understand.
// invariant: a silently mis-parsed manifest would build a different population and report a
// confident wrong number, which is the failure mode this whole file exists to avoid.
// invariant: the corpus filenames carry no escapes, so an escape sequence means the manifest's
// shape changed and the extraction must be revisited rather than guessed at.
struct ManifestField
{
    bool found{false};
    bool null_value{false};
    std::string value;
    std::string error;
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

struct Score
{
    std::size_t logs{0};
    std::size_t lines{0};
    std::size_t claimed_equal{0};
    std::size_t claimed_mismatch{0};
    std::size_t blank_decline{0};
    std::size_t empty_input{0};
    std::size_t bom_decline{0};
    std::size_t unstamped{0};
    std::size_t violations{0};
    std::vector<std::string> reported;

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
        const char* const raw{std::getenv(kSliceDirVar)};
        if (raw == nullptr || *raw == '\0')
            FAIL() << kSliceDirVar
                   << " unset — the private D11 corpus slice is not present. Point it at a "
                      "slice directory holding corpus.jsonl AND log_annotated/, i.e. "
                      ".../github_corpora/revert_corpus/data/v1/sample (fast, 33 MB) or "
                      ".../data/v1/full (the full claim, 2.3 GB).";

        slice_ = std::filesystem::path{raw};
        ASSERT_TRUE(std::filesystem::is_regular_file(slice_ / "corpus.jsonl"))
            << kSliceDirVar << " is set to '" << raw << "' but there is no corpus.jsonl under it. "
            << "The slice is declared present, so this is a wiring error, not an absent corpus — "
               "unset the variable if this runner has no local copy of the private slice.";
        ASSERT_TRUE(std::filesystem::is_directory(slice_ / "log_annotated"))
            << kSliceDirVar << " is set to '" << raw
            << "' but there is no log_annotated/ under it.";
    }

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
               "not a skip — either point "
            << kSliceDirVar << " at a pinned slice, or add the new slice's pins deliberately.";
        return out.str();
    }

    std::filesystem::path slice_;
};

TEST_F(TransportPeelEquivalenceGate, DeclaredPeelIsByteIdenticalToTheShippedDetectorOnCellA)
{
    const std::vector<std::filesystem::path> paths{population()};
    ASSERT_FALSE(paths.empty()) << "the manifest yielded an EMPTY population — a gate scoring zero "
                                   "lines reports green while measuring nothing";

    const SlicePins* pins{pins_for(paths.size())};
    ASSERT_NE(pins, nullptr) << unpinned_population_message(paths.size());

    const Score score{score_slice(paths)};

    const auto report{
        [&score, pins]
        {
            std::ostringstream out;
            out << "\n  slice            : " << pins->label
                << "\n  logs             : " << score.logs << " (pinned " << pins->logs << ")"
                << "\n  lines            : " << score.lines << " (pinned " << pins->lines << ")"
                << "\n  A  equal         : " << score.claimed_equal << " (pinned "
                << pins->claimed_equal << ")"
                << "\n  A  MISMATCH      : " << score.claimed_mismatch
                << " (pinned 0)   <<< THE CLAIM"
                << "\n  blank-decline    : " << score.blank_decline
                << "\n  empty-input      : " << score.empty_input
                << "\n  blank+empty      : " << (score.blank_decline + score.empty_input)
                << " (pinned " << pins->blank_and_empty << ")"
                << "\n  B  BOM decline   : " << score.bom_decline << " (pinned "
                << pins->bom_declines << ")"
                << "\n  C  unstamped     : " << score.unstamped << " (pinned " << pins->unstamped
                << ")"
                << "\n  decline VIOLATN  : " << score.violations << " (pinned 0)"
                << "\n  partition total  : " << score.partition_total() << " / " << score.lines;
            for (const std::string& line : score.reported)
                out << "\n  " << line;
            if (score.reported.size() == kMaxReportedLines)
                out << "\n  … reporting truncated at " << kMaxReportedLines << " lines";
            return out.str();
        }};

    EXPECT_EQ(score.claimed_mismatch, kExpectedMismatches)
        << "G1-PEEL FAILED: the declared transform and the shipped detector disagree on a line the "
           "detector CLAIMS. This is refactor-equivalence broken — the two implementations are not "
           "the same function."
        << report();

    EXPECT_EQ(score.violations, kExpectedViolations)
        << "DECLINE-SIDE INVARIANT FAILED: on a declined line the peel must either do nothing or "
           "blank it, never partially peel it into a different non-empty content."
        << report();

    EXPECT_EQ(score.partition_total(), score.lines)
        << "the cells do NOT sum to the line total — the partition is not closed, so some lines "
           "are "
           "unaccounted for and every cell number below is suspect."
        << report();

    EXPECT_EQ(score.logs, pins->logs) << report();
    EXPECT_EQ(score.lines, pins->lines)
        << "the line total moved: the slice's bytes changed, or the line split did "
           "(a count that differs from the study number is a FORMULA difference — how the reader "
           "splits lines — until proven a byte one)."
        << report();
    EXPECT_EQ(score.claimed_equal, pins->claimed_equal) << report();
    EXPECT_EQ(score.blank_decline + score.empty_input, pins->blank_and_empty) << report();
    EXPECT_EQ(score.unstamped, pins->unstamped) << report();

    EXPECT_EQ(score.bom_decline, pins->bom_declines)
        << "cell B moved. This arm cannot see the BOM row (it declares one transform, frozen), so "
           "this is a corpus or acceptor change, not the DN-25 fix landing."
        << report();

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

struct BomUndropScore
{
    std::size_t lines{0};
    std::size_t bom_at_head{0};
    std::size_t bom_declined{0};
    std::size_t rescued{0};
    std::size_t rescue_mismatch{0};
    std::size_t rescue_blank{0};
    std::size_t still_bom_prefixed{0};
    std::size_t moved_by_bom_row{0};
    std::size_t moved_without_bom{0};
    std::size_t reversed_rescued{0};
    std::size_t reversed_kept_stamp{0};
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
    ASSERT_EQ(reversed.size(), 2U)
        << "the reversed stack must resolve BOTH rows, or the red arm is "
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

            const std::string_view twin{line.substr(kUtf8Bom.size())};
            if (!is_gha_stamp(twin))
                continue;
            ++score.bom_declined;

            if (after.starts_with(kUtf8Bom))
            {
                ++score.still_bom_prefixed;
                if (score.reported.size() < kMaxReportedLines)
                    score.reported.push_back(
                        std::string{"BOM SURVIVED "} + path.filename().string() + ":" +
                        std::to_string(line_no) + "\n    2-row : \"" + escape(after) + "\"");
            }

            const std::optional<std::string_view> claim{oracle_claim(twin)};
            if (!claim.has_value())
                ++score.rescue_blank;
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

            const std::string_view wrong_order{reversed.peel_raw(line).content};
            if (claim.has_value() && wrong_order == *claim)
                ++score.reversed_rescued;
            if (is_gha_stamp(wrong_order))
                ++score.reversed_kept_stamp;
        }
    }

    const auto report{
        [&score, pins]
        {
            std::ostringstream out;
            out << "\n  slice              : " << pins->label
                << "\n  lines              : " << score.lines << " (pinned " << pins->lines << ")"
                << "\n  BOM at head        : " << score.bom_at_head << " (pinned "
                << pins->bom_at_head << ")"
                << "\n  BOM-declined       : " << score.bom_declined << " (pinned "
                << pins->bom_declines << ")"
                << "\n  RESCUED            : " << score.rescued << " (pinned " << pins->bom_declines
                << ")   <<< THE UN-DROP"
                << "\n  rescue MISMATCH    : " << score.rescue_mismatch << " (pinned 0)"
                << "\n  rescue blank       : " << score.rescue_blank << " (pinned 0)"
                << "\n  BOM survived peel  : " << score.still_bom_prefixed << " (pinned 0)"
                << "\n  moved by BOM row   : " << score.moved_by_bom_row << " (pinned "
                << pins->bom_at_head << ")"
                << "\n  moved WITHOUT BOM  : " << score.moved_without_bom
                << " (pinned 0)   <<< OVER-STRIP"
                << "\n  reversed RESCUED   : " << score.reversed_rescued
                << " (pinned 0)   <<< THE RED ARM"
                << "\n  reversed kept stamp: " << score.reversed_kept_stamp << " (pinned "
                << pins->bom_declines << ")";
            for (const std::string& line : score.reported)
                out << "\n  " << line;
            if (score.reported.size() == kMaxReportedLines)
                out << "\n  … reporting truncated at " << kMaxReportedLines << " lines";
            return out.str();
        }};

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
