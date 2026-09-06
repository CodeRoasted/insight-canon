// refs: F-SRC-insight-canon:test_run_outcome.cpp
// invariant: the resolution LADDER is core's — side-input, then the console tail's last match,
// then Unknown; this file guards only the GitHub DATA end of it.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::map_outcome_token;
using insight::resolve_run_outcome;
using insight::RunOutcome;
using insight::RunOutcomeScan;
using insight::scan_run_outcome;
using insight::semantic::ComposedSemantics;

namespace
{
// refs: DN-32.D6
// invariant: a caller-declared verdict resolves against the UNFILTERED vocabulary set, because it
// answers who SUPPLIED the verdict, not who WROTE the bytes.
// assert: `map_outcome_token_in` re-derives through `for_stream` itself, so handing it an
// already-filtered view is a silent no-op — which is why these are two named helpers.
[[nodiscard]] ComposedSemantics github_vocabularies()
{
    return insight::semantic::compose(std::array{insight::semantic::github::kManifest});
}

// invariant: the RESOLVED view of a stream that declared this dialect — the outcome vocabulary is
// reachable only through a declaration, never through per-line detection.
[[nodiscard]] ComposedSemantics github_only()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::github::kDialect,
                                                            {});
}

[[nodiscard]] ComposedSemantics undeclared_stream()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::kAnyDialect, {});
}

[[nodiscard]] std::vector<std::string> gha_console(bool failing)
{
    std::vector<std::string> lines{
        "2026-05-27T15:26:41.7842152Z ##[group]Run pytest -q",
        "2026-05-27T15:26:42.0000001Z collected 214 items",
    };
    if (failing)
    {
        lines.emplace_back("2026-05-27T15:26:59.1234567Z ##[error]Process completed with exit "
                           "code 1.");
    }
    return lines;
}
} // namespace

TEST(GithubOutcome, TheSevenNativeConclusionStringsMap)
{
    const ComposedSemantics composed{github_only()};
    EXPECT_EQ(map_outcome_token("success", composed), RunOutcome::Success);
    EXPECT_EQ(map_outcome_token("failure", composed), RunOutcome::Failure);
    // invariant: both non-completion conclusions are ONE class — an incomplete run whose log
    // stops early — so either one SUPPRESSES vanished-quantum alarms rather than raising them.
    EXPECT_EQ(map_outcome_token("cancelled", composed), RunOutcome::Aborted);
    EXPECT_EQ(map_outcome_token("timed_out", composed), RunOutcome::Aborted);
    // assert: a no-verdict conclusion MAPS to Unknown rather than missing, so rung 1 resolves and a
    // stale console tail is never consulted.
    for (const std::string_view token : {"skipped", "neutral", "action_required"})
    {
        const auto mapped{map_outcome_token(token, composed)};
        ASSERT_TRUE(mapped.has_value()) << "'" << token << "' must MAP (to Unknown), not miss";
        EXPECT_EQ(*mapped, RunOutcome::Unknown) << "'" << token << "' carries no pass/fail verdict";
    }
    // assert: GHA ships no native UNSTABLE string — the class is core's, and the Jenkins literal
    // must not leak into this dialect's vocabulary.
    EXPECT_FALSE(map_outcome_token("UNSTABLE", composed).has_value());
    // refs: SRC-II-6
    EXPECT_FALSE(map_outcome_token("SUCCESS", composed).has_value())
        << "GHA conclusions are lowercase — the uppercase form is Jenkins data, not GHA data";
    const ComposedSemantics undeclared{undeclared_stream()};
    EXPECT_FALSE(map_outcome_token("success", undeclared).has_value())
        << "a dialect's verdict token resolved on an UNDECLARED stream — the gate is fail-open";
}

TEST(GithubOutcome, NoMarkerMeansTheConsolePathIsHonestlyUnknown)
{
    const ComposedSemantics composed{github_only()};
    // note: `Process completed with exit code N` is PER-STEP — no console line is a run verdict
    const auto lines{gha_console(/*failing=*/true)};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    EXPECT_FALSE(scan.marker_present)
        << "GHA must ship NO console-tail marker (there is no run-verdict line to read) — got "
           "token '"
        << scan.token << "'";
    // invariant: there is no dialect LATCH — the dialect is declared, so `RunOutcomeScan` carries
    // no `LogFormat` at all and rung 2 falls to Unknown.
    const auto degenerate{resolve_run_outcome({.token = ""}, scan, composed, composed)};
    EXPECT_EQ(degenerate.outcome, RunOutcome::Unknown)
        << "only-a-console-log GHA is Unknown — never a guess from per-step exit codes";
    EXPECT_TRUE(degenerate.note.empty())
        << "absence is the default, not an error: " << degenerate.note;
}

TEST(GithubOutcome, AuthoritativeSideInputCarriesTheGhaVerdict)
{
    const ComposedSemantics composed{github_only()};
    // invariant: the Action forwards the job's own `result` output verbatim and untranslated, and
    // it is the ONLY channel a GHA verdict reaches the engine — no console corroboration is owed.
    const auto lines{gha_console(/*failing=*/false)};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const auto res{resolve_run_outcome(
        {.token = "cancelled", .vocabulary = insight::semantic::github::kManifest.name}, scan,
        composed, github_vocabularies())};
    EXPECT_EQ(res.outcome, RunOutcome::Aborted);
    EXPECT_TRUE(res.authoritative);
    EXPECT_FALSE(res.divergent) << "no console verdict exists — nothing to diverge from";
    EXPECT_TRUE(res.note.empty()) << res.note;
}
