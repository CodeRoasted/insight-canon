// invariant: the composition CONTRACT — canon's semantic-unaware machinery over SYNTHETIC
// manifests, carrying five permanent properties.
// invariant: FAIL-CLOSED — an exact-duplicate key across packages is a BUILD error through the
// constexpr conflict finder AND a startup FATAL through the runtime compose.
// invariant: THE COMPOSED-SET NAME FENCE — two packages under one manifest name conflict on the
// NAME ALONE, with no row of any kind in common.
// invariant: the isolating fixture is what makes that property provable at all.
// invariant: THE DEGENERATE core-only composition is a defined, RUNNABLE state — universal
// formats tokenize, no dialect row fires, and the identity is a stable hash of the versions.
// invariant: THE DECLARATION FENCES are exercised in BOTH directions, because they are consteval
// predicates and a `return true;` body would satisfy every positive arm ever written.
// invariant: only the NEGATIVE arms measure them, plus the empty-name fence's RUNTIME half, which
// is the grain a consteval predicate cannot reach.
// invariant: a package set assembled at runtime never meets a compile-time check.
// invariant: SEMANTIC IDENTITY is a reproducible, order-independent CONTENT hash — the
// comparability key that REPLACES a retired version literal.
// invariant: that makes the composed ruleset part of the comparison's identity; this is the
// unit-level leg, and the cross-build and cross-OS leg belongs to CI.
// invariant: determinism — byte-only, a truncated digest over a fixed-endian canonical
// serialization, order-independent by construction, with no RNG, clock or float.
#include <gtest/gtest.h>

import insight.canon.test;

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

// invariant: the walkers take normalized content as a TYPE, which is the stage-1 precondition
// expressed in the signature.
// invariant: every probe here is an escape-free literal, so normalization is the zero-copy fixed
// point over a shared scratch.
[[nodiscard]] static insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}
using insight::tokenization::Tokenizer;

namespace
{
constexpr std::array<StructuralRoleRow, 1> kRolesA{
    {{.prefix = "<AAA>", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect}}};
constexpr std::array<StructuralRoleRow, 1> kRolesAMut{
    {{.prefix = "<AAB>", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect}}};
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

// invariant: a deliberate cross-package DUPLICATE — two packages sharing an exact role key under
// gates that intersect, since the any-dialect gate intersects everything.
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

// invariant: THE FIXTURE IS THE WHOLE POINT — the obvious mutation proves NOTHING, because
// composing a package with itself duplicates its ROWS too.
// invariant: the row-keyed checks already catch that, so a green would not say which check fired.
// invariant: the isolating probe is a SHADOW manifest carrying not one row of any kind, so its rows
// are disjoint from every other package's by construction.
// invariant: every row-keyed check is then silent and only the NAME check can speak.
// invariant: that is not hypothetical — before the name check existed this exact set composed
// SILENTLY, serving both packages' gated rows flattened into one view.
// invariant: the package list showed the name twice and no diagnostic anywhere named the collision.
// invariant: it is reachable the moment a package is hand-written, which is the supported extension
// path.
constexpr SemanticPackageManifest kShadowOfA{.name = "alpha",
                                             .version = "9.9.9",
                                             .roles = {},
                                             .markers = {},
                                             .level_lifts = {},
                                             .locations = {},
                                             .value_classes = {},
                                             .strategy = nullptr,
                                             .echoed_source = nullptr};
constexpr std::array<SemanticPackageManifest, 2> kShadowSet{kPkgA, kShadowOfA};

// invariant: THE NEGATIVE CONTROL, and it is what makes the arm above a statement about the NAME.
// invariant: byte-for-byte the same empty manifest under a name of its own — if THIS conflicted,
// the arm above would be measuring a second package or an empty one, not a name collision.
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

// invariant: a set carrying BOTH failures at once — a name collision AND an exact role-key
// duplicate.
// invariant: the reported kind pins the check ORDER, which is CONTENT and not an implementation
// detail.
// invariant: with two packages under one name a reported ROW duplicate cannot say which package it
// came from, so the name must be answered first or the answer is ambiguous.
constexpr std::array<SemanticPackageManifest, 2> kNameAndRowDupSet{kDupA, kDupA};

// invariant: the any-dialect gate IS the empty string, so a manifest named with the empty string
// makes the gate-ownership check succeed VACUOUSLY for every ungated row.
// invariant: that package's rows then read as universally gated to every downstream reader.
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

// invariant: the dialect-REVISION vocabulary fence has three ways to fail, and each needs its own
// set because the predicate returns on the first one it meets.
// invariant: an EMPTY vocabulary is the undeclared state the coordinate exists to remove; an empty
// NAME inside it.
// invariant: a REPEATED name is the third, and its only symptom would be duplicate bytes in the
// identity preimage.
// refs: ADR-17.D9
constexpr std::array<std::string_view, 1> kRevisionsOne{{"v1"}};
constexpr std::array<std::string_view, 2> kRevisionsTwo{{"v1", "v2"}};
constexpr std::array<std::string_view, 1> kRevisionUnnamed{{""}};
constexpr std::array<std::string_view, 2> kRevisionsRepeated{{"v1", "v1"}};
} // namespace

