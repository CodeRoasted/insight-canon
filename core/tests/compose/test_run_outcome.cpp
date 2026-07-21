// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_run_outcome.cpp — the grammar-2 run-outcome MECHANISMS (ADR 0025 /
// insight_run_outcome_model.md §3–§4) over SYNTHETIC manifests (canon core stays semantic-unaware —
// no dialect literal here; the Jenkins/GHA vocabularies are package data, tested in their packages;
// the G-OUT-* gate suite is Kleio's homing). What CORE owns and this file guards:
//   • map_outcome_token — format-gated, byte-exact; "no row" (nullopt) is distinct from "a row that
//     maps to Unknown" (the NOT_BUILT shape).
//   • the IntentMarkerRow grammar-2 shapes — RemainderToClosingParen strictness + the payload
//     exclusion set's word-boundary semantics.
//   • scan_run_outcome — last-match-wins, strict verdict-word remainder, the dialect latch.
//   • resolve_run_outcome — the D-OUT-RUN-1 strict ladder: authoritative wins over a present-but-
//     divergent console tail (the divergence is FLAGGED, never a tiebreak), unmapped tokens surface
//     a note and fall down the ladder (fail-closed), absence resolves Unknown.
//   • find_conflict — a cross-package duplicate outcome token / marker prefix fails the build.
//   • outcome_regressed — strictly-worse on Success < Unstable < Failure; Aborted/Unknown excluded.
// Determinism: byte-only walks, integer line index; no RNG/clock/float.
#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogFormat;
using insight::map_outcome_token;
using insight::outcome_regressed;
using insight::resolve_run_outcome;
using insight::RunOutcome;
using insight::RunOutcomeResolution;
using insight::RunOutcomeScan;
using insight::scan_run_outcome;
using insight::semantic::compose;
using insight::semantic::ComposedSemantics;
using insight::semantic::find_conflict;
using insight::semantic::IntentMarkerRow;
using insight::semantic::OutcomeMarkerRow;
using insight::semantic::OutcomeTokenRow;
using insight::semantic::PayloadExtract;
using insight::semantic::SemanticPackageManifest;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

namespace
{
// ── A synthetic outcome-bearing dialect, gated on RawText (a core format any unstructured line
// routes to — no dialect literal, no package strategy needed). Mirrors the Jenkins SHAPE: four
// verdict classes + one token that maps TO Unknown (the NOT_BUILT form) + a console-tail marker. ──
constexpr std::array<OutcomeTokenRow, 5> kTokens{{
    {.token = "GOOD", .outcome = RunOutcome::Success, .format_gate = LogFormat::RawText},
    {.token = "BAD", .outcome = RunOutcome::Failure, .format_gate = LogFormat::RawText},
    {.token = "SHAKY", .outcome = RunOutcome::Unstable, .format_gate = LogFormat::RawText},
    {.token = "STOPPED", .outcome = RunOutcome::Aborted, .format_gate = LogFormat::RawText},
    {.token = "SKIPPED", .outcome = RunOutcome::Unknown, .format_gate = LogFormat::RawText},
}};
constexpr std::array<OutcomeMarkerRow, 1> kMarkers{{
    {.prefix = "Ended: ", .format_gate = LogFormat::RawText},
}};

// The grammar-2 marker shapes: a paren-delimited container row + a remainder row with an exclusion
// set (the Jenkins STAGE/STEP forms, expressed synthetically).
constexpr std::array<std::string_view, 3> kStepExcludes{"{", "}", "End of Run"};
constexpr std::array<IntentMarkerRow, 2> kIntentRows{{
    {.prefix = "[Mark] { (",
     .kind = IntentMarkerKind::Job,
     .child_order = ChildOrder::Unordered,
     .format_gate = LogFormat::RawText,
     .extract = PayloadExtract::RemainderToClosingParen},
    {.prefix = "[Mark] ",
     .kind = IntentMarkerKind::Step,
     .child_order = ChildOrder::Ordered,
     .format_gate = LogFormat::RawText,
     .extract = PayloadExtract::RemainderAfterPrefix,
     .payload_excludes = kStepExcludes},
}};

constexpr SemanticPackageManifest kOutcomePkg{.name = "synthetic_outcome",
                                              .version = "1.0.0",
                                              .markers = kIntentRows,
                                              .outcome_tokens = kTokens,
                                              .outcome_markers = kMarkers};

// A second package duplicating a token under an intersecting gate — the SP-3 conflict fixture.
constexpr std::array<OutcomeTokenRow, 1> kDupToken{
    {{.token = "GOOD", .outcome = RunOutcome::Failure, .format_gate = LogFormat::RawText}}};
constexpr SemanticPackageManifest kDupPkg{
    .name = "synthetic_dup", .version = "1.0.0", .outcome_tokens = kDupToken};
// The same token under a NON-intersecting gate is NOT a duplicate (a different dialect naming the
// same string is legal — the side-input resolves under one detected dialect).
constexpr std::array<OutcomeTokenRow, 1> kOtherGateToken{
    {{.token = "GOOD", .outcome = RunOutcome::Success, .format_gate = LogFormat::JSON}}};
constexpr SemanticPackageManifest kOtherGatePkg{
    .name = "synthetic_other", .version = "1.0.0", .outcome_tokens = kOtherGateToken};

[[nodiscard]] ComposedSemantics composed_outcome()
{
    return compose(std::array{kOutcomePkg});
}
} // namespace

