// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_github_roles.cpp — the GitHub-Actions STRUCTURAL-ROLE vocabulary (ADR 0024). Migrated from
// canon tests/utils/test_structural_role.cpp: the classification MECHANISM
// (insight::tokenization::classify, longest-match over the composed role rows) is CANON's; the
// VOCABULARY — `##[group]`/`::group::` → GroupBegin, `##[endgroup]`/`::endgroup::` → GroupEnd,
// `##[error]`/`::error::` → Terminator — is THIS package's kRoles rows, so the knowledge test homes
// here. The rows use kAnyFormat (the pre-split UNGATED behavior): a role fires regardless of the
// routed format. Determinism: byte-only, no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;           // compose / ComposedSemantics / classify / Tokenizer + enums
import insight.semantic.github; // kManifest

using insight::LogFormat;
using insight::StructuralRole;
using insight::to_string;
using insight::semantic::ComposedSemantics;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::classify;
using insight::tokenization::MaskConfig;
using insight::tokenization::Tokenizer;

namespace
{
[[nodiscard]] ComposedSemantics github_only()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests);
}
} // namespace

// ── The announced markers classify to their role ──
TEST(GithubRoles, RecognizesAnnouncedMarkers)
{
    const ComposedSemantics gh{github_only()};
    EXPECT_EQ(classify("##[group]Run cmake --build .", LogFormat::GitHubActions, gh),
              StructuralRole::GroupBegin);
    EXPECT_EQ(classify("::group::Build", LogFormat::GitHubActions, gh), StructuralRole::GroupBegin);
    EXPECT_EQ(classify("##[endgroup]", LogFormat::GitHubActions, gh), StructuralRole::GroupEnd);
    EXPECT_EQ(classify("::endgroup::", LogFormat::GitHubActions, gh), StructuralRole::GroupEnd);
    EXPECT_EQ(
        classify("##[error]Process completed with exit code 2.", LogFormat::GitHubActions, gh),
        StructuralRole::Terminator);
    EXPECT_EQ(classify("::error::file=x.cpp::boom", LogFormat::GitHubActions, gh),
              StructuralRole::Terminator);
}

// ── kAnyFormat: the role rows fire regardless of the routed format (the pre-split ungated
// behavior) ── This is the key vocabulary property of kRoles (format_gate = kAnyFormat): a
// `##[group]` on a line the detector routed to RawText/JSON/Syslog still classifies. The gate field
// carries this — assert it holds.
TEST(GithubRoles, FireOnAnyRoutedFormat)
{
    const ComposedSemantics gh{github_only()};
    for (const LogFormat fmt :
         {LogFormat::Unknown, LogFormat::RawText, LogFormat::JSON, LogFormat::Syslog})
    {
        EXPECT_EQ(classify("##[group]x", fmt, gh), StructuralRole::GroupBegin)
            << "kAnyFormat role failed to fire under " << to_string(fmt);
        EXPECT_EQ(classify("##[error]boom", fmt, gh), StructuralRole::Terminator)
            << "kAnyFormat role failed to fire under " << to_string(fmt);
    }
}

// ── No false role: ordinary content and a bare `error:` prose line announce nothing ──
// The vocabulary boundary: only the bracketed `##[…]`/`::…::` tokens announce; a plain `error:`
// message is content, not a role (the Terminator is the ANNOUNCED marker, never the failure WORD).
TEST(GithubRoles, NoFalseRoleOnPlainContent)
{
    const ComposedSemantics gh{github_only()};
    EXPECT_EQ(classify("compiling tokenizer.cpp", LogFormat::GitHubActions, gh),
              StructuralRole::None);
    EXPECT_EQ(classify("error: undefined reference to foo", LogFormat::GitHubActions, gh),
              StructuralRole::None);
}

// ── End-to-end (composed Tokenizer): the GHA strategy keeps the marker in `content`, so the
// tokenizer tags the role on CanonicalEvent. Migrated from canon test_structural_role.cpp — this is
// the composed integration (github strategy + core tokenizer + composed role rows), so it homes in
// the github suite.
TEST(GithubRoles, TokenizerTagsTerminatorOnGhaError)
{
    ArenaAllocator arena{64U * 1024U};
    const ComposedSemantics gh{github_only()};
    Tokenizer tokenizer{arena, MaskConfig{}, gh};
    const auto event{tokenizer.process_line(
        "2026-05-27T15:42:03.4000004Z ##[error]Process completed with exit code 2.")};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->structural_role, StructuralRole::Terminator);
}

TEST(GithubRoles, TokenizerTagsGroupBoundary)
{
    ArenaAllocator arena{64U * 1024U};
    const ComposedSemantics gh{github_only()};
    Tokenizer tokenizer{arena, MaskConfig{}, gh};
    const auto event{
        tokenizer.process_line("2026-05-27T15:42:03.4000004Z ##[group]Run cmake --build .")};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->structural_role, StructuralRole::GroupBegin);
}

TEST(GithubRoles, TokenizerPlainGhaLineHasNoRole)
{
    ArenaAllocator arena{64U * 1024U};
    const ComposedSemantics gh{github_only()};
    Tokenizer tokenizer{arena, MaskConfig{}, gh};
    const auto event{
        tokenizer.process_line("2026-05-27T15:42:03.4000004Z compiling tokenizer.cpp object")};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->structural_role, StructuralRole::None);
}
// NOLINTEND
