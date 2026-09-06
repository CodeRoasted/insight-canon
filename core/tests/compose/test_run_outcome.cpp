// invariant: the run-outcome MECHANISMS over SYNTHETIC manifests — canon core stays
// semantic-unaware, so no real dialect literal appears here.
// invariant: the real vocabularies are package DATA and are tested in their own packages.
// invariant: token mapping is a resolved-view lookup, byte-exact, where NO ROW is distinct from a
// row that MAPS to Unknown.
// invariant: the marker grammar's paren strictness and the payload exclusion set's word-boundary
// semantics are core's.
// invariant: the outcome scan is LAST-MATCH-WINS with a strict verdict-word remainder, and carries
// no format at all because the dialect is DECLARED.
// invariant: the resolution ladder is STRICT — an authoritative verdict wins over a
// present-but-divergent console tail, and the divergence is FLAGGED rather than used as a tiebreak.
// invariant: an unmapped token surfaces a note and falls down the ladder, which is fail-closed, and
// absence resolves Unknown.
// invariant: a cross-package duplicate outcome token or marker prefix FAILS THE BUILD.
// invariant: regression is strictly-worse on the pass-to-fail axis only, with the aborted and
// unknown classes excluded.
// invariant: determinism — byte-only walks and an integer line index, with no RNG, clock or
// float.
// refs: SRC-D-OUT-RUN-1
#include <gtest/gtest.h>

import insight.canon.test;

using insight::map_outcome_token;
using insight::outcome_regressed;
using insight::resolve_run_outcome;
using insight::RunOutcome;
using insight::RunOutcomeResolution;
using insight::RunOutcomeScan;
using insight::scan_run_outcome;
using insight::SideInputVerdict;
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

