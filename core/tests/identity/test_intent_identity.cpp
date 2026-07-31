// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_intent_identity.cpp — the frozen `intent-gha-2` canonicalizer contract
// (bibles/intent_identity.md §9; canon d2d460d). These pin the closure-as-identity
// constructor: matrix legs / shards / version-parameterized jobs of ONE intent
// canonicalize to ONE CLASS and pair across homologous runs (G1, studies/004),
// WITHOUT over-collapsing distinct WHERE (II-2: alignment must never eat the signal).
// canonicalize_intent is canon's SEMANTIC-UNAWARE algorithm, frozen under kCanonicalizationVersion;
// the composed-ruleset comparability key (II-7) is now semantic_identity (ADR 0024 §4,
// tests/compose/ test_composition.cpp). A diff here is a cross-run comparability break, not a
// retune — fix the code, never the assertion.

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

// Two distinct WHERE must stay DISTINCT (different ids) — the II-2 over-collapse guard.
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

// ── G1: matrix / shard / version recovery (studies/004, the load-bearing property) ──
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

// ── II-2 guard: a single BARE digit is a distinct WHERE ordinal, KEPT verbatim ──
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

// NOTE: the II-7 comparability-version assertion (formerly RegistryVersionIsFrozen, pinning
// kIntentRegistryVersion == "intent-gha-2") RETIRED with the constant (ADR 0024 §4.1): the
// composed-ruleset content hash `semantic_identity` supersedes it and is pinned in
// tests/compose/test_composition.cpp (stability + reproducibility + order-independence).
// canonicalize_intent below stays canon's frozen semantic-unaware algorithm.

// ── Co-location invariant (II-1) ──
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
