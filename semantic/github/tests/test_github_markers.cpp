// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_github_markers.cpp — the GitHub-Actions INTENT-MARKER vocabulary (ADR 0024). Migrated from canon
// tests/identity/test_intent_marker.cpp: the recognition MECHANISM (insight::tokenization::recognize over
// the composed marker rows) is CANON's algorithm; the VOCABULARY it walks — `Complete job name: ` → Job
// (Unordered), `Run ` → Step (Ordered), format-gated to GitHubActions — is THIS package's kMarkers rows,
// so the knowledge test homes with the vocabulary. Exercised BLACK-BOX through the public facade
// (compose → recognize); no detail shard. A diff is a segmentation-contract break, not a retune — fix the
// rows or the algorithm, never the assertion. Determinism: byte-only recognition, no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;          // compose / ComposedSemantics / recognize / canonicalize_intent + enums
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
// The composition under test: the github package alone, viewed through ONE declared Sink (ADR 0028).
// The Sink is REQUIRED here on purpose — GHA is the dialect that actually has two materializations, so
// "which Sink" is part of every recognition question about it, and a test that did not say would be
// asking an ill-posed one. `sink` names a declared Sink ("annotated" / "stripped"); kAnySink models the
// caller that declares nothing (D5 — concretely-gated rows stay dark).
[[nodiscard]] ComposedSemantics github_only(std::string_view sink)
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_sink(sink);
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
    return std::string{"{kind="} + std::string{kind_str(mark.kind)} + ", name=\"" + std::string{mark.name} +
           "\"}";
}
} // namespace

