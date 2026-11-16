// refs: DN-162.D4, DN-162.D5, DN-162.D8, DN-162.O3
// invariant: the GitHub dialect's second generation: `##[start-action ` names an inner step and
// `##[end-action ` ends the innermost open one, in the annotated channel only, both `since: v2`.
// invariant: homed with the vocabulary: the rows and their field roles are this package's data,
// and what a kind, a role and an extractor do is canon's grammar, proven over synthetic rows.
// invariant: determinism — byte-only recognition over the composed rows; no RNG, clock or float.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.github;

using insight::RunOutcome;
using insight::semantic::ComposedSemantics;
using insight::tokenization::IntentMarker;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;
using insight::tokenization::recognize_closer;
using insight::tokenization::recognize_end;
using insight::tokenization::recognize_opener;
using insight::tokenization::UnitEnd;

// pre: every probe is escape-free, so `normalize` is the zero-copy fixed point.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

namespace
{

[[nodiscard]] ComposedSemantics github_view(std::string_view dialect, std::string_view channel)
{
    const std::array manifests{insight::semantic::github::kManifest};
    return insight::semantic::compose(manifests).for_stream(dialect, channel);
}

[[nodiscard]] ComposedSemantics annotated()
{
    return github_view(insight::semantic::github::kDialect,
                       insight::semantic::github::kChannelAnnotated);
}

[[nodiscard]] std::string show(const IntentMarker& mark)
{
    return "{kind=" + std::to_string(static_cast<int>(mark.kind)) + ", name=\"" +
           std::string{mark.name} + "\", display=\"" + std::string{mark.display} + "\", since=\"" +
           std::string{mark.revision_since} + "\", rank=" + std::to_string(mark.revision_rank) +
           "}";
}

[[nodiscard]] std::string show(const UnitEnd& ended)
{
    return "{kind=" + std::to_string(static_cast<int>(ended.kind)) + ", outcome=\"" +
           std::string{ended.outcome} + "\", propagated_outcome=\"" +
           std::string{ended.propagated_outcome} + "\", since=\"" +
           std::string{ended.revision_since} + "\", rank=" + std::to_string(ended.revision_rank) +
           "}";
}

constexpr std::string_view kStart{"##[start-action display=Set up Node;id=setup-node]"};
constexpr std::string_view kEnd{
    "##[end-action id=setup-node;outcome=failure;conclusion=success;duration_ms=1520]"};

} // namespace

// refs: DN-162.D4, ADR-22.D6
// invariant: both rows fire at kind InnerStep on the annotated channel; the stripped channel and an
// undeclared stream recognize neither, and no other walker returns them.
TEST(GithubInnerStep, BothRowsFireAtTheirKindOnTheAnnotatedChannelOnly)
{
    const ComposedSemantics view{annotated()};
    const IntentMarker opened{recognize(norm_probe(kStart), view)};
    EXPECT_EQ(opened.kind, IntentMarkerKind::InnerStep) << show(opened);
    const UnitEnd ended{recognize_end(norm_probe(kEnd), view)};
    EXPECT_EQ(ended.kind, IntentMarkerKind::InnerStep) << show(ended);
    for (const std::string_view line : {kStart, kEnd})
    {
        EXPECT_EQ(recognize_opener(norm_probe(line), view), IntentMarkerKind::None) << line;
        EXPECT_FALSE(recognize_closer(norm_probe(line), view)) << line;
    }
    EXPECT_EQ(recognize(norm_probe(kEnd), view).kind, IntentMarkerKind::None)
        << "`recognize` returned the end line";

    for (const ComposedSemantics& other :
         {github_view(insight::semantic::github::kDialect,
                      insight::semantic::github::kChannelStripped),
          github_view(insight::semantic::kAnyDialect,
                      insight::semantic::github::kChannelAnnotated)})
    {
        EXPECT_EQ(recognize(norm_probe(kStart), other).kind, IntentMarkerKind::None)
            << "the start line fired off the annotated channel of a declared github stream: "
            << show(recognize(norm_probe(kStart), other));
        EXPECT_EQ(recognize_end(norm_probe(kEnd), other).kind, IntentMarkerKind::None)
            << "the end line fired off the annotated channel of a declared github stream: "
            << show(recognize_end(norm_probe(kEnd), other));
    }
}

