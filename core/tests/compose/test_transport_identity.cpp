
// invariant: the transport IDENTITY arm, homed in the compose suite, and it is the load-bearing
// half of the transport gate.
// invariant: THE PROPERTY IS A MUST — two runs differing only by a declared transform must carry
// the SAME semantic identity, or transport-invariance is not being built.
// invariant: the per-run DECLARATION is PROVENANCE and goes downstream; the transform GRAMMAR is
// IDENTITY and enters every composed digest.
// invariant: that quotient is the entire point of the transport vocabulary — it is what lets a
// stamped stream and an unstamped one be COMPARED.
// invariant: if declaring a transport stack moved the identity, every declaration would silently
// partition the corpus into incomparable halves.
// invariant: it would do so QUIETLY, since a digest that differs reads as a different ruleset
// rather than as a bug.
// invariant: HOMED as a UNIT test in the compose suite — not in transport's suite, because the
// property is about what stream resolution does to a COMPOSITION, which is compose's code.
// invariant: transport's own suite owns the peel's SHAPE.
// invariant: NOT a corpus gate, because real bytes cannot make this property more true — the
// identity is a hash of the RULESET, computed before the first line is read.
// invariant: it is not a function of any input bytes at all, so scoring it over tens of thousands
// of logs would measure nothing the empty composition does not already settle.
// invariant: that would cost about four orders of magnitude more, and homing it on the corpus
// because the sibling arms live there would be homing by NEIGHBOURHOOD rather than by PROPERTY.
// invariant: VACUITY — two things are equal is the classic can't-FAIL shape, staying green if the
// identity returned a constant or if stream resolution ignored its declaration entirely.
// invariant: two permanent arms close that — a one-byte change to a ROW must MOVE the digest, and
// an unknown dialect must FATAL, proving the declaration is actually read.
// invariant: no implementation satisfies the opposed arm and the invariance arms by accident, and
// an invariance claim about an argument nobody reads is worthless.
// invariant: determinism — byte-only, synthetic manifests authored here, no packages linked
// because a core test never links them, and no RNG, clock or float.
#include <gtest/gtest.h>

import insight.canon.test;

using insight::StructuralRole;
using insight::semantic::compose;
using insight::semantic::ComposedSemantics;
using insight::semantic::kAnyDialect;
using insight::semantic::resolve_stream;
using insight::semantic::ResolvedStream;
using insight::semantic::SemanticPackageManifest;
using insight::semantic::StructuralRoleRow;
using insight::transport::IngestDeclaration;

