// test_github_outcome.cpp — the GHA run-outcome VOCABULARY, the GHA half of the outcome mapping:
// the seven native `result`/`conclusion` strings map into the core four-class RunOutcome, the
// no-verdict conclusions (skipped/neutral/action_required) MAP to Unknown (never a guess, never a
// miss), and the package honestly ships NO OutcomeMarkerRow — GHA has no run-verdict console line
// (`Process completed with exit code N` is per-step), so the degenerate console path is Unknown and
// only the authoritative side-input carries a GHA verdict. The LADDER mechanics — authoritative
// side-input, then the console tail's LAST match, then Unknown — are core's (canon
// tests/compose/test_run_outcome.cpp); this file guards the GITHUB DATA end.
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
// The RESOLVED view of a stream that declared this dialect — the outcome vocabulary is reachable
// only through a declaration, never through per-line format detection.
// The UNFILTERED composition — every package row, no stream view. This is what a
// caller-declared verdict resolves against (DN-32.D6): the vocabulary answers WHO SUPPLIED
// the verdict, so it must not be filtered by the dialect of whoever WROTE the bytes.
// `map_outcome_token_in` re-derives through `for_stream` itself, so handing it an already
// filtered view is a silent no-op — which is why these are two named helpers, not one.
[[nodiscard]] ComposedSemantics github_vocabularies()
{
    return insight::semantic::compose(std::array{insight::semantic::github::kManifest});
}

[[nodiscard]] ComposedSemantics github_only()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::github::kDialect,
                                                            {});
}

// The same composition on a stream that declared no dialect — the fail-closed arm.
[[nodiscard]] ComposedSemantics undeclared_stream()
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::kAnyDialect, {});
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
    EXPECT_EQ(map_outcome_token("success", composed), RunOutcome::Success);
    EXPECT_EQ(map_outcome_token("failure", composed), RunOutcome::Failure);
    // Both non-completion conclusions are the SAME class: an incomplete run (log truncated at the
    // stop point) — either one suppresses vanished-quantum alarms rather than raising them.
    EXPECT_EQ(map_outcome_token("cancelled", composed), RunOutcome::Aborted);
    EXPECT_EQ(map_outcome_token("timed_out", composed), RunOutcome::Aborted);
    // The no-verdict conclusions MAP (engaged optional) to Unknown — a resolution, not a miss:
    // rung 1 resolves and a stale console tail is never consulted (the NOT_BUILT shape).
    for (const std::string_view token : {"skipped", "neutral", "action_required"})
    {
        const auto mapped{map_outcome_token(token, composed)};
        ASSERT_TRUE(mapped.has_value()) << "'" << token << "' must MAP (to Unknown), not miss";
        EXPECT_EQ(*mapped, RunOutcome::Unknown) << "'" << token << "' carries no pass/fail verdict";
    }
    // GHA has no native UNSTABLE string — the category is core, this dialect ships no row for it,
    // and the Jenkins literal must NOT leak in (fail-closed upstream surfaces the note).
    EXPECT_FALSE(map_outcome_token("UNSTABLE", composed).has_value());
    // Byte-exact + format-gated (SRC-II-6): the GHA strings are lowercase and GHA-only.
    EXPECT_FALSE(map_outcome_token("SUCCESS", composed).has_value())
        << "GHA conclusions are lowercase — the uppercase form is Jenkins data, not GHA data";
    // SRC-II-6, now STRUCTURAL: on a stream that declared no dialect the row is not in the view at
    // all, so the token cannot resolve however unambiguous it looks (fail-closed on DEPTH).
    const ComposedSemantics undeclared{undeclared_stream()};
    EXPECT_FALSE(map_outcome_token("success", undeclared).has_value())
        << "a dialect's verdict token resolved on an UNDECLARED stream — the gate is fail-open";
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
    // …and rung 2 falls to Unknown. (There is no dialect LATCH any more: the dialect is declared,
    // so `RunOutcomeScan` carries no LogFormat at all.)
    const auto degenerate{resolve_run_outcome({.token = ""}, scan, composed, composed)};
    EXPECT_EQ(degenerate.outcome, RunOutcome::Unknown)
        << "only-a-console-log GHA is Unknown — never a guess from per-step exit codes";
    EXPECT_TRUE(degenerate.note.empty())
        << "absence is the default, not an error: " << degenerate.note;
}

TEST(GithubOutcome, AuthoritativeSideInputCarriesTheGhaVerdict)
{
    const ComposedSemantics composed{github_only()};
    // The Action forwards `${{ needs.<job>.result }}` verbatim, untranslated — the ONLY channel
    // a GHA verdict reaches the engine (no marker). On a green-looking console, `cancelled` must
    // still resolve Aborted: the side-input needs no console corroboration.
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
