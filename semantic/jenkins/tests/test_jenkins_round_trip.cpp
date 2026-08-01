// test_jenkins_round_trip.cpp — round-trip closure on the Jenkins Pipeline dialect. The RUNTIME
// (value) half of the bidirectionality obligation the DialectIntent concept enforces at compile
// time (a package shipping a reader with no writer does not compile): for every recognition
// marker, materialize its PAIRED generation row (render_row) and assert canon recognizes the
// declared (kind, child_order, payload) back — recognize(render_row(W)) == R. Exercises
// PayloadThenClosingParen (the STAGE `[Pipeline] { (<name>)` dual of RemainderToClosingParen)
// alongside PayloadAfterPrefix (the STEP verb). The probe payload is a REAL step verb, never a
// kStepExcludes structural token — the writer only ever emits a real quantum, so the reader's
// exclusion set has no generation dual and the round-trip closes. Verbose-on-failure.
// Determinism: seedless, single-threaded, pure byte functions. Every declared marker must close:
// target 100%. NOLINTBEGIN — unit test: short identifiers and string literals are fine.
#include <gtest/gtest.h>

import std;
import insight.canon;             // compose / ComposedSemantics
import insight.canon.conformance; // round_trip_report
import insight.semantic.jenkins;  // kManifest (markers + emits)

TEST(JenkinsRoundTrip, RecognizeRendersBackToDeclaredIntent)
{
    // BOTH projections come off the ONE manifest — the same `emits` span
    // `semantic_identity` hashes.
    const std::array<insight::semantic::SemanticPackageManifest, 1> one{
        insight::semantic::jenkins::kManifest};
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose(one)};

    const auto report{insight::semantic::conformance::round_trip_report(
        insight::semantic::jenkins::kManifest, composed)};

    ASSERT_FALSE(report.checks.empty())
        << "no round-trip checks ran — the dialect declared no markers?";
    for (const auto& check : report.checks)
        EXPECT_TRUE(check.passed) << "[" << check.name << "] " << check.detail;
    EXPECT_TRUE(report.all_passed()) << report.summary();
}
// NOLINTEND
