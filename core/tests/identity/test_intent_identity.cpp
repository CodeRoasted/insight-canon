// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_intent_identity.cpp — the frozen `intent-gha-2` canonicalizer contract (canon d2d460d).
// These pin the closure-as-identity constructor: matrix legs / shards / version-parameterized
// jobs of ONE intent canonicalize to ONE CLASS and pair across homologous runs,
// WITHOUT over-collapsing distinct WHERE (SRC-II-2: alignment must never eat the signal).
// canonicalize_intent is canon's SEMANTIC-UNAWARE algorithm, frozen under kCanonicalizationVersion;
// the comparability key — the ruleset is part of the comparison's identity — is now the composed
// `semantic_identity`, pinned in tests/compose/test_composition.cpp. A diff here is a cross-run
// comparability break, not a retune — fix the code, never the assertion.

#include <gtest/gtest.h>

import insight.canon.test;

using insight::canonicalize_intent;
using insight::intent_id_of;
using insight::render;
using insight::template_id_of;
using insight::TemplateId;

namespace
{

// A canonicalization case: input → its expected intent CLASS. Verbose-on-failure prints
// input, actual canonical form, and expected — enough to diagnose without a debugger.
struct CanonCase
{
    std::string_view input;
    std::string_view expected;
};

void expect_canon(const CanonCase& kase)
{
    const std::string got{canonicalize_intent(kase.input)};
    EXPECT_EQ(got, kase.expected) << "canonicalize_intent(\"" << kase.input << "\") = \"" << got
                                  << "\"  expected \"" << kase.expected << '"';
}

// Two names of ONE intent must ALIGN (same id). On failure print both inputs, both
// canonical forms, and both rendered ids.
void expect_same_intent(std::string_view lhs, std::string_view rhs)
{
    EXPECT_EQ(intent_id_of(lhs), intent_id_of(rhs))
        << "intents must ALIGN but got distinct ids:\n"
        << "  \"" << lhs << "\" -> \"" << canonicalize_intent(lhs)
        << "\" = " << render(intent_id_of(lhs)) << '\n'
        << "  \"" << rhs << "\" -> \"" << canonicalize_intent(rhs)
        << "\" = " << render(intent_id_of(rhs));
}

// Two distinct WHERE must stay DISTINCT (different ids) — the SRC-II-2 over-collapse guard.
void expect_distinct_intent(std::string_view lhs, std::string_view rhs)
{
    EXPECT_NE(intent_id_of(lhs), intent_id_of(rhs))
        << "intents must stay DISTINCT but collapsed to one id:\n"
        << "  \"" << lhs << "\" -> \"" << canonicalize_intent(lhs)
        << "\" = " << render(intent_id_of(lhs)) << '\n'
        << "  \"" << rhs << "\" -> \"" << canonicalize_intent(rhs)
        << "\" = " << render(intent_id_of(rhs));
}

} // namespace

// ── G1: matrix / shard / version recovery — the load-bearing alignment property ──
// Version-parameterized jobs (R2), index tuples and matrix groups (R4) of ONE intent
// canonicalize to ONE class → identical id, so they pair across homologous runs. This is
// the exact mask G1 measured: ESLint v6/v7 go 0%→100% alignment once the tokens are masked.
TEST(IntentCanonicalize, MatrixLegsCollapseToOneClass)
{
    expect_same_intent("ESLint v6", "ESLint v7");                            // R2 v-version
    expect_same_intent("yarn test-build (1/10)", "yarn test-build (10/10)"); // R4 index tuple
    expect_same_intent("(Shard 1/5)", "(Shard 5/5)");                        // R4 whole-name group

    // …and the canonical form IS the intent class, not just "some equal string":
    expect_canon({"ESLint v6", "ESLint vX"});
    expect_canon({"ESLint v7", "ESLint vX"});
    expect_canon({"yarn test-build (1/10)", "yarn test-build (M)"});
}

