// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_github_markers.cpp — the GitHub-Actions INTENT-MARKER vocabulary (ADR 0024). Migrated from
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
using insight::LogFormat;
using insight::to_string;
using insight::semantic::ComposedSemantics;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarker;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

namespace
{
// The composition under test: the github package alone, viewed through ONE declared IntentChannel
// (ADR 0029). The channel is REQUIRED here on purpose — GHA is the dialect that actually has two
// materializations, so "which channel" is part of every recognition question about it, and a test
// that did not say would be asking an ill-posed one. `channel` names a declared channel
// ("annotated" / "stripped"); kAnyChannel models the caller that declares nothing (D5 —
// concretely-gated rows stay dark).
[[nodiscard]] ComposedSemantics github_only(std::string_view channel)
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_channel(channel);
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
    const auto job{
        recognize("Complete job name: build (ubuntu-latest)", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(job.kind, IntentMarkerKind::Job) << "expected Job, got " << show(job);
    EXPECT_EQ(job.name, "build (ubuntu-latest)") << "raw job payload wrong: " << show(job);

    const auto step{recognize("Run actions/checkout@v4", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(step.kind, IntentMarkerKind::Step) << "expected Step, got " << show(step);
    EXPECT_EQ(step.name, "actions/checkout@v4") << "raw step payload wrong: " << show(step);
}

// ── One Step intent, the real channel and our ablation, ONE identity (ADR 0030 D1/D6) ──
// The same step banner reads back to the same identity from GHA's real `##[group]Run <cmd>` and
// from the bare `Run <cmd>` our degrade() ablation produces. Each fires under ITS declared channel
// and both extract the IDENTICAL payload — which is what makes the channel a materialization detail
// and never an axis (D6).
//
// ⚠ WHAT THIS IS NOT (ADR 0030 D1 — the premise correction): this is NOT "materialization
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

    const auto wrapped{recognize("##[group]Run yarn lint", LogFormat::GitHubActions, annotated)};
    EXPECT_EQ(wrapped.kind, IntentMarkerKind::Step)
        << "the annotated channel's genuine banner was not recognized: " << show(wrapped);
    EXPECT_EQ(wrapped.name, "yarn lint")
        << "the wrapped payload must strip to the bare step name: " << show(wrapped);

    const auto bare{recognize("Run yarn lint", LogFormat::GitHubActions, stripped)};
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
    const auto prose{
        recognize("Run `npm audit` for details.", LogFormat::GitHubActions, annotated)};
    EXPECT_EQ(prose.kind, IntentMarkerKind::None)
        << "PHANTOM: bare `Run ` prose opened a Step in the ANNOTATED channel, where the genuine "
           "banner is "
           "`##[group]Run ` and this line is ordinary npm output: "
        << show(prose);
    const auto wrapped_in_stripped{
        recognize("##[group]Run yarn lint", LogFormat::GitHubActions, stripped)};
    EXPECT_EQ(wrapped_in_stripped.kind, IntentMarkerKind::None)
        << "the annotated banner fired under the STRIPPED channel, where `##[` cannot occur: "
        << show(wrapped_in_stripped);

    // `::group::Run ` is not a shipped row → it must NOT open a Step (guards against a speculative
    // row).
    const auto colon{recognize("::group::Run yarn lint", LogFormat::GitHubActions, annotated)};
    EXPECT_EQ(colon.kind, IntentMarkerKind::None)
        << "::group:: form unexpectedly recognized: " << show(colon);
}

// ── D5: an UNDECLARED channel fails closed on DEPTH — no phantom, and no structure either ──
// The caller that declares nothing gets the kAnyChannel rows (the Job banner) and NONE of the
// channel-gated
// Step rows: no dialect step structure, and — critically — no phantom either. Never a concrete
// default: "both Step rows live at once" IS the defect. Declaring the channel is the path to depth.
TEST(GithubMarkers, UndeclaredChannelFiresNoStepRowEitherWay)
{
    const ComposedSemantics undeclared{github_only(insight::semantic::kAnyChannel)};

    EXPECT_EQ(
        recognize("Complete job name: build (ubuntu-latest)", LogFormat::GitHubActions, undeclared)
            .kind,
        IntentMarkerKind::Job)
        << "the Job banner is kAnyChannel (identical in both channels) and must still fire "
           "undeclared";
    EXPECT_EQ(recognize("Run yarn lint", LogFormat::GitHubActions, undeclared).kind,
              IntentMarkerKind::None)
        << "a channel-gated Step row fired with NO channel declared — the composition defaulted to "
           "a "
           "concrete channel, which is exactly the fail-open defect ADR 0029 D5 closes";
    EXPECT_EQ(recognize("##[group]Run yarn lint", LogFormat::GitHubActions, undeclared).kind,
              IntentMarkerKind::None)
        << "same, for the annotated materialization";
}

// ── II-6: format-gated to GitHubActions — the dialect never fires cross-format ──
// The gate is the ONLY difference (proven by the GHA sanity line). Post-split the gate is a ROW
// field (kMarkers[*].format_gate = GitHubActions); the walker's gate_matches enforces it.
TEST(GithubMarkers, FormatGatedToGitHubActions)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kChannelStripped)};
    constexpr std::string_view run_line{"Run daemon started"};
    for (const LogFormat fmt :
         {LogFormat::Unknown, LogFormat::RawText, LogFormat::JSON, LogFormat::Syslog,
          LogFormat::KeyValue, LogFormat::Log4j, LogFormat::CloudWatch})
    {
        const auto mark{recognize(run_line, fmt, gh)};
        EXPECT_EQ(mark.kind, IntentMarkerKind::None)
            << "dialect fired cross-format on " << to_string(fmt) << ": " << show(mark);
    }
    EXPECT_EQ(recognize(run_line, LogFormat::GitHubActions, gh).kind, IntentMarkerKind::Step)
        << "the SAME line must open a Step under GHA — the format gate is the sole difference";
}

