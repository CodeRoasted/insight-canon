// invariant: the recognition MECHANISM is canon's algorithm and the VOCABULARY it walks is this
// package's marker rows, so the knowledge test homes with the vocabulary.
// note: a diff is a segmentation-contract break — fix the rows or the algorithm, never this
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::canonicalize_intent;
using insight::to_string;
using insight::semantic::ComposedSemantics;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarker;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

// pre: `NormalizedContent` is canon's ingest-normalization proof, unforgeable outside canon; every
// probe here is escape-free, so `normalize` is the zero-copy fixed point.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

namespace
{
// invariant: BOTH coordinates are named on purpose — GHA has two materializations, so which
// channel is part of every recognition question about it and omitting one asks an ill-posed one.
[[nodiscard]] ComposedSemantics github_only(std::string_view channel)
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::github::kDialect,
                                                            channel);
}

[[nodiscard]] ComposedSemantics undeclared_dialect(std::string_view channel)
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::kAnyDialect,
                                                            channel);
}

[[nodiscard]] std::string_view kind_str(IntentMarkerKind kind)
{
    switch (kind)
    {
    case IntentMarkerKind::None:
        return "None";
    case IntentMarkerKind::Job:
        return "Job";
    case IntentMarkerKind::Step:
        return "Step";
    }
    return "?";
}

[[nodiscard]] std::string show(const IntentMarker& mark)
{
    return std::string{"{kind="} + std::string{kind_str(mark.kind)} + ", name=\"" +
           std::string{mark.name} + "\"}";
}
} // namespace

TEST(GithubMarkers, RecognizesJobAndStepBanners)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kChannelStripped)};
    const auto job{recognize(norm_probe("Complete job name: build (ubuntu-latest)"), gh)};
    EXPECT_EQ(job.kind, IntentMarkerKind::Job) << "expected Job, got " << show(job);
    EXPECT_EQ(job.name, "build (ubuntu-latest)") << "raw job payload wrong: " << show(job);

    const auto step{recognize(norm_probe("Run actions/checkout@v4"), gh)};
    EXPECT_EQ(step.kind, IntentMarkerKind::Step) << "expected Step, got " << show(step);
    EXPECT_EQ(step.name, "actions/checkout@v4") << "raw step payload wrong: " << show(step);
}

// invariant: the stripped arm is OUR OWN lab ablation (`ci_revert_corpus.transform.degrade`), never
// bytes GitHub served, so this is NOT invariance across a dialect's real materializations.
TEST(GithubMarkers, StepIdentityIsInvariantAcrossTheRealChannelAndOurAblation)
{
    const ComposedSemantics annotated{github_only(insight::semantic::github::kChannelAnnotated)};
    const ComposedSemantics stripped{github_only(insight::semantic::github::kChannelStripped)};

    const auto wrapped{recognize(norm_probe("##[group]Run yarn lint"), annotated)};
    EXPECT_EQ(wrapped.kind, IntentMarkerKind::Step)
        << "the annotated channel's genuine banner was not recognized: " << show(wrapped);
    EXPECT_EQ(wrapped.name, "yarn lint")
        << "the wrapped payload must strip to the bare step name: " << show(wrapped);

    const auto bare{recognize(norm_probe("Run yarn lint"), stripped)};
    EXPECT_EQ(bare.kind, IntentMarkerKind::Step)
        << "the stripped channel's genuine banner was not recognized: " << show(bare);
    EXPECT_EQ(bare.name, wrapped.name) << "THE CHANNEL BECAME AN AXIS: the real channel and our "
                                          "ablation yielded different identities "
                                          "for one step (stripped=\""
                                       << bare.name << "\" annotated=\"" << wrapped.name
                                       << "\") — the "
                                          "lattice experiment compares exactly these two arms and "
                                          "would see every step vanished+new";

    const auto prose{recognize(norm_probe("Run `npm audit` for details."), annotated)};
    EXPECT_EQ(prose.kind, IntentMarkerKind::None)
        << "PHANTOM: bare `Run ` prose opened a Step in the ANNOTATED channel, where the genuine "
           "banner is "
           "`##[group]Run ` and this line is ordinary npm output: "
        << show(prose);
    const auto wrapped_in_stripped{recognize(norm_probe("##[group]Run yarn lint"), stripped)};
    EXPECT_EQ(wrapped_in_stripped.kind, IntentMarkerKind::None)
        << "the annotated banner fired under the STRIPPED channel, where `##[` cannot occur: "
        << show(wrapped_in_stripped);

    // assert: `::group::Run ` is not a shipped row, so this guards against a speculative one.
    const auto colon{recognize(norm_probe("::group::Run yarn lint"), annotated)};
    EXPECT_EQ(colon.kind, IntentMarkerKind::None)
        << "::group:: form unexpectedly recognized: " << show(colon);
}

