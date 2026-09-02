// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_masked_span_census.cpp — `DN-38`'s measure-first GATE 2, half one: is the PRODUCER'S name
// safe to render, and on how much of the real stream does the rendered headline move?
//
// WHY THAT QUESTION AND NOT ANOTHER. `DN-38.D1` clause 1 rules that `RankedChange::phase` carries
// the PRODUCER'S trimmed name — both halves, job and step — instead of the masked class it carries
// today, and the note gates that ruling behind a measurement that must precede a line of
// implementation: *"on the same pairs, print every row's `phase` under both rules. It settles at
// once whether any producer name is unreasonable as display (length, non-ASCII, CR) and whether
// any pair's row COUNT changes."* The two halves of that sentence have DIFFERENT GRAINS and this
// file answers exactly one of them:
//
//   * **HALF ONE — the rendered BYTES, and it is a property of the NAMES alone.** No aligner, no
//     report, no diff engine: whether `canonicalize_intent(name)` differs from the trimmed name is
//     a pure function of one payload, and so is every display-safety question about it. That is a
//     single-component property and it homes in the component — here.
//   * **HALF TWO — the row COUNT, and it needs the ENGINE.** A row count changes only where the
//     over-merge un-merges, which lives in `insight-eidos`'s failing-unit map and roll-up. Nothing
//     in this package can see it, and a census that pretended to would be answering a narrower
//     question under a wider name. It is measured over the same 1 000 pairs by the harness in
//     `coderoast-corpora/sift_assessment/1.10.2/gd_gate1_over_merge/`, which drives the shipped
//     `sift` CLI.
//
// ── WHAT "THE RENDER MOVES" IS, MEASURED DIRECTLY RATHER THAN INFERRED ────────────────────────
//
// A row's headline moves under `DN-38.D1` clause 1 exactly when `canonicalize_intent(name)` and
// the trimmed name are DIFFERENT BYTES. That is the whole predicate, it is one comparison, and it
// is deliberately NOT "the name carries at least one masked span".
//
// ⚠ THE TWO ARE NOT THE SAME SET, and the difference is not academic: a producer who names a job
// with the mask alphabet itself — `Test (M)` — carries one masked span whose mask is byte-identical
// to the bytes it replaced, so its class equals its name and its headline does NOT move.
// `DN-38.D3` clause 4 declares that residual ambiguity as a property of the CLASS. Counting spans
// would over-count the render delta by exactly that population; comparing the rendered bytes
// cannot.
//
// ⚠ AND IT IS WHY THIS FILE NO LONGER DECOMPOSES A NAME INTO SPANS. It used to, for `G-D` leg 1's
// ≥2-masked-span population, by walking a name with repeated `discriminant_of` on the tail — which
// was sound only while that function returned the FIRST span. `DN-38.D3` widened it to the
// ENVELOPE of the spans (`insight-canon` `ec832ff`), so the walk stops after one step and every
// name reads as one span. The instrument's premise was the very thing under test. Its number is
// banked (`gd_leg1_span_census/RESULTS.md`: 24 distinct payloads with ≥2 spans, 21 of 1 000 pairs
// reachable) and its purpose — bounding the widening's blast radius BEFORE the widening — is
// discharged by `gd_leg2_pinned_rerun/RESULTS.md` (990 of 1 000 reports byte-identical, every
// mover inside the enumerated 21). A derived instrument whose derivation is ruled away is DELETED,
// never forked: rebuilding the decomposition would mean spelling canon's mask literals here, and a
// second implementation of a shipped scan is the one thing this file has always refused.
//
// ── WHY IT HOMES HERE, AND NOT IN `insight-eidos` ─────────────────────────────────────────────
//
// The measured quantity is a function of canon's own `canonicalize_intent` over a population of
// NAMES. Homing it in a sift gate would put a package in every failure's suspect list that the
// measurement never touches. What the population DOES need is the GitHub-Actions marker
// vocabulary — which line is a marker and what its payload is — and that vocabulary is this
// package's `github.dialect.yaml` rows. Population defined here, function owned by canon core,
// both in canon: one repo, one build, no seam.
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
// `insight-eidos`'s declaration fold runs the identity functions on a COMPOSED declared name
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
// ⚠ A `phase` IS TWO HALVES JOINED, AND THIS FILE CENSUSES THE HALVES. `quantum_path` composes
// `job + " ▸ " + step`, so the rendered headline's length is the sum of two payloads plus the
// separator and its safety is the conjunction of theirs. Which job pairs with which step is the
// engine's join, not canon's — so the joined string is reported as a BOUND (the widest job beside
// the widest step), never as an observed row.
//
// Determinism: no RNG, no threads, no wall clock, no float. The subject is a committed, sorted TSV
// of file names; the file set, the walk order and every printed set are sorted, so the output is a
// pure function of the banked bytes.
//
// CORPUS-GATED (`cold_vs_unified_realcorpus` precedent): the banked run logs are third-party
// derivatives that live outside every checkout, so this SKIPS cleanly when the mount is absent —
// green in CI and on every clone.
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

