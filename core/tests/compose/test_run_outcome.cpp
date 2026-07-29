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
using insight::semantic::IntentEmitRow;
using insight::semantic::IntentMarkerRow;
using insight::semantic::OutcomeMarkerRow;
using insight::semantic::OutcomeMarkerShape;
using insight::semantic::OutcomeTokenRow;
using insight::semantic::PayloadEmit;
using insight::semantic::PayloadExtract;
using insight::semantic::render_row;
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

// ── grammar-5 (ADR 0069) — a second synthetic dialect exercising the two shapes GitLab forced:
// a marker payload behind a variable-length numeric field, and terminal lines whose PREFIX carries
// the verdict with a free-form remainder. Both projections are declared so the round trip is
// scored here too, at the level that owns the algorithms. Still no real ecosystem literal: canon
// core stays semantic-unaware. ──
constexpr std::string_view kNumericDialect{"synthetic_numeric"};

constexpr std::array<IntentMarkerRow, 1> kNumericRows{{
    {.prefix = "mark:",
     .kind = IntentMarkerKind::Step,
     .child_order = ChildOrder::Ordered,
     .dialect_gate = kNumericDialect,
     .extract = PayloadExtract::NumericFieldThenRemainder},
}};
constexpr std::array<IntentEmitRow, 1> kNumericEmits{{
    {.prefix = "mark:",
     .kind = IntentMarkerKind::Step,
     .child_order = ChildOrder::Ordered,
     .dialect_gate = kNumericDialect,
     .emit = PayloadEmit::PlaceholderNumericFieldThenPayload},
}};

// Three prefix-verdict rows, two of which NEST — the longest-prefix tie-break is the property, and
// the nesting pair is declared SHORTEST-FIRST on purpose: under the pre-grammar-5 walker the last
// matching row overwrote, so declaration order decided the verdict. Reversing this array must not
// change a single expectation below.
constexpr std::array<OutcomeMarkerRow, 3> kVerdictMarkers{{
    {.prefix = "Run finished",
     .dialect_gate = kNumericDialect,
     .shape = OutcomeMarkerShape::PrefixIsVerdict,
     .outcome = RunOutcome::Success},
    {.prefix = "FATAL: Run broke",
     .dialect_gate = kNumericDialect,
     .shape = OutcomeMarkerShape::PrefixIsVerdict,
     .outcome = RunOutcome::Failure},
    {.prefix = "FATAL: Run broke: stopped",
     .dialect_gate = kNumericDialect,
     .shape = OutcomeMarkerShape::PrefixIsVerdict,
     .outcome = RunOutcome::Aborted},
}};

constexpr SemanticPackageManifest kNumericPkg{.name = "synthetic_numeric",
                                              .version = "1.0.0",
                                              .markers = kNumericRows,
                                              .emits = kNumericEmits,
                                              .outcome_markers = kVerdictMarkers};

