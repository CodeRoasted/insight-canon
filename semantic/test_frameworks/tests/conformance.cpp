// conformance.cpp — instantiate the canon CONFORMANCE KIT (ADR 0024 §2.3, SRC-SP-2) on THIS
// package's manifest. Package-agnostic canon-shipped gate; this is the entire per-package
// instantiation. A failing check prints its own verbose diagnostic. Determinism: seedless,
// single-threaded, pure over manifest data. NOLINTBEGIN — unit test: short identifiers and string
// literals are fine.
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
// NOLINTEND