// ── No false RESET: the step prefix is `Run ` WITH the trailing space; empty opens nothing ──
TEST(GithubMarkers, NoFalseStepOnRunningOrEmpty)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kChannelStripped)};
    const auto running{recognize("Running database migrations", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(running.kind, IntentMarkerKind::None)
        << "\"Running …\" false-matched a Step: " << show(running);

    const auto empty{recognize("", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(empty.kind, IntentMarkerKind::None)
        << "empty content opened a quantum: " << show(empty);
}

// ── child_order is a declared per-level ROW property (ADR 0023 §2): job=Unordered, step=Ordered ──
// Migrated from test_instance_discriminant::JobUnorderedStepOrdered — it asserts THIS package's
// kMarkers child_order data (the level-typed alignment declaration), plus the marker carries its
// raw discriminant.
TEST(GithubMarkers, JobUnorderedStepOrdered)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kChannelStripped)};
    const auto job{
        recognize("Complete job name: Test (ubuntu-latest)", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(job.child_order, ChildOrder::Unordered) << "jobs are parallel → set-matched";
    EXPECT_EQ(job.discriminant, "(ubuntu-latest)") << "the marker carries its raw discriminant";

    const auto step{recognize("Run yarn build", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(step.child_order, ChildOrder::Ordered) << "steps are sequential → LCS-matched";
}

// ── The payoff: the RAW payload this package emits composes with canon's canonicalize_intent into
// the alignment CLASS (§5.2→§5.1). The COLLAPSE is canon's algorithm (pinned in canon's suite);
// here we pin that THIS package's recognition delivers the raw payload the collapse consumes — the
// composed contract.
TEST(GithubMarkers, RawPayloadFeedsAlignmentClass)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kChannelStripped)};
    const auto job{recognize("Complete job name: test (win-msvc, windows-latest, nightly)",
                             LogFormat::GitHubActions, gh)};
    ASSERT_EQ(job.kind, IntentMarkerKind::Job) << show(job);
    EXPECT_EQ(canonicalize_intent(job.name), "test (M)")
        << "raw \"" << job.name << "\" did not collapse to the matrix class";

    const auto step{recognize("Run actions/checkout@v4", LogFormat::GitHubActions, gh)};
    ASSERT_EQ(step.kind, IntentMarkerKind::Step) << show(step);
    EXPECT_EQ(canonicalize_intent(step.name), "actions/checkout@vX")
        << "raw \"" << step.name << "\" did not collapse to the versioned-action class";
}
// NOLINTEND