// ── SP-3 fail-closed, build-time half: duplicate outcome keys are constexpr-detectable ──
static_assert(find_conflict(std::array{kOutcomePkg, kDupPkg}).has_conflict,
              "a cross-package duplicate outcome token under intersecting gates must conflict");
static_assert(find_conflict(std::array{kOutcomePkg, kDupPkg}).kind == "outcome_token",
              "the conflict must be reported as an outcome_token duplicate");
static_assert(!find_conflict(std::array{kOutcomePkg, kOtherGatePkg}).has_conflict,
              "the same token under NON-intersecting gates is two dialects' data, not a conflict");

// ── map_outcome_token: format-gated, byte-exact; unmapped ≠ mapped-to-Unknown ──
TEST(RunOutcomeMap, FormatGatedExactMatch)
{
    const ComposedSemantics composed{composed_outcome()};
    EXPECT_EQ(map_outcome_token("GOOD", LogFormat::RawText, composed), RunOutcome::Success);
    EXPECT_EQ(map_outcome_token("SHAKY", LogFormat::RawText, composed), RunOutcome::Unstable);
    EXPECT_EQ(map_outcome_token("STOPPED", LogFormat::RawText, composed), RunOutcome::Aborted);
    // A row mapping TO Unknown (the NOT_BUILT shape) still MAPS — engaged optional, value Unknown.
    const auto skipped{map_outcome_token("SKIPPED", LogFormat::RawText, composed)};
    ASSERT_TRUE(skipped.has_value()) << "a token mapped to Unknown is a MAPPING, not a miss";
    EXPECT_EQ(*skipped, RunOutcome::Unknown);
    // No row: disengaged.
    EXPECT_FALSE(map_outcome_token("WEIRD", LogFormat::RawText, composed).has_value());
    // Wrong gate: a RawText-dialect token never resolves under another format (II-6)…
    EXPECT_FALSE(map_outcome_token("GOOD", LogFormat::JSON, composed).has_value());
    // …and a line/log routed Unknown fires no concretely-gated row.
    EXPECT_FALSE(map_outcome_token("GOOD", LogFormat::Unknown, composed).has_value());
    // Byte-exact: case matters (native tokens are verbatim dialect strings).
    EXPECT_FALSE(map_outcome_token("good", LogFormat::RawText, composed).has_value());
}

// ── grammar-2 marker shapes: RemainderToClosingParen strictness ──
TEST(RunOutcomeGrammar2, ParenExtractorIsStrict)
{
    const ComposedSemantics composed{composed_outcome()};
    // The named-container form: payload is the paren content, verbatim.
    const auto stage{recognize("[Mark] { (Build)", LogFormat::RawText, composed)};
    EXPECT_EQ(stage.kind, IntentMarkerKind::Job);
    EXPECT_EQ(stage.name, "Build");
    // Nested parens stay inside the payload; only the single final ')' delimits.
    const auto nested{recognize("[Mark] { (Branch: test (lts))", LogFormat::RawText, composed)};
    EXPECT_EQ(nested.kind, IntentMarkerKind::Job);
    EXPECT_EQ(nested.name, "Branch: test (lts)");
    // No line-final ')' → the paren row does NOT match; the line falls through to the shorter
    // remainder row, whose payload "{ (Build" is excluded by "{" → no marker at all.
    EXPECT_EQ(recognize("[Mark] { (Build", LogFormat::RawText, composed).kind,
              IntentMarkerKind::None)
        << "an unterminated paren form must not claim a quantum";
}

