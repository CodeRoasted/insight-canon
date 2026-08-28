// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_masked_span_census.cpp — `G-D` LEG 1: how many real GitHub-Actions intent-marker payloads
// carry TWO OR MORE masked spans?
//
// WHY THAT NUMBER AND NOT ANOTHER. `DN-38.D3` rules the instance coordinate to become the ENVELOPE
// of the masked spans (first span start → last span end) instead of the FIRST span, and proves the
// blast radius as a theorem rather than a measurement: zero spans → the envelope is empty, exactly
// as today; one span → that span, exactly as today; two or more → strictly wider. **So behaviour
// moves for names carrying ≥2 masked spans and for no other name, at every consumer at once.** The
// count of such names is therefore an EXACT UPPER BOUND on the population any later leg can move,
// and without it a re-run's surprise is unattributable.
//
// ⚠ THIS IS A CENSUS, NOT A PROPERTY GATE — the same distinction `d38_unit_identity_gate_test`
// draws for `DN-38`'s gate 1, and for the same reason. Pinning a prevalence would encode today's
// corpus as intended behaviour. Every assertion below is an INSTRUMENT-VALIDITY one: that the
// subject loaded whole, that line classification is total, that the span decomposition is sound
// against an independent oracle, and that the instrument can report a non-zero at all.
//
// ── WHY IT HOMES HERE, AND NOT IN `insight-eidos` ─────────────────────────────────────────────
//
// The measured quantity is a function of canon's `discriminant_of` over a population of NAMES. No
// aligner, no report, no diff engine is involved, so homing it in a sift gate would put a package
// in every failure's suspect list that the measurement never touches. What the population DOES
// need is the GitHub-Actions marker vocabulary — which line is a marker and what its payload is —
// and that vocabulary is this package's `github.dialect.yaml` rows. Population defined here,
// function owned by canon core, both in canon: one repo, one build, no seam.
//
// ── WHY IT DRIVES `recognize()` AND DOES NOT GREP FOR THE BANNER ──────────────────────────────
//
// A hand-rolled `line.find("Complete job name: ")` is a SECOND RIG for a decision the shipped path
// already makes, and two rigs over one corpus produce two honest numbers that read as a
// contradiction. The measured form of that mistake is on the record (`bank_harness.hpp`: a
// `cat`-assembled arm produced 1 row where the crawler's assembler produced 3). So this census
// resolves the SAME declaration the `G1` replay declares — stack `api-rfc3339-line-prefix`,
// dialect `github`, channel `annotated` — normalizes each line, peels it with the declared
// `TransportStack::peel` (the recognition path's stage 2, not the tokenizer's `peel_raw`) and asks
// `recognize()` what it is. The payload censused is `IntentMarker::name`: the exact bytes
// `insight-eidos`'s aligner and `insight-twin`'s reverse projection receive.
//
// ── WHAT IT DOES NOT MEASURE, STATED SO THE NUMBER IS NOT OVER-READ ───────────────────────────
//
// ⚠ THE ACQUIRER'S DECLARED DISPLAYS ARE A SECOND SURFACE AND THEY ARE NOT IN THIS CENSUS.
// `insight-eidos`'s declaration fold runs `discriminant_of` on a COMPOSED declared name
// (`diff_engine.cpp` — `coordinate_of()` over `DiffConfig::DeclaredJob::display`, which for a
// reusable-workflow fan-out is `<caller> / <callee>`). That name comes from the run's jobs listing
// joined to the workflow file, and NEITHER is banked for this corpus: the longitudinal collector
// banks log bytes and run metadata only (`longitudinal_state/index.jsonl` carries no job listing).
// The surface is unreachable here, it is named rather than silently omitted, and its own
// population is an OPEN measurement.
//
// ⚠ AND ON THE ONE SUBJECT THAT MATTERS FOR THE NEXT LEG, THAT SURFACE IS EMPTY BY CONSTRUCTION:
// the `1 000`-situation replay passes no `--changed-job-graph`, `DiffConfig::changed_job_graph` is
// therefore `nullopt`, and the fold — hence `coordinate_of` — never runs. So the marker-payload
// count below is the WHOLE bound for a re-run of that subject, and the declared surface's own
// number is owed to production rather than to this leg.
//
// Determinism: no RNG, no threads, no wall clock, no float. The subject is a committed, sorted TSV
// of file names; the file set, the walk order and every printed set are sorted, so the output is a
// pure function of the banked bytes.
//
// CORPUS-GATED (`d4_realcorpus` precedent): the banked run logs are third-party derivatives that
// live outside every checkout, so this SKIPS cleanly when the mount is absent — green in CI and on
// every clone.
//
//   CODEROAST_G1_LOGS     directory holding the `*.annotated.log` members
//   CODEROAST_G1_SUBJECT  the committed subject TSV (side, run_id, repo, workflow, file)
//                         `coderoast-corpora/sift_assessment/1.10.2/gd_leg1_span_census/`
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::canonicalize_intent;
using insight::discriminant_of;
using insight::semantic::ComposedSemantics;
using insight::semantic::ResolvedStream;
using insight::tokenization::IntentMarker;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;
using insight::transport::IngestDeclaration;
using insight::transport::PeeledLine;

