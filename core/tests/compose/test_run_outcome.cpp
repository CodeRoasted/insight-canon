// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_run_outcome.cpp — the grammar-2 run-outcome MECHANISMS (ADR 0025 /
// insight_run_outcome_model.md §3–§4) over SYNTHETIC manifests (canon core stays semantic-unaware —
// no dialect literal here; the Jenkins/GHA vocabularies are package data, tested in their packages;
// the G-OUT-* gate suite is Kleio's homing). What CORE owns and this file guards:
//   • map_outcome_token — resolved-view lookup, byte-exact; "no row" (nullopt) is distinct from "a row that
//     maps to Unknown" (the NOT_BUILT shape).
//   • the IntentMarkerRow grammar-2 shapes — RemainderToClosingParen strictness + the payload
//     exclusion set's word-boundary semantics.
//   • scan_run_outcome — last-match-wins, strict verdict-word remainder (no dialect latch: the
//     dialect is DECLARED, so the scan carries no LogFormat at all — ADR 0065 clause 2).
//   • resolve_run_outcome — the D-OUT-RUN-1 strict ladder: authoritative wins over a present-but-
//     divergent console tail (the divergence is FLAGGED, never a tiebreak), unmapped tokens surface
//     a note and fall down the ladder (fail-closed), absence resolves Unknown.
//   • find_conflict — a cross-package duplicate outcome token / marker prefix fails the build.
//   • outcome_regressed — strictly-worse on Success < Unstable < Failure; Aborted/Unknown excluded.
// Determinism: byte-only walks, integer line index; no RNG/clock/float.
#include <gtest/gtest.h>

import insight.canon.test;

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
// ── A synthetic outcome-bearing dialect, gated on its OWN package name (ADR 0065 clause 1 — the
// gate is a composed package NAME, so canon core stays semantic-unaware and there is still no
// dialect literal from a real ecosystem here). Mirrors the Jenkins SHAPE: four verdict classes +
// one token that maps TO Unknown (the NOT_BUILT form) + a console-tail marker. ──
constexpr std::string_view kSyntheticDialect{"synthetic_outcome"};
constexpr std::string_view kOtherDialect{"synthetic_other"};
constexpr std::string_view kUndeclared{};
constexpr std::array<OutcomeTokenRow, 5> kTokens{{
    {.token = "GOOD", .outcome = RunOutcome::Success, .dialect_gate = kSyntheticDialect},
    {.token = "BAD", .outcome = RunOutcome::Failure, .dialect_gate = kSyntheticDialect},
    {.token = "SHAKY", .outcome = RunOutcome::Unstable, .dialect_gate = kSyntheticDialect},
    {.token = "STOPPED", .outcome = RunOutcome::Aborted, .dialect_gate = kSyntheticDialect},
    {.token = "SKIPPED", .outcome = RunOutcome::Unknown, .dialect_gate = kSyntheticDialect},
}};
constexpr std::array<OutcomeMarkerRow, 1> kMarkers{{
    {.prefix = "Ended: ", .dialect_gate = kSyntheticDialect},
}};