// ── THE RENDERED BYTES, AND THE RISKS A HEADLINE CARRIES ──────────────────────────────────────

// canon's `is_intent_trim_byte`, restated so this file compares the bytes `DN-38.D1` clause 1
// actually renders — the payload with space, tab and CR trimmed off both ends. It is a
// RESTATEMENT, never a fork: `TrimIsCanonInvariant` requires both canon identity functions to
// answer identically on the name and on its trimmed form, so a divergence in the trim set fails
// loudly instead of quietly renaming what "the producer's name" means.
[[nodiscard]] std::string_view trimmed(std::string_view name)
{
    const auto is_trim{[](char byte) { return byte == ' ' || byte == '\t' || byte == '\r'; }};
    while (!name.empty() && is_trim(name.front()))
        name.remove_prefix(1);
    while (!name.empty() && is_trim(name.back()))
        name.remove_suffix(1);
    return name;
}

// The phase separator `quantum_path` joins the two halves with (`insight-eidos`,
// `diff_engine.cpp`). It is named here for ONE diagnostic — how many producer names already
// contain it — and that count is a READABILITY observation, never a correctness one:
// `DN-38.D1` clause 5 and `DN-38.D3` clause 5 both forbid any consumer from splitting `phase`, so
// a name containing the separator makes a headline harder to read and makes nothing wrong.
constexpr std::string_view kPhaseSeparator{" ▸ "};

// A reporting cut, not a limit anything enforces: it exists so the long tail is ENUMERATED rather
// than summarized into a maximum. `DN-38` asks whether any producer name is "unreasonable as
// display"; a maximum alone cannot answer that, because one 300-byte outlier and two hundred of
// them are the same maximum and a different decision.
constexpr std::size_t kLongName{100};

// What could make the producer's trimmed name unreasonable in a report headline. Each flag is a
// property of the RENDERED bytes, so the class carrying the same byte is reported beside it: a
// hazard the class already had is not created by `DN-38.D1` clause 1 and must not be charged to it.
struct DisplayRisk
{
    bool non_ascii{false};         // a byte >= 0x80 — the headline is not pure ASCII
    bool interior_control{false};  // a byte < 0x20 or 0x7F AFTER the trim — a CR or NUL mid-name
    bool carries_separator{false}; // see `kPhaseSeparator` — readability only
    bool carries_quote{false};     // `summary` renders the name inside double quotes

    [[nodiscard]] bool any() const noexcept
    {
        return non_ascii || interior_control || carries_separator || carries_quote;
    }
};

[[nodiscard]] DisplayRisk risk_of(std::string_view rendered)
{
    DisplayRisk out;
    for (const char byte : rendered)
    {
        const auto raw{static_cast<unsigned char>(byte)};
        if (raw >= 0x80U)
            out.non_ascii = true;
        if (raw < 0x20U || raw == 0x7FU)
            out.interior_control = true;
        if (raw == static_cast<unsigned char>('"'))
            out.carries_quote = true;
    }
    out.carries_separator = rendered.find(kPhaseSeparator) != std::string_view::npos;
    return out;
}

