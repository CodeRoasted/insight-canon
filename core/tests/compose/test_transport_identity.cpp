// NOLINTBEGIN — unit test: short identifiers, string literals and printed diagnostics are intended.
// test_transport_identity.cpp — G1's IDENTITY arm (ADR 0044 §9), homed in canon core's compose
// suite.
//
// THE PROPERTY, and why it is the load-bearing half of G1. ADR 0044 §6 states it as a MUST:
//
//     Two runs ±a transform MUST carry the same `semantic_identity`, or transport-invariance is not
//     being built.
//
// The per-run DECLARATION is PROVENANCE and goes to MetaLog; the transform GRAMMAR (the catalogue)
// is IDENTITY and enters every composed digest. That quotient is the entire point of the transport
// vocabulary: it is what lets a stamped stream and an unstamped one be COMPARED. If declaring a
// transport stack moved `semantic_identity`, every declaration would silently partition the corpus
// into incomparable halves — and it would do so quietly, since a digest that differs reads as
// "different ruleset", not as "bug".
//
// HOMING (Kleio). UNIT, in `tests/compose/` — not `tests/transport/` and not a corpus gate.
//   • Not transport's suite: the property is about what `resolve_stream` does to a COMPOSITION, and
//     `resolve_stream` is compose's (compose.cpp:250). Transport's suite owns the peel's shape.
//   • Not a corpus gate: real bytes cannot make this property more true. `semantic_identity` is a
//     hash of the RULESET, computed before the first line is read — it is not a function of any
//     input bytes at all, so scoring it over 22 000 logs would measure nothing the empty
//     composition does not already settle, at ~4 orders of magnitude more cost. Homing it on the
//     corpus because the sibling arms live there would be homing by neighbourhood, not by property.
//
// VACUITY — what makes the green mean something. "Two things are equal" is the classic can't-FAIL
// shape: it stays green if `identity()` returned a constant, or if `resolve_stream` ignored its
// declaration entirely. Two permanent arms close that:
//   • `IdentityIsSensitiveToTheRuleset` — a one-byte change to a ROW must MOVE the digest. This is
//     the opposed arm: no implementation satisfies both it and the invariance arms by accident.
//   • `DeclarationCoordinatesAreActuallyRead` — an unknown dialect FATALS, proving the declaration
//     reaches `resolve_stream` rather than being dropped on the floor. An invariance claim about an
//     argument nobody reads is worthless.
//
// Determinism: byte-only. Synthetic manifests authored here, no packages linked (a core test never
// links the semantic packages — that would invert the dependency arrow), no RNG, no clock, no float.

#include <gtest/gtest.h>

import insight.canon.test; // facade (compose / resolve_stream / transport) + spi (row grammar)

using insight::StructuralRole;
using insight::semantic::compose;
using insight::semantic::ComposedSemantics;
using insight::semantic::kAnyFormat;
using insight::semantic::resolve_stream;
using insight::semantic::ResolvedStream;
using insight::semantic::SemanticPackageManifest;
using insight::semantic::StructuralRoleRow;
using insight::transport::IngestDeclaration;