namespace
{

// ── THE MASKED-SPAN DECOMPOSITION, BUILT OUT OF THE SHIPPED FUNCTION ──────────────────────────
//
// `discriminant_of` returns the FIRST masked span as a view INTO the name, so its offset is
// recoverable and the scan can be resumed on the tail. Resuming is EXACT rather than approximate,
// and the reason is a property of the scan: a numeric claim ends only where `boundary_after` holds
// (the next byte is non-word or the string ends) and a paren claim ends on `)`, so the byte
// following any span is non-word in both cases — which is precisely the state a fresh scan starts
// in (`prev_is_word = false`, start-of-string is a boundary). No copy of R1–R4 is made here, and
// none may be: a second implementation of the scan is what makes a reproduction disagree with the
// thing it reproduces.
//
// The soundness of that argument is not left as prose — `TheSpanDecompositionReconstructsTheClass`
// below falsifies it against an INDEPENDENT oracle on every name the census sees.
struct Span
{
    std::size_t offset; // into the name handed in
    std::size_t size;
};

// canon's `is_intent_trim_byte`, restated so offsets are taken against a name whose ends both
// functions already ignore. It is a RESTATEMENT, never a fork: `TrimIsCanonInvariant` requires
// both canon functions to answer identically on the name and on its trimmed form, so a divergence
// in the trim set fails loudly instead of shifting every offset by one.
[[nodiscard]] std::string_view trimmed(std::string_view name)
{
    const auto is_trim{[](char byte) { return byte == ' ' || byte == '\t' || byte == '\r'; }};
    while (!name.empty() && is_trim(name.front()))
        name.remove_prefix(1);
    while (!name.empty() && is_trim(name.back()))
        name.remove_suffix(1);
    return name;
}

[[nodiscard]] std::vector<Span> masked_spans(std::string_view name)
{
    const std::string_view body{trimmed(name)};
    std::vector<Span> spans;
    std::size_t consumed{0};
    while (consumed < body.size())
    {
        const std::string_view tail{body.substr(consumed)};
        const std::string_view span{discriminant_of(tail)};
        if (span.empty())
            break;
        const std::size_t at{static_cast<std::size_t>(span.data() - tail.data())};
        spans.push_back(Span{.offset = consumed + at, .size = span.size()});
        consumed += at + span.size();
    }
    return spans;
}

// The independent oracle for the decomposition: rebuild the class from the pieces. Literal
// material is emitted verbatim (which is what `canonicalize_intent` does to it) and each span is
// replaced by `canonicalize_intent(span)` — the span alone, standing at a start-of-string
// boundary, re-fires exactly the rule that claimed it. If the two disagree, the decomposition is
// not a decomposition and every count below it is void. No mask literal (`vX`, `N`, `(M)`) is
// spelled here: they are canon's internals and a copy of them would be a third rig.
[[nodiscard]] std::string rebuilt_class(std::string_view name)
{
    const std::string_view body{trimmed(name)};
    std::string out;
    std::size_t cursor{0};
    for (const Span& span : masked_spans(name))
    {
        out.append(body.substr(cursor, span.offset - cursor));
        out.append(canonicalize_intent(body.substr(span.offset, span.size)));
        cursor = span.offset + span.size;
    }
    out.append(body.substr(cursor));
    return out;
}

// `DN-38.D3` clause 1: the subview from the START of the FIRST masked span to the END of the LAST.
// Derived from the shipped function's own answers, never from a copy of the ruled implementation —
// it is printed as a diagnostic beside each ≥2-span example so the implementer can read what the
// coordinate becomes, and it is asserted NOWHERE. Pinning it here would make this census the
// oracle for the fix that has not been written.
[[nodiscard]] std::string_view envelope(std::string_view name)
{
    const std::string_view body{trimmed(name)};
    const std::vector<Span> spans{masked_spans(name)};
    if (spans.empty())
        return {};
    const std::size_t from{spans.front().offset};
    const std::size_t to{spans.back().offset + spans.back().size};
    return body.substr(from, to - from);
}

// ── THE SUBJECT ────────────────────────────────────────────────────────────────────────────────

[[nodiscard]] std::optional<std::string> env(const char* key)
{
    if (const char* value = std::getenv(key); value != nullptr && *value != '\0')
        return std::string{value};
    return std::nullopt;
}

constexpr std::string_view kUnmounted{
    "CODEROAST_G1_LOGS / CODEROAST_G1_SUBJECT unset or missing — the banked GitHub run logs are "
    "third-party derivatives that stay outside every checkout, so this census runs only where the "
    "corpus is mounted."};

struct SubjectRow
{
    std::string pair; // the CHANGED run id — the key every other `G1` artefact is named by
    std::string side; // "baseline" | "changed" — the ROLE the run plays in its pair
    std::string run_id;
    std::string repo;
    std::string workflow;
    std::string file;
};

// The committed TSV, verbatim and in file order (it is written sorted, so the walk is fixed). A
// short row is FATAL rather than skipped: a subject that silently shrinks is the failure mode this
// whole file exists to avoid reporting.
[[nodiscard]] std::vector<SubjectRow> read_subject(const std::filesystem::path& path,
                                                   std::string& error)
{
    std::ifstream in{path};
    std::vector<SubjectRow> rows;
    if (!in)
    {
        error = "cannot open subject list " + path.string();
        return rows;
    }
    std::string line;
    std::size_t number{0};
    while (std::getline(in, line))
    {
        ++number;
        if (number == 1 && line.starts_with("pair\t"))
            continue; // the header
        if (line.empty())
            continue;
        std::vector<std::string> cells;
        for (std::size_t at{0}; at <= line.size();)
        {
            const std::size_t tab{line.find('\t', at)};
            const std::size_t end{tab == std::string::npos ? line.size() : tab};
            cells.push_back(line.substr(at, end - at));
            at = end + 1;
            if (tab == std::string::npos)
                break;
        }
        if (cells.size() != 6)
        {
            error = "subject line " + std::to_string(number) + " has " +
                    std::to_string(cells.size()) + " columns, expected 6: \"" + line + "\"";
            return {};
        }
        rows.push_back(SubjectRow{.pair = cells[0],
                                  .side = cells[1],
                                  .run_id = cells[2],
                                  .repo = cells[3],
                                  .workflow = cells[4],
                                  .file = cells[5]});
    }
    return rows;
}

// ── THE TALLY ──────────────────────────────────────────────────────────────────────────────────

// One (side × marker kind) cell. Names are counted BOTH ways on purpose: a DISTINCT-name count is
// the population a rule applies to, an OCCURRENCE count is how often a reader meets it, and a
// count reported without saying which one it is cannot be spent.
struct Tally
{
    std::map<std::string, std::size_t> occurrences_by_name; // payload → times recognized
    std::map<std::string, std::size_t> spans_by_name;       // payload → masked-span count
    std::size_t recognized{0};

