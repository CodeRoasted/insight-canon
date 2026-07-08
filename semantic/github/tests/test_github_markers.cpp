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
// The composition under test: the github package alone. recognize() walks its composed marker rows.
[[nodiscard]] ComposedSemantics github_only()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests);
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
    const ComposedSemantics gh{github_only()};
    const auto job{recognize("Complete job name: build (ubuntu-latest)", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(job.kind, IntentMarkerKind::Job) << "expected Job, got " << show(job);
    EXPECT_EQ(job.name, "build (ubuntu-latest)") << "raw job payload wrong: " << show(job);

    const auto step{recognize("Run actions/checkout@v4", LogFormat::GitHubActions, gh)};
    EXPECT_EQ(step.kind, IntentMarkerKind::Step) << "expected Step, got " << show(step);
    EXPECT_EQ(step.name, "actions/checkout@v4") << "raw step payload wrong: " << show(step);
}

// ── II-6: format-gated to GitHubActions — the dialect never fires cross-format ──
// The gate is the ONLY difference (proven by the GHA sanity line). Post-split the gate is a ROW field
// (kMarkers[*].format_gate = GitHubActions); the walker's gate_matches enforces it.
TEST(GithubMarkers, FormatGatedToGitHubActions)
{
    const ComposedSemantics gh{github_only()};
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
    const ComposedSemantics gh{github_only()};
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
    const ComposedSemantics gh{github_only()};
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
    const ComposedSemantics gh{github_only()};
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