namespace
{

constexpr std::string_view kGhaTransform{"api-rfc3339-line-prefix"};

// invariant: the declared stack's backing array must OUTLIVE every declaration built from it,
// because the declaration's stack field is a span.
constexpr std::array<std::string_view, 1> kDeclaredGha{{kGhaTransform}};

// invariant: core tests compose SYNTHETIC manifests to exercise the algorithms vocabulary-free.
// invariant: the mutated package differs in exactly ONE character of one prefix — the smallest
// change that must still move the digest.
constexpr std::array<StructuralRoleRow, 1> kRoles{
    {{.prefix = "<TRANSPORT-A>", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect}}};
constexpr std::array<StructuralRoleRow, 1> kRolesMutated{
    {{.prefix = "<TRANSPORT-B>", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect}}};

constexpr SemanticPackageManifest kPackage{
    .name = "transport_identity_fixture", .version = "1.0.0", .roles = kRoles};
constexpr SemanticPackageManifest kPackageMutated{
    .name = "transport_identity_fixture", .version = "1.0.0", .roles = kRolesMutated};

constexpr std::array<SemanticPackageManifest, 1> kPackages{{kPackage}};
constexpr std::array<SemanticPackageManifest, 1> kPackagesMutated{{kPackageMutated}};

TEST(TransportIdentity, DeclaringATransportStackDoesNotMoveSemanticIdentity)
{
    const ComposedSemantics composed{compose(kPackages)};
    const auto baseline{composed.identity()};

    const ResolvedStream without{resolve_stream(composed, IngestDeclaration{})};
    const ResolvedStream with{resolve_stream(
        composed, IngestDeclaration{.stack = kDeclaredGha, .dialect = {}, .channel = {}})};

    ASSERT_TRUE(without.transport.empty()) << "the ± arms are not actually ±: the 'without' arm "
                                              "resolved a non-empty stack";
    ASSERT_EQ(with.transport.size(), 1U) << "the ± arms are not actually ±: the 'with' arm "
                                            "resolved "
                                         << with.transport.size() << " rows, expected 1";

    EXPECT_EQ(without.semantics.identity(), baseline)
        << "resolving a DEGENERATE declaration moved semantic_identity.\n"
        << "  composed : " << composed.identity_hex() << "\n"
        << "  resolved : " << without.semantics.identity_hex() << "\n"
        << "semantic_identity is the RULESET's identity, not a stream's view of it "
           "(canon.compose.cppm:166).";

    EXPECT_EQ(with.semantics.identity(), baseline)
        << "DECLARING A TRANSPORT TRANSFORM MOVED semantic_identity — the transport-invariance "
           "MUST is violated: declaring a stack may not change the ruleset's identity.\n"
        << "  composed          : " << composed.identity_hex() << "\n"
        << "  ± transform (with): " << with.semantics.identity_hex() << "\n"
        << "The per-run DECLARATION is PROVENANCE (it goes to MetaLog); only the transform GRAMMAR "
           "— the catalogue — is identity. A stamped stream and an unstamped one must stay "
           "COMPARABLE.";

    EXPECT_EQ(without.semantics.identity(), with.semantics.identity())
        << "the two resolved streams disagree with each other: ± a transform is not an invariance.";
}

TEST(TransportIdentity, TheDegenerateCompositionCarriesTheSameInvariance)
{
    // invariant: the zero-package composition is a defined, runnable state whose identity is the
    // hash of just the version components.
    // invariant: the invariance must hold THERE too, or it would hold only when some row happened
    // to dominate the digest.
    const ComposedSemantics composed{compose({})};
    const auto baseline{composed.identity()};

    const ResolvedStream with{resolve_stream(
        composed, IngestDeclaration{.stack = kDeclaredGha, .dialect = {}, .channel = {}})};

    EXPECT_EQ(with.semantics.identity(), baseline)
        << "±a transform must be an invariance on the degenerate composition too.\n"
        << "  composed : " << composed.identity_hex() << "\n"
        << "  declared : " << with.semantics.identity_hex();
}

// invariant: if the identity were a constant, or the hash ignored row content, every assertion
// above would stay green while the digest guaranteed nothing.
// invariant: this arm demands the OPPOSITE outcome from the same machinery — one byte of one row
// must move it.
TEST(TransportIdentity, IdentityIsSensitiveToTheRuleset)
{
    const ComposedSemantics composed{compose(kPackages)};
    const ComposedSemantics mutated{compose(kPackagesMutated)};

    EXPECT_NE(composed.identity(), mutated.identity())
        << "a ONE-BYTE change to a structural-role prefix did not move semantic_identity, so the "
           "invariance arms above are vacuous — they would pass against a constant.\n"
        << "  '<TRANSPORT-A>' : " << composed.identity_hex() << "\n"
        << "  '<TRANSPORT-B>' : " << mutated.identity_hex();
}

// invariant: the other half of vacuity — stream resolution must actually READ the declaration,
// since an invariance over an argument that is dropped on the floor proves nothing at all.
// invariant: an unknown dialect FATALS, because canon VERIFIES a declared coordinate and never
// infers it, which is observable proof that it is read.
TEST(TransportIdentityDeathTest, DeclarationCoordinatesAreActuallyRead)
{
    const ComposedSemantics composed{compose(kPackages)};

    // invariant: the message must NAME the composed vocabulary, or the fail-closed posture is
    // unactionable.
    EXPECT_DEATH(
        {
            (void)resolve_stream(
                composed,
                IngestDeclaration{.stack = {}, .dialect = "no-such-dialect", .channel = {}});
        },
        "unknown dialect")
        << "an unknown declared dialect must fatal — this is also what proves resolve_stream reads "
           "the declaration at all, which the invariance arms depend on";

    EXPECT_DEATH(
        {
            (void)resolve_stream(
                composed,
                IngestDeclaration{.stack = {}, .dialect = "no-such-dialect", .channel = {}});
        },
        "transport_identity_fixture");

    // invariant: a KNOWN dialect must NOT fatal — an unknown name is a MISTAKE, an absent one is
    // a CHOICE, and a correct one is neither.
    // invariant: without this leg the death arm above would also pass against an implementation
    // that fataled on every dialect.
    const ResolvedStream resolved{resolve_stream(
        composed,
        IngestDeclaration{.stack = {}, .dialect = "transport_identity_fixture", .channel = {}})};
    EXPECT_EQ(resolved.semantics.identity(), composed.identity())
        << "declaring a KNOWN dialect must also leave semantic_identity untouched";
}

} // namespace
