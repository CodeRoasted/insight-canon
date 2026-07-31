// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_composition.cpp — the composition CONTRACT (ADR-17), canon's semantic-unaware
// machinery, over SYNTHETIC manifests. Three permanent properties the §10 gates name:
//   • G-SP-5 fail-closed — an exact-duplicate key across packages is a BUILD error (constexpr
//     find_conflict, static_assert'd here) AND a startup FATAL (the runtime compose,
//     EXPECT_DEATH'd).
//   • The degenerate core-only composition (compose({})) is a defined, runnable state: universal
//   formats
//     tokenize, no dialect row fires, the identity is the stable hash of just the version
//     components.
//   • semantic_identity is a reproducible, order-independent, CONTENT hash — the II-7 comparability
//   key
//     that REPLACES the retired kIntentRegistryVersion literal (ADR-17). This is the
//     unit-level G-SP-4 guard; the cross-build / cross-OS leg is Argos's CI (det_public_proof).
// Determinism: byte-only; the hash is truncated-SHA-256 over a fixed-endian canonical
// serialization, order-independent by construction (compose sorts packages by name). No
// RNG/clock/float.
#include <gtest/gtest.h>

import insight.canon.test; // facade (compose / find_conflict / Tokenizer / enums) + spi (row grammar)

using insight::LogFormat;
using insight::recognize_location;
using insight::StructuralRole;
using insight::semantic::compose;
using insight::semantic::ComposedSemantics;
using insight::semantic::find_conflict;
using insight::semantic::IntentMarkerRow;
using insight::semantic::kAnyDialect;
using insight::semantic::SemanticPackageManifest;
using insight::semantic::StructuralRoleRow;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::classify;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::MaskConfig;
using insight::tokenization::recognize;

// The walkers take NormalizedContent (ADR-21's precondition as a type); every probe here is an
// escape-free literal, so normalize() is the zero-copy fixed point over a shared scratch.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}
using insight::tokenization::Tokenizer;

namespace
{
// ── Synthetic packages A and B (distinct names, distinct rows) + A' (A with one prefix mutated) ──
constexpr std::array<StructuralRoleRow, 1> kRolesA{
    {{.prefix = "<AAA>", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect}}};
constexpr std::array<StructuralRoleRow, 1> kRolesAMut{
    {{.prefix = "<AAB>",
      .role = StructuralRole::GroupBegin,
      .dialect_gate = kAnyDialect}}}; // one byte changed
constexpr std::array<StructuralRoleRow, 1> kRolesB{
    {{.prefix = "<BBB>", .role = StructuralRole::GroupEnd, .dialect_gate = kAnyDialect}}};

constexpr SemanticPackageManifest kPkgA{.name = "alpha",
                                        .version = "1.0.0",
                                        .roles = kRolesA,
                                        .markers = {},
                                        .level_lifts = {},
                                        .locations = {},
                                        .value_classes = {},
                                        .strategy = nullptr,
                                        .echoed_source = nullptr};
constexpr SemanticPackageManifest kPkgAMut{.name = "alpha",
                                           .version = "1.0.0",
                                           .roles = kRolesAMut,
                                           .markers = {},
                                           .level_lifts = {},
                                           .locations = {},
                                           .value_classes = {},
                                           .strategy = nullptr,
                                           .echoed_source = nullptr};
constexpr SemanticPackageManifest kPkgB{.name = "beta",
                                        .version = "1.0.0",
                                        .roles = kRolesB,
                                        .markers = {},
                                        .level_lifts = {},
                                        .locations = {},
                                        .value_classes = {},
                                        .strategy = nullptr,
                                        .echoed_source = nullptr};

// ── A deliberate cross-package DUPLICATE: two packages sharing an exact role key with intersecting
// gates (kAnyDialect intersects everything). The G-SP-5 fixture. ──
constexpr std::array<StructuralRoleRow, 1> kDupRoleX{
    {{.prefix = "##DUP##", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect}}};
constexpr std::array<StructuralRoleRow, 1> kDupRoleY{
    {{.prefix = "##DUP##", .role = StructuralRole::Terminator, .dialect_gate = "dup_b"}}};
constexpr SemanticPackageManifest kDupA{.name = "dup_a",
                                        .version = "1.0.0",
                                        .roles = kDupRoleX,
                                        .markers = {},
                                        .level_lifts = {},
                                        .locations = {},
                                        .value_classes = {},
                                        .strategy = nullptr,
                                        .echoed_source = nullptr};
constexpr SemanticPackageManifest kDupB{.name = "dup_b",
                                        .version = "1.0.0",
                                        .roles = kDupRoleY,
                                        .markers = {},
                                        .level_lifts = {},
                                        .locations = {},
                                        .value_classes = {},
                                        .strategy = nullptr,
                                        .echoed_source = nullptr};
constexpr std::array<SemanticPackageManifest, 2> kDupSet{kDupA, kDupB};
} // namespace