[[nodiscard]] std::string show_risk(const DisplayRisk& risk)
{
    std::string out;
    const auto add{[&out](bool flag, std::string_view label)
                   {
                       if (flag)
                           out += (out.empty() ? "" : "+") + std::string{label};
                   }};
    add(risk.non_ascii, "non-ascii");
    add(risk.interior_control, "interior-control");
    add(risk.carries_separator, "separator");
    add(risk.carries_quote, "quote");
    return out.empty() ? "clean" : out;
}

// Every byte outside printable ASCII escaped, so a control byte in a payload cannot corrupt the
// census output it is being reported in — and so the reader can SEE which byte it was.
[[nodiscard]] std::string escaped(std::string_view text)
{
    static constexpr std::string_view kHex{"0123456789abcdef"};
    std::string out;
    for (const char byte : text)
    {
        const auto raw{static_cast<unsigned char>(byte)};
        if (raw >= 0x20U && raw < 0x7FU)
        {
            out.push_back(byte);
            continue;
        }
        out += "\\x";
        out.push_back(kHex[raw >> 4U]);
        out.push_back(kHex[raw & 0x0FU]);
    }
    return out;
}

// ── THE TALLY ──────────────────────────────────────────────────────────────────────────────────

// One (side × marker kind) cell. Names are counted BOTH ways on purpose: a DISTINCT-name count is
// the population a rule applies to, an OCCURRENCE count is how often a reader meets it, and a
// count reported without saying which one it is cannot be spent.
struct Tally
{
    std::map<std::string, std::size_t> occurrences_by_name; // payload → times recognized
    std::set<std::string> names;                            // every distinct payload
    std::size_t recognized{0};

    void add(const std::string& name)
    {
        ++occurrences_by_name[name];
        names.insert(name);
        ++recognized;
    }
};

// The census's arithmetic over one cell. `moved` is `DN-38.D1` clause 1's population on this
// surface: the names whose rendered headline is different bytes after the ruling.
struct Bucketed
{
    std::size_t names_moved{0};
    std::size_t names_unmoved{0};
    std::size_t occ_moved{0};
    std::size_t occ_unmoved{0};
    std::size_t longest_name{0};  // bytes, the TRIMMED producer name (what clause 1 renders)
    std::size_t longest_class{0}; // bytes, what is rendered TODAY
    std::size_t widest_growth{0}; // max(name − class) over the cell, 0 when no name grows
    std::size_t names_over_cut{0};
    std::size_t names_at_risk{0};
    // ⚠ THE NUMBER A RULING IS TAKEN ON, and it is NOT `names_at_risk`. A hazard the CLASS already
    // carries is on the wire today; `DN-38.D1` clause 1 neither creates it nor removes it, and
    // charging it to the ruling would make the count say the opposite of what it means
    // (`MEM:prescriptive-instrument-false-positive-costs-truth` — a firing count never bounds
    // correctness). Only a name whose rendered bytes are at risk while its class is NOT is a
    // hazard the ruling introduces.
    std::size_t names_risk_introduced{0};
};

// Does the ruling INTRODUCE a display hazard on this name, or was it already on the wire?
[[nodiscard]] bool risk_introduced(std::string_view name)
{
    return risk_of(trimmed(name)).any() && !risk_of(canonicalize_intent(name)).any();
}

[[nodiscard]] Bucketed bucket(const Tally& tally)
{
    Bucketed out;
    for (const std::string& name : tally.names)
    {
        const std::size_t occ{tally.occurrences_by_name.at(name)};
        const std::string_view rendered{trimmed(name)};
        const std::string clazz{canonicalize_intent(name)};
        if (rendered != clazz)
        {
            ++out.names_moved;
            out.occ_moved += occ;
        }
        else
        {
            ++out.names_unmoved;
            out.occ_unmoved += occ;
        }
        out.longest_name = std::max(out.longest_name, rendered.size());
        out.longest_class = std::max(out.longest_class, clazz.size());
        if (rendered.size() > clazz.size())
            out.widest_growth = std::max(out.widest_growth, rendered.size() - clazz.size());
        out.names_over_cut += static_cast<std::size_t>(rendered.size() > kLongName);
        out.names_at_risk += static_cast<std::size_t>(risk_of(rendered).any());
        out.names_risk_introduced += static_cast<std::size_t>(risk_introduced(name));
    }
    return out;
}