    void add(const std::string& name, std::size_t spans)
    {
        ++occurrences_by_name[name];
        spans_by_name[name] = spans;
        ++recognized;
    }
};

struct Bucketed
{
    std::size_t names_zero{0};
    std::size_t names_one{0};
    std::size_t names_two_plus{0};
    std::size_t occ_zero{0};
    std::size_t occ_one{0};
    std::size_t occ_two_plus{0};
    // Among the ≥2-span names: how many distinct CLASSES they fall into, and how many of those
    // classes carry more than one distinct name. The second number is the collision opportunity —
    // a class with one member cannot mis-pair with itself, so it bounds nothing on its own.
    std::size_t classes_two_plus{0};
    std::size_t colliding_classes_two_plus{0};
    std::size_t max_spans{0};
};

[[nodiscard]] Bucketed bucket(const Tally& tally)
{
    Bucketed out;
    std::map<std::string, std::set<std::string>> classes;
    for (const auto& [name, spans] : tally.spans_by_name)
    {
        const std::size_t occ{tally.occurrences_by_name.at(name)};
        out.max_spans = std::max(out.max_spans, spans);
        if (spans == 0)
        {
            ++out.names_zero;
            out.occ_zero += occ;
        }
        else if (spans == 1)
        {
            ++out.names_one;
            out.occ_one += occ;
        }
        else
        {
            ++out.names_two_plus;
            out.occ_two_plus += occ;
            classes[canonicalize_intent(name)].insert(name);
        }
    }
    out.classes_two_plus = classes.size();
    for (const auto& [clazz, members] : classes)
        if (members.size() > 1)
            ++out.colliding_classes_two_plus;
    return out;
}

[[nodiscard]] std::string show(const Bucketed& b, const Tally& t)
{
    std::ostringstream out;
    out << "recognized=" << t.recognized << " distinct=" << t.spans_by_name.size()
        << " | spans 0: " << b.names_zero << " names / " << b.occ_zero << " occ"
        << " | 1: " << b.names_one << " / " << b.occ_one << " | >=2: " << b.names_two_plus << " / "
        << b.occ_two_plus << " | >=2 classes " << b.classes_two_plus << " (of which "
        << b.colliding_classes_two_plus << " carry >1 name)"
        << " | max spans " << b.max_spans;
    return out.str();
}

[[nodiscard]] std::string kind_name(IntentMarkerKind kind)
{
    switch (kind)
    {
    case IntentMarkerKind::Job:
        return "Job";
    case IntentMarkerKind::Step:
        return "Step";
    case IntentMarkerKind::None:
        return "None";
    }
    return "?";
}

} // namespace

