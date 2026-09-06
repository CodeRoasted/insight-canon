
// invariant: the FROZEN intent canonicalizer contract — these pin the closure-as-identity
// constructor.
// invariant: matrix legs, shards and version-parameterized jobs of ONE intent canonicalize to ONE
// CLASS and pair across homologous runs.
// invariant: WITHOUT over-collapsing distinct WHERE — alignment must never eat the signal.
// invariant: the canonicalizer is canon's SEMANTIC-UNAWARE algorithm, frozen under the
// canonicalization version.
// invariant: the comparability key — that the ruleset is part of the comparison's identity — is
// now the composed semantic identity, pinned in the composition suite.
// invariant: a diff here is a cross-run COMPARABILITY BREAK and not a retune; fix the code, never
// the assertion.
// refs: SRC-II-2
#include <gtest/gtest.h>

import insight.canon.test;

using insight::canonicalize_intent;
using insight::intent_id_of;
using insight::render;
using insight::template_id_of;
using insight::TemplateId;

namespace
{

// invariant: verbose on failure — the input, the actual canonical form and the expected one,
// which is enough to diagnose without a debugger.
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

// invariant: two names of ONE intent must ALIGN, and on failure both inputs, both canonical forms
// and both rendered ids are printed.
void expect_same_intent(std::string_view lhs, std::string_view rhs)
{
    EXPECT_EQ(intent_id_of(lhs), intent_id_of(rhs))
        << "intents must ALIGN but got distinct ids:\n"
        << "  \"" << lhs << "\" -> \"" << canonicalize_intent(lhs)
        << "\" = " << render(intent_id_of(lhs)) << '\n'
        << "  \"" << rhs << "\" -> \"" << canonicalize_intent(rhs)
        << "\" = " << render(intent_id_of(rhs));
}

// invariant: two distinct WHERE must stay DISTINCT — the over-collapse guard.
// refs: SRC-II-2
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

// invariant: the load-bearing ALIGNMENT property — version-parameterized jobs, index tuples and
// matrix groups of ONE intent canonicalize to ONE class and therefore pair across runs.
// invariant: this is the exact mask that was measured taking alignment from zero to a hundred
// percent once the tokens are masked.
TEST(IntentCanonicalize, MatrixLegsCollapseToOneClass)
{
    expect_same_intent("ESLint v6", "ESLint v7");
    expect_same_intent("yarn test-build (1/10)", "yarn test-build (10/10)");
    expect_same_intent("(Shard 1/5)", "(Shard 5/5)");

    // invariant: the canonical form IS the intent class, not merely SOME equal string.
    expect_canon({"ESLint v6", "ESLint vX"});
    expect_canon({"ESLint v7", "ESLint vX"});
    expect_canon({"yarn test-build (1/10)", "yarn test-build (M)"});
}

// invariant: a single BARE digit is a distinct WHERE ordinal and is KEPT verbatim.
// invariant: collapsing it would OVER-MERGE — two shards are two instances, ordinal-separated
// downstream, never fused into one class here.
// refs: SRC-II-2
TEST(IntentCanonicalize, SingleBareDigitKeptNoOverCollapse)
{
    expect_canon({"Shard 1", "Shard 1"});
    expect_canon({"Shard 2", "Shard 2"});
    expect_distinct_intent("Shard 1", "Shard 2");
}

// invariant: a numeric or version run is masked ONLY when it is a FREE token — glued to letters
// on either edge it stays literal, so there is no over-eager masking.
// invariant: unbalanced parens stay literal, and the payload is trimmed because markers are
// extracted after the banner prefix.
TEST(IntentCanonicalize, WordBoundaryEdges)
{
    expect_canon({"worker-42", "worker-N"});
    expect_canon({"worker42", "worker42"});
    expect_canon({"v6x", "v6x"});
    expect_canon({"1.2.3rc", "1.2.3rc"});
    expect_canon({"(oops", "(oops"});
    expect_canon({"  Build  ", "Build"});
}

// invariant: the identity is the WHOLE canonical string and not a per-rule fragment, so distinct
// job names never fuse just because they share a masked token POSITION.
TEST(IntentCanonicalize, FrozenRuleSet)
{
    expect_canon({"Node 18", "Node N"});
    expect_canon({"build 1.2.3", "build vX"});
    expect_canon({"v1.2.3", "vX"});
    expect_canon({"v6", "vX"});
    expect_distinct_intent("Lint", "Build");
}

// invariant: RIPPED AT THE DRAIN — the two prefix-preservation arms and their escape catalogue
// stood here, CORRECT, until the ruling deleted their only consumer.
// invariant: they asserted that canonicalizing a whole separator-joined name leaves the anchor's
// own canonical form intact at the front, plus the two shapes where it does not.
// invariant: THEIR CONSUMER WAS THE FOLD'S PREFIX ARM AND IT NO LONGER EXISTS — the row
// membership test now has ONE arm, job-coordinate EQUALITY.
// invariant: the anchor-to-rendering containment moved to a raw-byte prefix between two names from
// ONE authority, mirrored in the action's own join.
// invariant: VERIFIED ACROSS BOTH REPOS AT THE DRAIN — nothing canonicalizes across a separator
// and then compares prefixes.
// invariant: an equality join is INSENSITIVE to what the mask does, because both sides apply the
// same function to the same producer bytes.
// invariant: AND THE TWO ESCAPES EVAPORATED WITH IT RATHER THAN MOVING — at the raw-byte
// containment there is nothing to exclude, because the byte comparison is true on the bytes.
// invariant: the escapes were ARTIFACTS of canonicalizing BEFORE comparing, so removing that
// comparison discharged the requirement BY CONSTRUCTION.
// invariant: no exclusion was ever written and none is needed.
// invariant: KEPT is the arm below, which is the OPPOSITE of the two ripped ones — it is the
// standing statement of WHY no prefix arm may exist, and it is cited from three live sites.
// refs: DN-37.D33
// invariant: A CLASS PREFIX IS A CLASS PREDICATE — it separates no siblings, BY CONSTRUCTION.
// invariant: the mask exists to collapse matrix legs to one class, so a prefix match at the
// separator CANNOT distinguish two legs of one job.
// invariant: it is an instance-BLIND test, and any consumer reading it as `this row is inside THAT
// leg` is over-reading it.
// invariant: the separation lives in the DISCRIMINANT, which this arm does not have — the anchor
// of a containment match carries no instance to compare.
// invariant: THIS ARM OUTLIVED THE CONSUMER ITS TWO SIBLINGS DIED WITH, and the asymmetry is the
// whole reason it is kept.
// invariant: they said a prefix arm is SOUND under these preconditions; this one says a prefix arm
// CANNOT separate siblings, EVER.
// invariant: the ruling deleted the fold's prefix arm on exactly that ground — every guard for it
// was REFUTED AT SOURCE.
// invariant: so this is now the standing statement of WHY no such arm may exist, and ripping it
// with them would have deleted the REASON along with the consumer.
// invariant: it is cited from three live sites, and they are the reason its NAME may not drift.
// invariant: IT COMPOSES A NAME ACROSS THE SEPARATOR ITSELF, and that is NOT the ripped property
// returning.
// invariant: the composition here is the arm's DEMONSTRATION — how a sibling leg's rendered name
// is shown to match this leg's class prefix — not a property any consumer relies on.
// invariant: its two anchors carry balanced parens and no trailing trim byte, so the escapes the
// ripped catalogue described cannot bite it.
// refs: DN-37.D33
TEST(IntentCanonicalize, AClassPrefixIsInstanceBlindAndSeparatesNoSiblings)
{
    // invariant: the separator's grammar is OWNED elsewhere and cited rather than restated —
    // canon knows only that these bytes are non-word.
    // refs: DN-37.D14
    constexpr std::string_view kPlatformSeparator{" / "};
    constexpr std::string_view lhs{"Test (ubuntu-latest, Node 24.x)"};
    constexpr std::string_view rhs{"Test (windows-latest, Node 24.x)"};
    ASSERT_EQ(canonicalize_intent(lhs), canonicalize_intent(rhs));
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

// invariant: the comparability-version assertion RETIRED with the constant it pinned — the
// composed-ruleset content hash supersedes it and is pinned in the composition suite.
// invariant: the canonicalizer below stays canon's FROZEN semantic-unaware algorithm.
// invariant: the CO-LOCATION invariant — the identity is a STRUCTURAL KEY and never a retained
// value.
// invariant: the intent id is BY DEFINITION the template id of the canonical form, so the identity
// is the structural hash under the registry version and never a separate hashing path.
TEST(IntentIdentity, IdIsHashOfCanonicalForm)
{
    for (const std::string_view name : {"ESLint v6", "Shard 1", "worker-42", "deploy (prod)", ""})
        EXPECT_EQ(intent_id_of(name), template_id_of(canonicalize_intent(name)))
            << "intent_id_of diverged from template_id_of(canonicalize_intent()) for: \"" << name
            << '"';
}
