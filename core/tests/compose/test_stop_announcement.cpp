// invariant: the STOP-ANNOUNCEMENT section over SYNTHETIC rows, so the mechanism is free of words.
// invariant: its matcher, its gate, its fence and its place in the identity are proven here.
// invariant: the declared GitHub wordings are proven in the github suite.
// invariant: determinism — literal rows and lines, no clock, no RNG.
// refs: DN-146.D10, ADR-17.D4, ADR-22.D6
#include <gtest/gtest.h>

import insight.canon.test;

using insight::StructuralRole;
using insight::semantic::compose;
using insight::semantic::ComposedSemantics;
using insight::semantic::kAnyDialect;
using insight::semantic::SemanticPackageManifest;
using insight::semantic::stop_announcement_row_well_formed;
using insight::semantic::StopAnnouncementRow;
using insight::semantic::StructuralRoleRow;
using insight::tokenization::announces_stop;
using insight::tokenization::classify;

namespace
{
[[nodiscard]] insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

constexpr std::string_view kDialect{"synth_stop"};

// invariant: a fixed stop line gated to the package, and a stop line with a hole for any dialect.
// invariant: a Terminator role row whose prefix the fixed stop line opens with.
constexpr std::array<StopAnnouncementRow, 2> kStops{{
    {.shape = "!!stop: the step was halted.", .dialect_gate = kDialect},
    {.shape = "!!stop: halted after {n} s", .dialect_gate = kAnyDialect},
}};
constexpr std::array<StructuralRoleRow, 1> kRoles{
    {{.prefix = "!!stop:", .role = StructuralRole::Terminator, .dialect_gate = kAnyDialect}}};

constexpr SemanticPackageManifest kManifest{
    .name = kDialect, .version = "1.0.0", .roles = kRoles, .stop_announcements = kStops};

[[nodiscard]] ComposedSemantics view_of(std::string_view dialect)
{
    const std::array manifests{kManifest};
    return compose(manifests).for_stream(dialect, {});
}
} // namespace

TEST(StopAnnouncement, AWholeLineMatchesAndAnyOtherByteDoesNot)
{
    const ComposedSemantics view{view_of(kDialect)};
    EXPECT_TRUE(announces_stop(norm_probe("!!stop: the step was halted."), view));
    EXPECT_TRUE(announces_stop(norm_probe("!!stop: the step was halted. \r"), view))
        << "trailing whitespace is trimmed before the match";
    EXPECT_TRUE(announces_stop(norm_probe("!!stop: halted after 12 s"), view));
    EXPECT_TRUE(announces_stop(norm_probe("!!stop: halted after 1.5 s"), view));
    for (const std::string_view other :
         {"!!stop: the step was halted.x", "!!stop: the step was halted",
          "x!!stop: the step was halted.", "!!stop: halted after s",
          "!!stop: halted after 12 s later", "!!stop:"})
        EXPECT_FALSE(announces_stop(norm_probe(other), view)) << "matched: \"" << other << '"';
}

TEST(StopAnnouncement, ARowObeysItsDialectGate)
{
    const ComposedSemantics own{view_of(kDialect)};
    const ComposedSemantics undeclared{view_of(kAnyDialect)};
    EXPECT_TRUE(announces_stop(norm_probe("!!stop: the step was halted."), own));
    EXPECT_FALSE(announces_stop(norm_probe("!!stop: the step was halted."), undeclared))
        << "a row gated to its package fired on an undeclared stream";
    EXPECT_TRUE(announces_stop(norm_probe("!!stop: halted after 3 s"), undeclared))
        << "a row gated to any dialect must fire on an undeclared stream";
    EXPECT_EQ(own.stop_announcements().size(), 2U);
    EXPECT_EQ(undeclared.stop_announcements().size(), 1U);
}

TEST(StopAnnouncement, AStopKeepsTheRoleItsLineHolds)
{
    const ComposedSemantics view{view_of(kDialect)};
    EXPECT_EQ(classify(norm_probe("!!stop: the step was halted."), view),
              StructuralRole::Terminator)
        << "declaring a line a stop must leave its role row's reading unchanged";
}

TEST(StopAnnouncement, TheFenceAdmitsAFixedLineAndRefusesAnAmbiguousShape)
{
    const auto row{[](std::string_view shape)
                   { return StopAnnouncementRow{.shape = shape, .dialect_gate = kAnyDialect}; }};
    for (const std::string_view admitted :
         {"no hole at all.", "{n} left", "left {n}", "a {n} b", "{n}.x", "(see: https://x/y#z)"})
        EXPECT_TRUE(stop_announcement_row_well_formed(row(admitted)))
            << "refused: \"" << admitted << '"';
    for (const std::string_view refused :
         {"a {x} b", "a {n} }", "a { b", "a {n}{n} b", "a {n}0 b", "a 1{n} b", "a {n}.5 b",
          "a {n}.{n} b", "a 1.{n} b", "a {n}.", "a b ", "a b\r", ""})
        EXPECT_FALSE(stop_announcement_row_well_formed(row(refused)))
            << "accepted: \"" << refused << '"';
}

TEST(StopAnnouncementDeathTest, AMalformedRowFailsClosedAtComposition)
{
    constexpr std::array<StopAnnouncementRow, 1> kTrailing{
        {{.shape = "halted ", .dialect_gate = kAnyDialect}}};
    const std::array packages{
        SemanticPackageManifest{.name = "p", .version = "1", .stop_announcements = kTrailing}};
    EXPECT_DEATH((void)compose(packages), R"(stop announcement "halted ")");
}

// invariant: the rows are recognition content, so they enter the composed identity.
// invariant: an empty section writes no byte, so a package that declares none keeps its preimage.
TEST(StopAnnouncement, TheRowsAreIdentityAndAnEmptySectionIsNot)
{
    constexpr std::array<StopAnnouncementRow, 1> kOne{
        {{.shape = "halted.", .dialect_gate = kAnyDialect}}};
    constexpr std::array<StopAnnouncementRow, 1> kOther{
        {{.shape = "stopped.", .dialect_gate = kAnyDialect}}};
    const SemanticPackageManifest bare{.name = "p", .version = "1"};
    const SemanticPackageManifest one{.name = "p", .version = "1", .stop_announcements = kOne};
    const SemanticPackageManifest other{.name = "p", .version = "1", .stop_announcements = kOther};
    const std::string bare_identity{compose(std::array{bare}).identity_hex()};
    EXPECT_NE(compose(std::array{one}).identity_hex(), bare_identity);
    EXPECT_NE(compose(std::array{one}).identity_hex(), compose(std::array{other}).identity_hex());

    // invariant: the section opens on a zero-length string, which no package name can be.
    const SemanticPackageManifest second{.name = "q", .version = "1"};
    EXPECT_NE(compose(std::array{one, second}).identity_hex(),
              compose(std::array{bare, second}).identity_hex());
}
