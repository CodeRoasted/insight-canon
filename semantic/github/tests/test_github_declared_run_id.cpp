// refs: DN-162.D13, DN-133.D1, DN-133.D2
// invariant: the rule is canon's mechanism and the floor is this package's data, so only a
// Tokenizer over this package's composition shows a declared run id reaching a template.
// invariant: every value below is synthetic: no run id here belongs to anyone.
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

constexpr std::string_view kRunId{"31415926535"};
constexpr std::string_view kSessionLine{"wrote /home/runner/.codex/31415926535.json ok"};
constexpr std::string_view kSessionMasked{"wrote /home/runner/.codex/<*>.json ok"};
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

[[nodiscard]] StreamContext run_id(std::string_view value)
{
    return StreamContext{.values = {{.key = "run_id", .value = std::string{value}}}};
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

TEST(GithubDeclaredRunId, TheRunsOwnIdMasksAsAWholeDigitRunWithNoMarker)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, run_id(kRunId)};
    EXPECT_EQ(template_of(tokenizer, arena, kSessionLine), kSessionMasked)
        << "a GitHub stream declaring run_id masks its own id in a path no rule masks";
    EXPECT_EQ(template_of(tokenizer, arena, "artifact RunId31415926535x kept"),
              "artifact RunId<*>x kept")
        << "a letter beside the run does not hold it back: only a digit neighbour does";
}

TEST(GithubDeclaredRunId, ADigitNeighbourAnotherIdAndNoValueStayLiteral)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, run_id(kRunId)};
    for (const std::string_view line :
         {std::string_view{"wrote /home/runner/.codex/731415926535.json ok"},
          std::string_view{"wrote /home/runner/.codex/314159265357.json ok"}})
        EXPECT_EQ(template_of(tokenizer, arena, line), line)
            << "the declared id inside a longer digit run is not the run's id: it stays literal";
    Tokenizer other{arena, MaskConfig{}, stream.semantics, run_id("27182818284")};
    EXPECT_EQ(template_of(other, arena, kSessionLine), kSessionLine)
        << "another run's id stays literal: only the acquirer's own value masks";
    Tokenizer undeclared{arena, MaskConfig{}, stream.semantics, StreamContext{}};
    EXPECT_EQ(template_of(undeclared, arena, kSessionLine), kSessionLine)
        << "a stream that declares no value is byte-identical to today";
}

TEST(GithubDeclaredRunId, BothKeysApplyTogether)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{github_stream(composed)};
    ArenaAllocator arena{kArenaBytes};
    const StreamContext both{.values = {{.key = "pull_request", .value = "6656"},
                                        {.key = "run_id", .value = std::string{kRunId}}}};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, both};
    EXPECT_EQ(template_of(tokenizer, arena, "PR-6656 wrote /home/runner/.codex/31415926535.json"),
              "PR-<*> wrote /home/runner/.codex/<*>.json")
        << "each declared key masks its own value, behind its own marker or its own floor";
}

TEST(GithubDeclaredRunId, AValueShorterThanTheFloorIsRefusedNamingIt)
{
    const ComposedSemantics composed{github_composition()};
    const auto short_value{insight::semantic::check_stream_context(run_id("314159265"), composed)};
    ASSERT_FALSE(short_value.has_value())
        << "a 9-digit run_id is below the key's floor of 10 and must be refused";
    EXPECT_NE(short_value.error().find("10"), std::string::npos)
        << "the refusal names the floor; actual: " << short_value.error();
    EXPECT_NE(short_value.error().find("\"run_id\""), std::string::npos)
        << "the refusal names the key; actual: " << short_value.error();
    EXPECT_TRUE(insight::semantic::check_stream_context(run_id("3141592653"), composed))
        << "a value exactly at the floor passes";
    EXPECT_TRUE(insight::semantic::check_stream_context(
        StreamContext{.values = {{.key = "pull_request", .value = "22"}}}, composed))
        << "a key behind markers declares no floor, so a short pull-request number passes";
}
