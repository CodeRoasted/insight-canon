// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_composition.cpp — the composition CONTRACT, canon's semantic-unaware
// machinery, over SYNTHETIC manifests. Five permanent properties:
//   • G-SP-5 fail-closed — an exact-duplicate key across packages is a BUILD error (constexpr
//     find_conflict, static_assert'd here) AND a startup FATAL (the runtime compose,
//     EXPECT_DEATH'd).
//   • The composed-set NAME fence — two packages under one manifest name conflict on the name
//     ALONE, with no row of any kind in common. Same two halves as G-SP-5, and the isolating
//     fixture is what makes the property provable at all (see the kShadow* block).
//   • The degenerate core-only composition (compose({})) is a defined, runnable state: universal
//     formats tokenize, no dialect row fires, the identity is the stable hash of just the version
//     components.
//   • The DECLARATION fences, exercised in BOTH directions — all_packages_named and
//     all_revisions_named. They are consteval predicates, so a `return true;` body would satisfy
//     every positive arm ever written and red nothing; only the negative arms measure them. Plus
//     the empty-name fence's RUNTIME half (EXPECT_DEATH), the grain a consteval predicate cannot
//     reach: a package set assembled at runtime never meets a compile-time check.
//   • semantic_identity is a reproducible, order-independent, CONTENT hash — the comparability key
//     that REPLACES the retired kIntentRegistryVersion literal, making the composed ruleset part
//     of the comparison's identity. This is the unit-level G-SP-4 guard; the cross-build /
//     cross-OS leg is Argos's CI (det_public_proof).
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

// The walkers take NormalizedContent (the stage-1 normalization precondition as a type); every
// probe here is an escape-free literal, so normalize() is the zero-copy fixed point over a shared
// scratch.
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

// ── The composed-set NAME fence, and the fixture is the whole point ──
// The obvious mutation proves NOTHING here: composing a package with itself duplicates its ROWS
// too, and the row-keyed checks above already catch that — a green would not say which check
// fired. The isolating probe is a SHADOW manifest that carries not one row of any kind: no roles,
// no markers, no level lifts, no outcome tokens, no value classes. Its rows are disjoint from
// every other package's by construction (the empty set intersects nothing), so every row-keyed
// check is silent and only the name check can speak.
//
// That is not a hypothetical: before the name check existed, this exact set composed SILENTLY —
// `for_stream("alpha")` served BOTH packages' gated rows flattened into one view, `packages()`
// listed "alpha" twice, and no diagnostic anywhere named the collision. It is reachable the moment
// a package is hand-written, which is the supported extension path.
constexpr SemanticPackageManifest kShadowOfA{.name = "alpha", // the SAME name as kPkgA
                                             .version = "9.9.9",
                                             .roles = {},
                                             .markers = {},
                                             .level_lifts = {},
                                             .locations = {},
                                             .value_classes = {},
                                             .strategy = nullptr,
                                             .echoed_source = nullptr};
constexpr std::array<SemanticPackageManifest, 2> kShadowSet{kPkgA, kShadowOfA};

// The negative control, and it is what makes the arm above a statement about the NAME. Byte-for
// byte the same empty manifest under a name of its own: if THIS conflicted, the arm above would be
// measuring "a second package" or "an empty package", not a name collision.
constexpr SemanticPackageManifest kNamedShadow{.name = "shadow",
                                               .version = "9.9.9",
                                               .roles = {},
                                               .markers = {},
                                               .level_lifts = {},
                                               .locations = {},
                                               .value_classes = {},
                                               .strategy = nullptr,
                                               .echoed_source = nullptr};
constexpr std::array<SemanticPackageManifest, 2> kNamedShadowSet{kPkgA, kNamedShadow};

// A set carrying BOTH failures at once: kDupA against itself is a name collision AND an exact
// role-key duplicate. The reported kind pins the check ORDER, which is content and not an
// implementation detail — with two packages under one name, a reported ROW duplicate cannot say
// which package it came from, so the name must be answered first or the answer is ambiguous.
constexpr std::array<SemanticPackageManifest, 2> kNameAndRowDupSet{kDupA, kDupA};