TEST(GithubMarkers, UndeclaredChannelFiresNoStepRowEitherWay)
{
    const ComposedSemantics undeclared{github_only(insight::semantic::kAnyChannel)};

    EXPECT_EQ(recognize(norm_probe("Complete job name: build (ubuntu-latest)"), undeclared).kind,
              IntentMarkerKind::Job)
        << "the Job banner is kAnyChannel (identical in both channels) and must still fire "
           "undeclared";
    EXPECT_EQ(recognize(norm_probe("Run yarn lint"), undeclared).kind, IntentMarkerKind::None)
        << "a channel-gated Step row fired with NO channel declared — the composition defaulted "
           "to a concrete channel. An undeclared stream must never inherit one: guessing the "
           "channel is the fail-open defect the gate exists to close";
    EXPECT_EQ(recognize(norm_probe("##[group]Run yarn lint"), undeclared).kind,
              IntentMarkerKind::None)
        << "same, for the annotated materialization";
}

// refs: SRC-II-6
// invariant: the dialect gate is the stream's DECLARATION, taken once before the first line, and
// never the line's own content as a per-line detector's winner once was.
TEST(GithubMarkers, DialectGatedToTheDeclaringStream)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kChannelStripped)};
    constexpr std::string_view run_line{"Run daemon started"};

    const auto undeclared{recognize(
        norm_probe(run_line), undeclared_dialect(insight::semantic::github::kChannelStripped))};
    EXPECT_EQ(undeclared.kind, IntentMarkerKind::None)
        << "a dialect-gated Step row fired on a stream that declared NO dialect — fail-closed on "
           "depth is not optional: "
        << show(undeclared);

    EXPECT_EQ(recognize(norm_probe(run_line), gh).kind, IntentMarkerKind::Step)
        << "the SAME line must open a Step once the stream declares \""
        << insight::semantic::github::kDialect
        << "\" — the dialect declaration is the sole difference";
}

// assert: the Step prefix is `Run ` WITH its trailing space, so `Running …` is no banner.
TEST(GithubMarkers, NoFalseStepOnRunningOrEmpty)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kChannelStripped)};
    const auto running{recognize(norm_probe("Running database migrations"), gh)};
    EXPECT_EQ(running.kind, IntentMarkerKind::None)
        << "\"Running …\" false-matched a Step: " << show(running);

    const auto empty{recognize(norm_probe(""), gh)};
    EXPECT_EQ(empty.kind, IntentMarkerKind::None)
        << "empty content opened a quantum: " << show(empty);
}

// refs: ADR-18
TEST(GithubMarkers, JobUnorderedStepOrdered)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kChannelStripped)};
    const auto job{recognize(norm_probe("Complete job name: Test (ubuntu-latest)"), gh)};
    EXPECT_EQ(job.child_order, ChildOrder::Unordered) << "jobs are parallel → set-matched";
    EXPECT_EQ(job.discriminant, "(ubuntu-latest)") << "the marker carries its raw discriminant";

    const auto step{recognize(norm_probe("Run yarn build"), gh)};
    EXPECT_EQ(step.child_order, ChildOrder::Ordered) << "steps are sequential → LCS-matched";
}

// invariant: the COLLAPSE is canon's algorithm, pinned in canon's own suite; what is pinned here is
// that this package's recognition delivers the raw payload the collapse consumes.
TEST(GithubMarkers, RawPayloadFeedsAlignmentClass)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kChannelStripped)};
    const auto job{
        recognize(norm_probe("Complete job name: test (win-msvc, windows-latest, nightly)"), gh)};
    ASSERT_EQ(job.kind, IntentMarkerKind::Job) << show(job);
    EXPECT_EQ(canonicalize_intent(job.name), "test (M)")
        << "raw \"" << job.name << "\" did not collapse to the matrix class";

    const auto step{recognize(norm_probe("Run actions/checkout@v4"), gh)};
    ASSERT_EQ(step.kind, IntentMarkerKind::Step) << show(step);
    EXPECT_EQ(canonicalize_intent(step.name), "actions/checkout@vX")
        << "raw \"" << step.name << "\" did not collapse to the versioned-action class";
}
