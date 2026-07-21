// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_instance_discriminant.cpp — discriminant_of, canon's SEMANTIC-UNAWARE complement of
// canonicalize_intent (ADR 0023, the third role on the identity spine, II-9). The class MASKS drift
// tokens to group siblings; the discriminant KEEPS the raw declared coordinate verbatim to SEPARATE
// co-occurring / cross-run-drifted siblings — same R1–R4 scan, first masked span kept raw.
// Runner-agnostic by construction (it reads the declared tuple, never a hardcoded runner lexicon).
// A diff here re-draws alignment (II-7) — it rides the composed semantic_identity (ADR 0024 §4);
// fix the code, never the assertion.

#include <gtest/gtest.h>

import insight.canon.test;

using insight::canonicalize_intent;
using insight::discriminant_of;

namespace
{
void expect_disc(std::string_view name, std::string_view expected)
{
    const std::string_view got{discriminant_of(name)};
    EXPECT_EQ(got, expected) << "discriminant_of(\"" << name << "\") = \"" << got
                             << "\"  expected \"" << expected << '"';
}
} // namespace

// ── The discriminant is the RAW complement of the class mask ──
TEST(InstanceDiscriminant, KeepsRawDeclaredCoordinateVerbatim)
{
    // Matrix tuple: class masks it to (M), discriminant keeps it raw → distinct legs separable.
    expect_disc("Test (ubuntu-latest, Node 24.x)", "(ubuntu-latest, Node 24.x)");
    expect_disc("Test (windows-latest, Node 24.x)", "(windows-latest, Node 24.x)");
    EXPECT_EQ(canonicalize_intent("Test (ubuntu-latest, Node 24.x)"),
              "Test (M)"); // the class collapses…
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
    expect_disc("Test (my-gpu-box)", "(my-gpu-box)"); // custom self-hosted runner
    expect_disc("bench (arm64-metal, cuda-12)", "(arm64-metal, cuda-12)");
    // distinct custom runners stay distinct (the whole point — align leg-to-homolog, any runner)
    EXPECT_NE(discriminant_of("Test (my-gpu-box)"), discriminant_of("Test (my-cpu-box)"));
}

// NOTE: the child_order marker-row property (job=Unordered, step=Ordered — ADR 0023 §2) migrated
// with the GitHub-Actions marker VOCABULARY to the github package suite
// (test_github_markers::JobUnorderedStepOrdered); discriminant_of / canonicalize_intent above are
// canon's SEMANTIC-UNAWARE algorithm and stay core. NOLINTEND