[[nodiscard]] std::string show(const Bucketed& b, const Tally& t)
{
    std::ostringstream out;
    out << "recognized=" << t.recognized << " distinct=" << t.names.size()
        << " | headline MOVES: " << b.names_moved << " names / " << b.occ_moved << " occ"
        << " | unchanged: " << b.names_unmoved << " / " << b.occ_unmoved
        << " | longest rendered name " << b.longest_name << "B (class today " << b.longest_class
        << "B, widest growth +" << b.widest_growth << "B)"
        << " | over the " << kLongName << "B cut: " << b.names_over_cut
        << " | at display risk: " << b.names_at_risk
        << " (of which INTRODUCED by clause 1: " << b.names_risk_introduced
        << "; the rest are hazards the class already carries and the wire already ships)";
    return out.str();
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

// ── THE INSTRUMENT'S OWN PROOF — runs with no mount, on every clone ────────────────────────────
//
// Two detectors decide everything this file prints, and a census whose detectors cannot fire
// reports zeros that mean nothing (`MEM:synthetic-gate-vacuity-vs-judgment`, the green-BLIND row).
// Both are exercised in BOTH directions here: a case that must fire and a case that must not.
TEST(MaskedSpanCensus, TheRenderDeltaPredicateAndTheRiskDetectorBothFireAndBothStaySilent)
{
    // ── THE RENDER-DELTA PREDICATE: does `DN-38.D1` clause 1 move this headline? ──
    struct DeltaCase
    {
        std::string_view name;
        bool moves;
        std::string_view why;
    };
    static constexpr std::array<DeltaCase, 6> kDeltaCases{{
        {"macos-14 (15.3)", true, "a runner-matrix cell: two masked spans, both rewritten"},
        {"ESLint v6", true, "one masked span, rewritten"},
        {"Lint", false, "no masked span at all — nothing to rewrite"},
        {"Build", false, "no masked span at all — nothing to rewrite"},
        // ⚠ THE CASE THAT SEPARATES THIS PREDICATE FROM A SPAN COUNT, and the reason the census
        // compares rendered BYTES. `DN-38.D3` clause 4's residual ambiguity: a producer who names
        // a job in canon's own mask alphabet carries a masked span whose mask is byte-identical to
        // the bytes it replaced, so the class equals the name and the HEADLINE DOES NOT MOVE. A
        // span-counting instrument would charge this name to clause 1's population; this one
        // cannot.
        {"Test (M)", false, "carries a masked span whose mask reproduces its own bytes"},
        {"  Lint\t", false, "differs from its class only by the trim, which clause 1 also applies"},
    }};
    for (const auto& [name, moves, why] : kDeltaCases)
    {
        const bool observed{trimmed(name) != canonicalize_intent(name)};
        EXPECT_EQ(observed, moves)
            << "the render-delta predicate answered " << (observed ? "MOVES" : "unchanged")
            << " for \"" << name << "\" and the case says " << (moves ? "MOVES" : "unchanged")
            << " — " << why << ".\n  trimmed name : \"" << escaped(trimmed(name))
            << "\"\n  class today  : \"" << escaped(canonicalize_intent(name)) << "\"";
    }
    // The separating case is only separating if it really does carry a span — otherwise it proves
    // nothing about the difference between the two instruments.
    EXPECT_FALSE(discriminant_of("Test (M)").empty())
        << "\"Test (M)\" no longer carries a masked span, so it is no longer the case that "
           "separates the render-delta predicate from a span count, and the comment above it is "
           "false.";

    // ── THE DISPLAY-RISK DETECTOR: each flag fires on its own witness and on nothing else ──
    EXPECT_FALSE(risk_of("Build and test (ubuntu-latest)").any())
        << "the risk detector fires on an ordinary ASCII job name, so every at-risk count below "
           "is noise.";
    EXPECT_TRUE(risk_of("Tests \xE2\x9C\x93").non_ascii) << "the non-ASCII flag does not fire";
    EXPECT_TRUE(risk_of("build\rtest").interior_control)
        << "the interior-control flag does not fire on a CR between two words — and an end-trimmed "
           "CR is NOT this case: canon's trim set removes those, this flag is for the ones it "
           "cannot reach.";
    EXPECT_FALSE(risk_of(trimmed("build \r")).interior_control)
        << "the interior-control flag fires on a CR the trim already removed, so it would charge "
           "`DN-38.D1` clause 1 for a byte clause 1 never renders.";
    EXPECT_TRUE(
        risk_of(std::string{"build"} + std::string{kPhaseSeparator} + "test").carries_separator)
        << "the separator flag does not fire";
    EXPECT_TRUE(risk_of("run \"the suite\"").carries_quote) << "the quote flag does not fire";

    // ── THE INTRODUCED/PRE-EXISTING SPLIT: the number a ruling is taken on, both directions ──
    //
    // A hazard the class already carries is on the wire TODAY and clause 1 is not answerable for
    // it. A census that reported the two together would say the opposite of what it means.
    EXPECT_FALSE(risk_introduced("run \"the suite\""))
        << "a double quote that survives masking is on the wire today — the class is \""
        << canonicalize_intent("run \"the suite\"")
        << "\" — so clause 1 introduces nothing and must not be charged for it.";
    EXPECT_TRUE(risk_introduced("deploy (\"eu-west-1\")"))
        << "a quote that the paren mask REMOVES from the class — class \""
        << canonicalize_intent("deploy (\"eu-west-1\")")
        << "\" — is a hazard clause 1 puts back in front of a reader, and the split must see it.";
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
            << "canon's trim set and this file's disagree on \"" << escaped(padded) << "\"";
        EXPECT_EQ(discriminant_of(padded), discriminant_of(body))
            << "canon's trim set and this file's disagree on \"" << escaped(padded) << "\"";
    }
}

