// NOLINTBEGIN Module-import proof: allow short identifiers / test patterns.
// test_canon_module_import.cpp
//
// In-repo proof that the `insight.canon` named module (modules/insight_canon.cppm)
// imports cleanly and re-exports the public surface, under BOTH toolchains
// (gcc-15/libstdc++ and clang-21/libc++). This is the unit-level proof that the
// §8.1 wrapper compiles, exposes its surface through the module boundary, and that
// the re-export resolves to definitions in the unchanged library .a — not just
// header declarations (cxx_modules_migration_contract §10.15).
//
// §8.1 BUILD GOTCHA: textual std-pulling includes (gtest) MUST precede the
// `import`, or GCC reports std redefinitions (GMF-std vs textual-std clash).

#include <gtest/gtest.h> // textual std-pulling include FIRST

#include <cstdint>
#include <string_view>

import insight.canon; // ...then the module under test

// det_math is HEADER-ONLY (in api/) — reaching its deterministic fixed-point log
// through the module proves the header surface re-exports. For exact powers of
// two, det_log2_fixed(2^k) = k * 2^kFracBits exactly (log2 is integral, no rounding).
TEST(CanonModuleImport, DetMathResolvesThroughModule)
{
    EXPECT_EQ(insight::det::det_log2_fixed(1U), 0);                      // log2(1) = 0
    EXPECT_EQ(insight::det::det_log2_fixed(2U), insight::det::kOne);     // log2(2) = 1.0 -> kOne
    EXPECT_EQ(insight::det::det_log2_fixed(4U), 2 * insight::det::kOne); // log2(4) = 2.0
}

// contains_failure_cue is declared out-of-line (defined in src/, not inline in the
// header) — reaching it through `import` proves the module re-export resolves to the
// library .a, not merely the header declaration.
TEST(CanonModuleImport, FailureLexiconLinksThroughModule)
{
    EXPECT_TRUE(insight::utils::contains_failure_cue("disk write error: operation failed"));
    EXPECT_FALSE(insight::utils::contains_failure_cue("the quick brown fox jumps"));
}

// NOLINTEND