// ── THE DECOMPOSITION'S OWN PROOF — runs with no mount, on every clone ─────────────────────────
//
// The census's whole arithmetic rests on `masked_spans`. This arm falsifies it against
// `canonicalize_intent`, which is a genuinely different function: the SUT walks the name by
// repeated `discriminant_of` on the tail, the ORACLE masks it in one pass. They can only agree if
// the decomposition found exactly the spans the mask claimed, in order, at the right offsets.
TEST(MaskedSpanCensus, TheSpanDecompositionReconstructsTheClass)
{
    // The names carry the shapes the census must get right, and the last four are adversarial:
    // a span at offset 0, two spans with class material between them, a name that is ALL span, and
    // the `DN-38.D3` clause-4 pair whose residual ambiguity lives in the CLASS.
    static constexpr std::array<std::pair<std::string_view, std::size_t>, 19> kNames{{
        {"Lint", 0},
        {"Build", 0},
        {"Test (ubuntu-latest, Node 24.x)", 1},
        {"Test (windows-latest, Node 24.x)", 1},
        {"ESLint v6", 1},
        {"ESLint v7", 1},
        {"Test (my-gpu-box)", 1},
        {"bench (arm64-metal, cuda-12)", 1},
        {"build (ubuntu-latest)", 1},
        {"test (ubuntu-latest)", 1},
        {"macos-14 (15.3)", 2},
        {"macos-15 (26.0.1)", 2},
        {"Test v2 on ubuntu-20", 2},
        // A COMPOSED declared name — the reusable-workflow rendering the fold's `coordinate_of`
        // would see. ONE span, not two: R4 is non-greedy to the first `)`, so `macos-14`'s digits
        // are INSIDE the paren group and are consumed by it. The composed surface is a different
        // domain from a marker payload, and this is one of the ways it differs.
        {"rust-ci / build (macos-14, 15.3)", 1},
        {"rust-ci / test v2 (macos-14)", 2},
        {"14 leading", 1},
        {"1.2.3", 1},
        {"N 42", 1},
        {"42 N", 1},
    }};

    for (const auto& [name, want_spans] : kNames)
    {
        EXPECT_EQ(masked_spans(name).size(), want_spans)
            << "masked_spans(\"" << name << "\") found " << masked_spans(name).size()
            << " span(s), expected " << want_spans;
        EXPECT_EQ(rebuilt_class(name), canonicalize_intent(name))
            << "THE DECOMPOSITION IS UNSOUND on \"" << name
            << "\": rebuilding the class from the spans it found gives \"" << rebuilt_class(name)
            << "\" but canonicalize_intent says \"" << canonicalize_intent(name)
            << "\". Every count in this file is derived from this decomposition and is void until "
               "they agree.";
    }
}

