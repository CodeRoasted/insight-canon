// invariant: the conformance kit's own VACUITY arms — a check may pass only about a row it
// measured, and a pass that measured nothing must say so in the one field every framework shows.
// invariant: every arm pairs a manifest the check cannot measure with a CONTROL it can, so the
// named check is shown to SEPARATE the two rather than to return a constant.
// invariant: determinism — byte-only compile-time fixtures, no RNG, no clock, no float.
// refs: ADR-17, LSRC-5
#include <gtest/gtest.h>

import insight.canon.test;

using insight::RunOutcome;
using insight::semantic::IntentEmitRow;
using insight::semantic::IntentMarkerRow;
using insight::semantic::kAnyChannel;
using insight::semantic::OutcomeMarkerRow;
using insight::semantic::OutcomeMarkerShape;
using insight::semantic::OutcomeTokenRow;
using insight::semantic::PayloadEmit;
using insight::semantic::PayloadExtract;
using insight::semantic::SemanticPackageManifest;
using insight::semantic::conformance::CheckResult;
using insight::semantic::conformance::marker_probe_for;
using insight::semantic::conformance::Report;
using insight::semantic::conformance::run;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarkerKind;

namespace
{

constexpr std::string_view kKitDialect{"kitprobe"};

constexpr std::array<IntentMarkerRow, 1> kMarkers{{{.prefix = "<KIT-step> ",
                                                    .kind = IntentMarkerKind::Step,
                                                    .child_order = ChildOrder::Ordered,
                                                    .dialect_gate = kKitDialect,
                                                    .extract = PayloadExtract::RemainderAfterPrefix,
                                                    .payload_excludes = {},
                                                    .channel_gate = kAnyChannel}}};

constexpr std::array<IntentEmitRow, 1> kEmits{{{.prefix = "<KIT-step> ",
                                                .kind = IntentMarkerKind::Step,
                                                .child_order = ChildOrder::Ordered,
                                                .dialect_gate = kKitDialect,
                                                .emit = PayloadEmit::PayloadAfterPrefix,
                                                .channel_gate = kAnyChannel}}};

constexpr std::array<OutcomeTokenRow, 2> kTokens{
    {{.token = "KIT_PASSED", .outcome = RunOutcome::Success, .dialect_gate = kKitDialect},
     {.token = "KIT_BROKEN", .outcome = RunOutcome::Failure, .dialect_gate = kKitDialect}}};

constexpr std::array<OutcomeMarkerRow, 1> kOutcomeMarkers{
    {{.prefix = "KIT run finished: ",
      .dialect_gate = kKitDialect,
      .shape = OutcomeMarkerShape::RemainderToken,
      .outcome = RunOutcome::Unknown}}};

// invariant: the CONTROL — one marker row and its writer dual, so the kit's probe fires.
constexpr SemanticPackageManifest kPaired{
    .name = kKitDialect, .version = "1.0.0", .markers = kMarkers, .emits = kEmits};

// invariant: the same reader with its writer withheld — a reader without a writer, the one shape
// whose probe is the empty string by the exported probe's own contract.
constexpr SemanticPackageManifest kUnpaired{
    .name = kKitDialect, .version = "1.0.0", .markers = kMarkers};

// invariant: the CONTROL for the outcome leg — two tokens and one RemainderToken marker, so the
// round trip closes once per token; a verdict word is `[A-Za-z0-9_]`, hence no hyphen.
constexpr SemanticPackageManifest kTokensAndMarker{.name = kKitDialect,
                                                   .version = "1.0.0",
                                                   .outcome_tokens = kTokens,
                                                   .outcome_markers = kOutcomeMarkers};

// invariant: tokens with NO outcome marker — a declared absence the GitHub package ships today, so
// the leg must PASS it and must not call that pass a measurement.
constexpr SemanticPackageManifest kTokensNoMarker{
    .name = kKitDialect, .version = "1.0.0", .outcome_tokens = kTokens};

// invariant: a RemainderToken marker with no token to render — the second way the loop runs zero
// round trips, reached through the other loop.
constexpr SemanticPackageManifest kMarkerNoTokens{
    .name = kKitDialect, .version = "1.0.0", .outcome_markers = kOutcomeMarkers};

// post: the report's first check whose name opens with `leg`, else nullptr.
// note: a prefix and not a name, because a leg's passing and failing names differ in spelling.
[[nodiscard]] const CheckResult* check_of(const Report& report, std::string_view leg)
{
    const auto found{std::ranges::find_if(report.checks, [leg](const CheckResult& check)
                                          { return check.name.starts_with(leg); })};
    return found == report.checks.end() ? nullptr : &*found;
}

[[nodiscard]] std::string every_check(const Report& report)
{
    std::string out;
    for (const CheckResult& check : report.checks)
        out += "  [" + std::string{check.name} + "] " + (check.passed ? "pass" : "FAIL") + ' ' +
               check.detail + '\n';
    return out;
}

} // namespace

