// refs: SRC-SP-2
// invariant: the kit is canon's and package-agnostic; this file is the whole of this package's
// instantiation of it.
// invariant: seedless, single-threaded and pure over manifest data.
#include <gtest/gtest.h>

import std;
import insight.canon.conformance;
import insight.semantic.test_frameworks;

TEST(TestFrameworksConformance, PassesTheCanonConformanceKit)
{
    const auto report{
        insight::semantic::conformance::run(insight::semantic::test_frameworks::kManifest)};
    for (const auto& check : report.checks)
        EXPECT_TRUE(check.passed) << "[" << check.name << "] " << check.detail;
    EXPECT_TRUE(report.all_passed()) << report.summary();
}