// invariant: the BUILD-TIME half — the conflict finder is constexpr, so a duplicate is caught in
// a static assertion.
static_assert(
    find_conflict(kDupSet).has_conflict,
    "G-SP-5: a cross-package exact-duplicate role key must be detectable at compile time");
static_assert(find_conflict(kDupSet).kind == "role",
              "the conflict must be reported as a role duplicate");
static_assert(find_conflict(kDupSet).key == "##DUP##", "the conflict must name the duplicated key");
static_assert(!find_conflict(std::array<SemanticPackageManifest, 2>{kPkgA, kPkgB}).has_conflict,
              "distinct-key packages must NOT conflict");

// invariant: WHAT ISOLATES THE NAME CHECK HERE IS THE FIXTURE, NOT THE REPORTED KIND.
// invariant: the conflict finder answers the package name FIRST, so that kind is what ANY
// name-colliding set reports, one carrying a live row duplicate included.
// invariant: reading the kind as proof that nothing row-keyed fired is reading the check ORDER and
// calling it a fixture property.
// invariant: the isolation comes from the shadow manifest carrying no row of any kind, together
// with the negative control that puts that same row-less manifest under a name of its own.
// invariant: between those two sets only the NAME differs, so only the name can explain the
// difference, and on this fixture the conflict flag alone already proves the name check answered.
// invariant: the kind is still asserted for a DIFFERENT reason — it is the operator-facing
// vocabulary the runtime branches its REMEDY on, so its spelling is contract, not diagnostics.
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

// invariant: the red arm is expressed as a NEGATION rather than as an observed build failure,
// because a build failure cannot be asserted by a translation unit that must itself build.
static_assert(insight::semantic::all_packages_named(kNamedSet),
              "two distinctly named packages must satisfy the manifest-name fence");
static_assert(!insight::semantic::all_packages_named(kUnnamedSet),
              "an empty manifest name must FAIL the fence — the empty string IS kAnyDialect, so "
              "such a package's rows would read as universally gated to every downstream reader");

// invariant: the POSITIVE arm alone is worthless here — the predicate is consteval and returns a
// bool, so a body of `return true;` passes every positive arm that will ever be written.
// invariant: the three NEGATIVE arms are what measure the predicate, one per rejection reason.
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

// invariant: the runtime death test's NON-VACUITY, proved at COMPILE TIME — this set carries no
// duplicate of any kind, so nothing but the empty-name fence can make composition fatal on it.
// invariant: delete that fence and the death test below goes RED rather than being caught by a
// neighbour.
static_assert(!find_conflict(kUnnamedSet).has_conflict,
              "kUnnamedSet must be conflict-free — otherwise the empty-name death test would be "
              "measuring find_conflict, not the name fence");

TEST(CompositionDeathTest, DuplicateKeyFailsClosedAtRuntime)
{
    EXPECT_DEATH((void)compose(kDupSet), "exact-duplicate role")
        << "compose() must fatal (fail-closed) on a cross-package duplicate, not silently merge";
}