// ── `DN-38` GATE 2, HALF ONE — THE RENDER DELTA ON THE `G1` BANK ───────────────────────────────
TEST(MaskedSpanCensus, TheProducerNameRenderDeltaOnTheG1Bank)
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
            tallies[row.side][kind_name(marker.kind)].add(payload);
            // The PER-PAIR view. A corpus-wide count is the ceiling; a report is one pair, so the
            // question "how many REPORTS does clause 1 move" is answered only per pair.
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
    ASSERT_FALSE(tallies.empty()) << "no marker was recognized in " << files_read
                                  << " file(s) — the census walked the corpus and saw no name, so "
                                     "every zero below is about the reader, not about the bytes.";

    std::cout << "\n[DN-38 gate 2 / half one] subject " << subject.size() << " side-records, "
              << files_read << " read | lines " << lines_total << " (peeled-blank "
              << lines_blank_after_peel << ", marker " << lines_recognized << ", other "
              << lines_other << ")" << std::endl;

    // side → kind → the widest rendered name, for the joined-headline BOUND below.
    std::map<std::string, std::map<std::string, std::string>> widest;
    for (const auto& [side, by_kind] : tallies)
        for (const auto& [kind, tally] : by_kind)
        {
            const Bucketed b{bucket(tally)};
            std::cout << "[DN-38 gate 2] side=" << side << " surface=marker/" << kind << "  "
                      << show(b, tally) << std::endl;
            // EVERY member, never a head: an enumeration truncated for readability is how a
            // complete enumeration gets read as a complete disposition. These two sets are the
            // ones a ruling is taken on, so both are printed whole.
            for (const std::string& name : tally.names)
            {
                const std::string_view rendered{trimmed(name)};
                if (rendered.size() > widest[side][kind].size())
                    widest[side][kind] = std::string{rendered};
                const DisplayRisk risk{risk_of(rendered)};
                if (risk.any())
                    std::cout << "        RISK  "
                              << (risk_introduced(name) ? "INTRODUCED " : "pre-existing ")
                              << show_risk(risk) << "  \"" << escaped(rendered) << "\"  class=\""
                              << escaped(canonicalize_intent(name)) << "\"  x"
                              << tally.occurrences_by_name.at(name) << std::endl;
                if (rendered.size() > kLongName)
                    std::cout << "        LONG  " << rendered.size() << "B  \"" << escaped(rendered)
                              << "\"  (class " << canonicalize_intent(name).size() << "B)  x"
                              << tally.occurrences_by_name.at(name) << std::endl;
            }
        }

    // ── THE JOINED HEADLINE, AS A BOUND ──────────────────────────────────────────────────────
    //
    // `phase` is `job ▸ step` and which job pairs with which step is the ENGINE's join, not
    // canon's. So the widest headline this corpus can produce is the widest job beside the widest
    // step — an upper bound that no observed row need reach, and it is labelled as one.
    for (const auto& [side, by_kind] : widest)
    {
        const auto job{by_kind.find("Job")};
        const auto step{by_kind.find("Step")};
        const std::size_t job_len{job == by_kind.end() ? 0 : job->second.size()};
        const std::size_t step_len{step == by_kind.end() ? 0 : step->second.size()};
        std::cout << "[DN-38 gate 2] side=" << side << " widest JOINED headline BOUND = " << job_len
                  << " + " << kPhaseSeparator.size() << " + " << step_len << " = "
                  << job_len + kPhaseSeparator.size() + step_len << "B"
                  << "\n        widest Job  \"" << escaped(job == by_kind.end() ? "" : job->second)
                  << "\"\n        widest Step \""
                  << escaped(step == by_kind.end() ? "" : step->second)
                  << "\"\n        ⚠ a BOUND: this job and this step need not sit in one row."
                  << std::endl;
    }

    // ── THE PER-REPORT PARTITION — how many of the 1 000 reports clause 1 moves ───────────────
    //
    // MOVED: the pair carries at least one payload whose rendered headline differs from its class.
    // Outside this set no row's `phase` can change a byte, so "every other report is
    // byte-identical under clause 1" is a claim about its complement — and that complement is what
    // a pinned re-run of clause 1's slice must predict.
    std::map<std::string, std::set<std::string>> moved;   // pair → kinds
    std::map<std::string, std::set<std::string>> at_risk; // pair → "kind: name"
    for (const auto& [pair, by_side] : payloads)
        for (const auto& [side, by_kind] : by_side)
            for (const auto& [kind, names] : by_kind)
                for (const std::string& name : names)
                {
                    const std::string_view rendered{trimmed(name)};
                    if (rendered != canonicalize_intent(name))
                        moved[pair].insert(kind);
                    if (risk_introduced(name))
                        at_risk[pair].insert(kind + ": \"" + escaped(rendered) + "\"");
                }

    const auto enumerate{
        [](std::string_view label, const std::map<std::string, std::set<std::string>>& set)
        {
            std::cout << "[DN-38 gate 2] " << label << ": " << set.size() << " pair(s)"
                      << std::endl;
            for (const auto& [pair, what] : set)
            {
                std::cout << "        pair " << pair << " —";
                for (const std::string& item : what)
                    std::cout << " [" << item << "]";
                std::cout << std::endl;
            }
        }};
    std::cout << "[DN-38 gate 2] MOVED (the pair carries >=1 payload whose rendered headline "
                 "changes under `DN-38.D1` clause 1; every OTHER report keeps every `phase` byte): "
              << moved.size()
              << " pair(s) — not enumerated, it is the majority set; its "
                 "COMPLEMENT is the prediction and it is "
              << (payloads.size() - moved.size()) << " pair(s)." << std::endl;
    enumerate("DISPLAY HAZARD INTRODUCED BY CLAUSE 1 (the rendered name carries a non-ASCII byte, "
              "an interior control byte, the phase separator or a double quote AND its class does "
              "not — the set the ruling is answerable for)",
              at_risk);
}
// NOLINTEND
