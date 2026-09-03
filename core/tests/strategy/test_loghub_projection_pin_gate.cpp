// test_loghub_projection_pin_gate.cpp — the instrument `DN-43.D14` (4) NAMES BUT DID NOT HAVE.
//
// ═══ WHY THIS FILE EXISTS ══════════════════════════════════════════════════════════════════════
// `DN-43.D14` (4) — the ruling that made `BGLStrategy` claim the alert-labelled shape — closes on
// a sentence about a number: the empty-projection count on the two pinned LogHub corpora is
// *"pinned by the conformance kit per corpus rather than read off a warning stream"*. Measured
// 2026-09-02 while landing that slice: no such pin existed. The conformance kit
// (`insight.canon.conformance`) is the package-agnostic MANIFEST harness — it reads no corpus and
// has no per-corpus expectation to carry; `scripts/run_corpus_gates.sh`'s registry had no loghub
// record and no `CORPUS_LOGHUB_*` variable; and the harness that produced the figure was a scratch
// tool that no longer exists. **A ruling that names an instrument which does not exist is worse
// than one that names none: it reads as covered.** This gate is that instrument, and the ruling's
// clause is repointed at it.
//
// ═══ WHAT IT PINS, per corpus and per axis ════════════════════════════════════════════════════
// FOUR numbers per corpus — two that carry the ruling's figures, and two that keep the first
// pair's stated REASON true, which is a separate obligation and was the one DN-43.D14 (3) failed:
//
//   * `RawText` lines — the DECLINE count. Before the slice, BGL's 348 460 labelled lines routed
//     to `RawText` with `level` Unknown; after it, the decline is only what fails the grammar's
//     own validation. The pin is what makes a re-widened or re-narrowed predicate visible as a
//     number instead of as a silent recall change.
//   * lines carrying a CONTROL BYTE, and the intersection of that population with the declines,
//     pinned at ZERO. DN-43.D14 (3) explained the decline as "18 binary-garbage lines" and was
//     wrong twice — the count (10, not 18) and the description, which is what hid the count. The
//     two populations are disjoint: see the RawText pin's own comment for the decomposition.
//   * empty projections — lines that carried bytes and projected to empty `content`, read off the
//     SHIPPED counter (`Tokenizer::empty_projections()`, the `ADR-16.D9` projection-totality
//     instrument) and never off the rate-limited warning stream, which is exactly what the ruling
//     asked for and exactly what no reader could do before that accessor existed.
//
// ═══ THE NUMBER IS A SUM, AND THE GATE SAYS SO ════════════════════════════════════════════════
// `DN-43.D14` (4): the empty-projection population has TWO members — (a) a genuinely empty body,
// which is the CORRECT identity of a content-less line, and (b) a projection bug that moved the
// message onto a cube dimension. The counter is their sum, so this gate is a CHANGE DETECTOR on a
// declared figure, never a claim that every counted line is legitimate. The verdict that they are
// all member (a) on BGL after the slice is the ruling's, carried by the per-strategy expectation
// in `DN-43.D14` (4) and re-derivable only by reading lines — which is why the gate additionally
// prints the component histogram of the counted population on failure rather than only the total.
//
// ═══ POPULATION ═══════════════════════════════════════════════════════════════════════════════
// The two sha256-pinned files `insight-canon/core/data/corpora/loghub/README.md` names, mounted
// out of tree under `CORPUS_LOGHUB_DIR` (the private warehouse's
// `zenodo_corpora/loghub/data/loghub-full/`; CC-BY-4.0, re-acquirable from Zenodo `8196385`, zero
// bytes in git). Every `\n`-split line of each, in file order, through the PUBLIC pipeline —
// `Tokenizer::process_line` under a zero-package composition (`SRC-SP-1`: a core test may not link
// the vocabulary packages), which is the same door the shipping ingest uses.
//
// ═══ THE CORPUS IDENTITY ANCHOR, and why it is not a sha256 ═══════════════════════════════════
// The README pins each file by sha256 AND by byte size AND by line count, and `download_logs.sh`
// verifies the digest inline at acquisition. This gate anchors on the SIZE and the LINE COUNT,
// both asserted before any projection number is read, for a reason that is about shape rather
// than rigour: the files are 743 MB and 868 MB, so a digest here means either holding 1.6 GB in
// memory (which is what every other corpus gate in the tree does, at a corpus three orders of
// magnitude smaller) or a second hand-rolled streaming SHA-256 beside the one in
// `test_bracket_peel_equivalence_gate.cpp`. Size and line count are exact, free — the walk counts
// lines anyway — and they fail LOUDLY and FIRST on the failure they exist to catch, which is a
// truncated or re-processed download (the README's own recorded dead-end: the Zenodo-18522101
// "LogTrie" copy was truncated AND label-stripped). And the projection counts are themselves an
// identity witness: 34 470 empty projections out of 4 747 963 lines does not arrive from a
// different file.
//
// ═══ FALSIFIABILITY ═══════════════════════════════════════════════════════════════════════════
// Every pin here is a count over the shipped pipeline, so any change to `BGLStrategy`'s predicate,
// its parse, or the projection-totality condition moves at least one cell. The
// cheapest mutation that reds it: restore `is_bgl_labelled_prefix`'s `str[0] != '-'` first test —
// BGL's `RawText` count returns to 348 460 against a pinned 10.
//
// ═══ DETERMINISM ══════════════════════════════════════════════════════════════════════════════
// Byte-only, single-threaded, committed file order, integer counts. No RNG, no clock, no float, no
// threads. The arena is reset per line, as the shipping ingest resets it.
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
// How many distinct components of the empty-projection population are printed on a failure — the
// discriminator between DN-43.D14 (4)'s member (a) and member (b), reported rather than asserted.
constexpr std::size_t kTopComponentsShown{8};

