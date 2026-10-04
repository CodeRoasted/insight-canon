// refs: LSRC-5
// invariant: the kit is canon's and package-agnostic; this file is the whole of this package's
// instantiation of it.
// invariant: seedless, single-threaded and pure over manifest data.
#include <gtest/gtest.h>

import std;
import insight.canon.conformance;
import insight.semantic.package_managers;

TEST(PackageManagersConformance, PassesTheCanonConformanceKit)
{
    const auto report{
        insight::semantic::conformance::run(insight::semantic::package_managers::kManifest)};
    for (const auto& check : report.checks)
        EXPECT_TRUE(check.passed) << "[" << check.name << "] " << check.detail;
    EXPECT_TRUE(report.all_passed()) << report.summary();
}
