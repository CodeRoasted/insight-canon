// invariant: the flag-gated, identity-derived WHERE on the tokenizer — the WHERE has ONE source,
// which is the identity spine.
// invariant: re-homed to core as a SYNTHETIC-composition mechanism test after the fix removed the
// dialect gate, which used to compare the event's format against one real ecosystem.
// invariant: the condition is now purely an EMPTY component, so the property is FORMAT-AGNOSTIC and
// provable with a synthetic location row over a raw-text line, with no package linked.
// invariant: this test is also the regression guard for that fix.
// invariant: a line WITHOUT a native component gets the identity-derived WHERE, proving the wiring
// never re-acquires a dialect literal.
// invariant: DEFAULT-OFF is the load-bearing invariant — with the flag off the component is
// byte-identical, so the gated block stays ADDITIVE and the wire version does not move.
// invariant: determinism — byte-only, with no RNG, clock or float.
// refs: SRC-II-8, SRC-SP-1
#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogFormat;
using insight::semantic::compose;
using insight::semantic::ComposedSemantics;
using insight::semantic::LocationMatchKind;
using insight::semantic::LocationRow;
using insight::semantic::SemanticPackageManifest;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

namespace
{
constexpr std::size_t kArenaSize{64U * 1024U};

// invariant: a synthetic location family, vocabulary-free and NOT any real framework's, so the
// composed row drives canon's algorithm and the point is the WIRING and not the vocabulary.
constexpr std::array<std::string_view, 1> kInfix{".chk."};
constexpr std::array<std::string_view, 1> kExt{"aa"};
constexpr std::array<LocationRow, 1> kLocations{{{.kind = LocationMatchKind::TestSpecExtension,
                                                  .infixes = kInfix,
                                                  .extensions = kExt,
                                                  .prefixes = {},
                                                  .suffixes = {}}}};
constexpr SemanticPackageManifest kManifest{.name = "synth_loc",
                                            .version = "1.0.0",
                                            .roles = {},
                                            .markers = {},
                                            .level_lifts = {},
                                            .locations = kLocations,
                                            .value_classes = {},
                                            .strategy = nullptr,
                                            .echoed_source = nullptr};

[[nodiscard]] ComposedSemantics composed()
{
    const std::array manifests{kManifest};
    return compose(manifests);
}

// invariant: a line whose synthetic test-file token routes to raw text and carries no native
// component — the empty-component tier the identity-derived WHERE populates.
// invariant: FORMAT-AGNOSTIC by construction, which is the regression guard.
// refs: SRC-SP-1
constexpr std::string_view kTestLine{"PASS src/auth/login.chk.aa"};
constexpr std::string_view kNonTestLine{"Syncing repository acme/widget"};
// invariant: the same token with a producer's annotation glued to it and no separator — the shape
// that put an annotation on a published alert's WHERE.
// invariant: the marker is spelled here as DATA on a probe line and NEVER as a rule, because no
// package is linked and nothing in this binary knows what it means.
constexpr std::string_view kAnnotatedTestLine{"##[error]src/auth/login.chk.aa:104:"};
} // namespace

// invariant: with the flag ON the identity-derived test-file WHERE populates the empty component,
// under ANY format.
TEST(WhereWiring, FlagOnPopulatesTestFileWhereOnEmptyComponentLine)
{
    ArenaAllocator arena{kArenaSize};
    const ComposedSemantics sc{composed()};
    Tokenizer tokenizer{arena, MaskConfig{.recognize_test_where = true}, sc};
    const auto result{tokenizer.process_line(kTestLine)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& ev{result.value()};
    EXPECT_NE(ev.format, LogFormat::GitHubActions)
        << "SRC-SP-1 guard: the wiring must fire on a NON-GHA line (no dialect literal in core)";
    EXPECT_EQ(ev.component, "src/auth/login.chk.aa")
        << "identity-derived WHERE not populated; component=\"" << ev.component << '"';
}

// invariant: DEFAULT-OFF populates no WHERE, which is the byte-identical invariant.
TEST(WhereWiring, FlagOffLeavesComponentEmpty)
{
    ArenaAllocator arena{kArenaSize};
    const ComposedSemantics sc{composed()};
    Tokenizer tokenizer{arena, MaskConfig{}, sc};
    const auto result{tokenizer.process_line(kTestLine)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_TRUE(result.value().component.empty())
        << "default-off must leave component empty (byte-identical); got \""
        << result.value().component << '"';
}

// invariant: the axis carries the LOCATION alone, so a glued producer annotation never enters the
// component.
// invariant: this is the END-TO-END leg of the property the walker suite pins on the algorithm.
// invariant: what a consumer reads off the WHERE axis is the path, not the path plus whatever the
// producer welded to its front.
TEST(WhereWiring, FlagOnWhereCarriesTheLocationWithoutTheGluedAnnotation)
{
    ArenaAllocator arena{kArenaSize};
    const ComposedSemantics sc{composed()};
    Tokenizer tokenizer{arena, MaskConfig{.recognize_test_where = true}, sc};
    const auto result{tokenizer.process_line(kAnnotatedTestLine)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& ev{result.value()};
    EXPECT_EQ(ev.component, "src/auth/login.chk.aa")
        << "the WHERE axis must carry the location alone; line \"" << kAnnotatedTestLine
        << "\" yielded component=\"" << ev.component << "\" (routed format "
        << insight::to_string(ev.format) << ')';
}

// invariant: a NON-test line stays empty even with the flag ON, so no false WHERE is manufactured.
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
