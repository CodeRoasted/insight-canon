// refs: DN-89.D40, ADR-17.D1
// invariant: the GitHub dialect declares `Post job cleanup.` and `Cleaning up orphan processes` as
// rows that CLOSE a job's declared steps, in both channels and only under its own dialect.
// invariant: only `recognize_closer` returns a closing row.
// invariant: `recognize` and `recognize_opener` never see a closing row, so no caller that asks
// them for a marker can read the runner's teardown as a unit or an opener.
// invariant: homed with the vocabulary, as the opener arm is: the rows are this package's data,
// and the role a row declares is canon's grammar.
// invariant: determinism — byte-only recognition over the composed rows; no RNG, clock or float.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::semantic::ComposedSemantics;
using insight::tokenization::IntentMarker;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;
using insight::tokenization::recognize_closer;
using insight::tokenization::recognize_opener;

// pre: every probe is escape-free, so `normalize` is the zero-copy fixed point.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

namespace
{

constexpr std::string_view kPostStep{"Post job cleanup."};
constexpr std::string_view kOrphans{"Cleaning up orphan processes"};

[[nodiscard]] ComposedSemantics github_view(std::string_view dialect, std::string_view channel)
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(dialect, channel);
}

[[nodiscard]] std::string show(const IntentMarker& mark)
{
    return "{kind=" + std::to_string(static_cast<int>(mark.kind)) + ", name=\"" +
           std::string{mark.name} + "\"}";
}

} // namespace

// refs: DN-89.D40
// invariant: both runner lines close the job's steps in the real channel and in our ablation alike,
// and neither other walker reads them.
TEST(GithubEpilogueCloser, BothRunnerTeardownLinesCloseInBothChannels)
{
    for (const std::string_view channel : {insight::semantic::github::kChannelAnnotated,
                                           insight::semantic::github::kChannelStripped})
    {
        const ComposedSemantics view{github_view(insight::semantic::github::kDialect, channel)};
        for (const std::string_view line : {kPostStep, kOrphans})
        {
            EXPECT_TRUE(recognize_closer(norm_probe(line), view))
                << "channel " << channel << ": \"" << line << "\" closes nothing";
            const IntentMarker named{recognize(norm_probe(line), view)};
            EXPECT_EQ(named.kind, IntentMarkerKind::None)
                << "channel " << channel << ": `recognize` returned \"" << line
                << "\": " << show(named);
            EXPECT_EQ(recognize_opener(norm_probe(line), view), IntentMarkerKind::None)
                << "channel " << channel << ": \"" << line << "\" was read as an opener";
        }
    }
}

// refs: DN-89.D40
// invariant: no naming or opening row of the dialect closes anything.
TEST(GithubEpilogueCloser, NoOtherMarkerRowCloses)
{
    const ComposedSemantics view{github_view(insight::semantic::github::kDialect,
                                             insight::semantic::github::kChannelAnnotated)};
    for (const std::string_view line : {std::string_view{"Current runner version: '2.337.0'"},
                                        std::string_view{"Complete job name: publish"},
                                        std::string_view{"##[group]Run actions/checkout@v4"},
                                        std::string_view{"##[group]Run '/job_completed.sh'"}})
        EXPECT_FALSE(recognize_closer(norm_probe(line), view))
            << "\"" << line << "\" was read as a closing row";
}

// refs: DN-89.D40, ADR-22.D6
// invariant: the closing rows are gated to this dialect and anchored at the line start, so an
// undeclared stream and a user step echoing the runner's text mid-line close nothing.
TEST(GithubEpilogueCloser, TheClosersFireOnlyAtTheLineStartOfADeclaredStream)
{
    EXPECT_FALSE(recognize_closer(
        norm_probe(kPostStep),
        github_view(insight::semantic::kAnyDialect, insight::semantic::github::kChannelAnnotated)))
        << "a closing row fired on a stream that declared no dialect";
    const ComposedSemantics view{github_view(insight::semantic::github::kDialect,
                                             insight::semantic::github::kChannelAnnotated)};
    EXPECT_FALSE(recognize_closer(norm_probe("echo Post job cleanup."), view))
        << "a closing row fired mid-line";
    EXPECT_FALSE(recognize_closer(norm_probe("echo Cleaning up orphan processes"), view))
        << "a closing row fired mid-line";
}
