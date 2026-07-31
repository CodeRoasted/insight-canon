// conformance.cpp — instantiate the canon CONFORMANCE KIT (ADR 0024 §2.3, SRC-SP-2) on THIS
// package's manifest. The kit is package-agnostic and canon-shipped; this file is the ENTIRE
// per-package instantiation — the ~15-line shape an external package author copies. A failing check
// prints its own verbose-on-failure diagnostic (which row, actual-vs-expected). Determinism:
// seedless, single-threaded; the kit's checks are pure functions of the manifest data. NOLINTBEGIN
// — unit test: short identifiers and string literals are fine.
#include <gtest/gtest.h>

import std;
import insight.canon.conformance;
import insight.semantic.gitlab;

TEST(GitLabConformance, PassesTheCanonConformanceKit)
{
    const auto report{insight::semantic::conformance::run(insight::semantic::gitlab::kManifest)};
    for (const auto& check : report.checks)
        EXPECT_TRUE(check.passed) << "[" << check.name << "] " << check.detail;
    EXPECT_TRUE(report.all_passed()) << report.summary();
}
// NOLINTEND
