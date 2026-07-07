// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_where_wiring.cpp — MaskConfig.recognize_test_where, the flag-gated identity-derived
// WHERE on the tokenizer (intent_identity_model.md §5.3, II-8; canon 83be994). A GitHub-Actions
// line whose NATIVE component is empty (GHA carries none) gets its recognize_location() test-file
// as `component`, populating the cube WHERE axis ABOVE the empty native tier — never faking a
// native field. Homed here (tests/identity/) with the intent-identity contract, not tests/tokenizer/:
// the property under test is the §5.3 identity-derived WHERE, whose recognizer is tested alongside.
// DEFAULT-OFF is the load-bearing invariant — every existing path must stay byte-identical
// ([[additive-gated-metalog-block-keeps-wire-version]]); a diff here is a golden/version break.

#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

namespace
{
constexpr std::size_t kArenaSize{64U * 1024U};

// A GHA line: the RFC3339 + 7-digit-fractional 'Z' prefix is what the format detector keys on
// (test_format_detector::DetectsGitHubActions); the content follows the timestamp.
constexpr std::string_view kGhaTestLine{"2026-05-27T15:26:41.7842152Z PASS src/auth/login.test.ts"};
constexpr std::string_view kGhaNonTestLine{"2026-05-27T15:26:41.7842152Z Syncing repository acme/widget"};
} // namespace

// ── Flag ON: the identity-derived test-file WHERE populates the empty GHA component ──
TEST(WhereWiring, FlagOnPopulatesTestFileWhereOnGhaLine)
{
    ArenaAllocator arena{kArenaSize};
    Tokenizer tokenizer{arena, MaskConfig{.recognize_test_where = true}};
    const auto result{tokenizer.process_line(kGhaTestLine)};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    ASSERT_EQ(ev.format, LogFormat::GitHubActions) << "fixture must be detected as GHA";
    EXPECT_EQ(ev.component, "src/auth/login.test.ts")
        << "identity-derived WHERE not populated; component=\"" << ev.component << '"';
}

// ── Default-OFF: no WHERE populated — the byte-identical invariant ──
TEST(WhereWiring, FlagOffLeavesComponentEmpty)
{
    ArenaAllocator arena{kArenaSize};
    Tokenizer tokenizer{arena}; // default MaskConfig → recognize_test_where = false
    const auto result{tokenizer.process_line(kGhaTestLine)};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    ASSERT_EQ(ev.format, LogFormat::GitHubActions);
    EXPECT_TRUE(ev.component.empty())
        << "default-off must leave component empty (byte-identical); got \"" << ev.component << '"';
}

// ── No false WHERE: a non-test GHA line stays empty even with the flag ON ──
TEST(WhereWiring, FlagOnNonTestGhaLineStaysEmpty)
{
    ArenaAllocator arena{kArenaSize};
    Tokenizer tokenizer{arena, MaskConfig{.recognize_test_where = true}};
    const auto result{tokenizer.process_line(kGhaNonTestLine)};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    ASSERT_EQ(ev.format, LogFormat::GitHubActions);
    EXPECT_TRUE(ev.component.empty())
        << "a non-test line names no WHERE; got \"" << ev.component << '"';
}

// NOLINTEND
