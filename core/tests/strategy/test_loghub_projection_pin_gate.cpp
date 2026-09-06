// invariant: the LogHub projection pin gate — the instrument a ruling NAMED but did not have.
// invariant: the ruling that made the BGL strategy claim the alert-labelled shape closes on a
// sentence about a NUMBER.
// invariant: it says the empty-projection count is pinned per corpus rather than read off a warning
// stream.
// invariant: MEASURED 2026-09-02 while landing that slice, no such pin existed — the conformance
// kit is a package-agnostic MANIFEST harness that reads no corpus.
// invariant: the corpus-gate registry had no loghub record, and the harness that produced the
// figure was a scratch tool now gone.
// invariant: A RULING THAT NAMES AN INSTRUMENT WHICH DOES NOT EXIST IS WORSE THAN ONE THAT NAMES
// NONE, because it reads as covered; this gate is that instrument.
// invariant: it pins FOUR numbers per corpus — two carrying the ruling's figures, and two keeping
// the first pair's stated REASON true, which is a separate obligation the ruling failed.
// invariant: the RawText decline count is what makes a re-widened or re-narrowed predicate visible
// as a number instead of as a silent recall change.
// invariant: the control-byte population and its intersection with the declines are pinned because
// the ruling explained the decline as binary garbage and was wrong TWICE.
// invariant: it was wrong in the COUNT and in the DESCRIPTION, and the description is what hid the
// count.
// invariant: empty projections are read off the SHIPPED counter and never off the rate-limited
// warning stream.
// invariant: that is what the ruling asked for, and what no reader could do before that accessor
// existed.
// invariant: THE NUMBER IS A SUM AND THE GATE SAYS SO.
// invariant: the population has two members, a genuinely empty body, which is the CORRECT identity
// of a content-less line, and a projection bug that moved the message onto a cube dimension.
// invariant: so this is a CHANGE DETECTOR on a declared figure and never a claim that every counted
// line is legitimate, which is why it prints the component histogram on failure.
// invariant: POPULATION — the two sha256-pinned files mounted out of tree, every newline-split
// line in file order through the PUBLIC pipeline under a zero-package composition.
// invariant: that is the same door the shipping ingest uses.
// invariant: THE IDENTITY ANCHOR IS THE SIZE AND THE LINE COUNT, NOT A DIGEST, and the reason is
// shape rather than rigour: the files are 743 MB and 868 MB.
// invariant: a digest means holding 1.6 GB in memory or a second hand-rolled streaming
// implementation.
// invariant: size and line count are exact, free — the walk counts lines anyway — and they fail
// LOUDLY and FIRST on the failure they exist to catch, a truncated or re-processed download.
// invariant: the projection counts are themselves an identity witness: 34 470 empty projections out
// of 4 747 963 lines does not arrive from a different file.
// invariant: FALSIFIABILITY — every pin is a count over the shipped pipeline, so any change to
// the strategy's predicate, its parse or the projection-totality condition moves a cell.
// invariant: the cheapest mutation that reds it is restoring the prefix predicate's first test,
// which returns BGL's RawText count to 348 460 against a pinned 10.
// invariant: determinism — byte-only, single-threaded, committed file order, integer counts, no
// RNG, no clock, no float, no threads, and the arena reset per line as the shipping ingest does.
// refs: ADR-16.D9, DN-43.D14, DN-43.D15, SRC-SP-1
#include <gtest/gtest.h>

import std;
import insight.canon;