[[nodiscard]] ComposedSemantics composed_numeric()
{
    return compose(std::array{kNumericPkg}).for_stream(kNumericDialect, {});
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

// ── grammar-5: NumericFieldThenRemainder — skip a variable-length numeric field ──
TEST(RunOutcomeGrammar5, NumericFieldIsSkippedAndThePayloadIsTheRemainder)
{
    const ComposedSemantics composed{composed_numeric()};
    const auto step{recognize("mark:1784657178:prepare_executor", composed)};
    EXPECT_EQ(step.kind, IntentMarkerKind::Step);
    EXPECT_EQ(step.name, "prepare_executor")
        << "the numeric field must be SKIPPED, not folded into the payload — a per-run epoch inside "
           "the name is the identity storm this extractor exists to prevent";
    // Field WIDTH is unconstrained: a width window would mirror the instrument that measured the
    // corpus, and it is anchoring — not the stamp — that excludes the echoed phantoms.
    EXPECT_EQ(recognize("mark:7:short_field", composed).name, "short_field");
    EXPECT_EQ(recognize("mark:123456789012345678:wide_field", composed).name, "wide_field");
}

TEST(RunOutcomeGrammar5, NumericFieldShapeFailuresDeclineTheRow)
{
    const ComposedSemantics composed{composed_numeric()};
    // The wireshark class: an unexpanded `%s` / `$(date +%s)` where the stamp belongs. DECLINED —
    // structure present, stamp absent — never mis-parsed into a section named after a shell
    // expression.
    EXPECT_EQ(recognize("mark:%s:prepare_executor", composed).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize("mark:$(date +%s):prepare_executor", composed).kind,
              IntentMarkerKind::None);
    // No separator after the digits.
    EXPECT_EQ(recognize("mark:1784657178", composed).kind, IntentMarkerKind::None);
    // No digits at all.
    EXPECT_EQ(recognize("mark::prepare_executor", composed).kind, IntentMarkerKind::None);
    // An empty payload is not a quantum.
    EXPECT_EQ(recognize("mark:1784657178:", composed).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize("mark:1784657178:\r", composed).kind, IntentMarkerKind::None);
}

TEST(RunOutcomeGrammar5, TheCarriageReturnTerminatorAndOptionGroupAreDropped)
{
    const ComposedSemantics composed{composed_numeric()};
    // The CR is the producer's marker TERMINATOR (`\r` + an erase-line escape canon's D-TID-11
    // ingest strip already removed). Left in, it would ride into every payload and into the
    // alignment key.
    EXPECT_EQ(recognize("mark:1784657178:prepare_executor\r", composed).name, "prepare_executor");
    // TERMINATOR, not a trailing byte to trim: the producer may continue the SAME line with a
    // human-readable header after the CR. Trimming instead of terminating names the section
    // `build_tools_section\rTools build` — an alignment key carrying arbitrary prose.
    EXPECT_EQ(recognize("mark:1784657178:build_tools_section\rTools build", composed).name,
              "build_tools_section");
    EXPECT_EQ(recognize("mark:1784657178:log_disk_usage[collapsed=true]\rDisk usage detail",
                        composed)
                  .name,
              "log_disk_usage")
        << "the option group is dropped AFTER the CR terminates the payload, not before";
    // The option group is producer presentation: without the drop, toggling it RENAMES the section.
    EXPECT_EQ(recognize("mark:1784657178:build[collapsed=true]\r", composed).name, "build");
    EXPECT_EQ(recognize("mark:1784657178:build[hide_duration=true,collapsed=true]", composed).name,
              "build");
    // A ']' that closes nothing is content, not a group.
    EXPECT_EQ(recognize("mark:1784657178:weird]", composed).name, "weird]");
    // A group that would consume the WHOLE payload leaves nothing to name → declined.
    EXPECT_EQ(recognize("mark:1784657178:[collapsed=true]", composed).kind, IntentMarkerKind::None);
}

TEST(RunOutcomeGrammar5, TheEmitDualRoundTripsThroughTheExtractor)
{
    const ComposedSemantics composed{composed_numeric()};
    const std::string line{render_row(kNumericEmits[0], "prepare_executor")};
    EXPECT_EQ(line, "mark:0:prepare_executor")
        << "the numeric field is a single PLACEHOLDER digit — a generated marker carries no "
           "wall-clock, and a plausible-looking epoch would hide that";
    const auto back{recognize(line, composed)};
    EXPECT_EQ(back.kind, kNumericEmits[0].kind);
    EXPECT_EQ(back.child_order, kNumericEmits[0].child_order);
    EXPECT_EQ(back.name, "prepare_executor") << "G2: recognize(render_row(row, p)) must recover p";
}

// ── grammar-5: PrefixIsVerdict outcome markers + longest-prefix-wins ──
TEST(RunOutcomeGrammar5, PrefixIsVerdictReadsTheVerdictOffTheRowNotTheRemainder)
{
    const ComposedSemantics composed{composed_numeric()};
    // The free-form remainder is exactly what RemainderToken cannot express, and it is the shape a
    // real terminal failure line has.
    const std::vector<std::string> failed{"building", "FATAL: Run broke: code 1"};
    const RunOutcomeScan scan{scan_run_outcome(failed, composed)};
    ASSERT_TRUE(scan.marker_present);
    ASSERT_TRUE(scan.verdict.has_value()) << "a PrefixIsVerdict row carries its own verdict";
    EXPECT_EQ(*scan.verdict, RunOutcome::Failure);
    EXPECT_TRUE(scan.token.empty()) << "this shape has no remainder token by construction";
    EXPECT_EQ(resolve_run_outcome({}, scan, composed).outcome, RunOutcome::Failure);

    // A bare prefix with NO remainder is still a match (the success form).
    const std::vector<std::string> ok{"Run finished"};
    EXPECT_EQ(resolve_run_outcome({}, scan_run_outcome(ok, composed), composed).outcome,
              RunOutcome::Success);
}

// ── The `\r`-anchored outcome line ──────────────────────────────────────────────────────────────
// Runners predating GitLab 18.9 frame the epilogue with a BARE `\r`, so a `\n`-split line vector
// carries the terminal verdict MID-ELEMENT. An at-offset-0 test cannot see it, and the miss is
// silent — the trace reads as having no verdict rather than raising anything. These arms are
// two-sided: the first was RED before the anchor was widened, the last two are what keeps the
// widening from being a blanket "search anywhere in the line".

TEST(RunOutcomeGrammar5, AVerdictFramedByABareCarriageReturnIsAnchored)
{
    const ComposedSemantics composed{composed_numeric()};
    // One `\n`-line, exactly as a byte-faithful splitter hands it over: the verdict is not at
    // offset 0, it is behind a lone `\r`. CRLF would MASK this — the `\r` here is deliberately bare.
    const std::vector<std::string> old_leg{"$ run tests\rFATAL: Run broke: code 1"};
    const RunOutcomeScan scan{scan_run_outcome(old_leg, composed)};
    ASSERT_TRUE(scan.marker_present)
        << "a verdict behind a lone \\r was not anchored; line=" << old_leg[0];
    ASSERT_TRUE(scan.verdict.has_value());
    EXPECT_EQ(*scan.verdict, RunOutcome::Failure);
    EXPECT_EQ(resolve_run_outcome({}, scan, composed).outcome, RunOutcome::Failure);

    // The longest-prefix rule composes with the new anchor rather than being bypassed by it.
    const std::vector<std::string> aborted{"cleanup\rFATAL: Run broke: stopped"};
    EXPECT_EQ(resolve_run_outcome({}, scan_run_outcome(aborted, composed), composed).outcome,
              RunOutcome::Aborted);

    // CRLF is the masking case: a `\n`-splitter has already cut there, leaving the `\r` trailing on
    // the previous element. That empty trailing segment must anchor nothing, and the verdict must
    // resolve exactly once off the next element.
    const std::vector<std::string> crlf{"$ run tests\r", "Run finished"};
    EXPECT_EQ(resolve_run_outcome({}, scan_run_outcome(crlf, composed), composed).outcome,
              RunOutcome::Success);
}

TEST(RunOutcomeGrammar5, TheCarriageReturnAnchorDoesNotBecomeASubstringSearch)
{
    const ComposedSemantics composed{composed_numeric()};
    // The widening admits an anchor after `\r` and NOWHERE else. A prefix sitting mid-line with no
    // delimiter in front of it is prose and must stay unmatched — otherwise the fix would trade a
    // silent miss for a silent false verdict, which is the worse direction.
    const std::vector<std::string> prose{"see also FATAL: Run broke: code 1 in the docs"};
    EXPECT_FALSE(scan_run_outcome(prose, composed).marker_present)
        << "a mid-line prefix with no \\r before it was matched; the anchor became a substring "
           "search. line="
        << prose[0];

    // The anchor is a POSITION, not a second recognizer: whatever a segment resolves to after a
    // `\r` is exactly what those same bytes resolve to as a line of their own. Asserting the
    // equivalence rather than a hand-written verdict is what keeps this arm honest — it cannot
    // drift from the line-start behaviour it is supposed to mirror, including the prefix peel
    // (timestamp / ANSI / leading space) that `parse_line` applies identically in both positions.
    for (const std::string_view segment : {"FATAL: Run broke: code 1", " FATAL: Run broke",
                                           "Run finished", "not a verdict at all", ""})
    {
        const std::vector<std::string> alone{std::string{segment}};
        const std::vector<std::string> anchored{"building\r" + std::string{segment}};
        const RunOutcomeScan at_line_start{scan_run_outcome(alone, composed)};
        const RunOutcomeScan after_carriage_return{scan_run_outcome(anchored, composed)};
        EXPECT_EQ(at_line_start.marker_present, after_carriage_return.marker_present)
            << "segment '" << segment << "' resolves differently after a \\r ("
            << after_carriage_return.marker_present << ") than at line start ("
            << at_line_start.marker_present << ")";
        EXPECT_EQ(at_line_start.verdict.has_value(), after_carriage_return.verdict.has_value())
            << "segment '" << segment << "'";
        if (at_line_start.verdict.has_value() && after_carriage_return.verdict.has_value())
            EXPECT_EQ(*at_line_start.verdict, *after_carriage_return.verdict)
                << "segment '" << segment << "'";
    }
}

TEST(RunOutcomeGrammar5, TheCarriageReturnAnchorDoesNotFoldTheByteItAnchorsAfter)
{
    const ComposedSemantics composed{composed_numeric()};
    // The `\r` is CONTENT, and anchoring after it must not consume, strip or fold it. The marker
    // payload extractor treats the same byte as its TERMINATOR, so if the scan had rewritten the
    // line the recognizer sharing these bytes would see a different string.
    constexpr std::string_view kSectionThenWarning{
        "mark:1784657178:after_script\rWARNING: after_script failed, but job will continue"};
    EXPECT_EQ(recognize(kSectionThenWarning, composed).name, "after_script")
        << "the \\r-anchored scan altered bytes the marker extractor depends on";

    // And that same line carries no verdict: `WARNING: …` is not one of the declared prefixes.
    // Fusing it with what precedes the `\r` is what would re-manufacture the false positive.
    const std::vector<std::string> warning{std::string{kSectionThenWarning}};
    EXPECT_FALSE(scan_run_outcome(warning, composed).marker_present)
        << "an after_script WARNING was read as a terminal verdict";
}

TEST(RunOutcomeGrammar5, LongestPrefixWinsRegardlessOfDeclarationOrder)
{
    const ComposedSemantics composed{composed_numeric()};
    // `FATAL: Run broke: stopped` is a strict EXTENSION of `FATAL: Run broke`, and it is declared
    // AFTER it — so under the pre-grammar-5 "last row that matched overwrites" walk the answer was a
    // function of where the rows sit in an array. It must now be a function of the bytes.
    const std::vector<std::string> stopped{"FATAL: Run broke: stopped"};
    const RunOutcomeScan scan{scan_run_outcome(stopped, composed)};
    ASSERT_TRUE(scan.verdict.has_value());
    EXPECT_EQ(*scan.verdict, RunOutcome::Aborted)
        << "the LONGER prefix must win: a cancellation announced with the failure prefix is a WRONG "
           "verdict, not a missing one";
    // And the shorter row still claims everything the longer one does not.
    const std::vector<std::string> plain{"FATAL: Run broke: code 137"};
    EXPECT_EQ(*scan_run_outcome(plain, composed).verdict, RunOutcome::Failure);
}

TEST(RunOutcomeGrammar5, LastVerdictLineStillWinsAcrossLines)
{
    const ComposedSemantics composed{composed_numeric()};
    const std::vector<std::string> lines{"FATAL: Run broke: code 1", "retrying", "Run finished"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    ASSERT_TRUE(scan.verdict.has_value());
    EXPECT_EQ(*scan.verdict, RunOutcome::Success) << "a run has ONE terminal verdict — the LAST one";
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
