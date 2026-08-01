// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_github_markers.cpp — the GitHub-Actions INTENT-MARKER vocabulary. Migrated from
// canon tests/identity/test_intent_marker.cpp: the recognition MECHANISM
// (insight::tokenization::recognize over the composed marker rows) is CANON's algorithm; the
// VOCABULARY it walks — `Complete job name: ` → Job (Unordered), `Run ` → Step (Ordered),
// format-gated to GitHubActions — is THIS package's kMarkers rows, so the knowledge test homes with
// the vocabulary. Exercised BLACK-BOX through the public facade (compose → recognize); no detail
// shard. A diff is a segmentation-contract break, not a retune — fix the rows or the algorithm,
// never the assertion. Determinism: byte-only recognition, no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon; // compose / ComposedSemantics / recognize / canonicalize_intent + enums
import insight.semantic.github; // kManifest

using insight::canonicalize_intent;
using insight::to_string;
using insight::semantic::ComposedSemantics;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarker;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

// The walkers take NormalizedContent — canon's ingest-normalization precondition carried by a type
// unforgeable outside canon; every probe here is an escape-free literal, so normalize() is the
// zero-copy fixed point over a shared scratch.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

namespace
{
// The composition under test: the github package alone, RESOLVED for a stream that declared this
// dialect and ONE IntentChannel. Both coordinates are REQUIRED here
// on purpose — GHA is the dialect that actually has two materializations, so "which channel" is
// part of every recognition question about it, and after T4 "which dialect" is part of every one
// too. A test that did not say would be asking an ill-posed question. `channel` names a declared
// channel ("annotated" / "stripped"); kAnyChannel models the caller that declares no channel —
// concretely-gated rows stay dark.
[[nodiscard]] ComposedSemantics github_only(std::string_view channel)
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::github::kDialect,
                                                            channel);
}

// The same composition on a stream that declared NO dialect — the fail-closed arm.
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

// ── The two RESET-class banners open their quanta; the payload is the RAW name ──
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

// ── One Step intent, the real channel and our ablation, ONE identity ──
// The same step banner reads back to the same identity from GHA's real `##[group]Run <cmd>` and
// from the bare `Run <cmd>` our degrade() ablation produces. Each fires under ITS declared channel
// and both extract the IDENTICAL payload — which is what makes the channel a materialization detail
// and never an axis.
//
// ⚠ WHAT THIS IS NOT: this is NOT "materialization
// invariance across two real GHA materializations". The stripped arm is OUR OWN lab ablation
// (ci_revert_corpus.transform.degrade()), not bytes GitHub ever served. The property asserted here
// is canon reading our ablation back to the same intent as the real channel — genuinely
// load-bearing (the template-lattice lift experiment depends on exactly this), but it must never be
// claimed as invariance across a dialect's real materializations. Naming it so would be the
// endogamy trap the corpus discipline exists to prevent.
//
// It REPLACES the pre-0028 claim that shipping both prefixes UNGATED delivered this: the payload
// half was true, but ungated recognition of the bare prefix is exactly what minted phantom Steps
// out of annotated prose. The property is preserved here — by two gated rows, not one ungated pair.
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

    // Each channel's OTHER form is not a banner there — the whole point of the coordinate.
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

    // `::group::Run ` is not a shipped row → it must NOT open a Step (guards against a speculative
    // row).
    const auto colon{recognize(norm_probe("::group::Run yarn lint"), annotated)};
    EXPECT_EQ(colon.kind, IntentMarkerKind::None)
        << "::group:: form unexpectedly recognized: " << show(colon);
}

// ── An UNDECLARED channel fails closed on DEPTH — no phantom, and no structure either ──────
// The caller that declares nothing gets the kAnyChannel rows (the Job banner) and NONE of the
// channel-gated
// Step rows: no dialect step structure, and — critically — no phantom either. Never a concrete
// default: "both Step rows live at once" IS the defect. Declaring the channel is the path to depth.
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

// ── SRC-II-6: DIALECT-gated to this package — the dialect never fires on a stream that did not
// declare it. The gate is the ONLY difference (proven by the declared sanity
// line).
//
// ⚠ WHAT CHANGED AND WHY IT MATTERS. This test used to loop over `LogFormat` values, passing each
// as `recognize`'s gate — and in production that argument was `LogParser::routed_format()`, the
// per-line detector winner under a sticky-strategy fast path. So "does the GHA dialect fire?" was
// answered by the line's own CONTENT. It is now answered by the stream's DECLARATION, once, before
// the first line. The undeclared arm is the one that would catch a filter that silently kept
// everything.
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

// ── No false RESET: the step prefix is `Run ` WITH the trailing space; empty opens nothing ──
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

// ── child_order is a declared per-level ROW property: job=Unordered, step=Ordered ───────────
// Migrated from test_instance_discriminant::JobUnorderedStepOrdered — it asserts THIS package's
// kMarkers child_order data (the level-typed alignment declaration), plus the marker carries its
// raw discriminant.
TEST(GithubMarkers, JobUnorderedStepOrdered)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kChannelStripped)};
    const auto job{recognize(norm_probe("Complete job name: Test (ubuntu-latest)"), gh)};
    EXPECT_EQ(job.child_order, ChildOrder::Unordered) << "jobs are parallel → set-matched";
    EXPECT_EQ(job.discriminant, "(ubuntu-latest)") << "the marker carries its raw discriminant";

    const auto step{recognize(norm_probe("Run yarn build"), gh)};
    EXPECT_EQ(step.child_order, ChildOrder::Ordered) << "steps are sequential → LCS-matched";
}

// ── The payoff: the RAW payload this package emits composes with canon's canonicalize_intent into
// the alignment CLASS — the aggressive canonical mask over the raw marker name, which is what
// groups siblings. The COLLAPSE is canon's algorithm (pinned in canon's suite);
// here we pin that THIS package's recognition delivers the raw payload the collapse consumes — the
// composed contract.
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
// NOLINTEND
