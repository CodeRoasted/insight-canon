// refs: ADR-18.D1
// invariant: the GitHub dialect DECLARES where a step's name holds its version: on both Step rows,
// the bytes after the last `@` of a one-token payload; the Job row declares none.
// invariant: canon applies the declaration and holds no `@` of its own, so the recognized marker
// carries the version bytes exactly when the row declares them and the payload has that shape.
// invariant: homed with the vocabulary, as the marker arms are: the declaration is this package's
// row, and the recognition walks it.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::semantic::ComposedSemantics;
using insight::semantic::VersionPayloadShape;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

// pre: every probe is escape-free, so `normalize` is the zero-copy fixed point.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

namespace
{

constexpr std::string_view kPin{"c0f6160ff80057923ff50e5e5676a2dbcf6d9c3a"};

[[nodiscard]] ComposedSemantics github_only(std::string_view channel)
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::github::kDialect,
                                                            channel);
}

// invariant: one step payload and the version the declaration reads off it, empty for none.
struct Probe
{
    std::string_view payload;
    std::string_view version;
};

constexpr std::array kProbes{
    Probe{.payload = "actions/checkout@c0f6160ff80057923ff50e5e5676a2dbcf6d9c3a", .version = kPin},
    Probe{.payload = "actions/checkout@v4", .version = "v4"},
    Probe{.payload = "dtolnay/rust-toolchain@stable", .version = "stable"},
    Probe{.payload = "pytorch/test-infra/.github/actions/setup-uv@release/2.13",
          .version = "release/2.13"},
    Probe{.payload = "scope/action@with@two", .version = "two"},
    Probe{.payload = "docker pull ghcr.io/acme/tool@c0f6160ff80057923ff50e5e5676a2dbcf6d9c3a",
          .version = ""},
    Probe{.payload = "echo user@host", .version = ""},
    Probe{.payload = "yarn lint", .version = ""},
    Probe{.payload = "make", .version = ""},
};

} // namespace

// refs: ADR-18.D1
// invariant: every Step row declares the introducer `@` on a one-token payload, and the Job row
// declares no version coordinate.
TEST(GithubVersionCoordinate, BothStepRowsDeclareItAndTheJobRowDeclaresNone)
{
    std::size_t step_rows{0};
    for (const auto& row : insight::semantic::github::kManifest.markers)
    {
        if (row.kind == IntentMarkerKind::Step)
        {
            ++step_rows;
            EXPECT_EQ(row.version.introducer, "@") << "Step row '" << row.prefix << "'";
            EXPECT_EQ(row.version.shape, VersionPayloadShape::OneToken)
                << "Step row '" << row.prefix << "'";
        }
        else
        {
            EXPECT_EQ(row.version.shape, VersionPayloadShape::None)
                << "row '" << row.prefix << "' declares a version coordinate";
            EXPECT_TRUE(row.version.introducer.empty()) << "row '" << row.prefix << "'";
        }
    }
    EXPECT_EQ(step_rows, 2U) << "the dialect no longer carries its two Step rows";
}

// refs: ADR-18.D1
// invariant: in each channel a one-token payload's step carries the bytes after its last `@` as
// its version and the whole payload as its name; any other payload carries no version.
TEST(GithubVersionCoordinate, TheRecognizedStepCarriesTheVersionOfAOneTokenPayloadOnly)
{
    const std::array<std::pair<std::string_view, std::string_view>, 2> kChannels{
        {{insight::semantic::github::kChannelStripped, "Run "},
         {insight::semantic::github::kChannelAnnotated, "##[group]Run "}}};
    for (const auto& [channel, prefix] : kChannels)
    {
        const ComposedSemantics semantics{github_only(channel)};
        for (const auto& [payload, version] : kProbes)
        {
            const std::string line{std::string{prefix} + std::string{payload}};
            const auto step{recognize(norm_probe(line), semantics)};
            ASSERT_EQ(step.kind, IntentMarkerKind::Step) << "\"" << line << "\" opened no step";
            EXPECT_EQ(step.name, payload) << "\"" << line << "\": the name is the whole payload";
            EXPECT_EQ(step.version, version) << "\"" << line << "\": version";
        }
    }
}

// refs: ADR-18.D1
// invariant: a job banner holding `@` carries no version: the Job row declares none.
TEST(GithubVersionCoordinate, AJobBannerHoldingAtCarriesNoVersion)
{
    const ComposedSemantics semantics{github_only(insight::semantic::github::kChannelAnnotated)};
    const auto job{recognize(norm_probe("Complete job name: deploy@production"), semantics)};
    ASSERT_EQ(job.kind, IntentMarkerKind::Job);
    EXPECT_EQ(job.name, "deploy@production");
    EXPECT_TRUE(job.version.empty()) << "a job banner gained the version '" << job.version << "'";
}
