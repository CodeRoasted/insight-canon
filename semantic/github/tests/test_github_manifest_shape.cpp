// refs: ADR-17.D3, DN-17.D22
// invariant: canon's kit asks only whether the rows a package ships are WELL-FORMED and
// deliberately admits an EMPTY package, so nothing in canon says THIS one ships any row.
// assert: all seventeen manifest members are bound, so an EIGHTEENTH is a compile error here.
// note: the generator-equivalence oracle must be proven LIVE elsewhere or its green is vacuous
#include <gtest/gtest.h>

import std;
import insight.semantic.github;

TEST(GithubManifestShape, ShipsTheDeclaredRulesetShapeAndNothingElse)
{
    const auto& [name, version, roles, markers, emits, level_lifts, locations, value_classes,
                 outcome_tokens, outcome_markers, channels, dialect_revisions, declared_values,
                 stop_announcements, instance_values, strategy,
                 echoed_source]{insight::semantic::github::kManifest};

    EXPECT_EQ(name, "github") << "the declared package name is the dialect coordinate every gated "
                                 "row carries and what a caller declares; actual: "
                              << name;
    EXPECT_EQ(version, "1.12.0")
        << "ruleset version moved without this pin moving with it — if the "
           "rows below changed, both edits belong in one pass (ADR-17.D3); "
           "actual: "
        << version;

    EXPECT_EQ(roles.size(), 8U) << "structural-role rows — six announced markers and two Progress "
                                   "shapes (ADR-17.D14), actual: "
                                << roles.size();
    EXPECT_EQ(markers.size(), 9U)
        << "intent-marker rows — seven v1 rows and the v2 inner step's start and end "
           "(DN-162.D4), actual: "
        << markers.size();
    EXPECT_EQ(emits.size(), 9U)
        << "generation-template rows — the writer dual, one per recognition row (ADR-18.D4), so "
           "this must equal markers.size() = "
        << markers.size() << "; actual: " << emits.size();
    EXPECT_EQ(level_lifts.size(), 8U) << "level-lift rows, actual: " << level_lifts.size();
    EXPECT_EQ(outcome_tokens.size(), 7U)
        << "run-outcome token rows, actual: " << outcome_tokens.size();
    EXPECT_EQ(channels.size(), 2U)
        << "declared intent-channel vocabulary (annotated + stripped), actual: " << channels.size();
    ASSERT_EQ(dialect_revisions.size(), 2U)
        << "declared vendor-revision vocabulary — v1 and the runner of 2026-07-16 onward, oldest "
           "first (DN-162.D1); actual: "
        << dialect_revisions.size();
    EXPECT_EQ(dialect_revisions[0], "v1") << "the span is ordered oldest first (DN-162.D1)";
    EXPECT_EQ(dialect_revisions[1], "v2") << "the span is ordered oldest first (DN-162.D1)";
    // refs: ADR-17.D15
    EXPECT_EQ(stop_announcements.size(), 3U)
        << "stop-announcement rows — the runner's cancellation and shutdown lines and depot's; "
           "actual: "
        << stop_announcements.size();
    // refs: DN-163.D1, DN-163.D7
    ASSERT_EQ(instance_values.size(), 4U)
        << "instance-value rows — the runner name and machine name lines, the results-storage "
           "shard shape, the runner's docker argument row; actual: "
        << instance_values.size();
    EXPECT_EQ(instance_values[0].form, insight::semantic::InstanceValueForm::Line);
    EXPECT_EQ(instance_values[0].bytes, "Runner name: ");
    EXPECT_EQ(instance_values[1].bytes, "Machine name: ");
    EXPECT_EQ(instance_values[2].form, insight::semantic::InstanceValueForm::Shape);
    EXPECT_EQ(instance_values[2].bytes, "productionresultssa{n}.blob.core.windows.net");
    EXPECT_EQ(instance_values[3].form, insight::semantic::InstanceValueForm::Argument);
    EXPECT_EQ(instance_values[3].bytes, "##[command]/usr/bin/docker ");
    // refs: DN-133.D1, DN-162.D13, DN-163.D4, DN-163.D6
    ASSERT_EQ(declared_values.size(), 4U)
        << "declared-value rows — the pull-request base row, run id, the pull-request v2 "
           "extension and the head branch; actual: "
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
    // refs: DN-162.D13
    EXPECT_EQ(declared_values[1].key, "run_id")
        << "the second key is the run's own id; actual: " << declared_values[1].key;
    EXPECT_TRUE(declared_values[1].markers.empty())
        << "a run id is matched as a whole digit run, behind no marker; actual markers: "
        << declared_values[1].markers.size();
    EXPECT_EQ(declared_values[1].min_digits, 10U)
        << "the floor that keeps a markerless value off ordinary words; actual: "
        << declared_values[1].min_digits;
    EXPECT_EQ(declared_values[1].revision_since, "v2")
        << "the revision this declaration adds the key in; actual: "
        << declared_values[1].revision_since;
    // refs: DN-163.D4, DN-163.D5
    EXPECT_EQ(declared_values[2].key, "pull_request")
        << "the third row extends the pull-request key; actual: " << declared_values[2].key;
    EXPECT_TRUE(declared_values[2].extends) << "it is an extension, so the base row stays v1's";
    ASSERT_EQ(declared_values[2].markers.size(), 1U);
    EXPECT_EQ(declared_values[2].markers[0], "pull/");
    EXPECT_EQ(declared_values[2].min_digits, 5U)
        << "the floor beside the markers; actual: " << declared_values[2].min_digits;
    EXPECT_EQ(declared_values[2].revision_since, "v2");
    // refs: DN-163.D6
    EXPECT_EQ(declared_values[3].key, "head_ref");
    EXPECT_EQ(declared_values[3].value, insight::semantic::DeclaredValueKind::Text);
    const std::vector<std::string_view> prefixes{declared_values[3].prefixes.begin(),
                                                 declared_values[3].prefixes.end()};
    EXPECT_EQ(prefixes,
              (std::vector<std::string_view>{"refs/heads/", "refs/remotes/origin/", "origin/"}))
        << "the three ref prefixes, in declared order";
    EXPECT_EQ(declared_values[3].revision_since, "v2");

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
