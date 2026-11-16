// refs: DN-163.D1, DN-163.D7, DN-163.D13
// invariant: the instance values are canon's mechanism over this package's rows, so only a
// Tokenizer over this package's composition shows a runner's instance reaching a template.
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

[[nodiscard]] ResolvedStream stream_of(const ComposedSemantics& composed, std::string_view dialect)
{
    return insight::semantic::resolve_stream(
        composed, IngestDeclaration{.stack = {},
                                    .dialect = dialect,
                                    .channel = dialect.empty()
                                                   ? std::string_view{}
                                                   : insight::semantic::github::kChannelAnnotated});
}

// post: the template of `line` and the number of params it pushed.
[[nodiscard]] std::pair<std::string, std::size_t>
template_of(Tokenizer& tokenizer, ArenaAllocator& arena, std::string_view line)
{
    const auto event{tokenizer.process_line(line)};
    std::pair<std::string, std::size_t> out{event.has_value() ? std::string{event->template_str}
                                                              : "<no event: " + event.error() + ">",
                                            event.has_value() ? event->params.size() : 0U};
    arena.reset();
    return out;
}

} // namespace

// refs: DN-163.D1
TEST(GithubInstanceValues, TwoRunnerNameLinesDifferingOnlyByTheirValueGiveOneTemplate)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{stream_of(composed, insight::semantic::github::kDialect)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, StreamContext{}};
    for (const std::string_view line :
         {"Runner name: 'depot-2p44nr537d'",
          "Runner name: 'blacksmith-01ktfw4t1rrnx7jfcd3m5h3r4c-4vcpu'",
          "Runner name: 'GitHub Actions 1000123'"})
    {
        const auto [got, params]{template_of(tokenizer, arena, line)};
        EXPECT_EQ(got, "Runner name: '<*>'")
            << "the runner's name is its instance and masks whole, quotes kept.\n  line:   " << line
            << "\n  actual: " << got;
        EXPECT_EQ(params, 0U) << "the masked value never leaves its line: no param.\n  line: "
                              << line << "\n  params: " << params;
    }
    EXPECT_EQ(template_of(tokenizer, arena, "Machine name: 'ip-172-31-64-123'").first,
              "Machine name: '<*>'")
        << "the machine name is the same instance coordinate";
}

// refs: DN-163.D1
TEST(GithubInstanceValues, ALineRowReachesItsLinesOnly)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{stream_of(composed, insight::semantic::github::kDialect)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, StreamContext{}};
    for (const std::string_view line :
         {"Runner group name: 'Default Larger Runners'", "Runner name: unquoted-value",
          "Runner name: 'one' 'two'", "Previous Runner name: 'depot-2p44nr537d'"})
    {
        const auto [got, params]{template_of(tokenizer, arena, line)};
        EXPECT_EQ(got.find("'<*>'"), std::string::npos)
            << "a line row masks its own whole line's quoted value and nothing else.\n  line:   "
            << line << "\n  actual: " << got;
    }
    const ResolvedStream undeclared{stream_of(composed, {})};
    Tokenizer other{arena, MaskConfig{}, undeclared.semantics, StreamContext{}};
    EXPECT_EQ(template_of(other, arena, "Runner name: 'depot-2p44nr537d'").first,
              "Runner name: 'depot-2p44nr537d'")
        << "a stream declaring no dialect reads no GitHub instance row";
}

// refs: DN-163.D7
TEST(GithubInstanceValues, TheResultsStorageShardMasksAsAShapeInsideAnyToken)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{stream_of(composed, insight::semantic::github::kDialect)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, StreamContext{}};
    const std::string at_12{
        template_of(tokenizer, arena,
                    "Redirecting to blob download url: "
                    "https://productionresultssa12.blob.core.windows.net/actions-results/x.zip")
            .first};
    const std::string at_16{
        template_of(tokenizer, arena,
                    "Redirecting to blob download url: "
                    "https://productionresultssa16.blob.core.windows.net/actions-results/x.zip")
            .first};
    EXPECT_EQ(at_12, at_16) << "the shard is the storage instance: one template.\n  shard 12: "
                            << at_12 << "\n  shard 16: " << at_16;
    EXPECT_NE(at_12.find("productionresultssa<*>.blob.core.windows.net"), std::string::npos)
        << "the hole masks and the literal bytes around it stay: " << at_12;
    EXPECT_EQ(
        template_of(tokenizer, arena, "resolved productionresultssa7.blob.core.windows.net.").first,
        "resolved productionresultssa<*>.blob.core.windows.net.")
        << "the shape in a DNS line masks too";
}

