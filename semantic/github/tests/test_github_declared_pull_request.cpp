// refs: DN-133.D1, DN-133.D2, DN-133.D5
// invariant: the rule is canon's mechanism and the markers are this package's data, so only a
// Tokenizer over this package's composition shows a declared value reaching a template.
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

constexpr std::string_view kComposeLine{
    "Deploying /stirling/V2-PR-6656/docker-compose.yml to the preview host"};
constexpr std::string_view kComposeMasked{
    "Deploying /stirling/V2-PR-<*>/docker-compose.yml to the preview host"};
constexpr std::string_view kNextRunLine{
    "Deploying /stirling/V2-PR-6657/docker-compose.yml to the preview host"};
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

[[nodiscard]] StreamContext pull_request(std::string_view number)
{
    return StreamContext{.values = {{.key = "pull_request", .value = std::string{number}}}};
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

} // namespace

TEST(GithubDeclaredPullRequest, TheRunsOwnNumberMasksBehindADeclaredMarker)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, pull_request("6656")};
    EXPECT_EQ(template_of(tokenizer, arena, kComposeLine), kComposeMasked)
        << "a GitHub stream declaring pull_request=6656 masks its own number behind `PR-`";
}

TEST(GithubDeclaredPullRequest, AnotherNumberAndNoValueStayLiteral)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer other{arena, MaskConfig{}, stream.semantics, pull_request("6657")};
    EXPECT_EQ(template_of(other, arena, kComposeLine), kComposeLine)
        << "another pull request's number stays literal: the line cannot say whose number it is";
    Tokenizer undeclared{arena, MaskConfig{}, stream.semantics, StreamContext{}};
    EXPECT_EQ(template_of(undeclared, arena, kComposeLine), kComposeLine)
        << "a stream that declares no value is byte-identical to today";
}

// refs: DN-133.D7
TEST(GithubDeclaredPullRequest, EveryEightMeasuredMarkerSpellingFires)
{
    constexpr std::array<std::string_view, 8> kSpellings{
        {"PR-", "pr-", "Pr-", "pull-", "PULL-", "Pull-", "PR#", "pulls/"}};
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, pull_request("6656")};
    for (const std::string_view marker : kSpellings)
    {
        const std::string line{"image ghcr.io/acme/app:" + std::string{marker} + "6656 pushed"};
        const std::string expected{"image ghcr.io/acme/app:" + std::string{marker} + "<*> pushed"};
        EXPECT_EQ(template_of(tokenizer, arena, line), expected)
            << "the declared marker `" << marker << "` must fire on the run's own number";
    }
}

// refs: DN-133.D7
// invariant: the set is closed: `pull/` and `issues/` were measured and refused, so the run's own
// number behind them stays literal, as it does behind a declared marker inside a word.
TEST(GithubDeclaredPullRequest, AnUndeclaredShapeAndAMarkerInsideAWordStayLiteral)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, pull_request("6656")};
    for (const std::string_view line :
         {std::string_view{"fetching refs/pull/6656/merge now"},
          std::string_view{"reading /repos/o/r/issues/6656/comments now"},
          std::string_view{"label XPR#6656 applied"}, std::string_view{"label PR#66560 applied"}})
        EXPECT_EQ(template_of(tokenizer, arena, line), line)
            << "the run's own number behind no declared marker stays literal";
    EXPECT_EQ(template_of(tokenizer, arena, "reading /repos/o/r/pulls/6656/files now"),
              "reading /repos/o/r/pulls/<*>/files now")
        << "behind the declared `pulls/` it masks";
}

TEST(GithubDeclaredPullRequest, AStreamThatDeclaresNoDialectAppliesTheValueNowhere)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream undeclared_dialect{
        insight::semantic::resolve_stream(composed, IngestDeclaration{})};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, undeclared_dialect.semantics, pull_request("6656")};
    EXPECT_EQ(template_of(tokenizer, arena, kComposeLine), kComposeLine)
        << "the key is GitHub's, so a stream not declaring the GitHub dialect holds no row for it "
           "and the value applies nothing";
}

// refs: DN-133.D5
// invariant: one tokenizer serving two windows takes each window's value from `declare_context`,
// which replaces the value and nothing else: the per-stream counters keep counting.
TEST(GithubDeclaredPullRequest, DeclareContextReplacesTheValueAndKeepsTheCounters)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, pull_request("6656")};
    EXPECT_EQ(template_of(tokenizer, arena, kComposeLine), kComposeMasked);

    tokenizer.declare_context(pull_request("6657"));
    EXPECT_EQ(template_of(tokenizer, arena, kNextRunLine), kComposeMasked)
        << "after the second declaration the second run's own number masks to the same template";
    EXPECT_EQ(template_of(tokenizer, arena, kComposeLine), kComposeLine)
        << "and the first run's number no longer does: the value was replaced, not added";

    tokenizer.declare_context(StreamContext{});
    EXPECT_EQ(template_of(tokenizer, arena, kNextRunLine), kNextRunLine)
        << "declaring the empty context returns the stream to undeclared; nothing is inherited";

    EXPECT_EQ(tokenizer.lines_parsed(), 4U)
        << "declare_context touches no per-stream counter; actual: " << tokenizer.lines_parsed();
    EXPECT_EQ(tokenizer.events_produced(), 4U)
        << "declare_context touches no per-stream counter; actual: " << tokenizer.events_produced();
}

TEST(GithubDeclaredPullRequest, ContextCheckNamesTheDeclaredKeysAndTheValueShape)
{
    const ComposedSemantics composed{github_composition()};
    const auto unknown{insight::semantic::check_stream_context(
        StreamContext{.values = {{.key = "merge_request", .value = "12"}}}, composed)};
    ASSERT_FALSE(unknown.has_value()) << "an unknown key is a mistake and must be refused";
    EXPECT_NE(unknown.error().find("\"pull_request\""), std::string::npos)
        << "the refusal names the declared keys; actual: " << unknown.error();

    for (const std::string_view bad : {"", "0", "0123", "12a", "-4"})
        EXPECT_FALSE(insight::semantic::check_stream_context(pull_request(bad), composed))
            << "a declared value is a run of decimal digits with no leading zero; `" << bad
            << "` must be refused";

    const auto twice{insight::semantic::check_stream_context(
        StreamContext{.values = {{.key = "pull_request", .value = "1"},
                                 {.key = "pull_request", .value = "2"}}},
        composed)};
    EXPECT_FALSE(twice.has_value()) << "one value per key";
    EXPECT_TRUE(insight::semantic::check_stream_context(pull_request("6656"), composed))
        << "a known key with a declarable value passes";
}

TEST(GithubDeclaredPullRequestDeathTest, ATokenizerRefusesAnUnknownKey)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    const StreamContext unknown_key{.values = {{.key = "merge_request", .value = "12"}}};
    EXPECT_DEATH((void)Tokenizer(arena, MaskConfig{}, stream.semantics, unknown_key),
                 R"(unknown context key "merge_request")");
}
