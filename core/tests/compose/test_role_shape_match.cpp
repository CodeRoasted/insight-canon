// invariant: the SHAPE role-row kind over SYNTHETIC rows, so the mechanism is vocabulary-free:
// its matcher, its precedence, its fence, its place in the identity and the duplicate check.
// invariant: the declared shapes are proven in the github and package_managers suites.
// refs: DN-134.D9, ADR-17.D4
#include <gtest/gtest.h>

import insight.canon.test;

using insight::StructuralRole;
using insight::semantic::compose;
using insight::semantic::ComposedSemantics;
using insight::semantic::find_conflict;
using insight::semantic::kAnyDialect;
using insight::semantic::role_row_well_formed;
using insight::semantic::RoleMatchKind;
using insight::semantic::SemanticPackageManifest;
using insight::semantic::shape_matches;
using insight::semantic::StructuralRoleRow;
using insight::tokenization::classify;

namespace
{
[[nodiscard]] insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

constexpr std::string_view kTransferShape{"Got {n} of {n} ({n}%), {n} u/s"};
constexpr std::string_view kCounterShape{"tick: done {n}, left {n}"};

// invariant: a shape row, a prefix row over the shape's own opening bytes, and a longer prefix row
// a sample line also starts with, so the precedence of the whole-line claim is distinguishable.
constexpr std::array<StructuralRoleRow, 4> kRows{{
    {.prefix = kTransferShape,
     .role = StructuralRole::Progress,
     .dialect_gate = kAnyDialect,
     .match = RoleMatchKind::Shape},
    {.prefix = "Got ", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect},
    {.prefix = "Got 1 of 2 (50%), 3 u",
     .role = StructuralRole::GroupEnd,
     .dialect_gate = kAnyDialect},
    {.prefix = kCounterShape,
     .role = StructuralRole::Progress,
     .dialect_gate = "synth_shape",
     .match = RoleMatchKind::Shape},
}};

constexpr SemanticPackageManifest kManifest{
    .name = "synth_shape",
    .version = "1.0.0",
    .roles = kRows,
};

[[nodiscard]] ComposedSemantics view_of(std::string_view dialect)
{
    const std::array manifests{kManifest};
    return compose(manifests).for_stream(dialect, {});
}
} // namespace

// invariant: each hole is exactly one decimal number, integer or with a fraction, and nothing else
// of the line is variable: the literal bytes, the count of holes and the line's end are exact.
TEST(RoleShapeMatch, AHoleIsOneDecimalNumberAndTheRestIsExact)
{
    EXPECT_TRUE(shape_matches(kTransferShape, "Got 75497472 of 157286400 (48.0%), 72.0 u/s"));
    EXPECT_TRUE(shape_matches(kTransferShape, "Got 0 of 0 (100%), 0 u/s"));
    EXPECT_TRUE(shape_matches(kTransferShape, "Got 1 of 2 (50%), 3 u/s \t\r"))
        << "the trailing whitespace is trimmed before matching";
    EXPECT_FALSE(shape_matches(kTransferShape, " Got 1 of 2 (50%), 3 u/s"))
        << "leading bytes are content: the shape is anchored at the start";
    EXPECT_FALSE(shape_matches(kTransferShape, "Got 200 OK"));
    EXPECT_FALSE(shape_matches(kTransferShape, "Got 3 of 5 files"));
    EXPECT_FALSE(shape_matches(kTransferShape, "Got x of 2 (50%), 3 u/s"))
        << "a hole needs a digit";
    EXPECT_FALSE(shape_matches(kTransferShape, "Got 1 of 2 (50%), 3 u/s and more"))
        << "the shape claims the WHOLE content";
    EXPECT_FALSE(shape_matches(kTransferShape, "Got 1.5.2 of 2 (50%), 3 u/s"))
        << "a hole takes one fraction, never a dotted version";
    EXPECT_FALSE(shape_matches(kTransferShape, "Got .5 of 2 (50%), 3 u/s"));
    EXPECT_FALSE(shape_matches(kTransferShape, ""));
    EXPECT_TRUE(shape_matches("{n} left", "12 left")) << "a shape may open on a hole";
    EXPECT_TRUE(shape_matches("left {n}", "left 12.5")) << "a shape may close on a hole";
    EXPECT_FALSE(shape_matches("left {n}", "left 12."));
}