// refs: DN-163.D7
TEST(GithubInstanceValues, TheRunnersContainerNameAndLabelMaskAsArguments)
{
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{stream_of(composed, insight::semantic::github::kDialect)};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, StreamContext{}};
    const auto [run, params]{template_of(
        tokenizer, arena,
        "##[command]/usr/bin/docker run --name ghcrio_de576d --label a1b2c3 --workdir /w")};
    EXPECT_EQ(run, "##[command]/usr/bin/docker run --name <*> --label <*> --workdir /w")
        << "the token after each flag masks whole";
    EXPECT_EQ(params, 2U) << "each masked argument is a whole-token wildcard, so a param";
    EXPECT_EQ(template_of(tokenizer, arena,
                          "##[command]/usr/bin/docker ps --all --filter \"label=a1b2c3\"")
                  .first,
              "##[command]/usr/bin/docker ps --all --filter \"label=<*>\"")
        << "a declared key masks its value inside a token, the shell kept";
    for (const std::string_view line :
         {"[command]/usr/bin/docker run --name ghcrio_de576d --label a1b2c3",
          "docker run --name mydb --label a1b2c3"})
    {
        const std::string got{template_of(tokenizer, arena, line).first};
        EXPECT_NE(got.find("--name "), std::string::npos) << got;
        EXPECT_EQ(got.find("--name <*>"), std::string::npos)
            << "an action's exec and a script's own docker run are not the runner's: literal.\n"
               "  line:   "
            << line << "\n  actual: " << got;
    }
}

// refs: DN-163.D7
// invariant: the arms of DN-163.D7's dated paragraph of 2026-11-16: the argument row's flag `-t`,
// and a masked argument value recurring on its own line as a token's bytes before its first `:`.
TEST(GithubInstanceValues, TheRunnersImageLabelMasksBehindTAndWhereItsLineRepeatsIt)
{
    constexpr std::string_view kHex{"0123456789abcdef0123456789abcdef"};
    const ComposedSemantics composed{github_composition()};
    const ResolvedStream stream{stream_of(composed, insight::semantic::github::kDialect)};
    const ResolvedStream undeclared{stream_of(composed, {})};
    ArenaAllocator arena{kArenaBytes};
    Tokenizer tokenizer{arena, MaskConfig{}, stream.semantics, StreamContext{}};
    Tokenizer other{arena, MaskConfig{}, undeclared.semantics, StreamContext{}};
    const std::string image{std::format("a1b2c3:{}", kHex)};

    const std::string build{
        std::format("##[command]/usr/bin/docker build -t {} -f /w/Dockerfile /w", image)};
    const auto [built, built_params]{template_of(tokenizer, arena, build)};
    EXPECT_EQ(built, "##[command]/usr/bin/docker build -t <*> -f /w/Dockerfile /w")
        << "the token after `-t` masks whole.\n  actual: " << built;
    EXPECT_EQ(built_params, 1U) << "the masked image is a whole-token wildcard, so a param";

    const std::string run{std::format(
        "##[command]/usr/bin/docker run --name n_d4e5f6 --label a1b2c3 --workdir /w {}", image)};
    const auto [ran, ran_params]{template_of(tokenizer, arena, run)};
    EXPECT_EQ(ran, "##[command]/usr/bin/docker run --name <*> --label <*> --workdir /w <*>")
        << "the image repeats the line's masked label before its first `:`, so it masks "
           "whole.\n  actual: "
        << ran;
    EXPECT_EQ(ran_params, 3U) << "the name, the label and the image are three params";

    const std::string earlier{
        std::format("##[command]/usr/bin/docker create {} --label a1b2c3", image)};
    EXPECT_EQ(template_of(tokenizer, arena, earlier).first,
              "##[command]/usr/bin/docker create <*> --label <*>")
        << "the recurrence is per line, wherever on it the masked value stands";

    const std::string another{std::format(
        "##[command]/usr/bin/docker run --name n_d4e5f6 --label a1b2c3 a1b2c4:{}", kHex)};
    EXPECT_EQ(template_of(tokenizer, arena, another).first,
              "##[command]/usr/bin/docker run --name <*> --label <*> a1b2c4:<*>")
        << "another label before the `:` keeps its bytes; only its hash masks, as today";

    for (const std::string& line :
         {std::format("docker build -t {} -f /w/Dockerfile /w", image),
          std::format("[command]/usr/bin/docker run --name n_d4e5f6 --label a1b2c3 {}", image),
          std::format("#7 naming to docker.io/library/{} done", image)})
    {
        const std::string got{template_of(tokenizer, arena, line).first};
        EXPECT_EQ(got, template_of(other, arena, line).first)
            << "a line without the runner's command prefix reads as with no GitHub row.\n"
               "  line:   "
            << line << "\n  actual: " << got;
    }
}

// refs: DN-163.D13
TEST(GithubInstanceValues, EveryNewRowIsSinceV2AndMasksInEveryReading)
{
    for (const insight::semantic::InstanceValueRow& row :
         insight::semantic::github::kManifest.instance_values)
        EXPECT_EQ(row.revision_since, "v2")
            << "an instance-value row v1 does not hold is bookkeeping since v2: " << row.bytes;
    EXPECT_EQ(insight::semantic::github::kManifest.instance_values.size(), 4U)
        << "the two line rows, the shard shape and the docker argument row";
}