[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

namespace
{
// invariant: a synthetic outcome-bearing dialect gated on its OWN package name, so the gate is a
// composed package NAME and core stays free of any real ecosystem literal.
// invariant: it mirrors a real dialect's SHAPE — four verdict classes, one token mapping TO
// Unknown, and a console-tail marker.
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

// invariant: a paren-delimited container row plus a remainder row with an exclusion set, expressing
// a real dialect's two marker forms synthetically.
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

// invariant: a second package duplicating a token under an INTERSECTING gate — the conflict
// fixture.
constexpr std::array<OutcomeTokenRow, 1> kDupToken{
    {{.token = "GOOD", .outcome = RunOutcome::Failure, .dialect_gate = kSyntheticDialect}}};
constexpr SemanticPackageManifest kDupPkg{
    .name = "synthetic_dup", .version = "1.0.0", .outcome_tokens = kDupToken};
// invariant: the same token under a NON-intersecting gate is NOT a duplicate — a different
// dialect naming the same string is legal.
// invariant: the side input resolves under the ONE dialect the stream declared.
constexpr std::array<OutcomeTokenRow, 1> kOtherGateToken{
    {{.token = "GOOD", .outcome = RunOutcome::Success, .dialect_gate = kOtherDialect}}};
constexpr SemanticPackageManifest kOtherGatePkg{
    .name = "synthetic_other", .version = "1.0.0", .outcome_tokens = kOtherGateToken};

// invariant: the resolved view of a stream that DECLARED this dialect, and the stream door is the
// ONE door.
// invariant: everything below scores against a declared stream, because an undeclared one carries
// no concretely-gated row at all.
[[nodiscard]] ComposedSemantics composed_outcome()
{
    return compose(std::array{kOutcomePkg}).for_stream(kSyntheticDialect, {});
}

// invariant: a second synthetic dialect exercising the two shapes a real dialect forced.
// invariant: a marker payload behind a variable-length numeric field, and terminal lines whose
// PREFIX carries the verdict with a free-form remainder.
// invariant: both projections are declared, so the round trip is scored at the level that owns the
// algorithms, still with no real ecosystem literal.
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

// invariant: three prefix-verdict rows, two of which NEST — the longest-prefix tie-break is the
// property.
// invariant: the nesting pair is declared SHORTEST-FIRST on purpose, because under the older walker
// the last matching row overwrote and declaration order decided the verdict.
// invariant: reversing this array must not change a single expectation below — an OBLIGATION on
// whoever edits it, because nothing here re-runs the suite against a reversed array.
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

static_assert(find_conflict(std::array{kOutcomePkg, kDupPkg}).has_conflict,
              "a cross-package duplicate outcome token under intersecting gates must conflict");
static_assert(find_conflict(std::array{kOutcomePkg, kDupPkg}).kind == "outcome_token",
              "the conflict must be reported as an outcome_token duplicate");
static_assert(!find_conflict(std::array{kOutcomePkg, kOtherGatePkg}).has_conflict,
              "the same token under NON-intersecting gates is two dialects' data, not a conflict");

TEST(RunOutcomeMap, DialectGatedExactMatch)
{
    const ComposedSemantics composed{composed_outcome()};
    EXPECT_EQ(map_outcome_token("GOOD", composed), RunOutcome::Success);
    EXPECT_EQ(map_outcome_token("SHAKY", composed), RunOutcome::Unstable);
    EXPECT_EQ(map_outcome_token("STOPPED", composed), RunOutcome::Aborted);
    // invariant: a row mapping TO Unknown still MAPS — an engaged optional whose value is
    // Unknown, which is not the same as a miss.
    const auto skipped{map_outcome_token("SKIPPED", composed)};
    ASSERT_TRUE(skipped.has_value()) << "a token mapped to Unknown is a MAPPING, not a miss";
    EXPECT_EQ(*skipped, RunOutcome::Unknown);
    EXPECT_FALSE(map_outcome_token("WEIRD", composed).has_value());
    // invariant: BYTE-EXACT, so case matters, because native tokens are verbatim dialect strings.
    EXPECT_FALSE(map_outcome_token("good", composed).has_value());

    // invariant: STRUCTURAL rather than tested per call — the row is not in another dialect's
    // view, and not in an UNDECLARED stream's view at all.
    // invariant: both are re-derived from the SAME composition, so this exercises the filter rather
    // than a second copy of it.
    // refs: SRC-II-6
    const ComposedSemantics all{compose(std::array{kOutcomePkg, kOtherGatePkg})};
    EXPECT_FALSE(map_outcome_token("BAD", all.for_stream(kOtherDialect, {})).has_value())
        << "a dialect's verdict token must not resolve on a stream declaring another dialect";
    EXPECT_FALSE(map_outcome_token("BAD", all.for_stream(kUndeclared, {})).has_value())
        << "an UNDECLARED stream withholds every concretely-gated row (fail-closed on depth)";
}

TEST(RunOutcomeGrammar2, ParenExtractorIsStrict)
{
    const ComposedSemantics composed{composed_outcome()};
    // invariant: the named-container form takes the paren content VERBATIM as its payload.
    const auto stage{recognize(norm_probe("[Mark] { (Build)"), composed)};
    EXPECT_EQ(stage.kind, IntentMarkerKind::Job);
    EXPECT_EQ(stage.name, "Build");
    // invariant: nested parens stay INSIDE the payload — only the single final closing paren
    // delimits.
    const auto nested{recognize(norm_probe("[Mark] { (Branch: test (lts))"), composed)};
    EXPECT_EQ(nested.kind, IntentMarkerKind::Job);
    EXPECT_EQ(nested.name, "Branch: test (lts)");
    // invariant: with no line-final closing paren the paren row does NOT match, and the line falls
    // through to the shorter remainder row.
    // invariant: that row's payload is excluded by the exclusion set, so the line claims no marker
    // at all.
    EXPECT_EQ(recognize(norm_probe("[Mark] { (Build"), composed).kind, IntentMarkerKind::None)
        << "an unterminated paren form must not claim a quantum";
}

TEST(RunOutcomeGrammar2, PayloadExcludesAreWordBounded)
{
    const ComposedSemantics composed{composed_outcome()};
    // invariant: an UN-NAMED wrapper open or close is scaffold, not a step.
    EXPECT_EQ(recognize(norm_probe("[Mark] {"), composed).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize(norm_probe("[Mark] }"), composed).kind, IntentMarkerKind::None);
    // invariant: a multi-word exclusion entry matches the WHOLE payload.
    EXPECT_EQ(recognize(norm_probe("[Mark] End of Run"), composed).kind, IntentMarkerKind::None);
    // invariant: FIRST-TOKEN semantics — an excluded token followed by trailing content still
    // excludes.
    EXPECT_EQ(recognize(norm_probe("[Mark] { retries"), composed).kind, IntentMarkerKind::None);
    // invariant: the boundary is nonetheless a WORD boundary, so a verb merely PREFIXED by an
    // exclusion entry is a real step.
    const auto step{recognize(norm_probe("[Mark] {}able"), composed)};
    EXPECT_EQ(step.kind, IntentMarkerKind::Step)
        << "exclusion must not over-reach past the boundary";
    const auto verb{recognize(norm_probe("[Mark] compile"), composed)};
    EXPECT_EQ(verb.kind, IntentMarkerKind::Step);
    EXPECT_EQ(verb.name, "compile");
}

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
        << "a decorated epilogue is not a terminal-verdict line: the verdict word must be the "
           "WHOLE remainder (^Finished: (\\w+)$), so a trailing '(took 3s)' disqualifies it";
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

TEST(RunOutcomeGrammar5, NumericFieldIsSkippedAndThePayloadIsTheRemainder)
{
    const ComposedSemantics composed{composed_numeric()};
    const auto step{recognize(norm_probe("mark:1784657178:prepare_executor"), composed)};
    EXPECT_EQ(step.kind, IntentMarkerKind::Step);
    EXPECT_EQ(step.name, "prepare_executor")
        << "the numeric field must be SKIPPED, not folded into the payload — a per-run epoch "
           "inside "
           "the name is the identity storm this extractor exists to prevent";
    // invariant: field WIDTH is unconstrained — a width window would mirror the instrument that
    // measured the corpus.
    // invariant: it is ANCHORING, not the stamp, that excludes the echoed phantoms.
    EXPECT_EQ(recognize(norm_probe("mark:7:short_field"), composed).name, "short_field");
    EXPECT_EQ(recognize(norm_probe("mark:123456789012345678:wide_field"), composed).name,
              "wide_field");
}

TEST(RunOutcomeGrammar5, NumericFieldShapeFailuresDeclineTheRow)
{
    const ComposedSemantics composed{composed_numeric()};
    // invariant: an unexpanded shell expression where the stamp belongs is DECLINED — the
    // structure is present and the stamp is absent.
    // invariant: it is never mis-parsed into a section named after a shell expression.
    EXPECT_EQ(recognize(norm_probe("mark:%s:prepare_executor"), composed).kind,
              IntentMarkerKind::None);
    EXPECT_EQ(recognize(norm_probe("mark:$(date +%s):prepare_executor"), composed).kind,
              IntentMarkerKind::None);
    EXPECT_EQ(recognize(norm_probe("mark:1784657178"), composed).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize(norm_probe("mark::prepare_executor"), composed).kind,
              IntentMarkerKind::None);
    // invariant: an EMPTY payload is not a quantum.
    EXPECT_EQ(recognize(norm_probe("mark:1784657178:"), composed).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize(norm_probe("mark:1784657178:\r"), composed).kind, IntentMarkerKind::None);
}

TEST(RunOutcomeGrammar5, TheCarriageReturnTerminatorAndOptionGroupAreDropped)
{
    const ComposedSemantics composed{composed_numeric()};
    // invariant: the carriage return is the producer's marker TERMINATOR, paired with an erase-line
    // escape that canon's ingest strip has already removed.
    // invariant: left in, it would ride into every payload and into the alignment key.
    // refs: SRC-D-TID-11
    EXPECT_EQ(recognize(norm_probe("mark:1784657178:prepare_executor\r"), composed).name,
              "prepare_executor");
    // invariant: a TERMINATOR and not a trailing byte to trim — the producer may continue the
    // SAME line with a human-readable header after it.
    // invariant: trimming instead of terminating names the section after arbitrary prose, which
    // makes an alignment key out of it.
    EXPECT_EQ(
        recognize(norm_probe("mark:1784657178:build_tools_section\rTools build"), composed).name,
        "build_tools_section");
    EXPECT_EQ(
        recognize(norm_probe("mark:1784657178:log_disk_usage[collapsed=true]\rDisk usage detail"),
                  composed)
            .name,
        "log_disk_usage")
        << "the option group is dropped AFTER the CR terminates the payload, not before";
    // invariant: the option group is producer PRESENTATION, so without the drop, toggling it
    // RENAMES the section.
    EXPECT_EQ(recognize(norm_probe("mark:1784657178:build[collapsed=true]\r"), composed).name,
              "build");
    EXPECT_EQ(
        recognize(norm_probe("mark:1784657178:build[hide_duration=true,collapsed=true]"), composed)
            .name,
        "build");
    // invariant: a closing bracket that closes nothing is CONTENT, not a group.
    EXPECT_EQ(recognize(norm_probe("mark:1784657178:weird]"), composed).name, "weird]");
    // invariant: a group that would consume the WHOLE payload leaves nothing to name, so it is
    // declined.
    EXPECT_EQ(recognize(norm_probe("mark:1784657178:[collapsed=true]"), composed).kind,
              IntentMarkerKind::None);
}

TEST(RunOutcomeGrammar5, TheEmitDualRoundTripsThroughTheExtractor)
{
    const ComposedSemantics composed{composed_numeric()};
    const std::string line{render_row(kNumericEmits[0], "prepare_executor")};
    EXPECT_EQ(line, "mark:0:prepare_executor")
        << "the numeric field is a single PLACEHOLDER digit — a generated marker carries no "
           "wall-clock, and a plausible-looking epoch would hide that";
    const auto back{recognize(norm_probe(line), composed)};
    EXPECT_EQ(back.kind, kNumericEmits[0].kind);
    EXPECT_EQ(back.child_order, kNumericEmits[0].child_order);
    EXPECT_EQ(back.name, "prepare_executor") << "G2: recognize(render_row(row, p)) must recover p";
}

TEST(RunOutcomeGrammar5, PrefixIsVerdictReadsTheVerdictOffTheRowNotTheRemainder)
{
    const ComposedSemantics composed{composed_numeric()};
    // invariant: the free-form remainder is exactly what a remainder-token row cannot express, and
    // it is the shape a real terminal failure line has.
    const std::vector<std::string> failed{"building", "FATAL: Run broke: code 1"};
    const RunOutcomeScan scan{scan_run_outcome(failed, composed)};
    ASSERT_TRUE(scan.marker_present);
    ASSERT_TRUE(scan.verdict.has_value()) << "a PrefixIsVerdict row carries its own verdict";
    EXPECT_EQ(*scan.verdict, RunOutcome::Failure);
    EXPECT_TRUE(scan.token.empty()) << "this shape has no remainder token by construction";
    EXPECT_EQ(resolve_run_outcome({}, scan, composed, composed).outcome, RunOutcome::Failure);

    // invariant: a bare prefix with NO remainder is still a match, which is the success form.
    const std::vector<std::string> ok{"Run finished"};
    EXPECT_EQ(resolve_run_outcome({}, scan_run_outcome(ok, composed), composed, composed).outcome,
              RunOutcome::Success);
}

// invariant: older runners frame the epilogue with a BARE carriage return, so a newline-split line
// vector carries the terminal verdict MID-ELEMENT.
// invariant: an at-offset-zero test cannot see it, and the miss is SILENT — the trace reads as
// having no verdict rather than raising anything.
// invariant: the treatment is two-sided — the arm below was RED before the anchor was widened.
// invariant: what keeps the widening from becoming a blanket search-anywhere lives in the SIBLING
// arm that follows this one, not in this test.
TEST(RunOutcomeGrammar5, AVerdictFramedByABareCarriageReturnIsAnchored)
{
    const ComposedSemantics composed{composed_numeric()};
    // invariant: one newline-split line exactly as a byte-faithful splitter hands it over — the
    // verdict is not at offset 0, it is behind a LONE carriage return.
    // invariant: a CRLF pair would MASK this, so the byte here is deliberately bare.
    const std::vector<std::string> old_leg{"$ run tests\rFATAL: Run broke: code 1"};
    const RunOutcomeScan scan{scan_run_outcome(old_leg, composed)};
    ASSERT_TRUE(scan.marker_present)
        << "a verdict behind a lone \\r was not anchored; line=" << old_leg[0];
    ASSERT_TRUE(scan.verdict.has_value());
    EXPECT_EQ(*scan.verdict, RunOutcome::Failure);
    EXPECT_EQ(resolve_run_outcome({}, scan, composed, composed).outcome, RunOutcome::Failure);

    // invariant: the longest-prefix rule COMPOSES with the new anchor rather than being bypassed by
    // it.
    const std::vector<std::string> aborted{"cleanup\rFATAL: Run broke: stopped"};
    EXPECT_EQ(
        resolve_run_outcome({}, scan_run_outcome(aborted, composed), composed, composed).outcome,
        RunOutcome::Aborted);

    // invariant: CRLF is the masking case — a newline splitter has already cut there, leaving the
    // carriage return trailing on the previous element.
    // invariant: that empty trailing segment must anchor NOTHING, and the verdict must resolve
    // exactly once off the next element.
    const std::vector<std::string> crlf{"$ run tests\r", "Run finished"};
    EXPECT_EQ(resolve_run_outcome({}, scan_run_outcome(crlf, composed), composed, composed).outcome,
              RunOutcome::Success);
}

TEST(RunOutcomeGrammar5, TheCarriageReturnAnchorDoesNotBecomeASubstringSearch)
{
    const ComposedSemantics composed{composed_numeric()};
    // invariant: the widening admits an anchor after a carriage return and NOWHERE else.
    // invariant: a prefix sitting mid-line with no delimiter in front of it is PROSE and must stay
    // unmatched, or the fix trades a silent miss for a silent false verdict, which is worse.
    const std::vector<std::string> prose{"see also FATAL: Run broke: code 1 in the docs"};
    EXPECT_FALSE(scan_run_outcome(prose, composed).marker_present)
        << "a mid-line prefix with no \\r before it was matched; the anchor became a substring "
           "search. line="
        << prose[0];

    // invariant: the anchor is a POSITION, not a second recognizer — whatever a segment resolves
    // to after a carriage return is what those same bytes resolve to as a line of their own.
    // invariant: asserting the EQUIVALENCE rather than a hand-written verdict is what keeps this
    // arm honest, because it cannot drift from the line-start behaviour it mirrors.
    // invariant: that includes the prefix peel the line parser applies identically in both
    // positions.
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
    // invariant: the carriage return is CONTENT, and anchoring after it must not consume, strip or
    // fold it.
    // invariant: the marker payload extractor treats the SAME byte as its TERMINATOR, so a scan
    // that rewrote the line would hand the recognizer sharing these bytes a different string.
    constexpr std::string_view kSectionThenWarning{
        "mark:1784657178:after_script\rWARNING: after_script failed, but job will continue"};
    EXPECT_EQ(recognize(norm_probe(kSectionThenWarning), composed).name, "after_script")
        << "the \\r-anchored scan altered bytes the marker extractor depends on";

    // invariant: that same line carries NO verdict, and fusing it with what precedes the carriage
    // return is what would re-manufacture the false positive.
    const std::vector<std::string> warning{std::string{kSectionThenWarning}};
    EXPECT_FALSE(scan_run_outcome(warning, composed).marker_present)
        << "an after_script WARNING was read as a terminal verdict";
}

TEST(RunOutcomeGrammar5, LongestPrefixWinsRegardlessOfDeclarationOrder)
{
    const ComposedSemantics composed{composed_numeric()};
    // invariant: the longer prefix is a strict EXTENSION of the shorter and is declared AFTER it,
    // so under the older last-row-overwrites walk the answer was a function of array position.
    // invariant: it must now be a function of the BYTES.
    const std::vector<std::string> stopped{"FATAL: Run broke: stopped"};
    const RunOutcomeScan scan{scan_run_outcome(stopped, composed)};
    ASSERT_TRUE(scan.verdict.has_value());
    EXPECT_EQ(*scan.verdict, RunOutcome::Aborted) << "the LONGER prefix must win: a cancellation "
                                                     "announced with the failure prefix is a WRONG "
                                                     "verdict, not a missing one";
    // invariant: the shorter row still claims everything the longer one does not.
    const std::vector<std::string> plain{"FATAL: Run broke: code 137"};
    EXPECT_EQ(*scan_run_outcome(plain, composed).verdict, RunOutcome::Failure);
}

TEST(RunOutcomeGrammar5, LastVerdictLineStillWinsAcrossLines)
{
    const ComposedSemantics composed{composed_numeric()};
    const std::vector<std::string> lines{"FATAL: Run broke: code 1", "retrying", "Run finished"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    ASSERT_TRUE(scan.verdict.has_value());
    EXPECT_EQ(*scan.verdict, RunOutcome::Success)
        << "a run has ONE terminal verdict — the LAST one";
}

TEST(RunOutcomeResolve, AuthoritativeWinsOverPresentDivergentConsole)
{
    // invariant: the shape of a real reported case — an authoritative Success against a present
    // console tail saying Aborted.
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> lines{"working", "Ended: STOPPED"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const RunOutcomeResolution res{resolve_run_outcome(
        {.token = "GOOD", .vocabulary = kOutcomePkg.name}, scan, composed, composed)};
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
    // invariant: with no side input the ladder falls to its second rung, where the console tail
    // recovers the verdict and an unstable result stays unstable.
    const RunOutcomeResolution res{resolve_run_outcome({.token = ""}, scan, composed, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Unstable);
    EXPECT_FALSE(res.authoritative);
    EXPECT_FALSE(res.divergent);
}

TEST(RunOutcomeResolve, UnmappedSideInputSurfacesANoteAndFallsThrough)
{
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> lines{"working", "Ended: BAD"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const RunOutcomeResolution res{resolve_run_outcome(
        {.token = "WEIRD", .vocabulary = kOutcomePkg.name}, scan, composed, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Failure) << "the ladder continues past an unmapped rung 1";
    EXPECT_FALSE(res.authoritative);
    EXPECT_FALSE(res.note.empty()) << "an unmapped token is NEVER silent (fail-closed)";
    EXPECT_NE(res.note.find("WEIRD"), std::string::npos) << "the note names the offending token";
}

// invariant: a structurally incomplete declaration is a WIRING error and TERMINATES.
// invariant: the three neighbouring cases have different right answers, and collapsing them is what
// shipped the defect.
// invariant: ABSENT — no token and no vocabulary — is a CHOICE: it degrades to Unknown with no
// note.
// invariant: HALF — a token with no vocabulary — is a WIRING MISTAKE and is fatal here.
// invariant: UNMAPPED — a token named against a vocabulary it is not in — is a VALUE error: a
// non-fatal note, and the ladder continues.
// invariant: the half-pair used to return nothing BY DESIGN, and a downstream crawl then declared
// exactly that shape on every pair it ever produced.
// invariant: 63 identical-commit pairs, ground-truth silence, 60 critical or high regression rows,
// and the rule that should have bounded them never ran once.
// invariant: A DECLARATION THAT RESOLVES NOTHING WHILE LOOKING LIKE A DECLARATION IS THE FAILURE
// MODE WITH NO OBSERVABLE.
// invariant: THE ARM ASSERTS BOTH THE MESSAGE AND THE DEATH, and that is not belt-and-braces.
// invariant: the measured mutation is exact — degrade the fatal to a plain return and the
// diagnostic line above it SURVIVES, so the correct message prints and the process lives.
// invariant: the harness then reports a failure to die while echoing the full text, so IT SAID THE
// RIGHT THING IS SATISFIABLE WITHOUT DYING.
// invariant: print-then-continue is the single most likely accidental refactor of a fatal, because
// the message and the terminate are two statements and only one looks load-bearing.
// refs: DN-32.D7
TEST(RunOutcomeResolveDeathTest, AHalfDeclaredVerdictTerminatesNamingTheComposition)
{
    const ComposedSemantics vocabularies{compose(std::array{kOutcomePkg})};
    const ComposedSemantics composed{vocabularies.for_stream(kSyntheticDialect, {})};
    const std::vector<std::string> lines{"working", "Ended: GOOD"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    // invariant: a token that WOULD resolve if its vocabulary were named, so the death is caused by
    // the missing half and by nothing else about the token.
    const SideInputVerdict half{.token = "GOOD", .vocabulary = {}};

    // invariant: the message must NAME the composed packages — a fail-closed error the operator
    // cannot act on is only half the posture.
    EXPECT_DEATH(
        { (void)resolve_run_outcome(half, scan, composed, vocabularies); },
        "declared with NO outcome vocabulary")
        << "a token declared without its vocabulary resolved something instead of terminating. It "
           "is half a declaration, not a weak one — and the silent nullopt it used to return is "
           "how 63 crawl pairs went out with every verdict-reading rule disarmed.";

    EXPECT_DEATH(
        { (void)resolve_run_outcome(half, scan, composed, vocabularies); }, "synthetic_outcome")
        << "the fatal does not list the vocabularies the caller could have named";
    EXPECT_DEATH(
        { (void)resolve_run_outcome(half, scan, composed, vocabularies); }, "--outcome-vocabulary")
        << "the fatal does not carry the remedy. This sentence used to be a resolution NOTE, "
           "printed after the damage; it belongs at the one moment it can still be acted on.";
}

// invariant: the ABSENT third state, asserted right beside its fatal sibling so the two cannot be
// conflated by anyone reading either one.
// invariant: declaring NOTHING is not a degenerate half-pair.
TEST(RunOutcomeResolve, AnAbsentDeclarationDegradesWhereAHalfOneWouldTerminate)
{
    // invariant: THIS ARM'S CLAIM WAS INVERTED, and the withdrawn version is recorded rather than
    // deleted because it was RIGHT about its mechanism and WRONG about its subject.
    // invariant: it asserted that an undeclared stream cannot resolve a side input, since such a
    // view carries no concretely-gated outcome row.
    // invariant: that is true of the CODE, and it described the defect rather than the contract.
    // invariant: the ruling is that a side input is interpreted by whoever SUPPLIED it, never by
    // the dialect of whoever wrote the bytes, and this is the case that ruling exists for.
    // invariant: a crawler holding a platform's own verdict for a raw build log is not missing a
    // dialect; it is holding a verdict from a declarer the stream never mentions.
    // invariant: what survives unchanged is the STREAM half — the console-tail marker row IS
    // dialect-gated, it is absent from this view, and it must stay absent.
    // invariant: the scan is canon reading the BYTES, and nothing about the side-input rule may
    // reach it.
    // refs: DN-32.D6
    const ComposedSemantics vocabularies{compose(std::array{kOutcomePkg})};
    const ComposedSemantics composed{vocabularies.for_stream(kSyntheticDialect, {})};
    const std::vector<std::string> lines{"working", "no epilogue here"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};

    const RunOutcomeResolution res{
        resolve_run_outcome({.token = {}, .vocabulary = {}}, scan, composed, vocabularies)};
    EXPECT_EQ(res.outcome, RunOutcome::Unknown);
    EXPECT_FALSE(res.authoritative);
    EXPECT_TRUE(res.note.empty())
        << "declaring NOTHING is a choice and must stay silent. A note here would make absence "
           "indistinguishable from a mistake, and ADR-22.D5 forbids the two sharing a path: "
        << res.note;
}

TEST(RunOutcomeResolve, ACompletePairResolvesOnAStreamThatDeclaredNoDialect)
{
    const ComposedSemantics vocabularies{compose(std::array{kOutcomePkg})};
    const ComposedSemantics undeclared{vocabularies.for_stream(kUndeclared, {})};
    const std::vector<std::string> lines{"working", "Ended: GOOD"};
    const RunOutcomeScan scan{scan_run_outcome(lines, undeclared)};
    EXPECT_FALSE(scan.marker_present)
        << "the console-tail marker row is itself dialect-gated, so it is not in this view either";

    const RunOutcomeResolution res{resolve_run_outcome(
        {.token = "GOOD", .vocabulary = kOutcomePkg.name}, scan, undeclared, vocabularies)};
    EXPECT_EQ(res.outcome, RunOutcome::Success)
        << "a COMPLETE pair must resolve on a stream that declared no dialect — that is the whole "
           "point of naming the vocabulary, and the case the crawler needed and never had";
    EXPECT_TRUE(res.authoritative) << "it is rung 1, not a fallback";
    EXPECT_TRUE(res.note.empty()) << "nothing failed, so nothing is surfaced: " << res.note;
}

TEST(RunOutcomeResolve, AbsenceIsUnknownWithoutFraming)
{
    // invariant: the mapped-to-Unknown shape.
    // invariant: the token MAPS and its value is Unknown, so the first rung resolves with no
    // fallback to a stale console tail and no note, because nothing failed.
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> lines{"no epilogue here"};
    const RunOutcomeResolution res{
        resolve_run_outcome({.token = ""}, scan_run_outcome(lines, composed), composed, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Unknown);
    EXPECT_TRUE(res.note.empty()) << "absence is the legacy default, not an error";
}

TEST(RunOutcomeResolve, MappedToUnknownIsAuthoritative)
{
    const ComposedSemantics composed{composed_outcome()};
    const std::vector<std::string> lines{"Ended: GOOD"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const RunOutcomeResolution res{resolve_run_outcome(
        {.token = "SKIPPED", .vocabulary = kOutcomePkg.name}, scan, composed, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Unknown);
    EXPECT_TRUE(res.authoritative) << "mapped-to-Unknown is a RESOLUTION, not a miss";
    EXPECT_TRUE(res.divergent) << "the mapped console tail disagrees — flagged, not consulted";
    EXPECT_TRUE(res.note.empty());
}

TEST(OutcomeRegressed, StrictlyWorseOnTheAxisOnly)
{
    // invariant: strictly worse on the axis.
    EXPECT_TRUE(outcome_regressed(RunOutcome::Success, RunOutcome::Failure));
    EXPECT_TRUE(outcome_regressed(RunOutcome::Success, RunOutcome::Unstable));
    EXPECT_TRUE(outcome_regressed(RunOutcome::Unstable, RunOutcome::Failure));
    // invariant: steady or better is NOT a regression.
    EXPECT_FALSE(outcome_regressed(RunOutcome::Unstable, RunOutcome::Unstable))
        << "steady-flaky is NOT a verdict regression: the axis is pass↔fail, and Unstable→Unstable "
           "did not move along it";
    EXPECT_FALSE(outcome_regressed(RunOutcome::Failure, RunOutcome::Success));
    EXPECT_FALSE(outcome_regressed(RunOutcome::Unstable, RunOutcome::Success));
    EXPECT_FALSE(outcome_regressed(RunOutcome::Failure, RunOutcome::Failure));
    // invariant: the aborted and unknown classes are OFF the axis and are never a regression in
    // either direction.
    EXPECT_FALSE(outcome_regressed(RunOutcome::Success, RunOutcome::Aborted));
    EXPECT_FALSE(outcome_regressed(RunOutcome::Aborted, RunOutcome::Failure));
    EXPECT_FALSE(outcome_regressed(RunOutcome::Success, RunOutcome::Unknown));
    EXPECT_FALSE(outcome_regressed(RunOutcome::Unknown, RunOutcome::Failure));
}