TEST(RunOutcomeGrammar2, PayloadExcludesAreWordBounded)
{
    const ComposedSemantics composed{composed_outcome()};
    // An un-named wrapper open/close is scaffold, not a step.
    EXPECT_EQ(recognize("[Mark] {", LogFormat::RawText, composed).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize("[Mark] }", LogFormat::RawText, composed).kind, IntentMarkerKind::None);
    // A multi-word exclusion entry matches the whole payload.
    EXPECT_EQ(recognize("[Mark] End of Run", LogFormat::RawText, composed).kind,
              IntentMarkerKind::None);
    // First-token semantics: an excluded token followed by trailing content still excludes…
    EXPECT_EQ(recognize("[Mark] { retries", LogFormat::RawText, composed).kind,
              IntentMarkerKind::None);
    // …but the boundary is a WORD boundary: a verb merely PREFIXED by an entry is a real step.
    const auto step{recognize("[Mark] {}able", LogFormat::RawText, composed)};
    EXPECT_EQ(step.kind, IntentMarkerKind::Step)
        << "exclusion must not over-reach past the boundary";
    // The ordinary verb form is a step with the verbatim remainder payload.
    const auto verb{recognize("[Mark] compile", LogFormat::RawText, composed)};
    EXPECT_EQ(verb.kind, IntentMarkerKind::Step);
    EXPECT_EQ(verb.name, "compile");
}

