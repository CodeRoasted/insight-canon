// refs: DN-89.D47, ADR-17.D1, ADR-22.D6
// invariant: the GitHub dialect declares `Prepare all required actions` as a row that OPENS a step,
// in the annotated channel only and only under its own dialect.
// invariant: only `recognize_opener` returns it.
// invariant: `recognize` never sees the opener, so no caller that asks it for a marker (the
// dialect vote, the channel probes) can read an opener as a unit.
// invariant: the stripped channel opens nothing at the line, because there a failed preparation
// carries no `##[error]` marker to tell it from a successful one.
// invariant: homed with the vocabulary, as the job opener arm is: the row is this package's data,
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

constexpr std::string_view kOpener{"Prepare all required actions"};

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

// refs: DN-89.D47
// invariant: the runner's preparation line opens a step on the annotated channel, and `recognize`
// and `recognize_closer` read it as no marker.
TEST(GithubStepOpener, ThePreparationLineOpensAStepOnTheAnnotatedChannel)
{
    const ComposedSemantics view{github_view(insight::semantic::github::kDialect,
                                             insight::semantic::github::kChannelAnnotated)};
    const IntentMarkerKind opened{recognize_opener(norm_probe(kOpener), view)};
    EXPECT_EQ(opened, IntentMarkerKind::Step) << "the opener opens no step: " << show(opened);
    const IntentMarker named{recognize(norm_probe(kOpener), view)};
    EXPECT_EQ(named.kind, IntentMarkerKind::None)
        << "`recognize` returned the opener: " << show(named);
    EXPECT_FALSE(recognize_closer(norm_probe(kOpener), view)) << "the opener was read as a closer";
}

// refs: DN-89.D47, ADR-22.D6
// invariant: the gate is load-bearing: on the stripped channel the same line opens nothing.
TEST(GithubStepOpener, ThePreparationLineOpensNothingOnTheStrippedChannel)
{
    const IntentMarkerKind opened{recognize_opener(
        norm_probe(kOpener), github_view(insight::semantic::github::kDialect,
                                         insight::semantic::github::kChannelStripped))};
    EXPECT_EQ(opened, IntentMarkerKind::None)
        << "the opener fired on the stripped channel: " << show(opened);
}

// refs: DN-89.D47, ADR-22.D6
// invariant: the opener is gated to this dialect and anchored at the line start, so an undeclared
// stream and a line quoting it mid-sentence open nothing.
TEST(GithubStepOpener, TheOpenerFiresOnlyAtTheLineStartOfADeclaredStream)
{
    const IntentMarkerKind undeclared{recognize_opener(
        norm_probe(kOpener),
        github_view(insight::semantic::kAnyDialect, insight::semantic::github::kChannelAnnotated))};
    EXPECT_EQ(undeclared, IntentMarkerKind::None)
        << "the opener fired on a stream that declared no dialect: " << show(undeclared);
    const IntentMarkerKind quoted{
        recognize_opener(norm_probe("echo Prepare all required actions"),
                         github_view(insight::semantic::github::kDialect,
                                     insight::semantic::github::kChannelAnnotated))};
    EXPECT_EQ(quoted, IntentMarkerKind::None) << "the opener fired mid-line: " << show(quoted);
}