// ── SRC-II-2 guard: a single BARE digit is a distinct WHERE ordinal, KEPT verbatim ──
// Collapsing it would over-merge — `Shard 1` and `Shard 2` are two instances, ordinal-
// separated downstream, never fused into one class here.
TEST(IntentCanonicalize, SingleBareDigitKeptNoOverCollapse)
{
    expect_canon({"Shard 1", "Shard 1"});
    expect_canon({"Shard 2", "Shard 2"});
    expect_distinct_intent("Shard 1", "Shard 2");
}

// ── Word-boundary correctness (\b-anchored, ASCII \w = [A-Za-z0-9_]) ──
// A numeric/version run is masked ONLY when it is a free token. Glued to letters on either
// edge it stays literal — no over-eager masking. Unbalanced parens stay literal; the
// payload is trimmed (markers are extracted after the banner prefix — G1 strips).
TEST(IntentCanonicalize, WordBoundaryEdges)
{
    expect_canon({"worker-42", "worker-N"}); // '-' is a leading boundary → R3 multi-digit fires
    expect_canon({"worker42", "worker42"});  // glued after 'r' → no leading boundary → kept
    expect_canon({"v6x", "v6x"});            // no trailing boundary (x) → not a v-version
    expect_canon({"1.2.3rc", "1.2.3rc"});    // no trailing boundary → not a dotted version
    expect_canon({"(oops", "(oops"});        // unbalanced '(' → literal, no (M)
    expect_canon({"  Build  ", "Build"});    // leading/trailing whitespace trimmed
}

// ── The frozen intent-gha-2 rule set, each rule on its own token ──
// The identity is the WHOLE canonical string, not a per-rule fragment: distinct job names
// never fuse just because they share a masked token position.
TEST(IntentCanonicalize, FrozenRuleSet)
{
    expect_canon({"Node 18", "Node N"});       // R3 multi-digit index
    expect_canon({"build 1.2.3", "build vX"}); // R1 dotted version
    expect_canon({"v1.2.3", "vX"});            // R1 dotted version with leading v
    expect_canon({"v6", "vX"});                // R2 bare v-version
    expect_distinct_intent("Lint", "Build");   // distinct jobs never collapse
}

// ── RIPPED AT THE DRAIN (`DN-37.D33`) — the prefix-preservation arms and their escape catalogue ──
//
// `ClassPrefixSurvivesANonWordSeparator` and
// `PrefixPreservationEscapesAreTheUnclosedParenAndTheTrailingTrimByte` stood here, correct, from
// `54143cb` until `DN-37.D33` deleted their only consumer. They asserted that canonicalizing a
// whole `"<anchor><sep><remainder>"` name leaves the anchor's own canonical form intact at the
// front, plus the two shapes where it does not (an unclosed `(` claiming the remainder's `)`, and
// a trailing trim byte turning interior).
//
// THEIR CONSUMER WAS THE FOLD'S PREFIX ARM, AND IT NO LONGER EXISTS. `row_belongs_to` now has ONE
// arm — job-coordinate EQUALITY — and the anchor→rendering containment moved to a raw-byte prefix
// between two names from ONE authority (`insight-eidos` `diff_engine.cpp` `renderings_of`, mirrored
// in `sift-action/src/jobgraph.ts` `joinDeclaredJobs`). Verified across both repos at the drain:
// nothing canonicalizes across a separator and then compares prefixes. An equality join is
// insensitive to what the mask does, because both sides apply the same function to the same
// producer bytes.
//
// ⚠ AND THE TWO ESCAPES EVAPORATED WITH IT RATHER THAN MOVING. `DN-37.D33` clause 4 says they
// "stay excluded, and now at ONE site". At the raw-byte containment there is nothing to exclude:
// `"build (x / test (fast)".starts_with("build (x" + " / ")` is true on the bytes, and so is the
// trailing-trim-byte case. The escapes were artifacts of canonicalizing BEFORE comparing, so
// removing that comparison discharged the requirement by construction — no exclusion was ever
// written, and none is needed.
//
// KEPT: the arm below. It is the opposite of the two ripped ones — it is the standing statement of
// WHY no prefix arm may exist, and it is cited from three live sites.

