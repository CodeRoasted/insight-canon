// refs: DN-89.D33, ADR-17.D1
// invariant: the GitHub dialect declares `Current runner version: ` as a row that OPENS a job,
// in both channels and only under its own dialect, and only `recognize_opener` returns it.
// invariant: `recognize` never sees the opener, so no caller that asks it for a marker (the
// dialect vote, the channel probes) can read an opener as a unit.
// invariant: `Complete job name: ` stays the row that NAMES the job, and opens nothing.
// invariant: homed with the vocabulary, as the marker arms are: the row is this package's data,
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
using insight::tokenization::recognize_opener;

// pre: every probe is escape-free, so `normalize` is the zero-copy fixed point.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

namespace
{

constexpr std::string_view kOpener{"Current runner version: '2.337.0'"};
constexpr std::string_view kNaming{"Complete job name: publish"};

[[nodiscard]] ComposedSemantics github_view(std::string_view dialect, std::string_view channel)
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(dialect, channel);
}

[[nodiscard]] std::string show(IntentMarkerKind kind)
{
    return "kind=" + std::to_string(static_cast<int>(kind));
}

[[nodiscard]] std::string show(const IntentMarker& mark)
{
    return "{" + show(mark.kind) + ", name=\"" + std::string{mark.name} + "\"}";
}

} // namespace

// refs: DN-89.D33
// invariant: the runner's first line opens a job in the real channel and in our ablation alike, and
// `recognize` reads it as no marker.
TEST(GithubJobOpener, TheRunnerVersionLineOpensAJobInBothChannels)
{
    for (const std::string_view channel : {insight::semantic::github::kChannelAnnotated,
                                           insight::semantic::github::kChannelStripped})
    {
        const ComposedSemantics view{github_view(insight::semantic::github::kDialect, channel)};
        const IntentMarkerKind opened{recognize_opener(norm_probe(kOpener), view)};
        EXPECT_EQ(opened, IntentMarkerKind::Job)
            << "channel " << channel << ": the opener opens no job: " << show(opened);
        const IntentMarker named{recognize(norm_probe(kOpener), view)};
        EXPECT_EQ(named.kind, IntentMarkerKind::None)
            << "channel " << channel << ": `recognize` returned the opener: " << show(named);
    }
}

// refs: DN-89.D33
// invariant: the naming row is unchanged: it names the job with its payload, and opens nothing.
TEST(GithubJobOpener, TheJobNameLineStillNamesTheJob)
{
    for (const std::string_view channel : {insight::semantic::github::kChannelAnnotated,
                                           insight::semantic::github::kChannelStripped})
    {
        const ComposedSemantics view{github_view(insight::semantic::github::kDialect, channel)};
        const IntentMarker named{recognize(norm_probe(kNaming), view)};
        EXPECT_EQ(named.kind, IntentMarkerKind::Job)
            << "channel " << channel << ": " << show(named);
        EXPECT_EQ(named.name, "publish") << "channel " << channel << ": " << show(named);
        const IntentMarkerKind opened{recognize_opener(norm_probe(kNaming), view)};
        EXPECT_EQ(opened, IntentMarkerKind::None)
            << "channel " << channel << ": the naming row was read as an opener: " << show(opened);
    }
}

// refs: DN-89.D33, ADR-22.D6
// invariant: the opener is gated to this dialect and anchored at the line start, so an undeclared
// stream and a line quoting it mid-sentence open nothing.
TEST(GithubJobOpener, TheOpenerFiresOnlyAtTheLineStartOfADeclaredStream)
{
    const IntentMarkerKind undeclared{recognize_opener(
        norm_probe(kOpener),
        github_view(insight::semantic::kAnyDialect, insight::semantic::github::kChannelAnnotated))};
    EXPECT_EQ(undeclared, IntentMarkerKind::None)
        << "the opener fired on a stream that declared no dialect: " << show(undeclared);
    const IntentMarkerKind quoted{
        recognize_opener(norm_probe("echo Current runner version: '2.337.0'"),
                         github_view(insight::semantic::github::kDialect,
                                     insight::semantic::github::kChannelAnnotated))};
    EXPECT_EQ(quoted, IntentMarkerKind::None) << "the opener fired mid-line: " << show(quoted);
}
