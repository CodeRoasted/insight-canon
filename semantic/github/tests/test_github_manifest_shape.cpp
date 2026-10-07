// refs: ADR-17.D3, DN-17.D22
// invariant: canon's kit asks only whether the rows a package ships are WELL-FORMED and
// deliberately admits an EMPTY package, so nothing in canon says THIS one ships any row.
// assert: all fifteen manifest members are bound, so a SIXTEENTH is a compile error here.
// note: the generator-equivalence oracle must be proven LIVE elsewhere or its green is vacuous
#include <gtest/gtest.h>

import std;
import insight.semantic.github;

TEST(GithubManifestShape, ShipsTheDeclaredRulesetShapeAndNothingElse)
{
    const auto& [name, version, roles, markers, emits, level_lifts, locations, value_classes,
                 outcome_tokens, outcome_markers, channels, dialect_revisions, declared_values,
                 strategy, echoed_source]{insight::semantic::github::kManifest};

    EXPECT_EQ(name, "github") << "the declared package name is the dialect coordinate every gated "
                                 "row carries and what a caller declares; actual: "
                              << name;
    EXPECT_EQ(version, "1.10.0")
        << "ruleset version moved without this pin moving with it — if the "
           "rows below changed, both edits belong in one pass (ADR-17.D3); "
           "actual: "
        << version;

    EXPECT_EQ(roles.size(), 8U) << "structural-role rows — six announced markers and two Progress "
                                   "shapes (DN-134.D9), actual: "
                                << roles.size();
    EXPECT_EQ(markers.size(), 7U) << "intent-marker rows, actual: " << markers.size();
    EXPECT_EQ(emits.size(), 7U)
        << "generation-template rows — the writer dual, one per recognition row (ADR-18.D4), so "
           "this must equal markers.size() = "
        << markers.size() << "; actual: " << emits.size();
    EXPECT_EQ(level_lifts.size(), 8U) << "level-lift rows, actual: " << level_lifts.size();
    EXPECT_EQ(outcome_tokens.size(), 7U)
        << "run-outcome token rows, actual: " << outcome_tokens.size();
    EXPECT_EQ(channels.size(), 2U)
        << "declared intent-channel vocabulary (annotated + stripped), actual: " << channels.size();
    EXPECT_EQ(dialect_revisions.size(), 1U)
        << "declared vendor-revision vocabulary — cardinality one until GitHub ships a second "
           "workflow-command syntax generation; actual: "
        << dialect_revisions.size();
    // refs: DN-133.D1
    ASSERT_EQ(declared_values.size(), 1U)
        << "declared-value rows — one key, the run's own pull-request number; actual: "
        << declared_values.size();
    EXPECT_EQ(declared_values[0].key, "pull_request")
        << "the key a caller declares the value under; actual: " << declared_values[0].key;
    const std::vector<std::string_view> markers_declared{declared_values[0].markers.begin(),
                                                         declared_values[0].markers.end()};
    // refs: DN-133.D7
    const std::vector<std::string_view> markers_measured{"PR-",   "pr-",   "Pr-", "pull-",
                                                         "PULL-", "Pull-", "PR#", "pulls/"};
    EXPECT_EQ(markers_declared, markers_measured)
        << "the eight spellings measured before their builds, byte-exact and in declared order; "
           "the set is extended only on measured evidence";

    // assert: each absence is argued in the declaration, and asserting it POSITIVELY is what
    // separates a measured exclusion from a row kind silently dropped.
    EXPECT_TRUE(locations.empty())
        << "this dialect declares NO location rows — that surface is the test_frameworks package's "
           "— so a non-empty span here is a row kind arriving unargued; actual: "
        << locations.size();
    EXPECT_TRUE(value_classes.empty())
        << "this dialect declares NO value classes — none has a consumer; actual: "
        << value_classes.size();
    EXPECT_TRUE(outcome_markers.empty())
        << "this dialect declares NO outcome-marker row: GHA emits no single run-verdict console "
           "line, so the degenerate console path is correctly Unknown; actual: "
        << outcome_markers.size();

    // note: presence only — whether two code tiers COMPUTE the same verdicts is a separate leg
    EXPECT_EQ(strategy, nullptr) << "this package ships no format strategy: the per-line "
                                    "delivery-stamp peel became declared "
                                    "transport, leaving one byte predicate as the whole code tier";
    EXPECT_NE(echoed_source, nullptr)
        << "the echoed-source raw-line provenance hook is this package's entire code tier and must "
           "be present";
}
