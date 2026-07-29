// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_gitlab_outcome.cpp — the GitLab run-verdict VOCABULARY (ADR 0025 §4, studies/012 §1.4).
// What it guards:
//   the API `status` token map — success/failed/canceled/skipped/manual, with the last two mapping
//     to Unknown DELIBERATELY (the Jenkins NOT_BUILT precedent: a known token that carries no
//     verdict, not an absent row);
//   NO Unstable row — GitLab has no native UNSTABLE and `allow_failure` is not silently one;
//   the three PrefixIsVerdict console-tail rows, and in particular THE CANCELLATION FINDING:
//     GitLab announces a cancel with the FAILURE prefix (`ERROR: Job failed: canceled`), 17 of the
//     25 cancelled jobs in marker_corpus_v1. A Jenkins-shaped row set reads all 17 as Failure — a
//     WRONG verdict, not a missing one — and longest-prefix-wins is what resolves it;
//   D-OUT-RUN-1 — the API result is authoritative, the console tail is the degenerate fallback, and
//     the measured divergence (2 cancelled jobs ending on `Job succeeded`) is exactly what that
//     precedence is for.
// Every terminal-line form here was taken from marker_corpus_v1. Determinism: byte-only scan +
// integer line index; no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.gitlab;

using insight::map_outcome_token;
using insight::resolve_run_outcome;
using insight::RunOutcome;
using insight::RunOutcomeScan;
using insight::scan_run_outcome;
using insight::semantic::ComposedSemantics;

namespace
{
[[nodiscard]] ComposedSemantics gitlab_only()
{
    const std::array manifests{insight::semantic::gitlab::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::gitlab::kDialect, {});
}
} // namespace

TEST(GitLabOutcome, TheApiStatusVocabularyMapsToTheFourClassModel)
{
    const ComposedSemantics composed{gitlab_only()};
    EXPECT_EQ(map_outcome_token("success", composed), RunOutcome::Success);
    EXPECT_EQ(map_outcome_token("failed", composed), RunOutcome::Failure);
    EXPECT_EQ(map_outcome_token("canceled", composed), RunOutcome::Aborted);
    // A row mapping TO Unknown still MAPS — engaged optional, value Unknown. The distinction from an
    // ABSENT row is the whole point: absence produces a fail-closed note about a token this dialect
    // does in fact define.
    const auto skipped{map_outcome_token("skipped", composed)};
    ASSERT_TRUE(skipped.has_value());
    EXPECT_EQ(*skipped, RunOutcome::Unknown);
    const auto manual{map_outcome_token("manual", composed)};
    ASSERT_TRUE(manual.has_value());
    EXPECT_EQ(*manual, RunOutcome::Unknown);
    // Byte-exact: GitLab's token is lowercase `canceled` (one 'l'), and it is not Jenkins's.
    EXPECT_FALSE(map_outcome_token("CANCELED", composed).has_value());
    EXPECT_FALSE(map_outcome_token("cancelled", composed).has_value());
    EXPECT_FALSE(map_outcome_token("SUCCESS", composed).has_value());
}

TEST(GitLabOutcome, NoUnstableRowExists)
{
    const ComposedSemantics composed{gitlab_only()};
    // GitLab has no native UNSTABLE. Composing one out of `allow_failure` is a product decision
    // about what a partial success MEANS, and it does not get made silently in a row table.
    for (const auto& row : insight::semantic::gitlab::kManifest.outcome_tokens)
        EXPECT_NE(row.outcome, RunOutcome::Unstable)
            << "token '" << row.token << "' claims Unstable — GitLab has no such verdict";
}

