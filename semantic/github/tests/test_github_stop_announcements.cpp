// assert: ADR-17.D15 — the github package declares the whole lines its runner writes into a step it
// assert: stops, each matched exactly as peeled, its trailing whitespace trimmed, and any appended
// assert: byte matching no row.
// assert: a stop announcement is a second declaration about the line: each matched line still
// assert: classifies Terminator and its declared level is still Error.
// assert: the rows enter semantic_identity: declaring them moves the github identity.
// invariant: determinism — literal lines, no clock, no RNG.
// refs: ADR-17.D15, ADR-17.D4, ADR-22.D6
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::StructuralRole;
using insight::semantic::ComposedSemantics;
using insight::semantic::SemanticPackageManifest;
using insight::tokenization::announces_stop;
using insight::tokenization::classify;

namespace
{
// pre: every probe is escape-free, so `normalize` is the zero-copy fixed point.
[[nodiscard]] insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

[[nodiscard]] ComposedSemantics github_stream()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::github::kDialect,
                                                            {});
}

[[nodiscard]] ComposedSemantics undeclared_stream()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::kAnyDialect, {});
}

constexpr std::array<std::string_view, 3> kStopLines{
    "##[error]The operation was canceled.",
    "##[error]The runner has received a shutdown signal. This can happen when the runner service "
    "is stopped, or a manually started runner is canceled.",
    "##[error]Step canceled by GitHub (see: "
    "https://depot.dev/docs/github-actions/troubleshooting#error-step-canceled-by-github)"};

// invariant: the stop-like wordings ADR-17.D15 read and left out.
constexpr std::array<std::string_view, 3> kUndeclaredStopLike{
    "##[error]A task was canceled.",
    "##[error][OSDC] Step cancelled by GitHub Actions (received SIGINT).",
    "##[error]Unhandled error: Error: One or more jobs failed or were cancelled"};
} // namespace

TEST(GithubStopAnnouncements, TheManifestDeclaresTheThreeRunnerWordings)
{
    const auto rows{insight::semantic::github::kManifest.stop_announcements};
    ASSERT_EQ(rows.size(), kStopLines.size()) << "stop announcements declared: " << rows.size();
    for (std::size_t idx{0}; idx < rows.size(); ++idx)
    {
        EXPECT_EQ(rows[idx].shape, kStopLines[idx]) << "row " << idx;
        EXPECT_EQ(rows[idx].dialect_gate, insight::semantic::github::kDialect) << "row " << idx;
    }
}

TEST(GithubStopAnnouncements, EachRowMatchesItsExactLineWithItsTrailingCarriageReturnTrimmed)
{
    const ComposedSemantics gh{github_stream()};
    for (const std::string_view line : kStopLines)
    {
        EXPECT_TRUE(announces_stop(norm_probe(line), gh)) << "not a stop: \"" << line << "\"";
        const std::string with_cr{std::string{line} + "\r"};
        EXPECT_TRUE(announces_stop(norm_probe(with_cr), gh))
            << "a trailing CR must be trimmed before the match: \"" << line << "\"";
    }
}

TEST(GithubStopAnnouncements, ALineWithAnyByteAppendedOrRemovedMatchesNoRow)
{
    const ComposedSemantics gh{github_stream()};
    for (const std::string_view line : kStopLines)
    {
        for (const std::string_view suffix : {"x", ".", " see log", "0"})
        {
            const std::string appended{std::string{line} + std::string{suffix}};
            EXPECT_FALSE(announces_stop(norm_probe(appended), gh))
                << "an appended byte still matched: \"" << appended << "\"";
        }
        const std::string_view shortened{line.substr(0, line.size() - 1U)};
        EXPECT_FALSE(announces_stop(norm_probe(shortened), gh))
            << "a shortened line still matched: \"" << shortened << "\"";
        const std::string prefixed{"x" + std::string{line}};
        EXPECT_FALSE(announces_stop(norm_probe(prefixed), gh))
            << "a prefixed line still matched: \"" << prefixed << "\"";
    }
}

TEST(GithubStopAnnouncements, AStopLikeWordingNoRowDeclaresIsNoStop)
{
    const ComposedSemantics gh{github_stream()};
    for (const std::string_view line : kUndeclaredStopLike)
        EXPECT_FALSE(announces_stop(norm_probe(line), gh))
            << "an undeclared wording read as a stop: \"" << line << "\"";
}

TEST(GithubStopAnnouncements, AStopKeepsItsTerminatorRoleAndItsErrorLevel)
{
    const ComposedSemantics gh{github_stream()};
    for (const std::string_view line : kStopLines)
    {
        EXPECT_EQ(classify(norm_probe(line), gh), StructuralRole::Terminator)
            << "the stop line lost its role: \"" << line << "\"";
        EXPECT_EQ(insight::tokenization::lift_level(line, gh), insight::LogLevel::Error)
            << "the stop line lost its declared level: \"" << line << "\"";
    }
}

TEST(GithubStopAnnouncements, AStreamThatDeclaresNoDialectReadsNoStop)
{
    const ComposedSemantics undeclared{undeclared_stream()};
    for (const std::string_view line : kStopLines)
        EXPECT_FALSE(announces_stop(norm_probe(line), undeclared))
            << "a self-gated stop row fired on an undeclared stream: \"" << line << "\"";
}

TEST(GithubStopAnnouncements, TheRowsMoveTheGithubIdentity)
{
    SemanticPackageManifest without{insight::semantic::github::kManifest};
    without.stop_announcements = {};
    const std::array with_rows{insight::semantic::github::kManifest};
    const std::array without_rows{without};
    EXPECT_NE(insight::semantic::compose(with_rows).identity_hex(),
              insight::semantic::compose(without_rows).identity_hex())
        << "the stop announcements must enter semantic_identity";
}
