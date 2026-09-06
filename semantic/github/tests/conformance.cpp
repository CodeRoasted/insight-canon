// refs: SRC-SP-2
// invariant: the kit is canon-shipped and package-agnostic, so this file is the WHOLE per-package
// instantiation an external package author copies.
// note: seedless and single-threaded — every check is a pure function of the manifest data
#include <gtest/gtest.h>

import std;
import insight.canon.conformance;
import insight.semantic.github;

TEST(GithubConformance, PassesTheCanonConformanceKit)
{
    const auto report{insight::semantic::conformance::run(insight::semantic::github::kManifest)};
    for (const auto& check : report.checks)
        EXPECT_TRUE(check.passed) << "[" << check.name << "] " << check.detail;
    EXPECT_TRUE(report.all_passed()) << report.summary();
}
