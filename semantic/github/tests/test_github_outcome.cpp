// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_github_outcome.cpp — the GHA run-outcome VOCABULARY (ADR 0025 §4, the G-OUT-1 GHA half):
// the seven native `result`/`conclusion` strings map into the core four-class RunOutcome, the
// no-verdict conclusions (skipped/neutral/action_required) MAP to Unknown (never a guess, never a
// miss), and the package honestly ships NO OutcomeMarkerRow — GHA has no run-verdict console line
// (`Process completed with exit code N` is per-step), so the degenerate console path is Unknown and
// only the authoritative side-input carries a GHA verdict. The D-OUT-RUN-1 LADDER mechanics are
// core's (canon tests/compose/test_run_outcome.cpp); this file guards the GITHUB DATA end.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::LogFormat;
using insight::map_outcome_token;
using insight::resolve_run_outcome;
using insight::RunOutcome;
using insight::RunOutcomeScan;
using insight::scan_run_outcome;
using insight::semantic::ComposedSemantics;

namespace
{
[[nodiscard]] ComposedSemantics github_only()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests);
}

// A genuine GHA console slice: the 7-digit-fractional-Z timestamper + workflow commands — the
// shapes the composed FormatDetector routes to GitHubActions (the dialect latch's input).
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
    // The pass↔fail axis.
    EXPECT_EQ(map_outcome_token("success", LogFormat::GitHubActions, composed),
              RunOutcome::Success);
    EXPECT_EQ(map_outcome_token("failure", LogFormat::GitHubActions, composed),
              RunOutcome::Failure);
    // Both non-completion conclusions are the SAME class: an incomplete run (log truncated at the
    // stop point) — the §6.3 suppression semantics apply to either.
    EXPECT_EQ(map_outcome_token("cancelled", LogFormat::GitHubActions, composed),
              RunOutcome::Aborted);
    EXPECT_EQ(map_outcome_token("timed_out", LogFormat::GitHubActions, composed),
              RunOutcome::Aborted);
    // The no-verdict conclusions MAP (engaged optional) to Unknown — a resolution, not a miss:
    // rung 1 resolves and a stale console tail is never consulted (the NOT_BUILT shape).
    for (const std::string_view token : {"skipped", "neutral", "action_required"})
    {
        const auto mapped{map_outcome_token(token, LogFormat::GitHubActions, composed)};
        ASSERT_TRUE(mapped.has_value()) << "'" << token << "' must MAP (to Unknown), not miss";
        EXPECT_EQ(*mapped, RunOutcome::Unknown) << "'" << token << "' carries no pass/fail verdict";
    }
    // GHA has no native UNSTABLE string — the category is core, this dialect ships no row for it,
    // and the Jenkins literal must NOT leak in (fail-closed upstream surfaces the note).
    EXPECT_FALSE(map_outcome_token("UNSTABLE", LogFormat::GitHubActions, composed).has_value());
    // Byte-exact + format-gated (II-6): the GHA strings are lowercase and GHA-only.
    EXPECT_FALSE(map_outcome_token("SUCCESS", LogFormat::GitHubActions, composed).has_value())
        << "GHA conclusions are lowercase — the uppercase form is Jenkins data, not GHA data";
    EXPECT_FALSE(map_outcome_token("success", LogFormat::Jenkins, composed).has_value());
}

TEST(GithubOutcome, NoMarkerMeansTheConsolePathIsHonestlyUnknown)
{
    const ComposedSemantics composed{github_only()};
    // Even a FAILING GHA console carries no run-verdict line — `Process completed with exit code N`
    // is per-step. The package ships no OutcomeMarkerRow, so the scan finds nothing…
    const auto lines{gha_console(/*failing=*/true)};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    EXPECT_FALSE(scan.marker_present)
        << "GHA must ship NO console-tail marker (there is no run-verdict line to read) — got "
           "token '"
        << scan.token << "'";
    // …but the dialect still latches (the side-input needs it), and rung 2 falls to Unknown.
    EXPECT_EQ(scan.dialect, LogFormat::GitHubActions)
        << "the dialect latch must fire off the routed GHA lines";
    const auto degenerate{resolve_run_outcome("", scan, composed)};
    EXPECT_EQ(degenerate.outcome, RunOutcome::Unknown)
        << "only-a-console-log GHA is Unknown — never a guess from per-step exit codes";
    EXPECT_TRUE(degenerate.note.empty()) << "absence is the default, not an error: "
                                         << degenerate.note;
}

TEST(GithubOutcome, AuthoritativeSideInputCarriesTheGhaVerdict)
{
    const ComposedSemantics composed{github_only()};
    // The Action forwards `${{ needs.<job>.result }}` verbatim (ADR 0025 §3.1) — the ONLY channel
    // a GHA verdict reaches the engine (no marker). On a green-looking console, `cancelled` must
    // still resolve Aborted: the side-input needs no console corroboration.
    const auto lines{gha_console(/*failing=*/false)};
    const RunOutcomeScan scan{scan_run_outcome(lines, composed)};
    const auto res{resolve_run_outcome("cancelled", scan, composed)};
    EXPECT_EQ(res.outcome, RunOutcome::Aborted);
    EXPECT_TRUE(res.authoritative);
    EXPECT_FALSE(res.divergent) << "no console verdict exists — nothing to diverge from";
    EXPECT_TRUE(res.note.empty()) << res.note;
}
// NOLINTEND
