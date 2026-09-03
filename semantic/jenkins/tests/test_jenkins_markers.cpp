// test_jenkins_markers.cpp — the Jenkins stage/step VOCABULARY (measured on real consoles by the
// frozen spike, then graduated into grammar-2 rows). What it guards, against those semantics:
//   STAGE = a NAMED `[Pipeline] { (<name>)` block open (declared stage OR parallel/matrix Branch),
//           kind=Job (the container level), UNORDERED (branches co-occur like matrix legs);
//   STEP  = `[Pipeline] <verb>`, kind=Step, ORDERED — excluding the closed structural-token set
//           (`{`, `}`, `stage`, `node`, `parallel`, `// …`, `End of Pipeline`);
//   SRC-II-6  = the rows are format-gated to Jenkins and inert elsewhere.
// Determinism: byte-only recognition over the composed rows; no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.jenkins;

using insight::semantic::ComposedSemantics;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

// The walkers take NormalizedContent — canon's ingest-normalization precondition carried by a type
// unforgeable outside canon; every probe here is an escape-free literal (the xtrace probe's `\\e`
// is TWO prose bytes, not an escape byte, so normalization has nothing to remove), so normalize()
// is the zero-copy fixed point over a shared scratch.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

namespace
{
// The RESOLVED view of a stream that declared this dialect — the concretely-gated rows are
// reachable only through a declaration, never through per-line format detection.
[[nodiscard]] ComposedSemantics jenkins_only()
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::jenkins::kDialect,
                                                            {});
}

// The same composition on a stream that declared NO dialect — the fail-closed arm.
[[nodiscard]] ComposedSemantics undeclared_stream()
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::kAnyDialect, {});
}
} // namespace

TEST(JenkinsMarkers, NamedBlockOpenIsAStage)
{
    const ComposedSemantics composed{jenkins_only()};
    const auto stage{recognize(norm_probe("[Pipeline] { (Build)"), composed)};
    EXPECT_EQ(stage.kind, IntentMarkerKind::Job)
        << "STAGE is the container level (GHA Job ≡ stage)";
    EXPECT_EQ(stage.name, "Build");
    EXPECT_EQ(stage.child_order, ChildOrder::Unordered)
        << "stages/branches co-occur (parallel/matrix), so the level's children are UNORDERED: "
           "their sequence is not a structural fact and must never be diffed as one";
}

TEST(JenkinsMarkers, ParallelBranchIsAStageWithItsDiscriminant)
{
    const ComposedSemantics composed{jenkins_only()};
    // A matrix/parallel leg: the axis tuple stays VERBATIM in the discriminant (SRC-II-9) while
    // canonicalize_intent (downstream) collapses the class — the guard against the measured
    // phantom vanish/insert storm a masked-away discriminant produces.
    const auto branch{recognize(norm_probe("[Pipeline] { (Branch: maven (lts))"), composed)};
    EXPECT_EQ(branch.kind, IntentMarkerKind::Job);
    EXPECT_EQ(branch.name, "Branch: maven (lts)") << "nested parens stay inside the payload";
    EXPECT_EQ(branch.discriminant, "(lts)") << "the raw declared coordinate is kept verbatim";
}

TEST(JenkinsMarkers, DeclarativeSyntheticStageRecognized)
{
    const ComposedSemantics composed{jenkins_only()};
    // The declarative engine's synthetic stages are console-visible named blocks — real stages for
    // the recognizer. Measured against the platform's flat stage tree this reads as
    // over-granularity: richer, not wrong.
    const auto synthetic{
        recognize(norm_probe("[Pipeline] { (Declarative: Checkout SCM)"), composed)};
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
        const auto step{recognize(norm_probe(line), composed)};
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
        EXPECT_EQ(recognize(norm_probe(scaffold), composed).kind, IntentMarkerKind::None)
            << "scaffold line must open no quantum: " << scaffold;
    }
}

// ── SRC-II-6, now against the DECLARATION rather than against per-line format detection ──
// The rows are gated to this package's NAME, so a stream that declared no
// dialect fires nothing. The old form of this test passed a `LogFormat` per call, sourced in
// production from `LogParser::routed_format()` — the per-line detector winner under a sticky
// strategy — which made "does the Jenkins dialect fire" a question about the line's own bytes.
TEST(JenkinsMarkers, DialectGatedToTheDeclaringStream)
{
    const ComposedSemantics undeclared{undeclared_stream()};
    EXPECT_EQ(recognize(norm_probe("[Pipeline] { (Build)"), undeclared).kind,
              IntentMarkerKind::None)
        << "a dialect-gated marker fired on a stream that declared NO dialect — fail-closed on "
           "depth is not optional";
    EXPECT_EQ(recognize(norm_probe("[Pipeline] sh"), undeclared).kind, IntentMarkerKind::None);

    // The control: the SAME lines DO fire once the stream declares this dialect, so the leg above
    // is a real gate rather than a line nothing would have claimed anyway.
    const ComposedSemantics declared{jenkins_only()};
    EXPECT_EQ(recognize(norm_probe("[Pipeline] { (Build)"), declared).kind, IntentMarkerKind::Job);
    EXPECT_EQ(recognize(norm_probe("[Pipeline] sh"), declared).kind, IntentMarkerKind::Step);
}