// refs: DN-162.D5
// invariant: the name table: an author id names its step; a synthesized action segment names its
// step; `__run` and `__run_2` are named by their display; a trailing `_2` is removed.
// invariant: the percent-encodings stay raw in both the name and the display: decoding is the
// render's, never identity's.
TEST(GithubInnerStep, AnInnerStepIsNamedByItsIdsLastSegmentOrItsDisplay)
{
    const ComposedSemantics view{annotated()};
    struct Case
    {
        std::string_view line;
        std::string_view name;
        std::string_view display;
    };
    for (const Case& probe : {
             Case{"##[start-action display=Restore cache;id=restore]", "restore", "Restore cache"},
             Case{"##[start-action display=Run actions/setup-node@v4;id=build.setup-node]",
                  "setup-node", "Run actions/setup-node@v4"},
             Case{"##[start-action display=Run make test;id=build.__run]", "Run make test",
                  "Run make test"},
             Case{"##[start-action display=Run make lint;id=build.__run_2]", "Run make lint",
                  "Run make lint"},
             Case{"##[start-action display=Checkout;id=build.checkout_2]", "checkout", "Checkout"},
             Case{"##[start-action display=Run a%3Bb%5D;id=build.__run_3]", "Run a%3Bb%5D",
                  "Run a%3Bb%5D"},
             Case{"##[start-action display=Tag a%3Bb%5D;id=tag-a]", "tag-a", "Tag a%3Bb%5D"},
         })
    {
        const IntentMarker got{recognize(norm_probe(probe.line), view)};
        EXPECT_EQ(got.kind, IntentMarkerKind::InnerStep) << probe.line << " -> " << show(got);
        EXPECT_EQ(got.name, probe.name) << probe.line << " -> " << show(got);
        EXPECT_EQ(got.display, probe.display) << probe.line << " -> " << show(got);
        EXPECT_TRUE(got.version.empty())
            << "an inner step's version is its banner's, never its name's: " << show(got);
    }
}

// refs: DN-162.D8, ADR-17.D5
// invariant: the end line yields the unit's own `outcome` and its containing unit's `conclusion`,
// each mapped through the dialect's outcome tokens; `skipped` maps to Unknown.
TEST(GithubInnerStep, TheEndLineYieldsBothOutcomesThroughTheOutcomeTokens)
{
    const ComposedSemantics view{annotated()};
    const UnitEnd tolerated{recognize_end(norm_probe(kEnd), view)};
    EXPECT_EQ(tolerated.outcome, "failure") << show(tolerated);
    EXPECT_EQ(tolerated.propagated_outcome, "success") << show(tolerated);
    EXPECT_EQ(insight::map_outcome_token(tolerated.outcome, view), RunOutcome::Failure);
    EXPECT_EQ(insight::map_outcome_token(tolerated.propagated_outcome, view), RunOutcome::Success);

    const UnitEnd skipped{recognize_end(
        norm_probe("##[end-action id=__run;outcome=skipped;conclusion=skipped;duration_ms=0]"),
        view)};
    EXPECT_EQ(skipped.kind, IntentMarkerKind::InnerStep) << show(skipped);
    EXPECT_EQ(insight::map_outcome_token(skipped.outcome, view), RunOutcome::Unknown)
        << show(skipped);
    EXPECT_EQ(insight::map_outcome_token(skipped.propagated_outcome, view), RunOutcome::Unknown)
        << show(skipped);

    for (const std::string_view token : {"success", "failure", "cancelled", "skipped"})
        EXPECT_TRUE(insight::map_outcome_token(token, view).has_value())
            << "measured token \"" << token << "\" has no outcome row (DN-162.D4)";
}

// refs: DN-162.D1, DN-162.D7
// invariant: a fired row reports the revision that introduced it: `v2` and rank 1 on the inner
// step's two lines, `v1` and rank 0 on a step banner.
TEST(GithubInnerStep, AFiredRowReportsTheRevisionThatIntroducedIt)
{
    const ComposedSemantics view{annotated()};
    const IntentMarker opened{recognize(norm_probe(kStart), view)};
    EXPECT_EQ(opened.revision_since, "v2") << show(opened);
    EXPECT_EQ(opened.revision_rank, 1U) << show(opened);
    const UnitEnd ended{recognize_end(norm_probe(kEnd), view)};
    EXPECT_EQ(ended.revision_since, "v2") << show(ended);
    EXPECT_EQ(ended.revision_rank, 1U) << show(ended);
    const IntentMarker banner{recognize(norm_probe("##[group]Run actions/checkout@v4"), view)};
    EXPECT_EQ(banner.kind, IntentMarkerKind::Step) << show(banner);
    EXPECT_EQ(banner.revision_since, "v1") << show(banner);
    EXPECT_EQ(banner.revision_rank, 0U) << show(banner);
}