TEST(ConformanceKitNonVacuity, TheDeterminismLegRedsAMarkerRowItCouldNotProbe)
{
    ASSERT_FALSE(marker_probe_for(kMarkers[0], kEmits).empty())
        << "the control's probe is empty — the paired fixture no longer pairs";
    const Report control{run(kPaired)};
    const CheckResult* const measured{check_of(control, "determinism")};
    ASSERT_NE(measured, nullptr) << "run() pushed no determinism check:\n" << every_check(control);
    EXPECT_TRUE(measured->passed) << "the paired control must pass the determinism leg:\n"
                                  << every_check(control);
    EXPECT_EQ(measured->name, "determinism")
        << "a probe that fires is a MEASURED pass, and carries the bare name:\n"
        << every_check(control);
    EXPECT_TRUE(control.all_passed()) << "the control is a conformant package, so every leg passes "
                                         "— "
                                      << control.summary() << '\n'
                                      << every_check(control);

    ASSERT_TRUE(marker_probe_for(kMarkers[0], {}).empty())
        << "the subject's premise: an unpaired row's exported probe is the empty string";
    const Report subject{run(kUnpaired)};
    const CheckResult* const unmeasured{check_of(subject, "determinism")};
    ASSERT_NE(unmeasured, nullptr) << "run() pushed no determinism check:\n"
                                   << every_check(subject);
    EXPECT_FALSE(unmeasured->passed)
        << "the determinism leg PASSED a marker row whose probe is empty — recognize(\"\") agrees "
           "with itself trivially, so the leg reported green about a row it never measured:\n"
        << every_check(subject);
    EXPECT_EQ(unmeasured->name, "determinism.unmeasured") << every_check(subject);
    EXPECT_NE(unmeasured->detail.find("<KIT-step> "), std::string::npos)
        << "the diagnostic must name the unmeasured row's key:\n"
        << every_check(subject);
}

TEST(ConformanceKitNonVacuity, TheOutcomeRoundTripNamesAPassThatMeasuredNothing)
{
    const Report control{run(kTokensAndMarker)};
    const CheckResult* const measured{check_of(control, "outcome")};
    ASSERT_NE(measured, nullptr) << "run() pushed no outcome check:\n" << every_check(control);
    EXPECT_TRUE(measured->passed) << every_check(control);
    EXPECT_EQ(measured->name, "outcome_round_trip")
        << "two tokens through one RemainderToken marker are two MEASURED round trips:\n"
        << every_check(control);

    for (const auto& [manifest, shape] :
         {std::pair{kTokensNoMarker, std::string_view{"tokens but no outcome marker"}},
          std::pair{kMarkerNoTokens, std::string_view{"a RemainderToken marker but no token"}}})
    {
        const Report subject{run(manifest)};
        const CheckResult* const vacuous{check_of(subject, "outcome")};
        ASSERT_NE(vacuous, nullptr) << shape << ": run() pushed no outcome check:\n"
                                    << every_check(subject);
        EXPECT_TRUE(vacuous->passed)
            << shape << ": a declared absence is not a defect, so the leg must pass it:\n"
            << every_check(subject);
        EXPECT_EQ(vacuous->name, "outcome_round_trip.nothing_measured")
            << shape
            << ": the leg ran zero round trips and reported the SAME name as a measured pass, "
               "so a caller holding only the report cannot tell the two apart:\n"
            << every_check(subject);
    }
}
