// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_jenkins_outcome.cpp — the Jenkins run-outcome VOCABULARY (ADR 0025 §4, studies/006 Table 4):
// the five native `result` strings map into the core four-class RunOutcome, and the console-tail
// `Finished: <RESULT>` epilogue is recognized through this package's own strategy + marker row over
// a realistic mini console (timestamper-prefixed AND bare). The D-OUT-RUN-1 LADDER mechanics are
// core's (tests/compose/test_run_outcome.cpp, synthetic manifests) and the G-OUT-* gate suite is
// Kleio's — this file guards only the JENKINS DATA and its end-to-end recognizability.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.jenkins;

using insight::LogFormat;
using insight::map_outcome_token;
using insight::resolve_run_outcome;
using insight::RunOutcome;
using insight::RunOutcomeScan;
using insight::scan_run_outcome;
using insight::semantic::ComposedSemantics;

namespace
{
[[nodiscard]] ComposedSemantics jenkins_only()
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    return insight::semantic::compose(manifests);
}
} // namespace

TEST(JenkinsOutcome, TheFiveNativeResultStringsMap)
{
    const ComposedSemantics composed{jenkins_only()};
    EXPECT_EQ(map_outcome_token("SUCCESS", LogFormat::Jenkins, composed), RunOutcome::Success);
    EXPECT_EQ(map_outcome_token("FAILURE", LogFormat::Jenkins, composed), RunOutcome::Failure);
    EXPECT_EQ(map_outcome_token("UNSTABLE", LogFormat::Jenkins, composed), RunOutcome::Unstable)
        << "UNSTABLE is its own class — never folded to Failure or Success";
    EXPECT_EQ(map_outcome_token("ABORTED", LogFormat::Jenkins, composed), RunOutcome::Aborted);
    const auto not_built{map_outcome_token("NOT_BUILT", LogFormat::Jenkins, composed)};
    ASSERT_TRUE(not_built.has_value()) << "NOT_BUILT is a MAPPING (to Unknown), not a miss";
    EXPECT_EQ(*not_built, RunOutcome::Unknown);
    // Unmapped stays unmapped (fail-closed upstream), and the map is format-gated (II-6).
    EXPECT_FALSE(map_outcome_token("GREEN", LogFormat::Jenkins, composed).has_value());
    EXPECT_FALSE(map_outcome_token("SUCCESS", LogFormat::GitHubActions, composed).has_value());
}

TEST(JenkinsOutcome, ConsoleTailRecoveredFromARealisticConsole)
{
    const ComposedSemantics composed{jenkins_only()};
    // A ci.jenkins.io-shaped console: timestamper-prefixed skeleton + plain output + the epilogue.
    const std::vector<std::string> lines{
        "[2025-06-25T14:31:12.339Z] [Pipeline] { (Build)",
        "[2025-06-25T14:31:12.501Z] + mvn -B verify",
        "some plain output the strategy does not claim",
        "[2025-06-25T14:40:01.007Z] [Pipeline] End of Pipeline",
        "[2025-06-25T14:40:01.100Z] Finished: UNSTABLE",
    };
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    ASSERT_TRUE(scan.marker_present) << "the timestamper-prefixed epilogue must be recognized";
    EXPECT_EQ(scan.token, "UNSTABLE");
    EXPECT_EQ(scan.marker_format, LogFormat::Jenkins);
    EXPECT_EQ(scan.dialect, LogFormat::Jenkins) << "the dialect latches off the first Jenkins line";

    const auto res{resolve_run_outcome("", scan, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Unstable)
        << "the degenerate only-a-console-log path preserves the four-class verdict";
}

TEST(JenkinsOutcome, BareFreestyleEpilogueStillResolves)
{
    const ComposedSemantics composed{jenkins_only()};
    // A freestyle console: NO [Pipeline] skeleton, NO timestamper — the epilogue alone latches the
    // dialect and carries the verdict (outcome depth is universal to any Jenkins job, §8).
    const std::vector<std::string> lines{"checking out sources", "compiling", "Finished: ABORTED"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    ASSERT_TRUE(scan.marker_present);
    EXPECT_EQ(scan.dialect, LogFormat::Jenkins);
    EXPECT_EQ(resolve_run_outcome("", scan, composed).outcome, RunOutcome::Aborted);
}

TEST(JenkinsOutcome, AuthoritativeSideInputOverridesDivergentConsole)
{
    const ComposedSemantics composed{jenkins_only()};
    // The Accumulo #498 live counterexample (studies/006 v2-Table 4): API result SUCCESS vs console
    // `Finished: ABORTED` — a real present-but-divergent disagreement. D-OUT-RUN-1: the
    // authoritative side-input stands; the divergence is flagged, never a tiebreak. (The named
    // G-OUT-3 fixture is Kleio's; this guards the Jenkins DATA end of it.)
    const std::vector<std::string> lines{"[Pipeline] { (ci)", "nested build interrupted",
                                         "Finished: ABORTED"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const auto res{resolve_run_outcome("SUCCESS", scan, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Success);
    EXPECT_TRUE(res.authoritative);
    EXPECT_TRUE(res.divergent);
    EXPECT_EQ(res.console, RunOutcome::Aborted);
}
// NOLINTEND