// The grammar-2 marker shapes: a paren-delimited container row + a remainder row with an exclusion
// set (the Jenkins STAGE/STEP forms, expressed synthetically).
constexpr std::array<std::string_view, 3> kStepExcludes{"{", "}", "End of Run"};
constexpr std::array<IntentMarkerRow, 2> kIntentRows{{
    {.prefix = "[Mark] { (",
     .kind = IntentMarkerKind::Job,
     .child_order = ChildOrder::Unordered,
     .dialect_gate = kSyntheticDialect,
     .extract = PayloadExtract::RemainderToClosingParen},
    {.prefix = "[Mark] ",
     .kind = IntentMarkerKind::Step,
     .child_order = ChildOrder::Ordered,
     .dialect_gate = kSyntheticDialect,
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
    {{.token = "GOOD", .outcome = RunOutcome::Failure, .dialect_gate = kSyntheticDialect}}};
constexpr SemanticPackageManifest kDupPkg{
    .name = "synthetic_dup", .version = "1.0.0", .outcome_tokens = kDupToken};
// The same token under a NON-intersecting gate is NOT a duplicate (a different dialect naming the
// same string is legal — the side-input resolves under the ONE dialect the stream declared).
constexpr std::array<OutcomeTokenRow, 1> kOtherGateToken{
    {{.token = "GOOD", .outcome = RunOutcome::Success, .dialect_gate = kOtherDialect}}};
constexpr SemanticPackageManifest kOtherGatePkg{
    .name = "synthetic_other", .version = "1.0.0", .outcome_tokens = kOtherGateToken};

// The RESOLVED view of a stream that declared this synthetic dialect — the ONE door (ADR 0065
// clause 2). Everything below scores against a declared stream, because after T4 an undeclared one
// carries no concretely-gated row at all.
[[nodiscard]] ComposedSemantics composed_outcome()
{
    return compose(std::array{kOutcomePkg}).for_stream(kSyntheticDialect, {});
}
} // namespace

// ── SP-3 fail-closed, build-time half: duplicate outcome keys are constexpr-detectable ──
static_assert(find_conflict(std::array{kOutcomePkg, kDupPkg}).has_conflict,
              "a cross-package duplicate outcome token under intersecting gates must conflict");
static_assert(find_conflict(std::array{kOutcomePkg, kDupPkg}).kind == "outcome_token",
              "the conflict must be reported as an outcome_token duplicate");
static_assert(!find_conflict(std::array{kOutcomePkg, kOtherGatePkg}).has_conflict,
              "the same token under NON-intersecting gates is two dialects' data, not a conflict");

// ── map_outcome_token: resolved-view lookup, byte-exact; unmapped ≠ mapped-to-Unknown ──
TEST(RunOutcomeMap, DialectGatedExactMatch)
{
    const ComposedSemantics composed{composed_outcome()};
    EXPECT_EQ(map_outcome_token("GOOD", composed), RunOutcome::Success);
    EXPECT_EQ(map_outcome_token("SHAKY", composed), RunOutcome::Unstable);
    EXPECT_EQ(map_outcome_token("STOPPED", composed), RunOutcome::Aborted);
    // A row mapping TO Unknown (the NOT_BUILT shape) still MAPS — engaged optional, value Unknown.
    const auto skipped{map_outcome_token("SKIPPED", composed)};
    ASSERT_TRUE(skipped.has_value()) << "a token mapped to Unknown is a MAPPING, not a miss";
    EXPECT_EQ(*skipped, RunOutcome::Unknown);
    // No row: disengaged.
    EXPECT_FALSE(map_outcome_token("WEIRD", composed).has_value());
    // Byte-exact: case matters (native tokens are verbatim dialect strings).
    EXPECT_FALSE(map_outcome_token("good", composed).has_value());

    // II-6, now STRUCTURAL rather than tested per call: the row is not in another dialect's view,
    // and not in an UNDECLARED stream's view at all. Both are re-derived from the same composition,
    // so this is the filter being exercised, not a second copy of it.
    const ComposedSemantics all{compose(std::array{kOutcomePkg, kOtherGatePkg})};
    EXPECT_FALSE(map_outcome_token("BAD", all.for_stream(kOtherDialect, {})).has_value())
        << "a dialect's verdict token must not resolve on a stream declaring another dialect";
    EXPECT_FALSE(map_outcome_token("BAD", all.for_stream(kUndeclared, {})).has_value())
        << "an UNDECLARED stream withholds every concretely-gated row (fail-closed on depth)";
}

// ── grammar-2 marker shapes: RemainderToClosingParen strictness ──
TEST(RunOutcomeGrammar2, ParenExtractorIsStrict)
{
    const ComposedSemantics composed{composed_outcome()};
    // The named-container form: payload is the paren content, verbatim.
    const auto stage{recognize("[Mark] { (Build)", composed)};
    EXPECT_EQ(stage.kind, IntentMarkerKind::Job);
    EXPECT_EQ(stage.name, "Build");
    // Nested parens stay inside the payload; only the single final ')' delimits.
    const auto nested{recognize("[Mark] { (Branch: test (lts))", composed)};
    EXPECT_EQ(nested.kind, IntentMarkerKind::Job);
    EXPECT_EQ(nested.name, "Branch: test (lts)");
    // No line-final ')' → the paren row does NOT match; the line falls through to the shorter
    // remainder row, whose payload "{ (Build" is excluded by "{" → no marker at all.
    EXPECT_EQ(recognize("[Mark] { (Build", composed).kind,
              IntentMarkerKind::None)
        << "an unterminated paren form must not claim a quantum";
}

TEST(RunOutcomeGrammar2, PayloadExcludesAreWordBounded)
{
    const ComposedSemantics composed{composed_outcome()};
    // An un-named wrapper open/close is scaffold, not a step.
    EXPECT_EQ(recognize("[Mark] {", composed).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize("[Mark] }", composed).kind, IntentMarkerKind::None);
    // A multi-word exclusion entry matches the whole payload.
    EXPECT_EQ(recognize("[Mark] End of Run", composed).kind,
              IntentMarkerKind::None);
    // First-token semantics: an excluded token followed by trailing content still excludes…
    EXPECT_EQ(recognize("[Mark] { retries", composed).kind,
              IntentMarkerKind::None);
    // …but the boundary is a WORD boundary: a verb merely PREFIXED by an entry is a real step.
    const auto step{recognize("[Mark] {}able", composed)};
    EXPECT_EQ(step.kind, IntentMarkerKind::Step)
        << "exclusion must not over-reach past the boundary";
    // The ordinary verb form is a step with the verbatim remainder payload.
    const auto verb{recognize("[Mark] compile", composed)};
    EXPECT_EQ(verb.kind, IntentMarkerKind::Step);
    EXPECT_EQ(verb.name, "compile");
}

// ── scan_run_outcome: last-match-wins, strict word remainder ──
TEST(RunOutcomeScanTest, LastMarkerWins)
{
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> lines{"building things", "Ended: BAD", "retrying the run",
                                         "Ended: GOOD"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    EXPECT_TRUE(scan.marker_present);
    EXPECT_EQ(scan.token, "GOOD") << "a run has ONE terminal verdict — the LAST marker match wins";
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
    EXPECT_FALSE(scan_run_outcome(none, composed).marker_present);
}

TEST(RunOutcomeScanTest, DegenerateCompositionScansNothing)
{
    const ComposedSemantics core{compose({})};
    const std::vector<std::string> lines{"Ended: GOOD"};
    EXPECT_FALSE(scan_run_outcome(lines, core).marker_present);
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

TEST(RunOutcomeResolve, AnUndeclaredStreamCannotResolveASideInput)
{
    // THE FAIL-CLOSED LEG, and after T4 it is about the DECLARATION rather than about detection: an
    // undeclared stream's view carries no concretely-gated outcome row, so the authoritative token
    // cannot resolve however unambiguous it looks. Before T4 the same test asked whether the log's
    // CONTENT had routed to an outcome-bearing format — a per-line detector output deciding what a
    // side-input meant.
    const ComposedSemantics undeclared{compose(std::array{kOutcomePkg}).for_stream(kUndeclared, {})};
    const std::vector<std::string> lines{"working", "Ended: GOOD"};
    const RunOutcomeScan scan{scan_run_outcome(lines, undeclared)};
    EXPECT_FALSE(scan.marker_present)
        << "the console-tail marker row is itself dialect-gated, so it is not in this view either";
    const RunOutcomeResolution res{resolve_run_outcome("GOOD", scan, undeclared)};
    EXPECT_EQ(res.outcome, RunOutcome::Unknown)
        << "a side-input cannot resolve on a stream that declared no dialect (fail-closed on depth)";
    EXPECT_FALSE(res.note.empty()) << "and the refusal is surfaced, never silent";
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