TEST(GitLabOutcome, TheSuccessTailResolvesWithNoRemainder)
{
    const ComposedSemantics composed{gitlab_only()};
    const std::vector<std::string> lines{"Running with gitlab-runner 18.9.0", "Job succeeded"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    ASSERT_TRUE(scan.marker_present);
    EXPECT_EQ(resolve_run_outcome({}, scan, composed).outcome, RunOutcome::Success);
}

TEST(GitLabOutcome, TheFailureTailHasAFreeFormRemainderAndStillResolves)
{
    const ComposedSemantics composed{gitlab_only()};
    // These four forms are the whole reason OutcomeMarkerShape exists: not one of them has a
    // single-word remainder, so under the shipped Jenkins shape ALL of them were unrecognizable —
    // 144 of 619 corpus traces.
    for (const std::string_view tail : {"ERROR: Job failed: exit code 1",
                                        "ERROR: Job failed: exit status 137",
                                        "ERROR: Job failed (system failure): prepare environment",
                                        "ERROR: Job failed"})
    {
        const std::vector<std::string> lines{"building", std::string{tail}};
        const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
        ASSERT_TRUE(scan.marker_present) << "unrecognized terminal line: " << tail;
        EXPECT_EQ(resolve_run_outcome({}, scan, composed).outcome, RunOutcome::Failure)
            << "terminal line: " << tail;
    }
}

TEST(GitLabOutcome, ACancellationAnnouncedWithTheFailurePrefixResolvesToAborted)
{
    const ComposedSemantics composed{gitlab_only()};
    // THE finding studies/012 owed this package. Longest-prefix-wins is what makes it work, and it
    // must not depend on where the three rows sit in the array.
    const std::vector<std::string> lines{"building", "ERROR: Job failed: canceled"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    ASSERT_TRUE(scan.marker_present);
    EXPECT_EQ(resolve_run_outcome({}, scan, composed).outcome, RunOutcome::Aborted)
        << "GitLab announces a CANCELLATION with the FAILURE prefix — reading it as Failure is a "
           "wrong verdict, not a missing one (17 of 25 cancelled jobs in marker_corpus_v1)";
}

TEST(GitLabOutcome, TheApiResultOutranksADivergentConsoleTail)
{
    const ComposedSemantics composed{gitlab_only()};
    // The measured divergence: 2 cancelled jobs end on `Job succeeded`. D-OUT-RUN-1 says the
    // authoritative side-input wins and the divergence is FLAGGED, never a tiebreak.
    const std::vector<std::string> lines{"working", "Job succeeded"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const auto resolution{resolve_run_outcome("canceled", scan, composed)};
    EXPECT_EQ(resolution.outcome, RunOutcome::Aborted);
    EXPECT_TRUE(resolution.authoritative);
    EXPECT_TRUE(resolution.divergent);
    EXPECT_EQ(resolution.console, RunOutcome::Success);
}

TEST(GitLabOutcome, OrdinaryProseIsNotATerminalVerdict)
{
    const ComposedSemantics composed{gitlab_only()};
    const std::vector<std::string> lines{"the job failed to find a compiler",
                                         "make: *** [Makefile:12: all] Error 1",
                                         "Cleaning up project directory and file based variables"};
    EXPECT_FALSE(scan_run_outcome(lines, composed).marker_present)
        << "a mention of failure is not GitLab's terminal line; only the anchored banner is";
}

TEST(GitLabOutcome, AnIndentedBannerMatchesAndTheLastMatchIsWhatSaves)
{
    const ComposedSemantics composed{gitlab_only()};
    // STATE, not apology. Canon trims leading ASCII whitespace before the outcome walk
    // (RawTextStrategy, so that indented continuation lines group with their peers), so a QUOTED or
    // NESTED verdict banner does match the row — this dialect inherits that from core and does not
    // get to opt out of it. What contains it is the rule the scan already carries: the LAST matching
    // line wins, and a real GitLab trace ends on its own banner. Asserted rather than left implicit,
    // because a reader who assumed anchoring alone excluded the nested case would be wrong.
    const std::vector<std::string> quoted_then_real{"  ERROR: Job failed: exit code 1",
                                                    "retrying the child job", "Job succeeded"};
    EXPECT_EQ(resolve_run_outcome({}, scan_run_outcome(quoted_then_real, composed), composed)
                  .outcome,
              RunOutcome::Success);
}
// NOLINTEND