// ── A class prefix is a CLASS predicate — it separates no siblings, by construction ──
//
// The mask exists to collapse matrix legs to one class (MatrixLegsCollapseToOneClass above), so a
// prefix match at the separator cannot distinguish two legs of one job: it is an instance-BLIND
// test and any consumer reading it as "this row is inside THAT leg" is over-reading it. The
// separation lives in the discriminant (test_instance_discriminant.cpp), which this arm does not
// have — the anchor of a containment match carries no instance to compare.
//
// ⚠ THIS ARM OUTLIVED THE CONSUMER ITS TWO SIBLINGS DIED WITH, and the asymmetry is the whole
// reason it is kept. They said "a prefix arm is SOUND under these preconditions"; this one says
// "a prefix arm CANNOT separate siblings, ever". `DN-37.D33` deleted the fold's prefix arm on
// exactly that ground — every guard for it was refuted at source — so this is now the standing
// statement of WHY no such arm may exist, and ripping it with them would have deleted the reason
// along with the consumer.
//
// It is cited from three live sites and they are the reason its name may not drift:
// `insight-eidos` `sift/src/engine/diff_engine.cpp` (the no-prefix-arm block),
// `sift/tests/report/needs_graph_fold_test.cpp` (the sibling-absorption arm), and
// `sift/tests/report/unit_identity_over_merge_test.cpp` (`DN-38`'s over-merge, which is the same
// collapse seen from the identity side).
//
// ⚠ IT COMPOSES A NAME ACROSS THE SEPARATOR ITSELF, and that is not the ripped property returning.
// The composition here is the arm's DEMONSTRATION — it is how a sibling leg's rendered name is
// shown to match this leg's class prefix — not a property any consumer relies on. Its two anchors
// carry balanced parens and no trailing trim byte, so the escapes the ripped catalogue described
// cannot bite it.
TEST(IntentCanonicalize, AClassPrefixIsInstanceBlindAndSeparatesNoSiblings)
{
    // The platform rendering's separator (`DN-37.D14` owns the grammar; cited, never restated —
    // canon knows only that these bytes are non-word).
    constexpr std::string_view kPlatformSeparator{" / "};
    constexpr std::string_view lhs{"Test (ubuntu-latest, Node 24.x)"};
    constexpr std::string_view rhs{"Test (windows-latest, Node 24.x)"};
    ASSERT_EQ(canonicalize_intent(lhs), canonicalize_intent(rhs)); // one class, two legs
    EXPECT_TRUE(canonicalize_intent(std::string{rhs} + std::string{kPlatformSeparator} + "inner")
                    .starts_with(canonicalize_intent(lhs) + std::string{kPlatformSeparator}))
        << "the OTHER leg's rendered name no longer matches this leg's class prefix. If the mask "
           "stopped collapsing legs, the REFUSAL this arm underwrites is stale — `DN-37.D33` "
           "deleted the fold's prefix arm because a class prefix cannot separate siblings, and "
           "`DN-38`'s over-merge is the same collapse on the roll-up's key. Both citations must be "
           "re-derived before this arm is relaxed.\n"
        << "  \"" << lhs << "\" -> \"" << canonicalize_intent(lhs) << "\"\n"
        << "  \"" << rhs << "\" -> \"" << canonicalize_intent(rhs) << '"';
}

// NOTE: the comparability-version assertion (formerly RegistryVersionIsFrozen, pinning
// kIntentRegistryVersion == "intent-gha-2") RETIRED with the constant: the
// composed-ruleset content hash `semantic_identity` supersedes it and is pinned in
// tests/compose/test_composition.cpp (stability + reproducibility + order-independence).
// canonicalize_intent below stays canon's frozen semantic-unaware algorithm.

// ── Co-location invariant: the identity is a structural key, never a retained value ──
// intent_id_of is BY DEFINITION template_id_of(canonicalize_intent(name)): the identity is
// "the structural hash under the registry version", never a separate hashing path.
TEST(IntentIdentity, IdIsHashOfCanonicalForm)
{
    for (const std::string_view name : {"ESLint v6", "Shard 1", "worker-42", "deploy (prod)", ""})
        EXPECT_EQ(intent_id_of(name), template_id_of(canonicalize_intent(name)))
            << "intent_id_of diverged from template_id_of(canonicalize_intent()) for: \"" << name
            << '"';
}

// NOLINTEND
