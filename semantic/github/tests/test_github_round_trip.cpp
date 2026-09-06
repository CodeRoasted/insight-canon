// refs: ADR-18.D4
// invariant: the RUNTIME half of the bidirectionality obligation `DialectIntent` fences at COMPILE
// time — a package shipping a reader with no writer does not compile.
// post: for every recognition row, `recognize(render_row(W))` returns the declared intent.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.canon.conformance;
import insight.semantic.github;

TEST(GithubRoundTrip, RecognizeRendersBackToDeclaredIntent)
{
    // invariant: both projections come off the ONE manifest, so what closes here is exactly what
    // `semantic_identity` hashes and claims.
    const std::array<insight::semantic::SemanticPackageManifest, 1> one{
        insight::semantic::github::kManifest};
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose(one)};

    const auto report{insight::semantic::conformance::round_trip_report(
        insight::semantic::github::kManifest, composed)};

    // assert: the two GHA Step media closing is materialization-invariance on READ equal to
    // medium-multiplicity on WRITE.
    ASSERT_FALSE(report.checks.empty())
        << "no round-trip checks ran — the dialect declared no markers?";
    for (const auto& check : report.checks)
        EXPECT_TRUE(check.passed) << "[" << check.name << "] " << check.detail;
    EXPECT_TRUE(report.all_passed()) << report.summary();
}