// invariant: a shape match is a claim on the whole content and outranks every prefix row, the
// longer prefix included, while a line the shape does not match keeps the longest prefix.
TEST(RoleShapeMatch, AShapeOutranksEveryPrefixRow)
{
    const ComposedSemantics any{view_of(kAnyDialect)};
    EXPECT_EQ(insight::to_string(classify(norm_probe("Got 1 of 2 (50%), 3 u/s"), any)),
              std::string_view{"Progress"});
    EXPECT_EQ(insight::to_string(classify(norm_probe("Got 1 of 2 (50%), 3 u/s, then more"), any)),
              std::string_view{"GroupEnd"})
        << "not the whole shape: the longest prefix row decides";
    EXPECT_EQ(insight::to_string(classify(norm_probe("Got 200 OK"), any)),
              std::string_view{"GroupBegin"});
}

// invariant: a shape row is gated like any role row: it fires only on a stream its gate admits.
TEST(RoleShapeMatch, AShapeRowObeysItsDialectGate)
{
    EXPECT_EQ(
        insight::to_string(classify(norm_probe("tick: done 3, left 4"), view_of("synth_shape"))),
        std::string_view{"Progress"});
    EXPECT_EQ(
        insight::to_string(classify(norm_probe("tick: done 3, left 4"), view_of(kAnyDialect))),
        std::string_view{"None"});
    EXPECT_EQ(insight::to_string(
                  classify(norm_probe("tick: done 3, left 4, done"), view_of("synth_shape"))),
              std::string_view{"None"})
        << "a summary line sharing the samples' prefix is content";
}

// invariant: the fence a hand-written package meets at composition, the same refusals the dialect
// codegen makes: every shape below would let a hole's extent decide the match, or names no sample.
TEST(RoleShapeMatch, TheFenceRefusesAnAmbiguousShapeAndAPrefixProgressRow)
{
    const auto shape_row{[](std::string_view shape)
                         {
                             return StructuralRoleRow{.prefix = shape,
                                                      .role = StructuralRole::Progress,
                                                      .dialect_gate = kAnyDialect,
                                                      .match = RoleMatchKind::Shape};
                         }};
    EXPECT_TRUE(role_row_well_formed(shape_row(kTransferShape)));
    EXPECT_TRUE(role_row_well_formed(shape_row("{n} left")));
    EXPECT_TRUE(role_row_well_formed(shape_row("left {n}")));
    EXPECT_TRUE(role_row_well_formed(shape_row("{n}.x")));
    for (const std::string_view refused :
         {"no hole", "a {x} b {n}", "a {n} }", "a {n}{n} b", "a {n}0 b", "a 1{n} b", "a {n}.5 b",
          "a {n}.{n} b", "a 1.{n} b", "a {n}.", "a {n} b ", ""})
        EXPECT_FALSE(role_row_well_formed(shape_row(refused))) << "accepted: \"" << refused << '"';
    EXPECT_FALSE(role_row_well_formed(StructuralRoleRow{
        .prefix = "Got ", .role = StructuralRole::Progress, .dialect_gate = kAnyDialect}))
        << "a Progress row matching a prefix would take the content lines sharing it";
    EXPECT_TRUE(role_row_well_formed(StructuralRoleRow{
        .prefix = "Got ", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect}));
}

// invariant: the match kind is recognition content: it enters the composed identity, and a shape
// and a prefix over the same bytes are two rules, never a duplicate.
TEST(RoleShapeMatch, TheMatchKindIsIdentityAndKeysTheDuplicateCheck)
{
    constexpr std::array<StructuralRoleRow, 1> kAsShape{{{.prefix = "a {n} b",
                                                          .role = StructuralRole::GroupBegin,
                                                          .dialect_gate = kAnyDialect,
                                                          .match = RoleMatchKind::Shape}}};
    constexpr std::array<StructuralRoleRow, 1> kAsPrefix{
        {{.prefix = "a {n} b", .role = StructuralRole::GroupBegin, .dialect_gate = kAnyDialect}}};
    const std::array shape_set{
        SemanticPackageManifest{.name = "p", .version = "1", .roles = kAsShape}};
    const std::array prefix_set{
        SemanticPackageManifest{.name = "p", .version = "1", .roles = kAsPrefix}};
    EXPECT_NE(compose(shape_set).identity_hex(), compose(prefix_set).identity_hex());

    const std::array both{SemanticPackageManifest{.name = "p", .version = "1", .roles = kAsShape},
                          SemanticPackageManifest{.name = "q", .version = "1", .roles = kAsPrefix}};
    EXPECT_FALSE(find_conflict(both).has_conflict);
    const std::array twice{SemanticPackageManifest{.name = "p", .version = "1", .roles = kAsShape},
                           SemanticPackageManifest{.name = "q", .version = "1", .roles = kAsShape}};
    EXPECT_TRUE(find_conflict(twice).has_conflict);
}
