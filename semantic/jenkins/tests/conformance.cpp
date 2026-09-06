// refs: SRC-SP-2, LSRC-5
// invariant: the canon conformance kit is package-agnostic and canon-shipped, so this file is the
// ENTIRE per-package instantiation — the shape an external package author copies.
// invariant: a failing check prints its own verbose-on-failure diagnostic naming the row and the
// actual-vs-expected values, so this TU adds no diagnosis of its own.
// note: determinism: seedless, single-threaded, pure functions of the manifest data
#include <gtest/gtest.h>

import std;
import insight.canon.conformance;
import insight.semantic.jenkins;

TEST(JenkinsConformance, PassesTheCanonConformanceKit)
{
    const auto report{insight::semantic::conformance::run(insight::semantic::jenkins::kManifest)};
    for (const auto& check : report.checks)
        EXPECT_TRUE(check.passed) << "[" << check.name << "] " << check.detail;
    EXPECT_TRUE(report.all_passed()) << report.summary();
}
