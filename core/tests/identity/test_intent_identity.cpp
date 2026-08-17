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

// ── A CLASS PREFIX SURVIVES A NON-WORD SEPARATOR — and the two shapes where it does not ──
//
// WHY THIS IS CANON'S PROPERTY AND NOT ITS CONSUMER'S. A downstream join (`DN-37.D32`, the
// `needs:` fold's containment arm over the platform's own `"<anchor> / <remainder>"` rendering)
// tests whether one canonicalized name CONTAINS another by matching a class PREFIX at a separator.
// That arm is sound only if canonicalizing the WHOLE composed name leaves the anchor's own
// canonical form intact at the front — which is a property of this algorithm, decided here.
//
// IT HOLDS BECAUSE THE PASS IS STATELESS AT A NON-WORD BYTE, and the argument is the code's: the
// rewrite is one left-to-right pass whose only carried state is `prev_is_word`, and R1/R2/R3 fire
// only at a LEADING word boundary and require a trailing one, so a separator made of non-word
// bytes both terminates the anchor's last claim and resets the scan. The remainder is then
// canonicalized exactly as it would be alone.
//
// IT IS NOT UNCONDITIONAL, and the two escapes are the whole content of the arm below:
//   • R4 IS NOT BOUNDARY-ANCHORED — it scans forward for the first ')'. An unclosed '(' is literal
//     in ISOLATION and claims a ')' the REMAINDER supplies, so the anchor's own bytes vanish into
//     an (M) that did not exist when it stood alone.
//   • THE TRIM IS A WHOLE-STRING OPERATION. A trim byte that is TRAILING in the anchor is INTERIOR
//     in the composition, so it survives there and is absent from the anchor's own canonical form.
//     (A LEADING one is trimmed in both and is harmless — asserted, because the asymmetry is the
//     kind of thing a reader assumes away.)
// Both shapes are producer-authorable — a job may legally be named `build (x` or `build ` — so a
// consumer of the prefix arm must EXCLUDE them at the point it derives the anchor's coordinate,
// never discover them as a silent mis-join.
namespace
{
// The platform rendering's separator (`DN-37.D14` owns the grammar; it is cited, never restated —
// canon knows only that these bytes are non-word). `kOtherSeparator` is here so the arm asserts the
// BYTE-CLASS property rather than one consumer's literal.
constexpr std::string_view kPlatformSeparator{" / "};
constexpr std::string_view kOtherSeparator{" ▸ "};

// Anchors drawn from this file's own frozen rule-set vocabulary — one per rule, plus the plain and
// already-masked shapes. No corpus bytes: the breaker class is characterized by the RULES, so the
// rules are the population (`MEM:synthetic-gate-vacuity-vs-judgment` — a scoped population with a
// stated predicate, never one picked by eye).
constexpr std::array<std::string_view, 9> kWellFormedAnchors{
    {"Lint", "Build", "ESLint v6", "Node 18", "build 1.2.3", "Shard 1", "deploy (prod)",
     "Test (ubuntu-latest, Node 24.x)", "yarn test-build (1/10)"}};
// Remainders, including every shape that could reach BACK into an anchor: a bare ')', a paren
// group, and the numeric anchors again.
constexpr std::array<std::string_view, 8> kRemainders{
    {"inner", "test (fast)", ")", "(x)", "v6", "1.2.3", "42", "build / deep"}};

void expect_prefix_preserved(std::string_view anchor, std::string_view remainder,
                             std::string_view separator)
{
    const std::string composed{std::string{anchor} + std::string{separator} +
                               std::string{remainder}};
    const std::string want{canonicalize_intent(anchor) + std::string{separator}};
    const std::string got{canonicalize_intent(composed)};
    EXPECT_TRUE(got.starts_with(want))
        << "the anchor's class did not survive composition:\n"
        << "  anchor    \"" << anchor << "\" -> \"" << canonicalize_intent(anchor) << "\"\n"
        << "  remainder \"" << remainder << "\"\n"
        << "  composed  \"" << composed << "\" -> \"" << got << "\"\n"
        << "  expected it to start with \"" << want << '"';
}
} // namespace

