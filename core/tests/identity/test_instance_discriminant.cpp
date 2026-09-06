
// invariant: the discriminant is canon's SEMANTIC-UNAWARE complement of the intent canonicalizer
// — the third role on the identity spine.
// invariant: the CLASS masks drift tokens to GROUP siblings; the DISCRIMINANT keeps the raw
// declared coordinate verbatim to SEPARATE co-occurring or cross-run-drifted siblings.
// invariant: same scan, same rules, with the first masked span kept RAW.
// invariant: RUNNER-AGNOSTIC by construction — it reads the declared tuple and never a hardcoded
// runner lexicon.
// invariant: a diff here RE-DRAWS cross-run alignment, because the discriminant rides the composed
// semantic identity, which is part of the comparison's identity.
// invariant: fix the CODE, never the assertion.
// invariant: the child-order marker-row property is NOT here — it migrated with the dialect
// marker VOCABULARY to that package's own suite.
// refs: SRC-II-9
#include <array>
#include <cstddef>
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

TEST(InstanceDiscriminant, KeepsRawDeclaredCoordinateVerbatim)
{
    // invariant: a matrix tuple — the class masks it while the discriminant keeps it raw, so the
    // legs stay separable.
    expect_disc("Test (ubuntu-latest, Node 24.x)", "(ubuntu-latest, Node 24.x)");
    expect_disc("Test (windows-latest, Node 24.x)", "(windows-latest, Node 24.x)");
    EXPECT_EQ(canonicalize_intent("Test (ubuntu-latest, Node 24.x)"), "Test (M)");
    EXPECT_NE(discriminant_of("Test (ubuntu-latest, Node 24.x)"),
              discriminant_of("Test (windows-latest, Node 24.x)"));

    // invariant: version drift ACROSS runs yields distinct discriminants, so the pair surfaces as
    // REPLACED rather than a masked-away empty result.
    expect_disc("ESLint v6", "v6");
    expect_disc("ESLint v7", "v7");

    // invariant: a name with NO drift token yields an EMPTY discriminant, which is a single
    // unparameterized job.
    expect_disc("Lint", "");
    expect_disc("Build", "");
}

// invariant: RUNNER-AGNOSTIC — a user's self-hosted runner is captured with no hardcoded lexicon.
TEST(InstanceDiscriminant, RunnerAgnosticNoHardcodedNames)
{
    expect_disc("Test (my-gpu-box)", "(my-gpu-box)");
    expect_disc("bench (arm64-metal, cuda-12)", "(arm64-metal, cuda-12)");
    // invariant: distinct custom runners stay distinct, which is the whole point — align leg to
    // homolog, on any runner.
    EXPECT_NE(discriminant_of("Test (my-gpu-box)"), discriminant_of("Test (my-cpu-box)"));
}

// invariant: PRE-REGISTERED RED and GREEN SINCE THE FIX — this arm was written against a
// discriminant that kept only the FIRST masked span, and it passes today.
// invariant: the coordinate is now the ENVELOPE — the first span's start to the last span's end,
// class material between them included — so the arm is a REGRESSION GUARD, not an open defect.
// invariant: a red here means the complement narrowed back towards a single span.
// invariant: THE DEFECT IT WAS WRITTEN FOR — nine distinct cells of one runner matrix share the
// ordinary spelling, and the canonicalizer collapses all nine to one class.
// invariant: that collapse is CORRECT and is what a class is FOR, so the discriminant must be what
// separates them.
// invariant: keeping only the first span gave five cells one coordinate and four cells another —
// FIVE CELLS, ONE KEY.
// invariant: the harm was not local to this function: alignment matches jobs as a multiset on the
// class and the instance key, and pairs colliding keys in reconciled file order.
// invariant: that comment NAMES ITS OWN ASSUMPTION, true retries of an identical key, and matrix
// legs complete in a DIFFERENT order on every run.
// invariant: so one cell's baseline was diffed against another cell's changed.
// invariant: MEASURED on 1 000 adjudicated green-to-green situations — 4 pairs affected, with 8
// or 9 of 11 cells mis-paired per pair.
// invariant: and 70 rows reporting one template as BOTH appeared and vanished in a single report.
// invariant: every such row was TRUE of its pair; the PAIR was wrong.
// invariant: THE SEVERITY OF THOSE 70 WAS STATED WRONG HERE UNTIL 2026-08-28, and the COUNT was
// not.
// invariant: the pinned re-run corrected it — the population is 35 new-template at high plus 35
// vanished-template at medium, one appeared-and-vanished pair per mis-paired cell.
// invariant: only the APPEARED half is high; all 70 went to zero on the widened coordinate, and
// each of the four reports went to zero ranked rows.
// invariant: THE RULE THIS ARM HOLDS THE CODE TO — the discriminant is the FULL complement of the
// class, never the first span alone.
// invariant: the two functions stay explicit complements, which is what makes a class and a
// coordinate agree about where a name starts.
// invariant: IT ASSERTS INJECTIVITY OVER THIS CLASS, NEVER A SPELLING — whether the complement
// renders as a joined string, an ordered tuple or a contiguous envelope is the implementer's call.
// invariant: pinning the BYTES here would turn a CORRECT fix red, which is exactly what the shipped
// envelope would have done.
// invariant: and the envelope is NOT injective over arbitrary strings — the declared bound is
// that it separates names whose spans occupy the same class positions.
// invariant: the exact-byte boundary for names with ZERO or ONE masked span is already held by the
// two arms above, whose every literal carries at most one span.
// invariant: so the widening left all of them byte-identical, and it is not restated here — one
// assertion, one home.
// refs: DN-38.D2, DN-38.D3
TEST(InstanceDiscriminant, SeparatesCellsSharingAFirstMaskedSpan)
{
    static constexpr std::array<std::string_view, 9> kCells{
        "macos-14 (15.0.1)", "macos-14 (15.1)", "macos-14 (15.2)",
        "macos-14 (15.3)",   "macos-14 (15.4)", "macos-15 (16.0)",
        "macos-15 (16.2)",   "macos-15 (16.4)", "macos-15 (26.0.1)"};

    // invariant: THE PREMISE, and the control on the fix's blast radius — these nine really ARE
    // one class.
    // invariant: the canonicalizer is UNTOUCHED by this ruling, so if this half moves, the MASK was
    // widened instead of the complement and the two functions have stopped being complements.
    for (const std::string_view cell : kCells)
        EXPECT_EQ(canonicalize_intent(cell), "macos-N (M)")
            << "class of \"" << cell
            << "\" is no longer the shared class — the collision premise "
               "this arm rests on is gone; the mask moved, and that is not what DN-38.D2 ruled";

    // invariant: THE PROPERTY — two DISTINCT cells of one class carry DISTINCT coordinates.
    // invariant: the 20 cross-group pairs pass TODAY and are the built-in proof that this assertion
    // is SATISFIABLE and not vacuously red; the 16 within-group pairs are the defect.
    for (std::size_t i{0}; i < kCells.size(); ++i)
        for (std::size_t j{i + 1}; j < kCells.size(); ++j)
            EXPECT_NE(discriminant_of(kCells[i]), discriminant_of(kCells[j]))
                << "cells \"" << kCells[i] << "\" and \"" << kCells[j]
                << "\" are two units of work and they share the coordinate \""
                << discriminant_of(kCells[i])
                << "\" — the aligner keys jobs on (class, instance) and will pair each of them "
                   "with the WRONG cell, in whatever order the legs happened to finish";
}
