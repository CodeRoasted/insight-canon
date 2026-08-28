// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_instance_discriminant.cpp — discriminant_of, canon's SEMANTIC-UNAWARE complement of
// canonicalize_intent — the third role on the identity spine (SRC-II-9). The class MASKS
// drift tokens to group siblings; the discriminant KEEPS the raw declared coordinate verbatim to
// SEPARATE co-occurring / cross-run-drifted siblings — same R1–R4 scan, first masked span kept raw.
// Runner-agnostic by construction (it reads the declared tuple, never a hardcoded runner lexicon).
// A diff here re-draws cross-run alignment — the discriminant rides the composed
// semantic_identity, which is part of the comparison's identity; fix the code, never the assertion.

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

// ── RED ON PURPOSE (DN-38.D2) — a discriminant that keeps ONE span is not a COMPLEMENT ──
// `macos-14 (15.0.1)` … `macos-14 (15.4)` … `macos-15 (26.0.1)` are NINE DISTINCT CELLS of one
// GitHub Actions runner matrix, and they are the ordinary spelling `<os>-<NN> (<version>)`.
// `canonicalize_intent` collapses all nine to the class `macos-N (M)` — correct, that is what a
// class is FOR. The discriminant must then SEPARATE them, and today it cannot: it keeps only the
// FIRST masked span, so the five `macos-14` cells all carry the coordinate `14` and the four
// `macos-15` cells all carry `15`. Five cells, one key.
//
// The harm is not local to this function. `align_quanta` matches jobs as a multiset on
// `(class, instance_key)` and pairs colliding keys "in reconciled file order" — a comment that
// names its own assumption (true retries of an identical key), and matrix legs complete in a
// DIFFERENT order on every run. So cell `(15.3)`'s baseline is diffed against cell `(15.1)`'s
// changed. Measured on 1 000 adjudicated green→green GitHub Actions situations: 4 pairs affected,
// 8–9 of 11 cells mis-paired per pair, 70 of the 76 HIGH rows that report one template as BOTH
// appeared and vanished in a single report. Every such row is TRUE of its pair; the pair is wrong.
//
// The rule this arm holds the code to: THE DISCRIMINANT IS THE FULL COMPLEMENT OF THE CLASS —
// every masked span, in declaration order, verbatim, never the first alone. The two functions stay
// explicit complements ("same scan, same rules"), which is what makes a class and a coordinate
// agree about where a name starts.
//
// IT ASSERTS INJECTIVITY, NEVER A SPELLING. Whether the complement renders as `14 (15.3)` or as an
// ordered tuple is the implementer's call; pinning the bytes here would turn a CORRECT fix red.
// The exact-byte boundary for names with ZERO or ONE masked span is already held above by
// `KeepsRawDeclaredCoordinateVerbatim` and `RunnerAgnosticNoHardcodedNames` — every literal there
// carries at most one span, so a correct widening leaves all of them byte-identical. Not restated
// here: one assertion, one home.
TEST(InstanceDiscriminant, SeparatesCellsSharingAFirstMaskedSpan)
{
    static constexpr std::array<std::string_view, 9> kCells{
        "macos-14 (15.0.1)", "macos-14 (15.1)", "macos-14 (15.2)",
        "macos-14 (15.3)",   "macos-14 (15.4)", "macos-15 (16.0)",
        "macos-15 (16.2)",   "macos-15 (16.4)", "macos-15 (26.0.1)"};

    // The premise, and the control on the fix's blast radius: these nine really ARE one class.
    // `canonicalize_intent` is UNTOUCHED by this ruling — if this half moves, the mask was widened
    // instead of the complement, and the two functions have stopped being complements.
    for (const std::string_view cell : kCells)
        EXPECT_EQ(canonicalize_intent(cell), "macos-N (M)")
            << "class of \"" << cell
            << "\" is no longer the shared class — the collision premise "
               "this arm rests on is gone; the mask moved, and that is not what DN-38.D2 ruled";

    // The property: two DISTINCT cells of one class carry DISTINCT coordinates. The
    // 20 cross-group pairs (a `macos-14` cell against a `macos-15` one) pass TODAY — they are the
    // built-in proof that this assertion is satisfiable and not vacuously red. The 16 within-group
    // pairs are the defect.
    for (std::size_t i{0}; i < kCells.size(); ++i)
        for (std::size_t j{i + 1}; j < kCells.size(); ++j)
            EXPECT_NE(discriminant_of(kCells[i]), discriminant_of(kCells[j]))
                << "cells \"" << kCells[i] << "\" and \"" << kCells[j]
                << "\" are two units of work and they share the coordinate \""
                << discriminant_of(kCells[i])
                << "\" — the aligner keys jobs on (class, instance) and will pair each of them "
                   "with the WRONG cell, in whatever order the legs happened to finish";
}

// NOTE: the child_order marker-row property (job=Unordered, step=Ordered) migrated
// with the GitHub-Actions marker VOCABULARY to the github package suite
// (test_github_markers::JobUnorderedStepOrdered); discriminant_of / canonicalize_intent above are
// canon's SEMANTIC-UNAWARE algorithm and stay core. NOLINTEND
