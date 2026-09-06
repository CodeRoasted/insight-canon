// refs: SRC-SP-7, DN-17.D22
// invariant: canon's kit asks only whether the rows a package ships are WELL-FORMED and
// deliberately admits an EMPTY package, so nothing in canon says THIS one ships any row.
// assert: all fourteen manifest members are bound, so a FIFTEENTH is a compile error here.
// note: the generator-equivalence oracle must be proven LIVE elsewhere or its green is vacuous
#include <gtest/gtest.h>

import std;
import insight.semantic.github;

TEST(GithubManifestShape, ShipsTheDeclaredRulesetShapeAndNothingElse)
{
    const auto& [name, version, roles, markers, emits, level_lifts, locations, value_classes,
                 outcome_tokens, outcome_markers, channels, dialect_revisions, strategy,
                 echoed_source]{insight::semantic::github::kManifest};

    EXPECT_EQ(name, "github") << "the declared package name is the dialect coordinate every gated "
                                 "row carries and what a caller declares; actual: "
                              << name;
    EXPECT_EQ(version, "1.4.0") << "ruleset version moved without this pin moving with it — if the "
                                   "rows below changed, both edits belong in one pass (SRC-SP-7); "
                                   "actual: "
                                << version;

    EXPECT_EQ(roles.size(), 6U) << "structural-role rows, actual: " << roles.size();
    EXPECT_EQ(markers.size(), 3U) << "intent-marker rows, actual: " << markers.size();
    EXPECT_EQ(emits.size(), 3U)
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
