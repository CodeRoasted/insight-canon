// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_location_recognizer.cpp — recognize_location, the intent registry's SECOND rule
// class (intent_identity_model.md §5.3/§5.4, II-8; canon 27ec2ca). Extracts the test-file
// WHERE coordinate from a line — the sub-quantum coverage set the where_set_shift verdict
// compares (P1 coverage-loss / P6 reshape live in this SET's membership, not template
// frequency). Universal framework families; framework file-naming is CI-dialect-independent.
// A diff here re-draws the coverage sets (rides kIntentRegistryVersion, II-7) — fix the code,
// never the assertion.

#include <gtest/gtest.h>

import insight.canon.test;

using insight::recognize_location;

namespace
{
// Verbose-on-failure: print the input line + actual vs expected WHERE.
void expect_loc(std::string_view content, std::string_view expected)
{
    const std::string_view got{recognize_location(content)};
    EXPECT_EQ(got, expected) << "recognize_location(\"" << content << "\") = \"" << got
                             << "\"  expected \"" << expected << '"';
}
} // namespace

// ── The five families extract their test-file path ──
TEST(RecognizeLocation, ExtractsAllFiveFamilies)
{
    // 1. jest/vitest/playwright: `*.test.<ext>` / `*.spec.<ext>`, ext ∈ {ts,tsx,js,jsx,mjs,cjs,py}
    expect_loc("PASS src/auth/login.test.ts", "src/auth/login.test.ts");
    expect_loc("src/components/Button.spec.tsx passed", "src/components/Button.spec.tsx");
    // 2. pytest bare module: `test_*.py` / `*_test.py`
    expect_loc("tests/test_login.py PASSED", "tests/test_login.py");
    expect_loc("app/api/login_test.py PASSED", "app/api/login_test.py");
    // 3. go: `*_test.go`
    expect_loc("ok internal/server/handler_test.go 0.42s", "internal/server/handler_test.go");
    // 4. ruby: `*_spec.rb`
    expect_loc("spec/models/user_spec.rb", "spec/models/user_spec.rb");
    // 5. ruby: `*_test.rb`
    expect_loc("test/unit/user_test.rb", "test/unit/user_test.rb");
}

// ── Trailing coordinates and leading glyphs are excluded BY CONSTRUCTION ──
// A path token has no spaces; `:line`/`::node`/`)` sit after the extension; a leading verdict
// glyph (`✓`, `PASS`, ANSI residue) is a separate whitespace token and never contaminates the path.
TEST(RecognizeLocation, StripsTrailingCoordinatesAndLeadingGlyphs)
{
    expect_loc("tests/login.test.ts:42:5", "tests/login.test.ts");         // trailing :line:col
    expect_loc("pkg/net/handler_test.go::TestHandler", "pkg/net/handler_test.go"); // trailing ::node
    expect_loc("\t\tspec/models/user_spec.rb", "spec/models/user_spec.rb");        // tab-indent
    expect_loc("\xE2\x9C\x93 src/util/date.test.js (12 ms)", "src/util/date.test.js"); // glyph + trailer
}

// ── No false positives ──
// `.spec.json` (non-script ext), a non-test `.py`, and a plain `.ts` import (no `.test.`/`.spec.`
// infix) name no recognized test file → empty.
TEST(RecognizeLocation, NoFalsePositives)
{
    expect_loc("config.spec.json is valid", "");         // .spec. but a non-script ext (json)
    expect_loc("Compiling src/main.py", "");             // .py but not test_*/*_test
    expect_loc("import helper from './helpers.ts'", "");  // .ts but no .test./.spec. infix
    expect_loc("", "");                                  // empty line opens no WHERE
}

// NOLINTEND
