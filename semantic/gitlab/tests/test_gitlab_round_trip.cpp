// test_gitlab_round_trip.cpp — round-trip closure on the GitLab CI dialect. The RUNTIME (value)
// half of the bidirectionality obligation the DialectIntent concept enforces at compile time (a
// package shipping a reader with no writer does not compile): for every recognition marker,
// materialize its PAIRED generation row (render_row) and assert canon recognizes the declared
// (kind, child_order, payload) back — recognize(render_row(W)) == R. Exercises
// PlaceholderNumericFieldThenPayload, the grammar-5 dual of NumericFieldThenRemainder.
// Verbose-on-failure. Determinism: seedless, single-threaded, pure byte functions. Every declared
// marker must close: target 100%. NOLINTBEGIN — unit test: short identifiers and string literals
// are fine.
#include <gtest/gtest.h>

import std;
import insight.canon;             // compose / ComposedSemantics
import insight.canon.conformance; // round_trip_report
import insight.semantic.gitlab;   // kManifest (markers + emits)

TEST(GitLabRoundTrip, RecognizeRendersBackToDeclaredIntent)
{
    // BOTH projections come off the ONE manifest — the same `emits` span
    // `semantic_identity` hashes.
    const std::array<insight::semantic::SemanticPackageManifest, 1> one{
        insight::semantic::gitlab::kManifest};
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose(one)};

    const auto report{insight::semantic::conformance::round_trip_report(
        insight::semantic::gitlab::kManifest, composed)};

    ASSERT_FALSE(report.checks.empty())
        << "no round-trip checks ran — the dialect declared no markers?";
    for (const auto& check : report.checks)
        EXPECT_TRUE(check.passed) << "[" << check.name << "] " << check.detail;
    EXPECT_TRUE(report.all_passed()) << report.summary();
}

TEST(GitLabRoundTrip, TheRenderedMarkerCarriesAPlaceholderStampNotAWallClock)
{
    // The declared limitation, asserted rather than left to prose: a generated GitLab marker has no
    // wall-clock and therefore no section duration. If a writer ever emits a VARYING stamp here,
    // that is a step_duration capability landing, and this expectation is where it announces
    // itself.
    const std::string line{insight::semantic::render_row(
        insight::semantic::gitlab::Dialect::emit_markers.front(), "build")};
    EXPECT_EQ(line, "section_start:0:build");
}
// NOLINTEND