// ── The blast-radius theorem, asserted on the two boundary cases (`DN-38.D3`) ─────────────────
//
// Zero spans → today's answer is empty. One span → today's answer IS that span. This is what makes
// "behaviour moves for ≥2-span names and no other" a theorem rather than a hope, and it is the
// half a reader must be able to check without reading the design note.
//
// ⚠ IT IS ALSO THE PRE-FLIGHT FOR THE FIX, AND THE LIST BELOW IS LONGER THAN THE ONE THE DESIGN
// NOTE ENUMERATES. `DN-38.D3` names two files whose currently-green literals must survive the
// widening byte-identical. A workspace sweep for arms that pin a discriminant VALUE finds FOUR,
// and the two the note does not name are in other packages:
//   * `insight-canon/core/tests/identity/test_instance_discriminant.cpp` — named
//   * `insight-twin/core/tests/test_twin_gates.cpp:806-812` — named; the payloads come from
//     `kTwoJobSourceYaml`'s `build (ubuntu-latest)` / `test (ubuntu-latest)`
//   * `insight-canon/semantic/github/tests/test_github_markers.cpp:221` — NOT named:
//     `EXPECT_EQ(job.discriminant, "(ubuntu-latest)")` over the payload `Test (ubuntu-latest)`
//   * `insight-canon/semantic/jenkins/tests/test_jenkins_markers.cpp:71` — NOT named, and it is a
//     DIFFERENT DIALECT: `EXPECT_EQ(branch.discriminant, "(lts)")` over `Branch: maven (lts)`.
//     The identity functions are canon's and dialect-blind, which is exactly why a Jenkins arm can
//     pin their output.
// Every one of the four carries at most ONE masked span, so all four stay byte-identical — but
// that is a MEASURED result rather than an assumed one, and it is measured here.
//
// ⚠ THE WINDOW THIS PRE-FLIGHT IS VALID IN: it restates literals that live in three other files,
// so it goes stale silently if one of them changes its payload to a ≥2-span name. It is a
// pre-flight for the `DN-38.D3` widening, not a standing guard; after the widening lands, the
// authoritative check is those four suites going green.
TEST(MaskedSpanCensus, TodaysDiscriminantIsAlreadyTheEnvelopeBelowTwoSpans)
{
    static constexpr std::array<std::string_view, 10> kBelowTwo{
        {"Lint", "Build", "Test (ubuntu-latest, Node 24.x)", "ESLint v6", "Test (my-gpu-box)",
         "bench (arm64-metal, cuda-12)", "build (ubuntu-latest)", "test (ubuntu-latest)",
         "Test (ubuntu-latest)", "Branch: maven (lts)"}};
    for (const std::string_view name : kBelowTwo)
    {
        ASSERT_LE(masked_spans(name).size(), 1U) << "\"" << name << "\" carries more than one span";
        EXPECT_EQ(discriminant_of(name), envelope(name))
            << "\"" << name << "\": today's discriminant \"" << discriminant_of(name)
            << "\" already differs from the masked-span envelope \"" << envelope(name)
            << "\" below two spans — the blast radius is NOT confined to the >=2-span set";
    }
    // And the population the widening moves, on leg 0's own cells: two spans, and the envelope is
    // strictly wider than today's answer.
    for (const std::string_view name : {"macos-14 (15.3)", "macos-15 (26.0.1)"})
    {
        ASSERT_EQ(masked_spans(name).size(), 2U);
        EXPECT_NE(discriminant_of(name), envelope(name))
            << "\"" << name << "\" carries two spans and its envelope did not widen";
    }
}