namespace
{

using insight::LogFormat;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

constexpr const char* kCorpusVar{"CORPUS_LOGHUB_DIR"};
constexpr std::size_t kArenaBytes{std::size_t{8} * 1024 * 1024};
// invariant: how many distinct components of the empty-projection population print on a failure —
// the discriminator between the ruling's two members, reported rather than asserted.
constexpr std::size_t kTopComponentsShown{8};

// invariant: one pinned corpus — the README's file identity and the two projection figures this
// gate holds.
struct CorpusPin
{
    std::string_view file;
    // invariant: the acquisition-verified size and line count, from the corpus README.
    std::uintmax_t bytes;
    std::uint64_t lines;
    // invariant: re-derived at this desk after the alert-label slice landed, against a stated
    // canonicalization generation.
    // refs: DN-43.D14
    std::uint64_t raw_text_lines;
    std::uint64_t empty_projections;
    // invariant: the BINARY population is here to keep the RawText pin's REASON honest and not only
    // its value.
    // invariant: a pin whose value is right and whose stated reason is wrong is the worst kind to
    // inherit, because the next reader to touch the number reasons from the comment.
    // invariant: these two cells make the disjointness a MEASURED fact — the whole control-byte
    // population, and its intersection with the declines, pinned at ZERO on both corpora.
    std::uint64_t nonprintable_lines;
    std::uint64_t raw_text_and_nonprintable;
    std::string_view why;
};

constexpr std::array<CorpusPin, 2> kPins{{
    {.file = "BGL.log",
     .bytes = 743185031U,
     .lines = 4747963U,
     // invariant: the decline is no longer the 348 460 labelled lines — the grammar validates
     // them now, and what is left is what fails validation.
     // invariant: the pre-registered `18 binary-garbage lines` is REFUSED rather than rescued.
     // invariant: it was wrong in the COUNT, 18 against a measured 10, and wrong in the
     // DESCRIPTION, which hid the first.
     // invariant: 12 lines carry a non-printable byte in TWO populations.
     // invariant: 8 with a blob where the repeated node sits and 4 with a NUL inside the message
     // body — and EVERY ONE OF THE 12 PARSES.
     // invariant: their headers are canonical, and the repeated node must stay unvalidated, so the
     // blob is dropped with the field rather than published.
     // invariant: the 10 that DECLINE are a THIRD shape containing no binary at all.
     // invariant: printable spliced message fragments where the repeated node should be, declining
     // because the splice is multi-token so the alignment probe fails at BOTH positions.
     // invariant: so no line in this corpus is both binary and declining, which is asserted below
     // and never assumed.
     // refs: DN-43.D14, DN-43.D15
     .raw_text_lines = 10U,
     // invariant: a complete header with no message, which is the ruling's legitimate member —
     // 0.73 % of the corpus.
     .empty_projections = 34470U,
     // invariant: 8 with a blob in the repeated node plus 4 with a NUL in the message body,
     // measured by byte scan over the pinned file on 2026-09-02; all 12 parse.
     .nonprintable_lines = 12U,
     .raw_text_and_nonprintable = 0U,
     .why = "BGL: the alert-labelled RAS shape, claimed by BGLStrategy since DN-43.D14"},
    {.file = "Thunderbird_5M.log",
     .bytes = 868147617U,
     .lines = 5000000U,
     // invariant: every line routes to a strategy — the Thunderbird branch's header validation
     // accepts the whole corpus, so a single RawText line here is a predicate regression.
     .raw_text_lines = 0U,
     // invariant: 6 489, NOT the 6 508 the ruling states — its figure was measured BEFORE its own
     // clause landed the one-token tag bound with the no-tag non-removal.
     // invariant: that clause moved 19 lines out of the population by keeping their remainder as
     // content.
     // invariant: so the ruling's own arithmetic reads 40 959 after the slice it ordered.
     // invariant: that movement is what this pin exists to have caught, and it is why a number
     // living only in a design note is not a pin.
     // refs: DN-43.D14
     .empty_projections = 6489U,
     .nonprintable_lines = 0U,
     .raw_text_and_nonprintable = 0U,
     .why = "Thunderbird: syslog-shaped after the label, no level column"},
}};

// invariant: counts only — no bytes of a private corpus leave this gate.
struct Walked
{
    std::uint64_t lines{0};
    std::uint64_t nonempty{0};
    std::uint64_t declined{0};
    std::uint64_t raw_text{0};
    std::uint64_t empty_projections{0};
    std::uint64_t nonprintable{0};
    std::uint64_t raw_text_and_nonprintable{0};
    std::map<std::string, std::uint64_t> empty_projection_components;
};

// invariant: a control byte is anything below 0x20 that is not a tab, or DEL; the trailing newline
// is already removed, so no separator is counted.
// invariant: this is the instrument's OWN predicate, deliberately — a statement about the BYTES
// and not about anything canon decides.
// invariant: the two must not share an owner, or the disjointness arm below could only ever agree
// with itself.
[[nodiscard]] constexpr bool is_control_byte(char chr) noexcept
{
    const auto byte{static_cast<unsigned char>(chr)};
    return (byte < 0x20U && byte != 0x09U) || byte == 0x7FU;
}

[[nodiscard]] std::string top_components(const std::map<std::string, std::uint64_t>& histogram)
{
    std::vector<std::pair<std::string, std::uint64_t>> rows{histogram.begin(), histogram.end()};
    // invariant: descending by count then by name is a TOTAL order, so the printed diagnostic is
    // byte-stable across runs and two failures can be diffed.
    std::ranges::sort(
        rows, [](const auto& lhs, const auto& rhs)
        { return lhs.second != rhs.second ? lhs.second > rhs.second : lhs.first < rhs.first; });
    std::string cells;
    for (const auto& [component, count] : rows | std::views::take(kTopComponentsShown))
        cells += std::format(" \"{}\"={}", component, count);
    if (rows.size() > kTopComponentsShown)
        cells += std::format(" (+{} more)", rows.size() - kTopComponentsShown);
    return cells.empty() ? std::string{" none"} : cells;
}

class LogHubProjectionPinGate : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // invariant: UNSET and SET-BUT-BROKEN must not share a verdict — the first is an absent
        // corpus on a dev box, the second is a wiring error.
        // invariant: the corpus-gate job turns the absence into a failure, which is the only place
        // an absent corpus is allowed to be loud.
        const char* const raw{std::getenv(kCorpusVar)};
        if (raw == nullptr || *raw == '\0')
            FAIL() << kCorpusVar
                   << " unset — the LogHub full corpus is not mounted. It lives in the "
                      "private warehouse (zenodo_corpora/loghub/data/loghub-full/) and is "
                      "re-acquirable from Zenodo 8196385 under CC-BY-4.0.";
        root_ = std::filesystem::path{raw};
        ASSERT_TRUE(std::filesystem::is_directory(root_))
            << kCorpusVar << "=" << root_.string()
            << " is set but is not a directory — a wiring error, not an absent corpus.";
        for (const CorpusPin& pin : kPins)
            ASSERT_TRUE(std::filesystem::is_regular_file(root_ / pin.file))
                << kCorpusVar << " is set but " << pin.file
                << " is missing under it — a wiring error, not an absent corpus.";
    }

    // invariant: the walk goes through the PUBLIC door and is verbose by construction.
    // invariant: every number the assertions read is returned, so a failure prints the whole
    // partition and not only the cell that moved.
    [[nodiscard]] static Walked walk(const std::filesystem::path& file)
    {
        Walked walked;
        const insight::semantic::ComposedSemantics composed{insight::semantic::compose({})};
        ArenaAllocator arena{kArenaBytes};
        Tokenizer tokenizer{arena, MaskConfig{}, composed};
        std::ifstream input{file, std::ios::binary};
        std::string line;
        while (std::getline(input, line))
        {
            ++walked.lines;
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;
            ++walked.nonempty;
            const bool nonprintable{std::ranges::any_of(line, is_control_byte)};
            if (nonprintable)
                ++walked.nonprintable;
            const auto event{tokenizer.process_line(line)};
            if (!event)
            {
                ++walked.declined;
                arena.reset();
                continue;
            }
            if (event->format == LogFormat::RawText)
            {
                ++walked.raw_text;
                if (nonprintable)
                    ++walked.raw_text_and_nonprintable;
            }
            // invariant: the empty-projection population's own shape, read at the event — content
            // projected away while the line had bytes.
            // invariant: the counter is canon's; this histogram is the DISCRIMINATOR the counter
            // cannot carry, and it is reported and never asserted.
            // invariant: on a grammar with a subsystem column an empty body prints a NON-EMPTY
            // component.
            // invariant: that is precisely the reading the ruling warns the next reader not to act
            // on.
            // invariant: NOT byte-identical to canon's own condition, and the difference is stated
            // rather than hidden.
            // invariant: canon counts an empty content on a non-empty line, this counts an empty
            // TEMPLATE, which additionally catches a content of pure whitespace.
            // invariant: so this histogram is an UPPER BOUND on the counter, which is exactly why
            // it is a diagnostic and the ASSERTION reads the shipped counter instead.
            // refs: DN-43.D14
            if (event->template_str.empty())
                ++walked.empty_projection_components[std::string{event->component}];
            arena.reset();
        }
        walked.empty_projections = tokenizer.empty_projections();
        return walked;
    }

    static std::filesystem::path root_;
};
std::filesystem::path LogHubProjectionPinGate::root_{};