// ── G-SP-5, build-time half: find_conflict is constexpr, so a duplicate is caught in a
// static_assert ──
static_assert(
    find_conflict(kDupSet).has_conflict,
    "G-SP-5: a cross-package exact-duplicate role key must be detectable at compile time");
static_assert(find_conflict(kDupSet).kind == "role",
              "the conflict must be reported as a role duplicate");
static_assert(find_conflict(kDupSet).key == "##DUP##", "the conflict must name the duplicated key");
static_assert(!find_conflict(std::array<SemanticPackageManifest, 2>{kPkgA, kPkgB}).has_conflict,
              "distinct-key packages must NOT conflict");

// ── G-SP-5, runtime half: compose() on the duplicate set FAILS CLOSED (fatal) — never a silent
// merge ──
TEST(CompositionDeathTest, DuplicateKeyFailsClosedAtRuntime)
{
    EXPECT_DEATH((void)compose(kDupSet), "exact-duplicate role")
        << "compose() must fatal (fail-closed) on a cross-package duplicate, not silently merge";
}

// ── The degenerate core-only composition is a defined, RUNNABLE state (SRC-II-4 at the composition
// layer) ──
TEST(Composition, DegenerateCoreOnlyRuns)
{
    const ComposedSemantics core{compose({})};
    EXPECT_TRUE(core.roles().empty());
    EXPECT_TRUE(core.markers().empty());
    EXPECT_TRUE(core.locations().empty());

    // No dialect row fires — a would-be GHA marker classifies to None, no marker, no test-file
    // WHERE.
    EXPECT_EQ(classify(norm_probe("##[error]boom"), core), StructuralRole::None);
    EXPECT_EQ(recognize(norm_probe("Run something"), core).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize_location(norm_probe("src/auth/login.test.ts"), core), "");

    // But the universal formats still tokenize (core is runnable semantic-unaware).
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena, MaskConfig{}, core};
    const auto event{tokenizer.process_line(
        R"({"ts":"2024-01-15T10:30:00Z","level":"INFO","component":"auth","message":"hi"})")};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->format, LogFormat::JSON)
        << "a JSON line must still tokenize under a zero-package composition";
}

// ── semantic_identity: reproducible run-to-run, order-independent, and CONTENT-sensitive ──
TEST(Composition, SemanticIdentityIsReproducibleOrderIndependentAndContentSensitive)
{
    // Reproducibility (SP-6 / G-SP-4 unit leg): the same package set → the same identity.
    const std::array oneA{kPkgA};
    EXPECT_EQ(compose(oneA).identity(), compose(oneA).identity())
        << "identity must be a pure function of the package set (got "
        << compose(oneA).identity_hex() << " then a different hash)";

    // Order-independence: compose({A,B}) == compose({B,A}) — canonical order is by name, not
    // argument order.
    const std::array ab{kPkgA, kPkgB};
    const std::array ba{kPkgB, kPkgA};
    EXPECT_EQ(compose(ab).identity(), compose(ba).identity())
        << "identity must not depend on the caller's argument order";

    // Content-sensitivity: mutating ONE row byte changes the identity (content hash, not a version
    // label).
    const std::array a{kPkgA};
    const std::array aMut{kPkgAMut};
    EXPECT_NE(compose(a).identity(), compose(aMut).identity())
        << "a changed rule row MUST change semantic_identity (content, not label)";

    // Adding a package changes the identity (composition is over the whole set).
    EXPECT_NE(compose(a).identity(), compose(ab).identity());

    // The degenerate composition has a stable, non-trivial identity (hash of the version
    // components).
    EXPECT_EQ(compose({}).identity(), compose({}).identity());
    EXPECT_NE(compose({}).identity(), compose(a).identity())
        << "core-only and core+alpha must not collide";
}
// NOLINTEND
