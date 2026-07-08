// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_where_wiring.cpp — MaskConfig.recognize_test_where, the flag-gated identity-derived WHERE on the
// tokenizer (intent_identity_model.md §5.3, II-8). REPLACES the 1.7.4 tests/identity/test_where_wiring.cpp,
// re-homed to core as a SYNTHETIC-composition mechanism test after the SP-1 fix removed the dialect gate
// (was `event.format == LogFormat::GitHubActions`; now purely `component.empty()` — tokenizer_engine.cpp:89,
// bugs.md 2026-07-08). The property is now FORMAT-AGNOSTIC, so it is a core Tokenizer property provable
// with a synthetic location row over a RawText line — no package linked. This test is also the SP-1
// regression guard: a line WITHOUT a native component (RawText here, any future dialect tomorrow) gets the
// identity-derived WHERE, proving the wiring never re-acquires a dialect literal.
//
// DEFAULT-OFF is the load-bearing invariant — the flag off leaves component byte-identical
// ([[additive-gated-metalog-block-keeps-wire-version]]). Determinism: byte-only, no RNG/clock/float.
#include <gtest/gtest.h>

import insight.canon.test; // facade (compose / Tokenizer / recognize_location) + spi (row grammar)

using insight::LogFormat;
using insight::semantic::ComposedSemantics;
using insight::semantic::compose;
using insight::semantic::LocationMatchKind;
using insight::semantic::LocationRow;
using insight::semantic::SemanticPackageManifest;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

namespace
{
constexpr std::size_t kArenaSize{64U * 1024U};

// A synthetic location family (vocabulary-free — NOT any real framework): `<base>.chk.aa`. The composed
// row drives canon's TestSpecExtension algorithm; the point is the WIRING, not the vocabulary.
constexpr std::array<std::string_view, 1> kInfix{".chk."};
constexpr std::array<std::string_view, 1> kExt{"aa"};
constexpr std::array<LocationRow, 1> kLocations{{
    {.kind = LocationMatchKind::TestSpecExtension, .infixes = kInfix, .extensions = kExt,
     .prefixes = {}, .suffixes = {}}}};
constexpr SemanticPackageManifest kManifest{
    .name = "synth_loc", .version = "1.0.0", .roles = {}, .markers = {}, .level_lifts = {},
    .locations = kLocations, .value_classes = {}, .strategy = nullptr, .echoed_source = nullptr};

[[nodiscard]] ComposedSemantics composed()
{
    const std::array manifests{kManifest};
    return compose(manifests);
}

// A line with a synthetic test-file token that routes to RawText (no native component) — the
// empty-component tier the identity-derived WHERE populates. Format-agnostic by construction (RawText,
// not GitHubActions) — the SP-1 regression guard.
constexpr std::string_view kTestLine{"PASS src/auth/login.chk.aa"};
constexpr std::string_view kNonTestLine{"Syncing repository acme/widget"};
} // namespace

// ── Flag ON: the identity-derived test-file WHERE populates the empty component (ANY format) ──
TEST(WhereWiring, FlagOnPopulatesTestFileWhereOnEmptyComponentLine)
{
    ArenaAllocator arena{kArenaSize};
    const ComposedSemantics sc{composed()};
    Tokenizer tokenizer{arena, MaskConfig{.recognize_test_where = true}, sc};
    const auto result{tokenizer.process_line(kTestLine)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& ev{result.value()};
    EXPECT_NE(ev.format, LogFormat::GitHubActions)
        << "SP-1 guard: the wiring must fire on a NON-GHA line (no dialect literal in core)";
    EXPECT_EQ(ev.component, "src/auth/login.chk.aa")
        << "identity-derived WHERE not populated; component=\"" << ev.component << '"';
}

// ── Default-OFF: no WHERE populated — the byte-identical invariant ──
TEST(WhereWiring, FlagOffLeavesComponentEmpty)
{
    ArenaAllocator arena{kArenaSize};
    const ComposedSemantics sc{composed()};
    Tokenizer tokenizer{arena, MaskConfig{}, sc}; // default → recognize_test_where = false
    const auto result{tokenizer.process_line(kTestLine)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_TRUE(result.value().component.empty())
        << "default-off must leave component empty (byte-identical); got \"" << result.value().component << '"';
}

// ── No false WHERE: a non-test line stays empty even with the flag ON ──
TEST(WhereWiring, FlagOnNonTestLineStaysEmpty)
{
    ArenaAllocator arena{kArenaSize};
    const ComposedSemantics sc{composed()};
    Tokenizer tokenizer{arena, MaskConfig{.recognize_test_where = true}, sc};
    const auto result{tokenizer.process_line(kNonTestLine)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_TRUE(result.value().component.empty())
        << "a non-test line names no WHERE; got \"" << result.value().component << '"';
}
// NOLINTEND
