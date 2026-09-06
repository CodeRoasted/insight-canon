// refs: ADR-23, STU-8
// invariant: this is the RUNTIME half of the bidirectionality obligation the DialectIntent concept
// enforces at compile time: recognize(render_row(W)) must equal R for every row.
// invariant: it exercises the STAGE dual and the STEP dual with the kit's own probe payload, which
// carries no structural token, so the STEP row's exclusion set cannot decline it.
// note: determinism: seedless, single-threaded, pure byte functions; every declared row closes
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.canon.conformance;
import insight.semantic.jenkins;

// invariant: BOTH projections come off the ONE manifest — the same `emits` span the semantic
// identity hashes — so a reader and its writer cannot drift apart unnoticed.
TEST(JenkinsRoundTrip, RecognizeRendersBackToDeclaredIntent)
{
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