// The empty-name fence's two arms. `kAnyDialect` IS the empty string, so a manifest named "" makes
// `all_dialect_gates_owned`'s `gate == manifest.name` succeed VACUOUSLY for every ungated row, and
// the package's rows then read as universally gated to every downstream reader.
constexpr SemanticPackageManifest kUnnamedPkg{.name = "",
                                              .version = "1.0.0",
                                              .roles = kRolesB,
                                              .markers = {},
                                              .level_lifts = {},
                                              .locations = {},
                                              .value_classes = {},
                                              .strategy = nullptr,
                                              .echoed_source = nullptr};
constexpr std::array<SemanticPackageManifest, 2> kNamedSet{kPkgA, kPkgB};
constexpr std::array<SemanticPackageManifest, 2> kUnnamedSet{kPkgA, kUnnamedPkg};

// ── The dialect-REVISION vocabulary fence's fixtures (grammar-6, ADR-17.D9). Three ways to fail,
// and each needs its own set because the predicate returns on the first one it meets: an EMPTY
// vocabulary (a package that declares no vendor generation at all — the undeclared state the
// coordinate exists to remove), an empty NAME inside it, and a REPEATED name (a copy-paste whose
// only symptom would be duplicate bytes in the identity preimage). ──
constexpr std::array<std::string_view, 1> kRevisionsOne{{"v1"}};
constexpr std::array<std::string_view, 2> kRevisionsTwo{{"v1", "v2"}};
constexpr std::array<std::string_view, 1> kRevisionUnnamed{{""}};
constexpr std::array<std::string_view, 2> kRevisionsRepeated{{"v1", "v1"}};
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

// ── The composed-set NAME fence, build-time half. What isolates the name check here is the
// FIXTURE, not the reported kind. `find_conflict` answers the package name FIRST (its definition
// in canon.compose.cppm), so `.kind == "package_name"` is what ANY name-colliding set reports, one
// carrying a live row duplicate included — the kNameAndRowDupSet arm below is that set and reports
// "package_name" over a real role duplicate. Reading `.kind` as proof that nothing row-keyed fired
// is therefore reading the check ORDER and calling it a fixture property.
//
// The isolation comes from kShadowOfA carrying no row of any kind — no row-keyed check has anything
// to compare — together with kNamedShadowSet, the negative control that puts that same row-less
// manifest under a name of its own and does NOT conflict. Between those two sets only the name
// differs, so only the name can explain the difference; and on this fixture `has_conflict` alone
// already proves the name check answered, because nothing else could have.
//
// `.kind` is still asserted, for a different reason: it is the operator-facing vocabulary the
// runtime fail_closed branches its REMEDY on (kConflictKindPackageName), so its spelling is
// contract rather than diagnostics. ──
static_assert(find_conflict(kShadowSet).has_conflict,
              "two packages under one manifest name must be detectable at compile time, with no "
              "row of any kind in common");
static_assert(find_conflict(kShadowSet).kind == "package_name",
              "the conflict must be reported as a PACKAGE-NAME duplicate — a row-keyed kind here "
              "would mean the shadow was caught incidentally and an all-empty shadow would still "
              "compose silently");
static_assert(find_conflict(kShadowSet).key == "alpha",
              "the conflict must name the duplicated package name");
static_assert(!find_conflict(kNamedShadowSet).has_conflict,
              "the negative control: the SAME row-less manifest under a name of its own must NOT "
              "conflict — otherwise the arm above measures 'a second package', not a name clash");
static_assert(find_conflict(kNameAndRowDupSet).kind == "package_name",
              "the package name must be answered FIRST: with two packages under one name, a "
              "reported row duplicate cannot say which package it came from");

// ── The empty-name fence, both arms. consteval, so the red arm is expressed as a NEGATION here
// rather than as an observed build failure — a build failure cannot be asserted by a TU that must
// itself build. ──
static_assert(insight::semantic::all_packages_named(kNamedSet),
              "two distinctly named packages must satisfy the manifest-name fence");
static_assert(!insight::semantic::all_packages_named(kUnnamedSet),
              "an empty manifest name must FAIL the fence — the empty string IS kAnyDialect, so "
              "such a package's rows would read as universally gated to every downstream reader");