TEST(IntentCanonicalize, ClassPrefixSurvivesANonWordSeparator)
{
    for (const std::string_view separator : {kPlatformSeparator, kOtherSeparator})
        for (const std::string_view anchor : kWellFormedAnchors)
            for (const std::string_view remainder : kRemainders)
                expect_prefix_preserved(anchor, remainder, separator);

    // NON-VACUITY: the arm must be able to observe a rewrite, or it is a property about strings
    // canon never touched. At least one anchor and one remainder MUST canonicalize to something
    // other than themselves, or the loop above proves only that concatenation concatenates.
    EXPECT_NE(canonicalize_intent("Test (ubuntu-latest, Node 24.x)"),
              "Test (ubuntu-latest, Node 24.x)");
    EXPECT_NE(canonicalize_intent("v6"), "v6");
    // …and the rewrite genuinely happens on BOTH sides of the separator in one composition.
    EXPECT_EQ(
        canonicalize_intent(std::string{"ESLint v6"} + std::string{kPlatformSeparator} + "Node 18"),
        "ESLint vX / Node N");
}

// ── The two escapes, asserted POSITIVELY: this is the precondition, not a defect ──
// A consumer excluding these two shapes has a sound prefix arm; one that does not has a silent
// mis-join. Stating the boundary is what makes the arm above safe to rely on.
TEST(IntentCanonicalize, PrefixPreservationEscapesAreTheUnclosedParenAndTheTrailingTrimByte)
{
    // ① R4 reaches ACROSS the separator when the anchor leaves a '(' open.
    EXPECT_EQ(canonicalize_intent("build (x"), "build (x"); // literal in isolation…
    EXPECT_EQ(canonicalize_intent("build (x / test (fast)"),
              "build (M)"); // …claimed the remainder's ')'
    EXPECT_FALSE(
        canonicalize_intent("build (x / test (fast)")
            .starts_with(canonicalize_intent("build (x") + std::string{kPlatformSeparator}))
        << "an unclosed '(' anchor no longer breaks the prefix — if R4 became boundary-anchored, "
           "this boundary is stale and the consumer's exclusion can be dropped.";

    // ② A TRAILING trim byte on the anchor is INTERIOR after composition and survives.
    EXPECT_EQ(canonicalize_intent("build "), "build");
    EXPECT_EQ(canonicalize_intent("build  / inner"), "build  / inner"); // the anchor's space stayed
    EXPECT_FALSE(canonicalize_intent("build  / inner")
                     .starts_with(canonicalize_intent("build ") + std::string{kPlatformSeparator}));
    // The same byte class, on the shape that is NOT hypothetical: a Windows runner's CR.
    EXPECT_EQ(canonicalize_intent("build\r"), "build");
    EXPECT_FALSE(
        canonicalize_intent("build\r / inner")
            .starts_with(canonicalize_intent("build\r") + std::string{kPlatformSeparator}));

    // ③ …and the asymmetry: a LEADING trim byte is trimmed in BOTH, so it is harmless.
    expect_prefix_preserved(" build", "inner", kPlatformSeparator);
}

// ── A class prefix is a CLASS predicate — it separates no siblings, by construction ──
// The mask exists to collapse matrix legs to one class (MatrixLegsCollapseToOneClass above), so a
// prefix match at the separator cannot distinguish two legs of one job: it is an instance-BLIND
// test and any consumer reading it as "this row is inside THAT leg" is over-reading it. The
// separation lives in the discriminant (test_instance_discriminant.cpp), which this arm does not
// have — the anchor of a containment match carries no instance to compare.
TEST(IntentCanonicalize, AClassPrefixIsInstanceBlindAndSeparatesNoSiblings)
{
    constexpr std::string_view lhs{"Test (ubuntu-latest, Node 24.x)"};
    constexpr std::string_view rhs{"Test (windows-latest, Node 24.x)"};
    ASSERT_EQ(canonicalize_intent(lhs), canonicalize_intent(rhs)); // one class, two legs
    EXPECT_TRUE(canonicalize_intent(std::string{rhs} + std::string{kPlatformSeparator} + "inner")
                    .starts_with(canonicalize_intent(lhs) + std::string{kPlatformSeparator}))
        << "the OTHER leg's rendered name no longer matches this leg's class prefix — if the mask "
           "stopped collapsing legs, the consumer's instance-blindness caveat is stale.\n"
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