// invariant: the name fence's RUNTIME half — the grain the row duplicate already had and the name
// did not.
// invariant: the build-time half proves the constant evaluator answers, and says nothing about a
// set assembled at runtime, which is the shape a hand-written package arrives in.
// invariant: the pattern pins the conflict KIND and the key and deliberately NOT the surrounding
// prose.
// invariant: the message's closing advice cannot be followed for a name collision — there are no
// rows to fix and no gate that resolves it — so pinning it would make wrong advice a contract.
TEST(CompositionDeathTest, DuplicatePackageNameFailsClosedAtRuntime)
{
    EXPECT_DEATH((void)compose(kShadowSet), R"(exact-duplicate package_name match key "alpha")")
        << "compose() must fatal on two packages sharing a manifest name even when they share no "
           "row of any kind — the silent alternative is one flattened view serving both packages' "
           "rows under one name";
}

// invariant: the empty-name fence's RUNTIME half, and it is the half that was MISSING.
// invariant: the compile-time predicate reaches a package set written down in a translation unit
// and nothing else, while composition accepts a span assembled at RUNTIME.
// invariant: before this branch existed a runtime set carrying a package named with the empty
// string composed successfully.
// invariant: every ungated row of that package then read as universally gated, which is the exact
// reading the fence exists to prevent.
TEST(CompositionDeathTest, UnnamedPackageFailsClosedAtRuntime)
{
    EXPECT_DEATH((void)compose(kUnnamedSet),
                 "package at position 1 of 2 declares an EMPTY manifest name")
        << "compose() must fatal on a runtime-assembled set carrying a package named \"\" — the "
           "silent alternative is that package's rows reading as universally gated downstream";
}

// invariant: the degenerate core-only composition is a defined, RUNNABLE state at the composition
// layer.
// refs: SRC-II-4
TEST(Composition, DegenerateCoreOnlyRuns)
{
    const ComposedSemantics core{compose({})};
    EXPECT_TRUE(core.roles().empty());
    EXPECT_TRUE(core.markers().empty());
    EXPECT_TRUE(core.locations().empty());

    // invariant: no dialect row fires — a would-be marker classifies to nothing, with no marker
    // and no test-file WHERE.
    EXPECT_EQ(classify(norm_probe("##[error]boom"), core), StructuralRole::None);
    EXPECT_EQ(recognize(norm_probe("Run something"), core).kind, IntentMarkerKind::None);
    EXPECT_EQ(recognize_location(norm_probe("src/auth/login.test.ts"), core), "");

    // invariant: the universal formats still tokenize, which is what makes the core runnable while
    // semantic-unaware.
    ArenaAllocator arena{64U * 1024U};
    Tokenizer tokenizer{arena, MaskConfig{}, core};
    const auto event{tokenizer.process_line(
        R"({"ts":"2024-01-15T10:30:00Z","level":"INFO","component":"auth","message":"hi"})")};
    ASSERT_TRUE(event.has_value()) << event.error();
    EXPECT_EQ(event->format, LogFormat::JSON)
        << "a JSON line must still tokenize under a zero-package composition";
}

TEST(Composition, SemanticIdentityIsReproducibleOrderIndependentAndContentSensitive)
{
    // invariant: REPRODUCIBILITY — the same package set yields the same identity.
    const std::array oneA{kPkgA};
    EXPECT_EQ(compose(oneA).identity(), compose(oneA).identity())
        << "identity must be a pure function of the package set (got "
        << compose(oneA).identity_hex() << " then a different hash)";

    // invariant: ORDER-INDEPENDENCE — the canonical order is by NAME, not by argument order.
    const std::array ab{kPkgA, kPkgB};
    const std::array ba{kPkgB, kPkgA};
    EXPECT_EQ(compose(ab).identity(), compose(ba).identity())
        << "identity must not depend on the caller's argument order";

    // invariant: CONTENT-SENSITIVITY — mutating ONE row byte changes the identity, because it is
    // a content hash and not a version label.
    const std::array a{kPkgA};
    const std::array aMut{kPkgAMut};
    EXPECT_NE(compose(a).identity(), compose(aMut).identity())
        << "a changed rule row MUST change semantic_identity (content, not label)";

    EXPECT_NE(compose(a).identity(), compose(ab).identity());

    // invariant: the degenerate composition has a stable, non-trivial identity — the hash of just
    // the version components.
    EXPECT_EQ(compose({}).identity(), compose({}).identity());
    EXPECT_NE(compose({}).identity(), compose(a).identity())
        << "core-only and core+alpha must not collide";
}