// ── The two RESET-class banners open their quanta; the payload is the RAW name ──
TEST(GithubMarkers, RecognizesJobAndStepBanners)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kSinkStripped)};
    const auto job{recognize("Complete job name: build (ubuntu-latest)", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(job.kind, IntentMarkerKind::Job) << "expected Job, got " << show(job);
    EXPECT_EQ(job.name, "build (ubuntu-latest)") << "raw job payload wrong: " << show(job);

    const auto step{recognize("Run actions/checkout@v4", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(step.kind, IntentMarkerKind::Step) << "expected Step, got " << show(step);
    EXPECT_EQ(step.name, "actions/checkout@v4") << "raw step payload wrong: " << show(step);
}

// ── One Step intent, two SINKS, ONE identity — materialization invariance (ADR 0028 D4/G-SINK-1) ──
// The same step banner materializes as the runner's `##[group]Run <cmd>` in the ANNOTATED Sink and as
// the §5.3-stripped bare `Run <cmd>` in the STRIPPED one. Each fires under ITS declared Sink and both
// extract the IDENTICAL payload — which is what makes the Sink a materialization detail and never an
// axis: an annotated and a stripped rendering of the same build reach the same step identity, so a
// cross-Sink comparison across the pyramid axis (D4's legal case) sees no phantom churn.
//
// This is the unit-level statement of the property; G-SINK-1 asserts it end-to-end on real corpus pairs.
// It REPLACES the pre-0028 claim that shipping both prefixes UNGATED delivered this invariance: the
// payload half was true, but ungated recognition of the bare prefix is exactly what minted phantom Steps
// out of annotated prose. Invariance is preserved here — by two gated rows, not one ungated pair.
TEST(GithubMarkers, StepIdentityIsInvariantAcrossTheTwoSinks)
{
    const ComposedSemantics annotated{github_only(insight::semantic::github::kSinkAnnotated)};
    const ComposedSemantics stripped{github_only(insight::semantic::github::kSinkStripped)};

    const auto wrapped{recognize("##[group]Run yarn lint", LogFormat::GitHubActions, annotated)};
    EXPECT_EQ(wrapped.kind, IntentMarkerKind::Step)
        << "the annotated Sink's genuine banner was not recognized: " << show(wrapped);
    EXPECT_EQ(wrapped.name, "yarn lint")
        << "the wrapped payload must strip to the bare step name: " << show(wrapped);

    const auto bare{recognize("Run yarn lint", LogFormat::GitHubActions, stripped)};
    EXPECT_EQ(bare.kind, IntentMarkerKind::Step)
        << "the stripped Sink's genuine banner was not recognized: " << show(bare);
    EXPECT_EQ(bare.name, wrapped.name)
        << "THE SINK BECAME AN AXIS: the two materializations of one step yielded different identities "
           "(stripped=\"" << bare.name << "\" annotated=\"" << wrapped.name << "\") — a cross-Sink diff "
           "of the same build would report every step vanished+new";

    // Each Sink's OTHER form is not a banner there — the whole point of the coordinate.
    const auto prose{recognize("Run `npm audit` for details.", LogFormat::GitHubActions, annotated)};
    EXPECT_EQ(prose.kind, IntentMarkerKind::None)
        << "PHANTOM: bare `Run ` prose opened a Step in the ANNOTATED Sink, where the genuine banner is "
           "`##[group]Run ` and this line is ordinary npm output: " << show(prose);
    const auto wrapped_in_stripped{recognize("##[group]Run yarn lint", LogFormat::GitHubActions, stripped)};
    EXPECT_EQ(wrapped_in_stripped.kind, IntentMarkerKind::None)
        << "the annotated banner fired under the STRIPPED Sink, where `##[` cannot occur: "
        << show(wrapped_in_stripped);

    // `::group::Run ` is not a shipped row → it must NOT open a Step (guards against a speculative row).
    const auto colon{recognize("::group::Run yarn lint", LogFormat::GitHubActions, annotated)};
    EXPECT_EQ(colon.kind, IntentMarkerKind::None) << "::group:: form unexpectedly recognized: " << show(colon);
}

// ── D5: an UNDECLARED Sink fails closed on DEPTH — no phantom, and no structure either ──
// The caller that declares nothing gets the kAnySink rows (the Job banner) and NONE of the Sink-gated
// Step rows: no dialect step structure, and — critically — no phantom either. Never a concrete default:
// "both Step rows live at once" IS the defect. Declaring the Sink is the path to depth.
TEST(GithubMarkers, UndeclaredSinkFiresNoStepRowEitherWay)
{
    const ComposedSemantics undeclared{github_only(insight::semantic::kAnySink)};

    EXPECT_EQ(recognize("Complete job name: build (ubuntu-latest)", LogFormat::GitHubActions, undeclared).kind,
              IntentMarkerKind::Job)
        << "the Job banner is kAnySink (identical in both Sinks) and must still fire undeclared";
    EXPECT_EQ(recognize("Run yarn lint", LogFormat::GitHubActions, undeclared).kind, IntentMarkerKind::None)
        << "a Sink-gated Step row fired with NO Sink declared — the composition defaulted to a concrete "
           "Sink, which is exactly the fail-open defect ADR 0028 D5 closes";
    EXPECT_EQ(recognize("##[group]Run yarn lint", LogFormat::GitHubActions, undeclared).kind,
              IntentMarkerKind::None)
        << "same, for the annotated materialization";
}

// ── II-6: format-gated to GitHubActions — the dialect never fires cross-format ──
// The gate is the ONLY difference (proven by the GHA sanity line). Post-split the gate is a ROW field
// (kMarkers[*].format_gate = GitHubActions); the walker's gate_matches enforces it.
TEST(GithubMarkers, FormatGatedToGitHubActions)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kSinkStripped)};
    constexpr std::string_view run_line{"Run daemon started"};
    for (const LogFormat fmt : {LogFormat::Unknown, LogFormat::RawText, LogFormat::JSON, LogFormat::Syslog,
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
    const ComposedSemantics gh{github_only(insight::semantic::github::kSinkStripped)};
    const auto running{recognize("Running database migrations", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(running.kind, IntentMarkerKind::None) << "\"Running …\" false-matched a Step: " << show(running);

    const auto empty{recognize("", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(empty.kind, IntentMarkerKind::None) << "empty content opened a quantum: " << show(empty);
}

// ── child_order is a declared per-level ROW property (ADR 0023 §2): job=Unordered, step=Ordered ──
// Migrated from test_instance_discriminant::JobUnorderedStepOrdered — it asserts THIS package's kMarkers
// child_order data (the level-typed alignment declaration), plus the marker carries its raw discriminant.
TEST(GithubMarkers, JobUnorderedStepOrdered)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kSinkStripped)};
    const auto job{recognize("Complete job name: Test (ubuntu-latest)", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(job.child_order, ChildOrder::Unordered) << "jobs are parallel → set-matched";
    EXPECT_EQ(job.discriminant, "(ubuntu-latest)") << "the marker carries its raw discriminant";

    const auto step{recognize("Run yarn build", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(step.child_order, ChildOrder::Ordered) << "steps are sequential → LCS-matched";
}

// ── The payoff: the RAW payload this package emits composes with canon's canonicalize_intent into the
// alignment CLASS (§5.2→§5.1). The COLLAPSE is canon's algorithm (pinned in canon's suite); here we pin
// that THIS package's recognition delivers the raw payload the collapse consumes — the composed contract.
TEST(GithubMarkers, RawPayloadFeedsAlignmentClass)
{
    const ComposedSemantics gh{github_only(insight::semantic::github::kSinkStripped)};
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
