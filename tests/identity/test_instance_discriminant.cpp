// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_instance_discriminant.cpp — discriminant_of + IntentMarker.child_order, the ADR 0023
// third role on the identity spine (II-9). The class MASKS drift tokens to group siblings;
// the discriminant KEEPS the raw declared coordinate verbatim to SEPARATE co-occurring /
// cross-run-drifted siblings — it is the exact COMPLEMENT of canonicalize_intent (same R1–R4
// scan, first masked span kept raw). Runner-agnostic by construction (it reads the declared
// tuple, never a hardcoded runner lexicon). A diff here re-draws alignment (II-7) — it rides
// kIntentRegistryVersion; fix the code, never the assertion.

#include <gtest/gtest.h>

import insight.canon.test;

using insight::canonicalize_intent;
using insight::discriminant_of;
using insight::LogFormat;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarkerRegistry;

namespace
{
void expect_disc(std::string_view name, std::string_view expected)
{
    const std::string_view got{discriminant_of(name)};
    EXPECT_EQ(got, expected) << "discriminant_of(\"" << name << "\") = \"" << got << "\"  expected \""
                             << expected << '"';
}
} // namespace

// ── The discriminant is the RAW complement of the class mask ──
TEST(InstanceDiscriminant, KeepsRawDeclaredCoordinateVerbatim)
{
    // Matrix tuple: class masks it to (M), discriminant keeps it raw → distinct legs separable.
    expect_disc("Test (ubuntu-latest, Node 24.x)", "(ubuntu-latest, Node 24.x)");
    expect_disc("Test (windows-latest, Node 24.x)", "(windows-latest, Node 24.x)");
    EXPECT_EQ(canonicalize_intent("Test (ubuntu-latest, Node 24.x)"), "Test (M)"); // the class collapses…
    EXPECT_NE(discriminant_of("Test (ubuntu-latest, Node 24.x)"),
              discriminant_of("Test (windows-latest, Node 24.x)")); // …but the legs stay distinct

    // Version drift ACROSS runs: v6 vs v7 → distinct discriminants → the pair surfaces as REPLACED,
    // never a masked-away 0-row (the G1 under-report the class mask caused).
    expect_disc("ESLint v6", "v6");
    expect_disc("ESLint v7", "v7");

    // A name with NO drift token → empty discriminant (a single unparameterized job).
    expect_disc("Lint", "");
    expect_disc("Build", "");
}

// ── Runner-agnostic: a user's self-hosted runner is captured with no hardcoded lexicon ──
TEST(InstanceDiscriminant, RunnerAgnosticNoHardcodedNames)
{
    expect_disc("Test (my-gpu-box)", "(my-gpu-box)");                 // custom self-hosted runner
    expect_disc("bench (arm64-metal, cuda-12)", "(arm64-metal, cuda-12)");
    // distinct custom runners stay distinct (the whole point — align leg-to-homolog, any runner)
    EXPECT_NE(discriminant_of("Test (my-gpu-box)"), discriminant_of("Test (my-cpu-box)"));
}

// ── child_order is a declared per-level property (ADR 0023 §2): job=Unordered, step=Ordered ──
TEST(InstanceDiscriminant, JobUnorderedStepOrdered)
{
    const auto job{IntentMarkerRegistry::recognize("Complete job name: Test (ubuntu-latest)",
                                                   LogFormat::GitHubActions)};
    EXPECT_EQ(job.child_order, ChildOrder::Unordered) << "jobs are parallel → set-matched";
    EXPECT_EQ(job.discriminant, "(ubuntu-latest)") << "the marker carries its raw discriminant";

    const auto step{IntentMarkerRegistry::recognize("Run yarn build", LogFormat::GitHubActions)};
    EXPECT_EQ(step.child_order, ChildOrder::Ordered) << "steps are sequential → LCS-matched";
}

// NOLINTEND