// One pinned corpus: the README's file identity, and the two projection figures this gate holds.
struct CorpusPin
{
    std::string_view file;
    // From core/data/corpora/loghub/README.md — the acquisition-verified size and line count.
    std::uintmax_t bytes;
    std::uint64_t lines;
    // DN-43.D14 (4), re-derived at this desk after the alert-label slice landed
    // (insight-canon fb404fb, kCanonicalizationVersion stateless-masks-14).
    std::uint64_t raw_text_lines;
    std::uint64_t empty_projections;
    // The BINARY population, and it is here to keep the RawText pin's REASON honest rather than
    // only its value. DN-43.D14 (3) explained the decline as binary garbage; it is not, and a
    // pin whose value is right and whose stated reason is wrong is the worst kind to inherit —
    // the next reader to touch the number reasons from the comment. These two cells make the
    // disjointness a measured fact: `nonprintable_lines` is the whole population carrying a
    // control byte, and `raw_text_and_nonprintable` is its intersection with the declines, pinned
    // at ZERO on both corpora.
    std::uint64_t nonprintable_lines;
    std::uint64_t raw_text_and_nonprintable;
    std::string_view why;
};

constexpr std::array<CorpusPin, 2> kPins{{
    {.file = "BGL.log",
     .bytes = 743185031U,
     .lines = 4747963U,
     // The decline is no longer the 348 460 labelled lines — the grammar validates them now. What
     // is left is what fails validation, and DN-43.D14 (3)'s pre-registered "18 binary-garbage
     // lines" is REFUSED rather than rescued: it was wrong TWICE, and the second error hid the
     // first. Wrong in the COUNT — 18 against a measured 10 — and wrong in the DESCRIPTION, which
     // called all 18 binary garbage. Re-derived at the sha256-pinned corpus (byte scan, and the
     // disjointness arm below):
     //   * 12 lines carry a non-printable byte, in TWO populations — 8 with a blob where <node2>
     //     sits, and 4 with a NUL inside the message body — and EVERY ONE OF THE 12 PARSES. Their
     //     headers are canonical; DN-43.D15 rules <node2> must stay unvalidated, so the blob is
     //     dropped with the field rather than published.
     //   * the 10 that DECLINE are a THIRD shape and contain no binary at all: printable spliced
     //     message fragments where <node2> should be, declining because the splice is multi-token,
     //     so the RAS/NULL alignment probe fails at BOTH positions (bgl.cpp, scan_ras_record).
     // So no line in this corpus is both binary and declining — asserted below, not assumed.
     .raw_text_lines = 10U,
     // DN-43.D14 (4) member (a): a complete header with no message — `… RAS KERNEL FATAL` and
     // nothing after it. 0.73 % of the corpus.
     .empty_projections = 34470U,
     // 12 = 8 with a blob in <node2> + 4 with a NUL in the message body. Measured by byte scan
     // over the pinned file, 2026-09-02; all 12 parse.
     .nonprintable_lines = 12U,
     .raw_text_and_nonprintable = 0U,
     .why = "BGL: the alert-labelled RAS shape, claimed by BGLStrategy since DN-43.D14"},
    {.file = "Thunderbird_5M.log",
     .bytes = 868147617U,
     .lines = 5000000U,
     // Every line routes to a strategy: the Thunderbird branch's header validation accepts the
     // whole corpus. A single RawText line here is a predicate regression.
     .raw_text_lines = 0U,
     // 6 489, NOT the 6 508 DN-43.D14 (4) states. The ruling's figure was measured BEFORE its own
     // clause (3) landed the one-token tag bound with the no-tag non-removal, which moved 19 lines
     // out of the empty-projection population by keeping their remainder as `content`. The
     // ruling's arithmetic (34 470 + 6 508 = 40 978) therefore reads 34 470 + 6 489 = 40 959 after
     // the slice it ordered. That is the movement this pin exists to have caught, and it is why a
     // number living only in a design note is not a pin.
     .empty_projections = 6489U,
     .nonprintable_lines = 0U,
     .raw_text_and_nonprintable = 0U,
     .why = "Thunderbird: syslog-shaped after the label, no level column"},
}};