TEST_F(LogHubProjectionPinGate, EveryPinnedCorpusHoldsItsDeclineAndEmptyProjectionCounts)
{
    std::uint64_t total_empty{0};
    for (const CorpusPin& pin : kPins)
    {
        const std::filesystem::path file{root_ / pin.file};

        // invariant: the identity anchor comes FIRST — every number below is a statement about
        // THIS file.
        std::error_code size_error;
        const std::uintmax_t bytes{std::filesystem::file_size(file, size_error)};
        ASSERT_FALSE(size_error) << pin.file << ": cannot stat — " << size_error.message();
        ASSERT_EQ(bytes, pin.bytes)
            << pin.file << " is not the pinned file: " << bytes << " bytes, expected " << pin.bytes
            << " (core/data/corpora/loghub/README.md). A truncated or re-processed download is the "
               "recorded failure mode for this corpus; every projection count below would be a "
               "number about the wrong bytes.";

        const Walked walked{walk(file)};

        ASSERT_EQ(walked.lines, pin.lines)
            << pin.file << ": " << walked.lines << " lines, expected " << pin.lines
            << " (core/data/corpora/loghub/README.md).";

        EXPECT_EQ(walked.raw_text, pin.raw_text_lines)
            << pin.why << "\n  format=RawText lines: got " << walked.raw_text << ", pinned "
            << pin.raw_text_lines << " (DN-43.D14 (3): what fails the grammar's own validation is "
            << "DECLINED to RawText, and nothing else is).\n  partition: lines=" << walked.lines
            << " non-empty=" << walked.nonempty << " declined-by-the-pipeline=" << walked.declined
            << " raw-text=" << walked.raw_text;

        // invariant: the pin's REASON and not only its value, because the ruling got the reason
        // wrong.
        // refs: DN-43.D14
        EXPECT_EQ(walked.nonprintable, pin.nonprintable_lines)
            << pin.why << "\n  lines carrying a control byte: got " << walked.nonprintable
            << ", pinned " << pin.nonprintable_lines
            << ". On BGL the 12 are 8 with a blob where <node2> sits plus 4 with a NUL in the "
               "message body; all 12 PARSE, because DN-43.D15 rules a consumed-but-unpublished "
               "field is validated only where its value proves the record's FIELD ALIGNMENT, and "
               "<node2> proves nothing.";

        EXPECT_EQ(walked.raw_text_and_nonprintable, pin.raw_text_and_nonprintable)
            << pin.why << "\n  lines that BOTH carry a control byte AND decline to RawText: got "
            << walked.raw_text_and_nonprintable << ", pinned " << pin.raw_text_and_nonprintable
            << ".\n  THIS ARM IS THE ONE THAT KEEPS THE RawText PIN'S REASON TRUE. The decline "
               "population and the binary population are DISJOINT: the declines are printable "
               "spliced message fragments where <node2> should be, and the binary lines have "
               "canonical headers. A non-zero here means either that a binary line started "
               "declining — which would move its blob out of a dropped field and into "
               "RawTextStrategy's whole-line content, i.e. into a PUBLISHED template name — or "
               "that the decline population changed shape. Both are rulings, never re-pins.";

        EXPECT_EQ(walked.empty_projections, pin.empty_projections)
            << pin.why
            << "\n  empty projections (Tokenizer::empty_projections(), the ADR-16.D9 "
               "projection-totality counter): got "
            << walked.empty_projections << ", pinned " << pin.empty_projections
            << ".\n  THIS IS A SUM, NOT A DEFECT COUNT (DN-43.D14 (4)): member (a) is a genuinely "
               "empty body and is the CORRECT identity for a content-less line; member (b) is a "
               "body moved onto a cube dimension. The component histogram below separates them ON "
               "A SYSLOG-SHAPED GRAMMAR ONLY — on BGL the component is a header field (KERNEL), so "
               "a legitimate empty body prints NON-EMPTY here and must not be read as the defect "
               "shape.\n  components of the counted population:"
            << top_components(walked.empty_projection_components)
            << "\n  partition: lines=" << walked.lines << " non-empty=" << walked.nonempty
            << " declined-by-the-pipeline=" << walked.declined;

        total_empty += walked.empty_projections;
    }

    // invariant: the figure the ruling states as a single number across both corpora, pinned as the
    // SUM of the two per-corpus pins rather than as a third independent constant.
    // invariant: a total that could disagree with its own addends is a fourth number to keep true.
    // refs: DN-43.D14
    constexpr std::uint64_t kExpectedTotal{kPins[0].empty_projections + kPins[1].empty_projections};
    EXPECT_EQ(total_empty, kExpectedTotal)
        << "the cross-corpus empty-projection total DN-43.D14 (4) names: got " << total_empty
        << ", pinned " << kExpectedTotal
        << ". The ruling's own text says 34 470 + 6 508 = 40 978, measured before its clause (3) "
           "landed; after the slice it ordered the figure is 34 470 + 6 489 = 40 959.";
}

} // namespace