namespace
{

constexpr std::string_view kGhaTransform{"gha-api-line-prefix"};

// The declared stack's backing array — `IngestDeclaration::stack` is a span, so this must outlive
// every declaration built from it.
constexpr std::array<std::string_view, 1> kDeclaredGha{{kGhaTransform}};

// ── A synthetic package, and the same package with ONE byte changed ────────────────────────────
// Core tests compose SYNTHETIC manifests to exercise the algorithms vocabulary-free (ADR 0024
// §2.4). `kRolesMutated` differs from `kRoles` in exactly one character of one prefix — the
// smallest change that must still move the digest.
constexpr std::array<StructuralRoleRow, 1> kRoles{
    {{.prefix = "<TRANSPORT-A>", .role = StructuralRole::GroupBegin, .format_gate = kAnyFormat}}};
constexpr std::array<StructuralRoleRow, 1> kRolesMutated{
    {{.prefix = "<TRANSPORT-B>", .role = StructuralRole::GroupBegin, .format_gate = kAnyFormat}}};

constexpr SemanticPackageManifest kPackage{
    .name = "transport_identity_fixture", .version = "1.0.0", .roles = kRoles};
constexpr SemanticPackageManifest kPackageMutated{
    .name = "transport_identity_fixture", .version = "1.0.0", .roles = kRolesMutated};

constexpr std::array<SemanticPackageManifest, 1> kPackages{{kPackage}};
constexpr std::array<SemanticPackageManifest, 1> kPackagesMutated{{kPackageMutated}};

// ══════════════════════════════════════════════════════════════════════════════════════════════
// The MUST — ±a declared transform leaves semantic_identity untouched
// ══════════════════════════════════════════════════════════════════════════════════════════════

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
        << "DECLARING A TRANSPORT TRANSFORM MOVED semantic_identity — ADR 0044 §6's MUST is "
           "violated and transport-invariance is not being built.\n"
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
    // The zero-package composition is a defined, runnable state whose identity is the hash of just
    // the version components. The invariance must hold there too — otherwise it would hold only
    // when some row happened to dominate the digest.
    const ComposedSemantics composed{compose({})};
    const auto baseline{composed.identity()};

    const ResolvedStream with{resolve_stream(
        composed, IngestDeclaration{.stack = kDeclaredGha, .dialect = {}, .channel = {}})};

    EXPECT_EQ(with.semantics.identity(), baseline)
        << "±a transform must be an invariance on the degenerate composition too.\n"
        << "  composed : " << composed.identity_hex() << "\n"
        << "  declared : " << with.semantics.identity_hex();
}

// ══════════════════════════════════════════════════════════════════════════════════════════════
// The OPPOSED arms — what stops the invariance above from being vacuous
// ══════════════════════════════════════════════════════════════════════════════════════════════

// If `identity()` were a constant, or the hash ignored row content, every assertion above would
// stay green while the digest guaranteed nothing. This arm demands the OPPOSITE outcome from the
// same machinery: one byte of one row must move it.
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

// The other half of vacuity: `resolve_stream` must actually READ the declaration. An invariance
// over an argument that is dropped on the floor proves nothing at all. An unknown dialect fatals
// (ADR 0044 §6 — canon VERIFIES, never infers), which is observable proof the coordinate is read.
TEST(TransportIdentityDeathTest, DeclarationCoordinatesAreActuallyRead)
{
    const ComposedSemantics composed{compose(kPackages)};

    EXPECT_DEATH(
        {
            (void)resolve_stream(composed, IngestDeclaration{.stack = {},
                                                             .dialect = "no-such-dialect",
                                                             .channel = {}});
        },
        "unknown dialect")
        << "an unknown declared dialect must fatal — this is also what proves resolve_stream reads "
           "the declaration at all, which the invariance arms depend on";

    // The message must name the composed vocabulary, or the fail-closed posture is unactionable.
    EXPECT_DEATH(
        {
            (void)resolve_stream(composed, IngestDeclaration{.stack = {},
                                                             .dialect = "no-such-dialect",
                                                             .channel = {}});
        },
        "transport_identity_fixture");

    // A KNOWN dialect must NOT fatal: an unknown name is a MISTAKE, an absent one is a CHOICE, and
    // a correct one is neither. Without this leg the death arm above would also pass against an
    // implementation that fataled on every dialect.
    const ResolvedStream resolved{resolve_stream(
        composed, IngestDeclaration{.stack = {}, .dialect = "transport_identity_fixture",
                                    .channel = {}})};
    EXPECT_EQ(resolved.semantics.identity(), composed.identity())
        << "declaring a KNOWN dialect must also leave semantic_identity untouched";
}

} // namespace
// NOLINTEND
