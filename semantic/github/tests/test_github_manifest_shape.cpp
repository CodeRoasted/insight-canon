// test_github_manifest_shape.cpp — pin the SHAPE of the shipped GHA ruleset, member by member.
//
// WHY THIS EXISTS, and it is not a mirror of the manifest. `run()` (the conformance kit) asks
// whether the rows a package ships are WELL-FORMED, and deliberately passes an EMPTY package —
// `manifest_equivalence_report`'s own contract says so ("pretending otherwise would make an empty
// package permanently non-conformant"). So nothing in canon asserts that THIS package ships any
// rows at all, and a generator that silently emitted a manifest with three empty spans would clear
// every existing check in this suite.
//
// That gap is load-bearing twice over:
//   * `manifest_equivalence_report` is an EQUIVALENCE report, never a NON-VACUITY one — it returns
//     14 green checks for two empty manifests, and its contract hands the choice of subject to the
//     caller. The dialect-generator equivalence leg is that caller, so the oracle it compares
//     against must be independently proven LIVE or its green means nothing.
//   * three members are empty ON PURPOSE (`locations`, `value_classes`, `outcome_markers`) and the
//     declaration argues each absence. A value-side test is where "empty because we measured and
//     there is nothing to declare" stops being indistinguishable from "empty because a row kind got
//     dropped": asserting the emptiness POSITIVELY is what makes an exclusion and an omission look
//     different at the value tier.
//
// THE STRUCTURED BINDING IS THE POINT, not a shorthand. Binding all fourteen members means a
// FIFTEENTH manifest member is a COMPILE ERROR in this file, never a silently unpinned one — the
// same mechanism `manifest_equivalence_report` uses for the same reason. Member access would let a
// new member ship unpinned and this file would still read as complete.
//
// The counts and the version are pinned TOGETHER on purpose: SRC-SP-7's immutable-release
// discipline says a released version's rows are frozen and a content change is a new version, so a
// row change without a bump and a bump without a row change are both red here. A grammar-shape
// change moves neither (DN-17.D22) and does not disturb this test.
//
// Determinism: seedless, single-threaded, no wall clock — every assertion is a pure function of
// constexpr static data. NOLINTBEGIN — unit test: short identifiers and string literals are fine.
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

    // The populated members: the ruleset is these rows, and a generator that drops a kind is red
    // here before any equivalence comparison is asked to mean something.
    EXPECT_EQ(roles.size(), 6U) << "structural-role rows, actual: " << roles.size();
    EXPECT_EQ(markers.size(), 3U) << "intent-marker rows, actual: " << markers.size();
    EXPECT_EQ(emits.size(), 3U)
        << "generation-template rows — the writer dual, one per recognition row (ADR-18.D4), so "
           "this must equal markers.size() = "
        << markers.size() << "; actual: " << emits.size();
    EXPECT_EQ(level_lifts.size(), 8U) << "level-lift rows, actual: " << level_lifts.size();
    EXPECT_EQ(outcome_tokens.size(), 7U) << "run-outcome token rows, actual: "
                                         << outcome_tokens.size();
    EXPECT_EQ(channels.size(), 2U)
        << "declared intent-channel vocabulary (annotated + stripped), actual: " << channels.size();
    EXPECT_EQ(dialect_revisions.size(), 1U)
        << "declared vendor-revision vocabulary — cardinality one until GitHub ships a second "
           "workflow-command syntax generation; actual: "
        << dialect_revisions.size();

    // The DECLARED ABSENCES, asserted positively. Each is argued in the declaration; here is the
    // value-tier witness that the absence is the one that was argued.
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

    // The code tier, by presence — the only thing a manifest-level check can say about it. Whether
    // two code tiers COMPUTE the same verdicts is a separate obligation with a separate leg, and
    // nothing here may be read as covering it.
    EXPECT_EQ(strategy, nullptr)
        << "this package ships no format strategy: the per-line delivery-stamp peel became declared "
           "transport, leaving one byte predicate as the whole code tier";
    EXPECT_NE(echoed_source, nullptr)
        << "the echoed-source raw-line provenance hook is this package's entire code tier and must "
           "be present";
}
// NOLINTEND
