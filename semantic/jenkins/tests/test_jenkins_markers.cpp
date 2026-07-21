// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_jenkins_markers.cpp — the Jenkins stage/step VOCABULARY (studies/006 §Reproduction,
// graduated into grammar-2 rows). What it guards, against the spike's ratified semantics:
//   STAGE = a NAMED `[Pipeline] { (<name>)` block open (declared stage OR parallel/matrix Branch),
//           kind=Job (the container level), UNORDERED (branches co-occur like matrix legs);
//   STEP  = `[Pipeline] <verb>`, kind=Step, ORDERED — excluding the closed structural-token set
//           (`{`, `}`, `stage`, `node`, `parallel`, `// …`, `End of Pipeline`);
//   II-6  = the rows are format-gated to Jenkins and inert elsewhere.
// Determinism: byte-only recognition over the composed rows; no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.jenkins;

using insight::LogFormat;
using insight::semantic::ComposedSemantics;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

namespace
{
[[nodiscard]] ComposedSemantics jenkins_only()
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    return insight::semantic::compose(manifests);
}
} // namespace

TEST(JenkinsMarkers, NamedBlockOpenIsAStage)
{
    const ComposedSemantics composed{jenkins_only()};
    const auto stage{recognize("[Pipeline] { (Build)", LogFormat::Jenkins, composed)};
    EXPECT_EQ(stage.kind, IntentMarkerKind::Job)
        << "STAGE is the container level (GHA Job ≡ stage)";
    EXPECT_EQ(stage.name, "Build");
    EXPECT_EQ(stage.child_order, ChildOrder::Unordered)
        << "stages/branches co-occur (parallel/matrix) — the level matches UNORDERED (ADR 0023)";
}

TEST(JenkinsMarkers, ParallelBranchIsAStageWithItsDiscriminant)
{
    const ComposedSemantics composed{jenkins_only()};
    // A matrix/parallel leg: the axis tuple stays VERBATIM in the discriminant (II-9) while
    // canonicalize_intent (downstream) collapses the class — the studies/004 vanish-storm guard.
    const auto branch{
        recognize("[Pipeline] { (Branch: maven (lts))", LogFormat::Jenkins, composed)};
    EXPECT_EQ(branch.kind, IntentMarkerKind::Job);
    EXPECT_EQ(branch.name, "Branch: maven (lts)") << "nested parens stay inside the payload";
    EXPECT_EQ(branch.discriminant, "(lts)") << "the raw declared coordinate is kept verbatim";
}

TEST(JenkinsMarkers, DeclarativeSyntheticStageRecognized)
{
    const ComposedSemantics composed{jenkins_only()};
    // The declarative engine's synthetic stages are console-visible named blocks — real stages for
    // the recognizer (studies/006: the over-granularity vs the flat wfapi oracle,
    // richer-not-wrong).
    const auto synthetic{
        recognize("[Pipeline] { (Declarative: Checkout SCM)", LogFormat::Jenkins, composed)};
    EXPECT_EQ(synthetic.kind, IntentMarkerKind::Job);
    EXPECT_EQ(synthetic.name, "Declarative: Checkout SCM");
}

TEST(JenkinsMarkers, VerbAnnotationIsAStep)
{
    const ComposedSemantics composed{jenkins_only()};
    for (const std::string_view verb :
         {"sh", "echo", "junit", "checkout", "withEnv", "readMavenPom"})
    {
        // The line outlives the returned marker (its name is a view into the content).
        const std::string line{std::string{"[Pipeline] "} + std::string{verb}};
        const auto step{recognize(line, LogFormat::Jenkins, composed)};
        EXPECT_EQ(step.kind, IntentMarkerKind::Step) << "verb: " << verb;
        EXPECT_EQ(step.name, verb);
        EXPECT_EQ(step.child_order, ChildOrder::Ordered)
            << "steps are sequential within their stage";
    }
}

TEST(JenkinsMarkers, StructuralTokensAreScaffoldNotQuanta)
{
    const ComposedSemantics composed{jenkins_only()};
    for (const std::string_view scaffold :
         {"[Pipeline] {", "[Pipeline] }", "[Pipeline] stage", "[Pipeline] node",
          "[Pipeline] parallel", "[Pipeline] End of Pipeline", "[Pipeline] // stage",
          "[Pipeline] // node", "[Pipeline] { (Build"})
    {
        EXPECT_EQ(recognize(scaffold, LogFormat::Jenkins, composed).kind, IntentMarkerKind::None)
            << "scaffold line must open no quantum: " << scaffold;
    }
}

TEST(JenkinsMarkers, FormatGatedToJenkinsOnly)
{
    const ComposedSemantics composed{jenkins_only()};
    // II-6: a `[Pipeline]` line in a NON-Jenkins-routed stream fires nothing.
    EXPECT_EQ(recognize("[Pipeline] { (Build)", LogFormat::GitHubActions, composed).kind,
              IntentMarkerKind::None);
    EXPECT_EQ(recognize("[Pipeline] sh", LogFormat::RawText, composed).kind,
              IntentMarkerKind::None);
    EXPECT_EQ(recognize("[Pipeline] sh", LogFormat::Unknown, composed).kind,
              IntentMarkerKind::None);
}
// NOLINTEND