// ── The dialect-REVISION vocabulary fence, all four arms. The positive arm alone is worthless
// here: `all_revisions_named` is consteval and returns bool, so a body of `return true;` passes
// every positive arm that will ever be written and reds nothing. The three negative arms are what
// measure the predicate, one per rejection reason. ──
static_assert(insight::semantic::all_revisions_named(kRevisionsOne),
              "a single non-empty revision name must satisfy the fence");
static_assert(insight::semantic::all_revisions_named(kRevisionsTwo),
              "two distinct non-empty revision names must satisfy the fence — the cardinality-one "
              "bound is a SCHEMA rule enforced by the declaration tool, never by core");
static_assert(!insight::semantic::all_revisions_named(std::span<const std::string_view>{}),
              "an EMPTY revision vocabulary must FAIL — a package that names no vendor generation "
              "is the undeclared state this coordinate exists to remove, not a degenerate case");
static_assert(!insight::semantic::all_revisions_named(kRevisionUnnamed),
              "an empty revision NAME must FAIL — it names no vendor generation while occupying a "
              "slot, and it enters the identity preimage as zero bytes");
static_assert(!insight::semantic::all_revisions_named(kRevisionsRepeated),
              "a REPEATED revision name must FAIL — a copy-paste whose only other symptom would be "
              "duplicate bytes in the identity preimage");

// The runtime empty-name death test's NON-VACUITY, proved at compile time: kUnnamedSet carries no
// duplicate of any kind, so nothing but the empty-name fence can make compose() fatal on it. Delete
// that fence and the EXPECT_DEATH below goes red rather than being caught by a neighbour.
static_assert(!find_conflict(kUnnamedSet).has_conflict,
              "kUnnamedSet must be conflict-free — otherwise the empty-name death test would be "
              "measuring find_conflict, not the name fence");

// ── G-SP-5, runtime half: compose() on the duplicate set FAILS CLOSED (fatal) — never a silent
// merge ──
TEST(CompositionDeathTest, DuplicateKeyFailsClosedAtRuntime)
{
    EXPECT_DEATH((void)compose(kDupSet), "exact-duplicate role")
        << "compose() must fatal (fail-closed) on a cross-package duplicate, not silently merge";
}

// ── The name fence's RUNTIME half — the grain the row duplicate already had and the name did not.
// The build-time half above proves the constant evaluator answers; it says nothing about a set
// assembled at runtime, which is the shape a hand-written or externally supplied package arrives
// in. The regex pins the CONFLICT KIND and the key, deliberately not the surrounding prose: the
// operator message's closing advice ("fix the package rows or gate them") cannot be followed for a
// name collision — there are no rows to fix and no gate that resolves it — so pinning it would
// make wrong advice a contract. ──
TEST(CompositionDeathTest, DuplicatePackageNameFailsClosedAtRuntime)
{
    EXPECT_DEATH((void)compose(kShadowSet), R"(exact-duplicate package_name match key "alpha")")
        << "compose() must fatal on two packages sharing a manifest name even when they share no "
           "row of any kind — the silent alternative is one flattened view serving both packages' "
           "rows under one name";
}

// ── The empty-name fence's RUNTIME half, and it is the half that was missing. `all_packages_named`
// is consteval: it reaches a package set written down in a translation unit and nothing else, while
// compose() accepts a span assembled at runtime — the hand-written / externally supplied package
// path this kit supports. Before this branch existed a runtime set carrying a package named ""
// composed successfully, and every ungated row of that package then read as universally gated
// (kAnyDialect IS the empty string), which is the exact reading the fence exists to prevent.
// The regex pins the failure CLASS and the offending position, not the surrounding advice. ──
TEST(CompositionDeathTest, UnnamedPackageFailsClosedAtRuntime)
{
    EXPECT_DEATH((void)compose(kUnnamedSet),
                 "package at position 1 of 2 declares an EMPTY manifest name")
        << "compose() must fatal on a runtime-assembled set carrying a package named \"\" — the "
           "silent alternative is that package's rows reading as universally gated downstream";
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
