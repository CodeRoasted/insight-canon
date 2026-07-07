// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_intent_marker.cpp — IntentMarkerRegistry::recognize, the §5.2 registry's
// segmentation rule class (intent_identity_model.md §5.2; canon 93287ac). On the
// STRIPPED stream Sift consumes, `Complete job name: X` opens a Job-scoped parent and
// `Run X` a Step quantum (job▸step, the finest RESET grain). The recognizer is
// FORMAT-GATED (II-6): the GitHub-Actions dialect never fires cross-format. Its raw
// payload composes with canonicalize_intent into the alignment CLASS. A diff here is a
// segmentation-contract break, not a retune — fix the code, never the assertion.

#include <gtest/gtest.h>

import insight.canon.test;

using insight::canonicalize_intent;
using insight::LogFormat;
using insight::to_string;
using insight::tokenization::IntentMarker;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::IntentMarkerRegistry;

namespace
{

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

// Verbose-on-failure render of the ACTUAL marker (kind + raw payload).
[[nodiscard]] std::string show(const IntentMarker& mark)
{
    return std::string{"{kind="} + std::string{kind_str(mark.kind)} + ", name=\"" +
           std::string{mark.name} + "\"}";
}

} // namespace

// ── The two RESET-class banners open their quanta; the payload is the RAW name ──
TEST(IntentMarkerRegistry, RecognizesJobAndStepBanners)
{
    const auto job{IntentMarkerRegistry::recognize("Complete job name: build (ubuntu-latest)",
                                                   LogFormat::GitHubActions)};
    EXPECT_EQ(job.kind, IntentMarkerKind::Job) << "expected Job, got " << show(job);
    EXPECT_EQ(job.name, "build (ubuntu-latest)") << "raw job payload wrong: " << show(job);

    const auto step{IntentMarkerRegistry::recognize("Run actions/checkout@v4", LogFormat::GitHubActions)};
    EXPECT_EQ(step.kind, IntentMarkerKind::Step) << "expected Step, got " << show(step);
    EXPECT_EQ(step.name, "actions/checkout@v4") << "raw step payload wrong: " << show(step);
}

// ── II-6: format-gated to GitHubActions — a dialect never fires cross-format ──
// The doc's `Run daemon started` phantom WOULD open a step under GHA; under every other
// format it must be inert. The gate is the ONLY difference (proven by the GHA sanity line).
TEST(IntentMarkerRegistry, FormatGatedToGitHubActions)
{
    constexpr std::string_view run_line{"Run daemon started"};
    for (const LogFormat fmt : {LogFormat::Unknown, LogFormat::RawText, LogFormat::JSON,
                                LogFormat::Syslog, LogFormat::KeyValue, LogFormat::Log4j,
                                LogFormat::CloudWatch})
    {
        const auto mark{IntentMarkerRegistry::recognize(run_line, fmt)};
        EXPECT_EQ(mark.kind, IntentMarkerKind::None)
            << "dialect fired cross-format on " << to_string(fmt) << ": " << show(mark);
    }
    // sanity: the SAME line opens a Step under GHA — the format gate is the sole difference
    EXPECT_EQ(IntentMarkerRegistry::recognize(run_line, LogFormat::GitHubActions).kind,
              IntentMarkerKind::Step);
}

// ── No false RESET: the step prefix is `Run ` WITH the trailing space; empty opens nothing ──
TEST(IntentMarkerRegistry, NoFalseStepOnRunningOrEmpty)
{
    const auto running{IntentMarkerRegistry::recognize("Running database migrations",
                                                       LogFormat::GitHubActions)};
    EXPECT_EQ(running.kind, IntentMarkerKind::None) << "\"Running …\" false-matched a Step: " << show(running);

    const auto empty{IntentMarkerRegistry::recognize("", LogFormat::GitHubActions)};
    EXPECT_EQ(empty.kind, IntentMarkerKind::None) << "empty content opened a quantum: " << show(empty);
}

// ── The payoff: recognizer(raw) → canonicalize_intent → alignment CLASS (§5.2→§5.1) ──
// The exact real-corpus case from 93287ac: a matrix job's raw name collapses to one class
// so its legs pair across runs; a versioned action collapses to its vX class.
TEST(IntentMarkerRegistry, RawPayloadComposesToAlignmentClass)
{
    const auto job{IntentMarkerRegistry::recognize(
        "Complete job name: test (win-msvc, windows-latest, nightly)", LogFormat::GitHubActions)};
    ASSERT_EQ(job.kind, IntentMarkerKind::Job) << show(job);
    EXPECT_EQ(canonicalize_intent(job.name), "test (M)")
        << "raw \"" << job.name << "\" did not collapse to the matrix class";

    const auto step{IntentMarkerRegistry::recognize("Run actions/checkout@v4", LogFormat::GitHubActions)};
    ASSERT_EQ(step.kind, IntentMarkerKind::Step) << show(step);
    EXPECT_EQ(canonicalize_intent(step.name), "actions/checkout@vX")
        << "raw \"" << step.name << "\" did not collapse to the versioned-action class";
}

// NOLINTEND
