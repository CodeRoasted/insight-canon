// refs: ADR-17, SRC-D-OUT-RUN-1
// invariant: this file guards the JENKINS run-outcome DATA and its end-to-end recognizability,
// never the resolution ladder — that is core's, over synthetic manifests.
// refs: F-SRC-insight-canon:test_run_outcome.cpp
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
// refs: DN-32.D6
// invariant: the UNFILTERED composition is what a caller-declared verdict resolves against: the
// vocabulary answers WHO SUPPLIED the verdict, so it must not be filtered by who WROTE the bytes.
// assert: `map_outcome_token_in` re-derives through `for_stream` itself, so handing it an already
// filtered view is a silent no-op — which is why these are two named helpers and not one.
[[nodiscard]] ComposedSemantics jenkins_vocabularies()
{
    return insight::semantic::compose(std::array{insight::semantic::jenkins::kManifest});
}

[[nodiscard]] ComposedSemantics jenkins_only()
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::jenkins::kDialect,
                                                            {});
}

// invariant: the same composition on a stream that declared NO dialect — the fail-closed arm.
[[nodiscard]] ComposedSemantics undeclared_stream()
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::kAnyDialect, {});
}
} // namespace

// refs: SRC-II-6
// invariant: UNSTABLE is its own class and is never folded into Failure or Success; NOT_BUILT is a
// MAPPING to Unknown rather than a miss, which is why its arm asserts a value is present.
// invariant: an unmapped token stays unmapped, and the map is dialect-gated: on a stream that
// declared no dialect the row is not in the view at all.
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
    EXPECT_FALSE(map_outcome_token("GREEN", composed).has_value());
    EXPECT_FALSE(map_outcome_token("SUCCESS", undeclared_stream()).has_value())
        << "a dialect's verdict token resolved on an UNDECLARED stream — the gate is fail-open";
}

// invariant: the whole-stream class declares its stamp as transport, so the caller peels through
// the declared stack and the scan receives the PEELED lines — the production shape.
// invariant: a stamp-only line peels blank and DROPS; there is no strategy inside the parser to
// strip a detection any more.
// assert: the fail-closed arm scans the SAME console UNDECLARED, stamps still in content, and
// recovers no verdict marker — an undeclared stamped stream never falls open to detection.
TEST(JenkinsOutcome, ConsoleTailRecoveredFromADeclaredWholeStreamConsole)
{
    const ComposedSemantics composed{jenkins_only()};
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

    const auto res{resolve_run_outcome({.token = ""}, scan, composed, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Unstable)
        << "the degenerate only-a-console-log path preserves the four-class verdict";

    const RunOutcomeScan raw_scan{scan_run_outcome(raw_lines, composed)};
    EXPECT_FALSE(raw_scan.marker_present)
        << "a stamped epilogue was recognized WITHOUT the declared peel — detection returned";
}

// assert: a freestyle console has no `[Pipeline]` skeleton and no stamp, so the epilogue alone
// carries the verdict — outcome depth is universal to any Jenkins job type.
TEST(JenkinsOutcome, BareFreestyleEpilogueStillResolves)
{
    const ComposedSemantics composed{jenkins_only()};
    const std::vector<std::string> lines{"checking out sources", "compiling", "Finished: ABORTED"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    ASSERT_TRUE(scan.marker_present);
    EXPECT_EQ(resolve_run_outcome({.token = ""}, scan, composed, composed).outcome,
              RunOutcome::Aborted);
}

// refs: SRC-D-OUT-RUN-1
// invariant: the ladder is a total PRECEDENCE, never a reconciliation: the authoritative side-input
// stands, the divergence is flagged, and neither side is a tiebreak.
// note: a present console verdict can be a nested outcome the whole-run API verdict overrides
// assert: this is a real present-but-divergent disagreement measured on a real console — API
// SUCCESS against a console `Finished: ABORTED`.
TEST(JenkinsOutcome, AuthoritativeSideInputOverridesDivergentConsole)
{
    const ComposedSemantics composed{jenkins_only()};
    const std::vector<std::string> lines{"[Pipeline] { (ci)", "nested build interrupted",
                                         "Finished: ABORTED"};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const auto res{resolve_run_outcome(
        {.token = "SUCCESS", .vocabulary = insight::semantic::jenkins::kManifest.name}, scan,
        composed, jenkins_vocabularies())};
    EXPECT_EQ(res.outcome, RunOutcome::Success);
    EXPECT_TRUE(res.authoritative);
    EXPECT_TRUE(res.divergent);
    EXPECT_EQ(res.console, RunOutcome::Aborted);
}
