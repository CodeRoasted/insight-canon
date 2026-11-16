// refs: DN-163.D4, DN-163.D5, DN-163.D6, DN-163.D13
// invariant: the pull-request extension and the head branch are canon's mechanism over this
// package's rows, so only a Tokenizer over its composition shows them reaching a template.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::semantic::ComposedSemantics;
using insight::semantic::ResolvedStream;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::MaskConfig;
using insight::tokenization::StreamContext;
using insight::tokenization::Tokenizer;
using insight::transport::IngestDeclaration;

namespace
{

constexpr std::size_t kArenaBytes{std::size_t{64} * 1024U};

[[nodiscard]] ComposedSemantics github_composition()
{
    return insight::semantic::compose(std::array{insight::semantic::github::kManifest});
}

[[nodiscard]] ResolvedStream github_stream(const ComposedSemantics& composed)
{
    return insight::semantic::resolve_stream(
        composed, IngestDeclaration{.stack = {},
                                    .dialect = insight::semantic::github::kDialect,
                                    .channel = insight::semantic::github::kChannelAnnotated});
}

[[nodiscard]] StreamContext declared(std::string_view key, std::string_view value)
{
    return StreamContext{.values = {{.key = std::string{key}, .value = std::string{value}}}};
}

[[nodiscard]] std::string template_of(Tokenizer& tokenizer, ArenaAllocator& arena,
                                      std::string_view line)
{
    const auto event{tokenizer.process_line(line)};
    std::string out{event.has_value() ? std::string{event->template_str}
                                      : "<no event: " + event.error() + ">"};
    arena.reset();
    return out;
}

struct Arm
{
    std::string_view line;
    std::string_view expected;
};

} // namespace

// refs: DN-163.D4
TEST(GithubDeclaredPullRef, TheRunsOwnNumberMasksBehindPull)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, declared("pull_request", "6656")};
    for (const Arm& arm :
         {Arm{"fetching refs/pull/6656/merge now", "fetching refs/pull/<*>/merge now"},
          Arm{"git fetch +refs/pull/6656/merge:refs/remotes/pull/6656/merge",
              "git fetch +refs/pull/<*>/merge:refs/remotes/pull/<*>/merge"},
          Arm{"GET https://github.com/o/r/pull/6656.diff",
              "GET https://github.com/o/r/pull/<*>.diff"}})
        EXPECT_EQ(template_of(tokenizer, arena, arm.line), arm.expected)
            << "the v2 extension adds `pull/` to the pull-request markers.\n  line: " << arm.line;
    for (const std::string_view line :
         {"see https://github.com/o/r/pull/6657.diff too", "fetching refs/pull/66560/merge now"})
        EXPECT_EQ(template_of(tokenizer, arena, line), line)
            << "another request's number, or a run that is not the value, stays literal";
    Tokenizer undeclared{arena, MaskConfig{}, stream.semantics, StreamContext{}};
    EXPECT_EQ(template_of(undeclared, arena, "fetching refs/pull/6656/merge now"),
              "fetching refs/pull/6656/merge now")
        << "an undeclared stream stays byte-identical";
}

// refs: DN-163.D5
TEST(GithubDeclaredPullRef, ANumberOfFiveDigitsOrMoreMasksWithNoMarker)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer five{arena, MaskConfig{}, stream.semantics, declared("pull_request", "38187")};
    EXPECT_EQ(template_of(five, arena, "uploading diff_report_38187.zip"),
              "uploading diff_report_<*>.zip")
        << "a whole digit run equal to a value of five digits masks with no marker";
    Tokenizer other{arena, MaskConfig{}, stream.semantics, declared("pull_request", "38408")};
    EXPECT_EQ(template_of(other, arena, "artifact ui-report-1-38408 kept"),
              "artifact ui-report-1-<*> kept");
    EXPECT_EQ(template_of(other, arena, "artifact ui-report-1-384087 kept"),
              "artifact ui-report-1-384087 kept")
        << "a run that is not maximal stays literal";
    EXPECT_EQ(template_of(other, arena, "artifact ui-report-1-38409 kept"),
              "artifact ui-report-1-38409 kept")
        << "another run's number stays literal";
    Tokenizer four{arena, MaskConfig{}, stream.semantics, declared("pull_request", "3818")};
    EXPECT_EQ(template_of(four, arena, "uploading diff_report_3818.zip"),
              "uploading diff_report_3818.zip")
        << "below five digits the floor does not apply: no marker, no mask";
    EXPECT_EQ(template_of(four, arena, "fetching refs/pull/3818/merge now"),
              "fetching refs/pull/<*>/merge now")
        << "below the floor the markers still apply at any length";
    EXPECT_TRUE(insight::semantic::check_stream_context(declared("pull_request", "22"), composed))
        << "a key some marker finds declares no length floor on the context";
}

// refs: DN-163.D6
TEST(GithubDeclaredPullRef, TheHeadBranchMasksAsAWholeTokenBehindAShellOrAPrefix)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics,
                        declared("head_ref", "fix/upload-asset")};
    for (const Arm& arm : {Arm{"headRef: 'fix/upload-asset',", "headRef: '<*>',"},
                           Arm{"checkout origin/fix/upload-asset done", "checkout origin/<*> done"},
                           Arm{"ref refs/heads/fix/upload-asset", "ref refs/heads/<*>"}})
        EXPECT_EQ(template_of(tokenizer, arena, arm.line), arm.expected)
            << "the declared head branch masks, its shell and prefix kept.\n  line: " << arm.line;
    for (const std::string_view line :
         {"headRef: 'fix/other-branch',", "+refs/heads/fix/upload-asset*:refs/x",
          "upstream/fix/upload-asset"})
        EXPECT_EQ(template_of(tokenizer, arena, line), line)
            << "another branch, a value inside a larger token, an undeclared prefix: literal";

    Tokenizer word{arena, MaskConfig{}, stream.semantics, declared("head_ref", "master")};
    EXPECT_EQ(template_of(word, arena, "headRef: 'master',"), "headRef: 'master',")
        << "a value of letters only masks nothing: a word coincides with words";
    Tokenizer undeclared{arena, MaskConfig{}, stream.semantics, StreamContext{}};
    EXPECT_EQ(template_of(undeclared, arena, "headRef: 'fix/upload-asset',"),
              "headRef: 'fix/upload-asset',")
        << "an undeclared stream stays byte-identical";
}

// refs: DN-163.D6
TEST(GithubDeclaredPullRef, ATextValueIsRefusedOnlyWhenEmpty)
{
    const ComposedSemantics composed{github_composition()};
    const auto empty{insight::semantic::check_stream_context(declared("head_ref", ""), composed)};
    ASSERT_FALSE(empty.has_value()) << "an empty head branch must be refused";
    EXPECT_NE(empty.error().find("\"head_ref\""), std::string::npos)
        << "the refusal names the key; actual: " << empty.error();
    EXPECT_TRUE(insight::semantic::check_stream_context(
        declared("head_ref", "dependabot/npm/x-1.2"), composed))
        << "a text value is not a digit run, and passes";
}
