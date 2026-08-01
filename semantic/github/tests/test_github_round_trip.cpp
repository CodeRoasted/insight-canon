// test_github_round_trip.cpp — round-trip closure on the GitHub Actions dialect. The RUNTIME
// (value) half of the bidirectionality obligation the DialectIntent concept enforces at compile
// time (a package shipping a reader with no writer does not compile): for every recognition
// marker, materialize its PAIRED generation row (render_row) and assert canon recognizes the
// declared (kind, child_order, payload) back — recognize(render_row(W)) == R. The kit is
// package-agnostic (canon-shipped, self-adapting over the row spans); this file is the ~10-line
// GitHub instantiation. Verbose-on-failure: each check prints the rendered line + the
// expected-vs-got intent tuple. Determinism: seedless, single-threaded, pure byte functions over
// the composed manifest — no RNG, no wall-clock. Every declared marker must close: target 100%.
// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
#include <gtest/gtest.h>

import std;
import insight.canon;             // compose / ComposedSemantics
import insight.canon.conformance; // round_trip_report
import insight.semantic.github;   // kManifest (markers + emits)

TEST(GithubRoundTrip, RecognizeRendersBackToDeclaredIntent)
{
    // BOTH projections come off the ONE manifest: the recognizer is the shipped
    // reader composed from it, and the generation rows are the same `emits` span
    // `semantic_identity` hashes — so what closes here is what the digest claims.
    const std::array<insight::semantic::SemanticPackageManifest, 1> one{
        insight::semantic::github::kManifest};
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose(one)};

    const auto report{insight::semantic::conformance::round_trip_report(
        insight::semantic::github::kManifest, composed)};

    // Every intent marker (Job, both Step media `Run ` / `##[group]Run `) must round-trip — the two
    // GHA Step media closing proves materialization-invariance on read == medium-multiplicity on
    // write.
    ASSERT_FALSE(report.checks.empty())
        << "no round-trip checks ran — the dialect declared no markers?";
    for (const auto& check : report.checks)
        EXPECT_TRUE(check.passed) << "[" << check.name << "] " << check.detail;
    EXPECT_TRUE(report.all_passed()) << report.summary();
}
// NOLINTEND
