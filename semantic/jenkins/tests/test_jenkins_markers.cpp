// refs: SRC-II-6, STU-6
// invariant: this file guards the stage/step VOCABULARY the frozen spike measured on real consoles
// and graduated into rows — the rows, never canon's walkers.
// invariant: the guarded semantics are STAGE = a named block open at the container level and
// UNORDERED, STEP = a verb annotation at the leaf level and Ordered, minus the exclusion set.
// invariant: the rows are DIALECT-gated to Jenkins and inert on a stream that declared anything
// else.
// note: determinism: byte-only recognition over the composed rows; no RNG, clock or float
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.jenkins;

using insight::semantic::ComposedSemantics;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;

// pre: the walkers take NormalizedContent, canon's ingest-normalization precondition carried by a
// type unforgeable outside canon.
// invariant: every probe here is an escape-free literal, so normalize() is the zero-copy fixed
// point over a shared scratch and the probe reaches the walker byte-for-byte.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

namespace
{
// invariant: the RESOLVED view of a stream that DECLARED this dialect — the concretely-gated rows
// are reachable only through a declaration, never through per-line format detection.
[[nodiscard]] ComposedSemantics jenkins_only()
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    return insight::semantic::compose(manifests).for_stream(insight::semantic::jenkins::kDialect,
                                                            {});
}

// invariant: the same composition on a stream that declared NO dialect — the fail-closed arm.
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

// refs: SRC-II-9
// invariant: a matrix/parallel leg keeps its axis tuple VERBATIM in the discriminant while
// downstream canonicalization collapses the class.
// invariant: that is the guard against the measured phantom vanish/insert storm a masked-away
// discriminant produces.
TEST(JenkinsMarkers, ParallelBranchIsAStageWithItsDiscriminant)
{
    const ComposedSemantics composed{jenkins_only()};
    const auto branch{recognize(norm_probe("[Pipeline] { (Branch: maven (lts))"), composed)};
    EXPECT_EQ(branch.kind, IntentMarkerKind::Job);
    EXPECT_EQ(branch.name, "Branch: maven (lts)") << "nested parens stay inside the payload";
    EXPECT_EQ(branch.discriminant, "(lts)") << "the raw declared coordinate is kept verbatim";
}

// assert: the declarative engine's synthetic stages are console-visible named blocks, so they are
// real stages to the recognizer.
// note: against the platform's flat stage tree that reads as over-granularity: richer, not wrong
TEST(JenkinsMarkers, DeclarativeSyntheticStageRecognized)
{
    const ComposedSemantics composed{jenkins_only()};
    const auto synthetic{
        recognize(norm_probe("[Pipeline] { (Declarative: Checkout SCM)"), composed)};
    EXPECT_EQ(synthetic.kind, IntentMarkerKind::Job);
    EXPECT_EQ(synthetic.name, "Declarative: Checkout SCM");
}

// note: the probe line outlives the returned marker — its name is a view into the content
TEST(JenkinsMarkers, VerbAnnotationIsAStep)
{
    const ComposedSemantics composed{jenkins_only()};
    for (const std::string_view verb :
         {"sh", "echo", "junit", "checkout", "withEnv", "readMavenPom"})
    {
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

// refs: SRC-II-6, ADR-22
// assert: the gate is against the DECLARATION, never against per-line format detection: the old
// form passed a LogFormat per call and made this a question about the line's own bytes.
// invariant: a stream that declared no dialect fires nothing — failing closed on depth is not
// optional.
// assert: the second half is the CONTROL: the same lines DO fire once the stream declares this
// dialect, so the leg is a real gate rather than a line nothing would have claimed anyway.
TEST(JenkinsMarkers, DialectGatedToTheDeclaringStream)
{
    const ComposedSemantics undeclared{undeclared_stream()};
    EXPECT_EQ(recognize(norm_probe("[Pipeline] { (Build)"), undeclared).kind,
              IntentMarkerKind::None)
        << "a dialect-gated marker fired on a stream that declared NO dialect — fail-closed on "
           "depth is not optional";
    EXPECT_EQ(recognize(norm_probe("[Pipeline] sh"), undeclared).kind, IntentMarkerKind::None);

    const ComposedSemantics declared{jenkins_only()};
    EXPECT_EQ(recognize(norm_probe("[Pipeline] { (Build)"), declared).kind, IntentMarkerKind::Job);
    EXPECT_EQ(recognize(norm_probe("[Pipeline] sh"), declared).kind, IntentMarkerKind::Step);
}