// What one corpus's walk produced. Counts only — no bytes of a private corpus leave this gate.
struct Walked
{
    std::uint64_t lines{0};
    std::uint64_t nonempty{0};
    std::uint64_t declined{0}; // process_line produced no event at all
    std::uint64_t raw_text{0};
    std::uint64_t empty_projections{0};
    std::uint64_t nonprintable{0};              // the line carries a control byte
    std::uint64_t raw_text_and_nonprintable{0}; // ... AND the pipeline declined it to RawText
    std::map<std::string, std::uint64_t> empty_projection_components;
};

// A control byte: anything below 0x20 that is not a tab, or DEL. The line has already had its
// trailing newline removed, so no separator is counted. This is the instrument's OWN predicate and
// deliberately so — it is a statement about the BYTES, not about anything canon decides, and the
// two must not share an owner or the disjointness arm below could only ever agree with itself.
[[nodiscard]] constexpr bool is_control_byte(char chr) noexcept
{
    const auto byte{static_cast<unsigned char>(chr)};
    return (byte < 0x20U && byte != 0x09U) || byte == 0x7FU;
}

[[nodiscard]] std::string top_components(const std::map<std::string, std::uint64_t>& histogram)
{
    std::vector<std::pair<std::string, std::uint64_t>> rows{histogram.begin(), histogram.end()};
    // Descending by count, then by name — a total order, so the printed diagnostic is byte-stable
    // across runs and two failures can be diffed.
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
        // UNSET and SET-BUT-BROKEN must not share a verdict: the first is an absent corpus (a dev
        // box), the second is a wiring error. run_corpus_gates.sh turns the skip into a failure,
        // which is the only place an absent corpus is allowed to be loud.
        const char* const raw{std::getenv(kCorpusVar)};
        if (raw == nullptr || *raw == '\0')
            GTEST_SKIP() << kCorpusVar
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

    // The walk, through the public door. Verbose by construction: every number the assertions
    // read is returned, so a failure prints the whole partition and not only the cell that moved.
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
            // The empty-projection population's own shape, read at the event: `content` projected
            // away while the line had bytes. The counter below is canon's; this histogram is the
            // DISCRIMINATOR the counter cannot carry, and it is reported, never asserted — on a
            // grammar with a subsystem column an empty body prints a NON-EMPTY component, which is
            // precisely the reading DN-43.D14 (4) warns the next reader not to act on.
            // NOT byte-identical to canon's own condition, and the difference is stated rather
            // than hidden: canon counts `content.empty() && !raw_line.empty()`, this counts an
            // empty TEMPLATE, which additionally catches a content of pure whitespace (non-empty
            // to canon, zero tokens to the masker). So this histogram is an upper bound on the
            // counter, which is exactly why it is a diagnostic and the ASSERTION reads the
            // shipped counter instead.
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

        // ── The identity anchor, FIRST: every number below is a statement about THIS file. ─────
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

        // ── The gate's own two numbers ─────────────────────────────────────────────────────────
        EXPECT_EQ(walked.raw_text, pin.raw_text_lines)
            << pin.why << "\n  format=RawText lines: got " << walked.raw_text << ", pinned "
            << pin.raw_text_lines << " (DN-43.D14 (3): what fails the grammar's own validation is "
            << "DECLINED to RawText, and nothing else is).\n  partition: lines=" << walked.lines
            << " non-empty=" << walked.nonempty << " declined-by-the-pipeline=" << walked.declined
            << " raw-text=" << walked.raw_text;

        // ── THE PIN'S REASON, not only its value (DN-43.D14 (3) got the reason wrong) ──────
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

    // The figure DN-43.D14 (4) states as a single number across both corpora. Pinned here as the
    // SUM of the two per-corpus pins above rather than as a third independent constant: a total
    // that could disagree with its own addends is a fourth number to keep true.
    constexpr std::uint64_t kExpectedTotal{kPins[0].empty_projections + kPins[1].empty_projections};
    EXPECT_EQ(total_empty, kExpectedTotal)
        << "the cross-corpus empty-projection total DN-43.D14 (4) names: got " << total_empty
        << ", pinned " << kExpectedTotal
        << ". The ruling's own text says 34 470 + 6 508 = 40 978, measured before its clause (3) "
           "landed; after the slice it ordered the figure is 34 470 + 6 489 = 40 959.";
}

} // namespace
