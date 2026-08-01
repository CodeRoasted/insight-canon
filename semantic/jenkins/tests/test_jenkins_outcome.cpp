// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_jenkins_outcome.cpp — the Jenkins run-outcome VOCABULARY (ADR-17, studies/006 Table 4):
// the five native `result` strings map into the core four-class RunOutcome, and the console-tail
// `Finished: <RESULT>` epilogue is recognized through this package's own strategy + marker row over
// a realistic mini console (timestamper-prefixed AND bare). The D-OUT-RUN-1 LADDER mechanics are
// core's (tests/compose/test_run_outcome.cpp, synthetic manifests) and the G-OUT-* gate suite is
// Kleio's — this file guards only the JENKINS DATA and its end-to-end recognizability.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.jenkins;

using insight::map_outcome_token;
using insight::resolve_run_outcome;
using insight::RunOutcome;
using insight::RunOutcomeScan;
using insight::scan_run_outcome;
using insight::semantic::ComposedSemantics;

namespace
{
// The RESOLVED view of a stream that declared this dialect (ADR-22) — after T4 the
// concretely-gated rows are reachable only through a declaration.
[[nodiscard]] ComposedSemantics jenkins_only()
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::jenkins::kDialect,
                                                            {});
}

// The same composition on a stream that declared NO dialect — the fail-closed arm.
[[nodiscard]] ComposedSemantics undeclared_stream()
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::kAnyDialect, {});
}
} // namespace

TEST(JenkinsOutcome, TheFiveNativeResultStringsMap)
{
    const ComposedSemantics composed{jenkins_only()};
    EXPECT_EQ(map_outcome_token("SUCCESS", composed), RunOutcome::Success);
    EXPECT_EQ(map_outcome_token("FAILURE", composed), RunOutcome::Failure);
    EXPECT_EQ(map_outcome_token("UNSTABLE", composed), RunOutcome::Unstable)
        << "UNSTABLE is its own class — never folded to Failure or Success";
    EXPECT_EQ(map_outcome_token("ABORTED", composed), RunOutcome::Aborted);
    const auto not_built{map_outcome_token("NOT_BUILT", composed)};
    ASSERT_TRUE(not_built.has_value()) << "NOT_BUILT is a MAPPING (to Unknown), not a miss";
    EXPECT_EQ(*not_built, RunOutcome::Unknown);
    // Unmapped stays unmapped (fail-closed upstream), and the map is DIALECT-gated (SRC-II-6): on a
    // stream that declared no dialect the row is not in the view at all.
    EXPECT_FALSE(map_outcome_token("GREEN", composed).has_value());
    EXPECT_FALSE(map_outcome_token("SUCCESS", undeclared_stream()).has_value())
        << "a dialect's verdict token resolved on an UNDECLARED stream — the gate is fail-open";
}

TEST(JenkinsOutcome, ConsoleTailRecoveredFromADeclaredWholeStreamConsole)
{
    const ComposedSemantics composed{jenkins_only()};
    // A ci.jenkins.io-shaped console: timestamper-prefixed skeleton + plain output + the epilogue.
    // POST-T5-5.2 this is the WHOLE-STREAM class and its stamp is DECLARED transport
    // (`bracket-rfc3339-line-prefix`): the caller peels through the declared stack and the scan
    // receives the PEELED lines — the production shape (there is no strategy inside the parser to
    // strip detections any more; blank peels DROP).
    const std::vector<std::string> raw_lines{
        "[2025-06-25T14:31:12.339Z] [Pipeline] { (Build)",
        "[2025-06-25T14:31:12.501Z] + mvn -B verify",
        "some plain output nobody claims",
        "[2025-06-25T14:40:01.007Z] [Pipeline] End of Pipeline",
        "[2025-06-25T14:40:01.100Z] Finished: UNSTABLE",
    };
    const std::array<std::string_view, 1> declared_names{"bracket-rfc3339-line-prefix"};
    const insight::transport::TransportStack stack{insight::transport::resolve_transport_stack(
        insight::transport::IngestDeclaration{.stack = declared_names})};
    std::vector<std::string> peeled_lines;
    for (const std::string& line : raw_lines)
    {
        const insight::transport::RawPeeledLine peeled{stack.peel_raw(line)};
        if (!peeled.content.empty())
            peeled_lines.emplace_back(peeled.content);
    }
    const RunOutcomeScan scan{scan_run_outcome(peeled_lines, composed)};
    ASSERT_TRUE(scan.marker_present) << "the declared-peel epilogue must be recognized";
    EXPECT_EQ(scan.token, "UNSTABLE");

    const auto res{resolve_run_outcome("", scan, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Unstable)
        << "the degenerate only-a-console-log path preserves the four-class verdict";

    // The FAIL-CLOSED arm (T5 §4 item 4 — the ruling's intent, not its price): the SAME console
    // scanned UNDECLARED (raw, stamps in content) recovers NO verdict marker — an undeclared
    // stamped stream yields nothing, it never falls open to detection.
    const RunOutcomeScan raw_scan{scan_run_outcome(raw_lines, composed)};
    EXPECT_FALSE(raw_scan.marker_present)
        << "a stamped epilogue was recognized WITHOUT the declared peel — detection returned";
}

TEST(JenkinsOutcome, BareFreestyleEpilogueStillResolves)
{
    const ComposedSemantics composed{jenkins_only()};
    // A freestyle console: NO [Pipeline] skeleton, NO timestamper — the epilogue alone latches the
    // dialect and carries the verdict (outcome depth is universal to any Jenkins job).
    const std::vector<std::string> lines{"checking out sources", "compiling", "Finished: ABORTED"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    ASSERT_TRUE(scan.marker_present);
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