// ── scan_run_outcome: last-match-wins, strict word remainder, the dialect latch ──
TEST(RunOutcomeScanTest, LastMarkerWinsAndDialectLatches)
{
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> lines{"building things", "Ended: BAD", "retrying the run",
                                         "Ended: GOOD"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    EXPECT_TRUE(scan.marker_present);
    EXPECT_EQ(scan.token, "GOOD") << "a run has ONE terminal verdict — the LAST marker match wins";
    EXPECT_EQ(scan.marker_format, LogFormat::RawText);
    EXPECT_EQ(scan.dialect, LogFormat::RawText)
        << "the dialect latch: the first routed format carrying outcome-token rows";
}

TEST(RunOutcomeScanTest, RemainderMustBeASingleVerdictWord)
{
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> trailing{"Ended: GOOD (took 3s)"};
    EXPECT_FALSE(scan_run_outcome(trailing, composed).marker_present)
        << "a decorated epilogue is not a terminal-verdict line (studies/006 ^Finished: (\\w+)$)";
    const std::vector<std::string> empty_tok{"Ended: "};
    EXPECT_FALSE(scan_run_outcome(empty_tok, composed).marker_present);
    const std::vector<std::string> none{"a log with no epilogue at all"};
    const RunOutcomeScan scan{scan_run_outcome(none, composed)};
    EXPECT_FALSE(scan.marker_present);
    EXPECT_EQ(scan.dialect, LogFormat::RawText) << "the dialect still latches without a marker";
}

TEST(RunOutcomeScanTest, DegenerateCompositionScansNothing)
{
    const ComposedSemantics core{compose({})};
    const std::vector<std::string> lines{"Ended: GOOD"};
    const RunOutcomeScan scan{scan_run_outcome(lines, core)};
    EXPECT_FALSE(scan.marker_present);
    EXPECT_EQ(scan.dialect, LogFormat::Unknown);
}

// ── resolve_run_outcome: the D-OUT-RUN-1 strict ladder ──
TEST(RunOutcomeResolve, AuthoritativeWinsOverPresentDivergentConsole)
{
    // The Accumulo #498 SHAPE: authoritative Success vs a present console tail saying Aborted.
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> lines{"working", "Ended: STOPPED"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const RunOutcomeResolution res{resolve_run_outcome("GOOD", scan, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Success)
        << "rung 1 resolves; a present-but-divergent console tail is NOT a tiebreak";
    EXPECT_TRUE(res.authoritative);
    EXPECT_TRUE(res.divergent) << "the disagreement is made legible, never silently dropped";
    EXPECT_EQ(res.console, RunOutcome::Aborted);
    EXPECT_TRUE(res.note.empty());
}

TEST(RunOutcomeResolve, ConsoleTailIsTheFallback)
{
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> lines{"working", "Ended: SHAKY"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    // No side-input → rung 2: the console tail recovers the verdict — UNSTABLE stays UNSTABLE.
    const RunOutcomeResolution res{resolve_run_outcome("", scan, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Unstable);
    EXPECT_FALSE(res.authoritative);
    EXPECT_FALSE(res.divergent);
}

TEST(RunOutcomeResolve, UnmappedSideInputSurfacesANoteAndFallsThrough)
{
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> lines{"working", "Ended: BAD"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const RunOutcomeResolution res{resolve_run_outcome("WEIRD", scan, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Failure) << "the ladder continues past an unmapped rung 1";
    EXPECT_FALSE(res.authoritative);
    EXPECT_FALSE(res.note.empty()) << "an unmapped token is NEVER silent (fail-closed)";
    EXPECT_NE(res.note.find("WEIRD"), std::string::npos) << "the note names the offending token";
}

TEST(RunOutcomeResolve, NoDialectMeansNoSideInputResolution)
{
    const ComposedSemantics composed{composed_outcome()};
    // A log whose lines never route to an outcome-bearing format: JSON lines (no RawText).
    const std::vector<std::string> lines{
        R"({"ts":"2024-01-15T10:30:00Z","level":"INFO","component":"auth","message":"hi"})"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    EXPECT_EQ(scan.dialect, LogFormat::Unknown);
    const RunOutcomeResolution res{resolve_run_outcome("GOOD", scan, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Unknown)
        << "a side-input cannot resolve outside a detected outcome-bearing dialect (fail-closed)";
    EXPECT_FALSE(res.note.empty());
}

TEST(RunOutcomeResolve, AbsenceIsUnknownWithoutFraming)
{
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> lines{"no epilogue here"};
    const RunOutcomeResolution res{
        resolve_run_outcome("", scan_run_outcome(lines, composed), composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Unknown);
    EXPECT_TRUE(res.note.empty()) << "absence is the legacy default, not an error";
}

TEST(RunOutcomeResolve, MappedToUnknownIsAuthoritative)
{
    // The NOT_BUILT shape: the token MAPS, its value is Unknown — rung 1 resolves (no fallback to
    // a stale console tail), no note (nothing failed).
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> lines{"Ended: GOOD"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const RunOutcomeResolution res{resolve_run_outcome("SKIPPED", scan, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Unknown);
    EXPECT_TRUE(res.authoritative) << "mapped-to-Unknown is a RESOLUTION, not a miss";
    EXPECT_TRUE(res.divergent) << "the mapped console tail disagrees — flagged, not consulted";
    EXPECT_TRUE(res.note.empty());
}

// ── outcome_regressed: the §6.1 pass↔fail axis ──
TEST(OutcomeRegressed, StrictlyWorseOnTheAxisOnly)
{
    // Strictly worse.
    EXPECT_TRUE(outcome_regressed(RunOutcome::Success, RunOutcome::Failure));
    EXPECT_TRUE(outcome_regressed(RunOutcome::Success, RunOutcome::Unstable));
    EXPECT_TRUE(outcome_regressed(RunOutcome::Unstable, RunOutcome::Failure));
    // Steady or better.
    EXPECT_FALSE(outcome_regressed(RunOutcome::Unstable, RunOutcome::Unstable))
        << "steady-flaky is NOT a verdict regression (§6.1)";
    EXPECT_FALSE(outcome_regressed(RunOutcome::Failure, RunOutcome::Success));
    EXPECT_FALSE(outcome_regressed(RunOutcome::Unstable, RunOutcome::Success));
    EXPECT_FALSE(outcome_regressed(RunOutcome::Failure, RunOutcome::Failure));
    // Aborted / Unknown are OFF the axis — never a regression in either direction.
    EXPECT_FALSE(outcome_regressed(RunOutcome::Success, RunOutcome::Aborted));
    EXPECT_FALSE(outcome_regressed(RunOutcome::Aborted, RunOutcome::Failure));
    EXPECT_FALSE(outcome_regressed(RunOutcome::Success, RunOutcome::Unknown));
    EXPECT_FALSE(outcome_regressed(RunOutcome::Unknown, RunOutcome::Failure));
}
// NOLINTEND