// ── The trim restatement is CHECKED, never trusted ─────────────────────────────────────────────
TEST(MaskedSpanCensus, TrimIsCanonInvariant)
{
    static constexpr std::array<std::string_view, 4> kPadded{
        {"  macos-14 (15.3)", "macos-14 (15.3)\r", "\tESLint v6 ", "  Lint\t\r"}};
    for (const std::string_view padded : kPadded)
    {
        const std::string_view body{trimmed(padded)};
        EXPECT_EQ(canonicalize_intent(padded), canonicalize_intent(body))
            << "canon's trim set and this file's disagree on \"" << padded << "\"";
        EXPECT_EQ(discriminant_of(padded), discriminant_of(body))
            << "canon's trim set and this file's disagree on \"" << padded << "\"";
    }
}

// ── `G-D` LEG 1 — THE CENSUS ───────────────────────────────────────────────────────────────────
TEST(MaskedSpanCensus, TheTwoOrMoreSpanPopulationOnTheG1Bank)
{
    const std::optional<std::string> logs{env("CODEROAST_G1_LOGS")};
    const std::optional<std::string> subject_path{env("CODEROAST_G1_SUBJECT")};
    if (!logs || !subject_path || !std::filesystem::exists(*logs) ||
        !std::filesystem::exists(*subject_path))
        GTEST_SKIP() << kUnmounted;

    std::string subject_error;
    const std::vector<SubjectRow> subject{read_subject(*subject_path, subject_error)};
    ASSERT_TRUE(subject_error.empty()) << subject_error;
    ASSERT_FALSE(subject.empty()) << "the subject list is empty — the census measured nothing.";

    // THE PRODUCTION DECLARATION, identical to the one the `1 000`-situation replay passes on its
    // command line (`--dialect github --channel annotated --transport api-rfc3339-line-prefix`).
    // Declared, never deduced: deduction is content-sensitive and content is exactly what a pair
    // varies.
    static constexpr std::array<std::string_view, 1> kStack{{"api-rfc3339-line-prefix"}};
    const ComposedSemantics composed{
        insight::semantic::compose(std::array{insight::semantic::github::kManifest})};
    const ResolvedStream stream{insight::semantic::resolve_stream(
        composed, IngestDeclaration{.stack = kStack,
                                    .dialect = insight::semantic::github::kDialect,
                                    .channel = insight::semantic::github::kChannelAnnotated})};

    // side → kind → tally (corpus-wide), and pair → side → kind → distinct payloads (per report).
    std::map<std::string, std::map<std::string, Tally>> tallies;
    std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::string>>>>
        payloads;
    std::size_t files_read{0};
    std::size_t lines_total{0};
    std::size_t lines_blank_after_peel{0};
    std::size_t lines_recognized{0};
    std::size_t lines_other{0};
    std::vector<std::string> missing;
    std::string scratch;

    for (const SubjectRow& row : subject)
    {
        const std::filesystem::path path{std::filesystem::path{*logs} / row.file};
        std::ifstream in{path, std::ios::binary};
        if (!in)
        {
            missing.push_back(row.file);
            continue;
        }
        ++files_read;
        std::string line;
        while (std::getline(in, line))
        {
            ++lines_total;
            // THE RECOGNITION PATH, in the shipped order: stage 1 (canon's universal ANSI ingest
            // normalization) then the DECLARED stage 2 (`peel`, which takes a NormalizedLine and
            // hands back the currency the content walkers accept). `peel_raw` is the other path —
            // the tokenizer's — and using it here would be a different seam.
            const insight::tokenization::NormalizedLine normalized{
                insight::tokenization::normalize(line, scratch)};
            const PeeledLine peeled{stream.transport.peel(normalized)};
            if (peeled.is_blank())
            {
                ++lines_blank_after_peel;
                continue;
            }
            const IntentMarker marker{recognize(peeled.content, stream.semantics)};
            if (marker.kind == IntentMarkerKind::None)
            {
                ++lines_other;
                continue;
            }
            ++lines_recognized;
            const std::string payload{marker.name};
            tallies[row.side][kind_name(marker.kind)].add(payload, masked_spans(payload).size());
            // The PER-PAIR view. A corpus-wide class collision is not a collision: the aligner
            // works inside ONE pair, so two names of one class living in two different repos
            // separate nothing and pair nothing. Every set below is therefore keyed by
            // (pair, side, kind) and the corpus-wide bucket above is only the theorem's ceiling.
            payloads[row.pair][row.side][kind_name(marker.kind)].insert(payload);
        }
    }

    // A member that stopped existing must fail loudly, not shrink the arm into a smaller,
    // plausible number.
    ASSERT_TRUE(missing.empty()) << missing.size() << " subject member(s) absent under " << *logs
                                 << " — the census read a smaller corpus than it claims. First: "
                                 << (missing.empty() ? std::string{} : missing.front());

    // LINE CLASSIFICATION IS TOTAL: every line landed in exactly one bucket, so the denominator is
    // the file's lines and not "the lines the parser happened to understand".
    ASSERT_EQ(lines_blank_after_peel + lines_recognized + lines_other, lines_total)
        << "line classification is not total: " << lines_blank_after_peel << " + "
        << lines_recognized << " + " << lines_other << " != " << lines_total;

    // ARMING: the instrument can report a non-zero at all. `macos-14 (15.3)` is leg 0's own cell
    // and it must decompose into two spans — if it does not, every zero below is unfalsifiable.
    ASSERT_EQ(masked_spans("macos-14 (15.3)").size(), 2U)
        << "THE INSTRUMENT IS BLIND: the >=2-span shape this census counts does not decompose "
           "into two spans, so a zero from it would mean nothing.";

    std::cout << "\n[G-D leg 1] subject " << subject.size() << " side-records, " << files_read
              << " read | lines " << lines_total << " (peeled-blank " << lines_blank_after_peel
              << ", marker " << lines_recognized << ", other " << lines_other << ")" << std::endl;

    std::set<std::string> two_plus_union;
    for (const auto& [side, by_kind] : tallies)
        for (const auto& [kind, tally] : by_kind)
        {
            const Bucketed b{bucket(tally)};
            std::cout << "[G-D leg 1] side=" << side << " surface=marker/" << kind << "  "
                      << show(b, tally) << std::endl;
            // EVERY member, never a head: an enumeration truncated for readability is how a
            // complete enumeration gets read as a complete disposition.
            for (const auto& [name, spans] : tally.spans_by_name)
            {
                if (spans < 2)
                    continue;
                two_plus_union.insert(name);
                std::cout << "        >=2  \"" << name << "\"  spans=" << spans << "  today=\""
                          << discriminant_of(name) << "\"  envelope=\"" << envelope(name)
                          << "\"  class=\"" << canonicalize_intent(name) << "\"  x"
                          << tally.occurrences_by_name.at(name) << std::endl;
            }
        }

    // ── THE PER-REPORT PARTITION — what the next leg's prediction is actually about ───────────
    //
    // Three nested sets, each strictly inside the last, and they answer three different questions.
    // TOUCHED: the pair carries at least one >=2-span payload somewhere. Outside this set nothing
    //   can move, so "every other report is byte-identical" is a claim about its complement.
    // REGROUPED: inside ONE run, one class holds two or more DISTINCT >=2-span payloads. Only here
    //   does the widening change what the aligner's multiset key groups — a class with a single
    //   member cannot mis-pair with itself, so a coordinate that merely changes VALUE moves no row
    //   (the coordinate is transient and, after `DN-38.D4`, rendered nowhere).
    // ASYMMETRIC: a class whose >=2-span member SET DIFFERS between baseline and changed. This is
    //   the population that can mint a `ReplacedIntent` row that does not exist today: under the
    //   colliding key the two cells match on the shared first span and mint nothing; under the
    //   widened key they no longer match, and `emit_level` pairs the unmatched vanished with the
    //   unmatched inserted node inside the class.
    std::map<std::string, std::set<std::string>> touched;    // pair → kinds
    std::map<std::string, std::set<std::string>> regrouped;  // pair → "side/kind/class"
    std::map<std::string, std::set<std::string>> asymmetric; // pair → "kind/class"
    // A FOURTH set, and it belongs here because the two rulings are scheduled to ship as ONE
    // comparability event. `DN-38.D1` clause 1 replaces `RankedChange::phase`'s halves with the
    // PRODUCER'S name in place of the class, so a row's headline moves wherever the class differs
    // from the name — that is every name carrying at least ONE masked span, not two. The >=2-span
    // bound below says nothing about that population, and a prediction of the form "every report
    // outside the >=2-span set is byte-identical" is FALSE for a binary carrying both slices.
    std::map<std::string, std::set<std::string>> renamed; // pair → kinds (>=1 masked span)
    for (const auto& [pair, by_side] : payloads)
    {
        // kind → class → side → EVERY member declared on that side, plus the classes that hold at
        // least one >=2-span member. The full member set is what the two questions below need: a
        // class where one side declares `macos-14 (15.3)` and the other declares a name with no
        // second span is still a class whose pairing moves, and restricting the sets to >=2-span
        // members alone would answer a narrower question than the one asked.
        std::map<std::string, std::map<std::string, std::map<std::string, std::set<std::string>>>>
            by_class;
        std::map<std::string, std::set<std::string>> widened_classes; // kind → class
        for (const auto& [side, by_kind] : by_side)
            for (const auto& [kind, names] : by_kind)
                for (const std::string& name : names)
                {
                    const std::string clazz{canonicalize_intent(name)};
                    by_class[kind][clazz][side].insert(name);
                    const std::size_t spans{masked_spans(name).size()};
                    if (spans >= 1)
                        renamed[pair].insert(kind);
                    if (spans >= 2)
                    {
                        touched[pair].insert(kind);
                        widened_classes[kind].insert(clazz);
                    }
                }
        for (const auto& [kind, classes] : widened_classes)
            for (const std::string& clazz : classes)
            {
                const auto& sides{by_class.at(kind).at(clazz)};
                for (const auto& [side, members] : sides)
                    if (members.size() > 1)
                        regrouped[pair].insert(side + "/" + kind + "/" + clazz);
                const auto base{sides.find("baseline")};
                const auto chg{sides.find("changed")};
                const bool differs{base == sides.end() || chg == sides.end() ||
                                   base->second != chg->second};
                if (differs)
                    asymmetric[pair].insert(kind + "/" + clazz);
            }
    }

    const auto enumerate{
        [](std::string_view label, const std::map<std::string, std::set<std::string>>& set)
        {
            std::cout << "[G-D leg 1] " << label << ": " << set.size() << " pair(s)" << std::endl;
            for (const auto& [pair, what] : set)
            {
                std::cout << "        pair " << pair << " —";
                for (const std::string& item : what)
                    std::cout << " [" << item << "]";
                std::cout << std::endl;
            }
        }};
    enumerate("TOUCHED (carry >=1 payload with >=2 masked spans; every OTHER report is "
              "byte-identical under the widening)",
              touched);
    enumerate("REGROUPED (one class, >=2 distinct >=2-span payloads INSIDE one run — where the "
              "aligner's pairing can change)",
              regrouped);
    enumerate("ASYMMETRIC (a >=2-span class whose member set differs across the two sides — where "
              "a ReplacedIntent row can be MINTED that does not exist today)",
              asymmetric);

    // ⚠ THE STEP ROWS OF THE THREE SETS ABOVE ARE AN UPPER BOUND, AND THE JOB ROWS ARE NOT.
    // `emit_level` runs at BOTH tiers, so a step class can mint a REPLACED row exactly as a job
    // class can — but the step tier is scoped to the unmatched steps INSIDE one MATCHED job and
    // aligns them order-respecting, while this census pools every step of a run together and
    // compares sets. So a step class listed here is a candidate; a step class absent from here
    // cannot move. The job tier's multiset match over (class, instance) is what this arithmetic
    // models directly.
    std::cout << "[G-D leg 1] RENAMED (carry >=1 payload with >=1 masked span — the population "
                 "`DN-38.D1` clause 1 moves, NOT this leg's bound): "
              << renamed.size() << " pair(s)" << std::endl;

    std::cout << "[G-D leg 1] ⚠ the Step rows above are an UPPER bound (this census pools a run's "
                 "steps; the engine scopes them per matched job and aligns them order-respecting). "
                 "The Job rows model the aligner's multiset key directly."
              << std::endl;

    std::cout << "[G-D leg 1] UNION of >=2-span distinct payloads across every side and surface: "
              << two_plus_union.size()
              << "\n            this is the EXACT UPPER BOUND on the names whose coordinate the "
                 "`DN-38.D3` widening can move on this corpus; every other name keeps today's "
                 "answer byte-for-byte."
              << "\n            ⚠ the acquirer's DECLARED-DISPLAY surface is NOT in this number: "
                 "no jobs listing is banked for this corpus, and the replay that defines the next "
                 "leg's subject passes no --changed-job-graph, so that surface is empty there."
              << std::endl;
}
// NOLINTEND
