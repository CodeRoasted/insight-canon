// refs: ADR-17, SRC-D-OUT-RUN-1
// invariant: every terminal-line form here is verbatim from marker_corpus_v1 — the run-verdict
// vocabulary was measured on real traces, not invented.
// note: determinism — byte-only scan plus an integer line index, no RNG, clock or float
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
// refs: DN-32.D6
// invariant: a side-input verdict resolves against the DECLARER's vocabulary, so the UNFILTERED
// composition is what a caller-declared verdict maps in.
// assert: `map_outcome_token_in` re-derives through `for_stream` itself, so handing it an already
// filtered view is a silent no-op — which is why these are two named helpers.
[[nodiscard]] ComposedSemantics gitlab_vocabularies()
{
    return insight::semantic::compose(std::array{insight::semantic::gitlab::kManifest});
}

[[nodiscard]] ComposedSemantics gitlab_only()
{
    const std::array manifests{insight::semantic::gitlab::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::gitlab::kDialect,
                                                            {});
}
} // namespace

TEST(GitLabOutcome, TheApiStatusVocabularyMapsToTheFourClassModel)
{
    const ComposedSemantics composed{gitlab_only()};
    EXPECT_EQ(map_outcome_token("success", composed), RunOutcome::Success);
    EXPECT_EQ(map_outcome_token("failed", composed), RunOutcome::Failure);
    EXPECT_EQ(map_outcome_token("canceled", composed), RunOutcome::Aborted);
    // invariant: a row mapping TO Unknown still MAPS — engaged optional, value Unknown; an ABSENT
    // row instead produces a fail-closed note about a token this dialect does define.
    const auto skipped{map_outcome_token("skipped", composed)};
    ASSERT_TRUE(skipped.has_value());
    EXPECT_EQ(*skipped, RunOutcome::Unknown);
    const auto manual{map_outcome_token("manual", composed)};
    ASSERT_TRUE(manual.has_value());
    EXPECT_EQ(*manual, RunOutcome::Unknown);
    EXPECT_FALSE(map_outcome_token("CANCELED", composed).has_value());
    EXPECT_FALSE(map_outcome_token("cancelled", composed).has_value());
    EXPECT_FALSE(map_outcome_token("SUCCESS", composed).has_value());
}

TEST(GitLabOutcome, NoUnstableRowExists)
{
    const ComposedSemantics composed{gitlab_only()};
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
    EXPECT_EQ(resolve_run_outcome({}, scan, composed, composed).outcome, RunOutcome::Success);
}

TEST(GitLabOutcome, TheFailureTailHasAFreeFormRemainderAndStillResolves)
{
    const ComposedSemantics composed{gitlab_only()};
    // assert: not one of the 144 API-`failed` traces has a single-word remainder, so a
    // Jenkins-shaped row set left every one of them unrecognizable.
    for (const std::string_view tail :
         {"ERROR: Job failed: exit code 1", "ERROR: Job failed: exit status 137",
          "ERROR: Job failed (system failure): prepare environment", "ERROR: Job failed"})
    {
        const std::vector<std::string> lines{"building", std::string{tail}};
        const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
        ASSERT_TRUE(scan.marker_present) << "unrecognized terminal line: " << tail;
        EXPECT_EQ(resolve_run_outcome({}, scan, composed, composed).outcome, RunOutcome::Failure)
            << "terminal line: " << tail;
    }
}

TEST(GitLabOutcome, ACancellationAnnouncedWithTheFailurePrefixResolvesToAborted)
{
    const ComposedSemantics composed{gitlab_only()};
    // assert: longest-VALID-prefix-wins is what makes this work, and it must not depend on where
    // the three rows sit in the array.
    const std::vector<std::string> lines{"building", "ERROR: Job failed: canceled"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    ASSERT_TRUE(scan.marker_present);
    EXPECT_EQ(resolve_run_outcome({}, scan, composed, composed).outcome, RunOutcome::Aborted)
        << "GitLab announces a CANCELLATION with the FAILURE prefix — reading it as Failure is a "
           "wrong verdict, not a missing one (17 of 25 cancelled jobs in marker_corpus_v1)";
}

TEST(GitLabOutcome, TheApiResultOutranksADivergentConsoleTail)
{
    const ComposedSemantics composed{gitlab_only()};
    // invariant: the ladder is a TOTAL precedence — the authoritative side-input wins and the
    // divergence is FLAGGED, never a tiebreak (2 of the 25 cancelled jobs end on `Job succeeded`).
    const std::vector<std::string> lines{"working", "Job succeeded"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const auto resolution{resolve_run_outcome(
        {.token = "canceled", .vocabulary = insight::semantic::gitlab::kManifest.name}, scan,
        composed, gitlab_vocabularies())};
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
    // invariant: canon trims leading ASCII whitespace before the outcome walk, so a QUOTED or
    // NESTED banner DOES match; what contains it is that the LAST matching line wins.
    // note: a reader assuming anchoring alone excluded the nested case would be wrong
    const std::vector<std::string> quoted_then_real{"  ERROR: Job failed: exit code 1",
                                                    "retrying the child job", "Job succeeded"};
    EXPECT_EQ(
        resolve_run_outcome({}, scan_run_outcome(quoted_then_real, composed), composed, composed)
            .outcome,
        RunOutcome::Success);
}
