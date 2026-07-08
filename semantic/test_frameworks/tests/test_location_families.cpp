// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_location_families.cpp — the test-framework FILE-LOCATION vocabulary (ADR 0024, II-8). Migrated from
// canon tests/identity/test_location_recognizer.cpp: the matching MECHANISM (insight::recognize_location,
// the three closed LocationMatchKind algorithms over the composed location rows) is CANON's; the
// VOCABULARY — the jest/vitest/playwright `.test.`/`.spec.` families, pytest `test_*`/`*_test`, go/ruby
// suffixes — is THIS package's kLocations rows, so the knowledge test homes here. The realistic
// trailing-coordinate / leading-glyph forms are how these families appear in real reporter output, so they
// are asserted with the real vocabulary (the boundary MECHANICS are also pinned abstractly in canon's
// synthetic tests/compose/test_semantic_walkers.cpp). Determinism: byte-only, no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;                    // compose / ComposedSemantics / recognize_location
import insight.semantic.test_frameworks; // kManifest

using insight::recognize_location;
using insight::semantic::ComposedSemantics;

namespace
{
[[nodiscard]] ComposedSemantics test_frameworks_only()
{
    const std::array manifests{insight::semantic::test_frameworks::kManifest};
    return insight::semantic::compose(manifests);
}

void expect_loc(const ComposedSemantics& composed, std::string_view content, std::string_view expected)
{
    const std::string_view got{recognize_location(content, composed)};
    EXPECT_EQ(got, expected) << "recognize_location(\"" << content << "\") = \"" << got << "\"  expected \""
                             << expected << '"';
}
} // namespace

// ── The five families extract their test-file path ──
TEST(LocationFamilies, ExtractsAllFiveFamilies)
{
    const ComposedSemantics tf{test_frameworks_only()};
    // 1. jest/vitest/playwright: `*.test.<ext>` / `*.spec.<ext>`, ext ∈ {ts,tsx,js,jsx,mjs,cjs,py}
    expect_loc(tf, "PASS src/auth/login.test.ts", "src/auth/login.test.ts");
    expect_loc(tf, "src/components/Button.spec.tsx passed", "src/components/Button.spec.tsx");
    // 2. pytest bare module: `test_*.py` / `*_test.py`
    expect_loc(tf, "tests/test_login.py PASSED", "tests/test_login.py");
    expect_loc(tf, "app/api/login_test.py PASSED", "app/api/login_test.py");
    // 3. go: `*_test.go`
    expect_loc(tf, "ok internal/server/handler_test.go 0.42s", "internal/server/handler_test.go");
    // 4. ruby: `*_spec.rb`
    expect_loc(tf, "spec/models/user_spec.rb", "spec/models/user_spec.rb");
    // 5. ruby: `*_test.rb`
    expect_loc(tf, "test/unit/user_test.rb", "test/unit/user_test.rb");
}

// ── Trailing coordinates and leading glyphs are excluded (realistic reporter-line forms) ──
TEST(LocationFamilies, StripsTrailingCoordinatesAndLeadingGlyphs)
{
    const ComposedSemantics tf{test_frameworks_only()};
    expect_loc(tf, "tests/login.test.ts:42:5", "tests/login.test.ts");             // trailing :line:col
    expect_loc(tf, "pkg/net/handler_test.go::TestHandler", "pkg/net/handler_test.go"); // trailing ::node
    expect_loc(tf, "\t\tspec/models/user_spec.rb", "spec/models/user_spec.rb");     // tab-indent
    expect_loc(tf, "\xE2\x9C\x93 src/util/date.test.js (12 ms)", "src/util/date.test.js"); // glyph + trailer
}

// ── No false positives — the vocabulary boundary ──
TEST(LocationFamilies, NoFalsePositives)
{
    const ComposedSemantics tf{test_frameworks_only()};
    expect_loc(tf, "config.spec.json is valid", "");        // .spec. but a non-script ext (json)
    expect_loc(tf, "Compiling src/main.py", "");            // .py but not test_*/*_test
    expect_loc(tf, "import helper from './helpers.ts'", ""); // .ts but no .test./.spec. infix
    expect_loc(tf, "", "");                                 // empty line opens no WHERE
}
// NOLINTEND
