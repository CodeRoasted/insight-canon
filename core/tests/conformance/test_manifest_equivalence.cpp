// invariant: the conformance kit's THIRD entry point — do these two semantic packages agree,
// field for field?
// invariant: the comparator exists to say WHERE two rulesets differ; the composed identity already
// says THAT they differ, better than any comparator can.
// invariant: so this suite proves the two things a LOCATOR can silently fail at, and neither is
// provable by a green run alone.
// invariant: DISCRIMINATION, per manifest MEMBER — one arm per member, each a manifest identical
// to the subject except in that one member.
// invariant: every arm asserts the report goes RED, that EXACTLY ONE check fails, and that the
// failing check is the one NAMED for the mutated member.
// invariant: without these arms the suite would be DEGENERATE-SATISFIABLE — a comparator that
// returns equal unconditionally passes every equality assertion ever written.
// invariant: and two empty manifests are genuinely equal, so the equality arm alone proves nothing.
// invariant: COVERAGE — that the members compared are the MANIFEST'S members and not the members
// someone remembered, proven by TWO independent instruments.
// invariant: the comparator binds all members with a structured binding, so an added member is a
// COMPILE error there rather than a silently uncompared one.
// invariant: and every arm cross-checks against the composed identity, an oracle written in another
// file by another hand and hashed independently.
// invariant: an uncompared member then shows up as the digest moving while the report stays green,
// which is LOUD.
// invariant: THE ORACLE IS NEVER THE COMPARATOR — expected check names and expected detail
// coordinates are written BY HAND, and the equality oracle is the digest.
// invariant: the two sides are always INDEPENDENT objects over INDEPENDENT storage, never one
// manifest passed twice.
// invariant: a comparator that read one side twice would pass an equality suite trivially, which is
// the can't-FAIL shape.
// invariant: determinism — byte-only, no RNG, no clock, no float, and every fixture is a compile
// time constant.
// refs: DN-17.D21
#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogLevel;
using insight::RunOutcome;
using insight::StructuralRole;
using insight::semantic::compose;
using insight::semantic::IntentEmitRow;
using insight::semantic::IntentMarkerRow;
using insight::semantic::kAnyChannel;
using insight::semantic::LevelLiftRow;
using insight::semantic::LocationMatchKind;
using insight::semantic::LocationRow;
using insight::semantic::OutcomeMarkerRow;
using insight::semantic::OutcomeMarkerShape;
using insight::semantic::OutcomeTokenRow;
using insight::semantic::PayloadEmit;
using insight::semantic::PayloadExtract;
using insight::semantic::SemanticPackageManifest;
using insight::semantic::StructuralRoleRow;
using insight::semantic::ValueClass;
using insight::semantic::ValueClassRow;
using insight::semantic::conformance::CheckResult;
using insight::semantic::conformance::manifest_equivalence_report;
using insight::semantic::conformance::Report;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarkerKind;

namespace
{

// invariant: TWO REAL SYMBOLS and not placeholders — the comparator compares PRESENCE and the
// identity serializer writes one presence byte, so what these DO is irrelevant to both.
// invariant: but a factory returning nothing would be a STUB standing where a package's real hook
// stands, and the suite would be asserting over a shape no package ships.
[[nodiscard]] std::unique_ptr<insight::tokenization::IFormatStrategy> make_probe_strategy()
{
    return std::make_unique<insight::tokenization::JsonStrategy>();
}

[[nodiscard]] bool probe_echoed_source(std::string_view raw_line) noexcept
{
    return raw_line.starts_with("+ ");
}

// invariant: the SUBJECT carries content in EVERY member except the two code-tier hooks, which are
// absent so their mutation arm runs in the absent-to-present direction.
// invariant: an absent subject and a present mutant are two DISTINGUISHABLE states; two present
// hooks are not, by construction.
constexpr std::array<std::string_view, 2> kExcludesA{{"{", "}"}};

constexpr std::array<StructuralRoleRow, 2> kRolesA{
    {{.prefix = "<AAA-group>", .role = StructuralRole::GroupBegin, .dialect_gate = "alpha"},
     {.prefix = "<AAA-end>", .role = StructuralRole::GroupEnd, .dialect_gate = "alpha"}}};

constexpr std::array<IntentMarkerRow, 2> kMarkersA{
    {{.prefix = "<AAA-job> ",
      .kind = IntentMarkerKind::Job,
      .child_order = ChildOrder::Unordered,
      .dialect_gate = "alpha",
      .extract = PayloadExtract::RemainderAfterPrefix,
      .payload_excludes = kExcludesA,
      .channel_gate = kAnyChannel},
     {.prefix = "<AAA-step> ",
      .kind = IntentMarkerKind::Step,
      .child_order = ChildOrder::Ordered,
      .dialect_gate = "alpha",
      .extract = PayloadExtract::RemainderAfterPrefix,
      .payload_excludes = {},
      .channel_gate = "annotated"}}};

constexpr std::array<IntentEmitRow, 2> kEmitsA{{{.prefix = "<AAA-job> ",
                                                 .kind = IntentMarkerKind::Job,
                                                 .child_order = ChildOrder::Unordered,
                                                 .dialect_gate = "alpha",
                                                 .emit = PayloadEmit::PayloadAfterPrefix,
                                                 .channel_gate = kAnyChannel},
                                                {.prefix = "<AAA-step> ",
                                                 .kind = IntentMarkerKind::Step,
                                                 .child_order = ChildOrder::Ordered,
                                                 .dialect_gate = "alpha",
                                                 .emit = PayloadEmit::PayloadAfterPrefix,
                                                 .channel_gate = "annotated"}}};

constexpr std::array<LevelLiftRow, 1> kLiftsA{
    {{.prefix = "<AAA-error>", .level = LogLevel::Error, .dialect_gate = "alpha"}}};

constexpr std::array<std::string_view, 2> kLocExtA{{"py", "pyi"}};
constexpr std::array<std::string_view, 1> kLocPrefixA{{"test_"}};
constexpr std::array<LocationRow, 1> kLocationsA{{{.kind = LocationMatchKind::PrefixAndExtension,
                                                   .infixes = {},
                                                   .extensions = kLocExtA,
                                                   .prefixes = kLocPrefixA,
                                                   .suffixes = {}}}};

constexpr std::array<ValueClassRow, 1> kValueClassesA{
    {{.key = "alpha.duration_ms", .cls = ValueClass::None, .schedule_id = "", .scale = 1000}}};

constexpr std::array<OutcomeTokenRow, 2> kTokensA{
    {{.token = "AAA-PASSED", .outcome = RunOutcome::Success, .dialect_gate = "alpha"},
     {.token = "AAA-BROKEN", .outcome = RunOutcome::Failure, .dialect_gate = "alpha"}}};

constexpr std::array<OutcomeMarkerRow, 1> kOutcomeMarkersA{
    {{.prefix = "<AAA-done> ",
      .dialect_gate = "alpha",
      .shape = OutcomeMarkerShape::RemainderToken,
      .outcome = RunOutcome::Unknown}}};

constexpr std::array<std::string_view, 2> kChannelsA{{"annotated", "stripped"}};
constexpr std::array<std::string_view, 1> kRevisionsA{{"v1"}};

constexpr SemanticPackageManifest kAlpha{.name = "alpha",
                                         .version = "1.0.0",
                                         .roles = kRolesA,
                                         .markers = kMarkersA,
                                         .emits = kEmitsA,
                                         .level_lifts = kLiftsA,
                                         .locations = kLocationsA,
                                         .value_classes = kValueClassesA,
                                         .outcome_tokens = kTokensA,
                                         .outcome_markers = kOutcomeMarkersA,
                                         .channels = kChannelsA,
                                         .dialect_revisions = kRevisionsA,
                                         .strategy = nullptr,
                                         .echoed_source = nullptr};

// invariant: THE TWIN — the same CONTENT declared over a DISJOINT set of arrays, not the subject
// again and not spans into the subject's arrays.
// invariant: the equality arm has to be able to FAIL.
// invariant: a comparator comparing span POINTERS would report these two as different, and one
// reading a single argument twice would report equal without looking.
// invariant: either way the arm below catches it.
constexpr std::array<std::string_view, 2> kExcludesTwin{{"{", "}"}};

constexpr std::array<StructuralRoleRow, 2> kRolesTwin{
    {{.prefix = "<AAA-group>", .role = StructuralRole::GroupBegin, .dialect_gate = "alpha"},
     {.prefix = "<AAA-end>", .role = StructuralRole::GroupEnd, .dialect_gate = "alpha"}}};

constexpr std::array<IntentMarkerRow, 2> kMarkersTwin{
    {{.prefix = "<AAA-job> ",
      .kind = IntentMarkerKind::Job,
      .child_order = ChildOrder::Unordered,
      .dialect_gate = "alpha",
      .extract = PayloadExtract::RemainderAfterPrefix,
      .payload_excludes = kExcludesTwin,
      .channel_gate = kAnyChannel},
     {.prefix = "<AAA-step> ",
      .kind = IntentMarkerKind::Step,
      .child_order = ChildOrder::Ordered,
      .dialect_gate = "alpha",
      .extract = PayloadExtract::RemainderAfterPrefix,
      .payload_excludes = {},
      .channel_gate = "annotated"}}};

constexpr std::array<IntentEmitRow, 2> kEmitsTwin{{{.prefix = "<AAA-job> ",
                                                    .kind = IntentMarkerKind::Job,
                                                    .child_order = ChildOrder::Unordered,
                                                    .dialect_gate = "alpha",
                                                    .emit = PayloadEmit::PayloadAfterPrefix,
                                                    .channel_gate = kAnyChannel},
                                                   {.prefix = "<AAA-step> ",
                                                    .kind = IntentMarkerKind::Step,
                                                    .child_order = ChildOrder::Ordered,
                                                    .dialect_gate = "alpha",
                                                    .emit = PayloadEmit::PayloadAfterPrefix,
                                                    .channel_gate = "annotated"}}};

constexpr std::array<LevelLiftRow, 1> kLiftsTwin{
    {{.prefix = "<AAA-error>", .level = LogLevel::Error, .dialect_gate = "alpha"}}};

constexpr std::array<std::string_view, 2> kLocExtTwin{{"py", "pyi"}};
constexpr std::array<std::string_view, 1> kLocPrefixTwin{{"test_"}};
constexpr std::array<LocationRow, 1> kLocationsTwin{{{.kind = LocationMatchKind::PrefixAndExtension,
                                                      .infixes = {},
                                                      .extensions = kLocExtTwin,
                                                      .prefixes = kLocPrefixTwin,
                                                      .suffixes = {}}}};

constexpr std::array<ValueClassRow, 1> kValueClassesTwin{
    {{.key = "alpha.duration_ms", .cls = ValueClass::None, .schedule_id = "", .scale = 1000}}};

constexpr std::array<OutcomeTokenRow, 2> kTokensTwin{
    {{.token = "AAA-PASSED", .outcome = RunOutcome::Success, .dialect_gate = "alpha"},
     {.token = "AAA-BROKEN", .outcome = RunOutcome::Failure, .dialect_gate = "alpha"}}};

constexpr std::array<OutcomeMarkerRow, 1> kOutcomeMarkersTwin{
    {{.prefix = "<AAA-done> ",
      .dialect_gate = "alpha",
      .shape = OutcomeMarkerShape::RemainderToken,
      .outcome = RunOutcome::Unknown}}};

constexpr std::array<std::string_view, 2> kChannelsTwin{{"annotated", "stripped"}};
constexpr std::array<std::string_view, 1> kRevisionsTwin{{"v1"}};

constexpr SemanticPackageManifest kAlphaTwin{.name = "alpha",
                                             .version = "1.0.0",
                                             .roles = kRolesTwin,
                                             .markers = kMarkersTwin,
                                             .emits = kEmitsTwin,
                                             .level_lifts = kLiftsTwin,
                                             .locations = kLocationsTwin,
                                             .value_classes = kValueClassesTwin,
                                             .outcome_tokens = kTokensTwin,
                                             .outcome_markers = kOutcomeMarkersTwin,
                                             .channels = kChannelsTwin,
                                             .dialect_revisions = kRevisionsTwin,
                                             .strategy = nullptr,
                                             .echoed_source = nullptr};

constexpr std::array<StructuralRoleRow, 2> kRolesMut{
    // invariant: a NON-KEY field on purpose — a comparator keying on the prefix alone would call
    // these two row sets equal.
    {{.prefix = "<AAA-group>", .role = StructuralRole::GroupEnd, .dialect_gate = "alpha"},
     {.prefix = "<AAA-end>", .role = StructuralRole::GroupEnd, .dialect_gate = "alpha"}}};

constexpr std::array<IntentMarkerRow, 2> kMarkersMut{
    // invariant: non-key again, and it is the ALIGNMENT declaration — the field whose silent
    // divergence would mis-align every comparison downstream.
    // refs: ADR-18
    {{.prefix = "<AAA-job> ",
      .kind = IntentMarkerKind::Job,
      .child_order = ChildOrder::Unordered,
      .dialect_gate = "alpha",
      .extract = PayloadExtract::RemainderAfterPrefix,
      .payload_excludes = kExcludesA,
      .channel_gate = kAnyChannel},
     {.prefix = "<AAA-step> ",
      .kind = IntentMarkerKind::Step,
      .child_order = ChildOrder::Unordered,
      .dialect_gate = "alpha",
      .extract = PayloadExtract::RemainderAfterPrefix,
      .payload_excludes = {},
      .channel_gate = "annotated"}}};

// invariant: one row SHORT — the LENGTH-MISMATCH arm, an appended or removed row rather than a
// mutated one.
constexpr std::array<IntentMarkerRow, 1> kMarkersShort{
    {{.prefix = "<AAA-job> ",
      .kind = IntentMarkerKind::Job,
      .child_order = ChildOrder::Unordered,
      .dialect_gate = "alpha",
      .extract = PayloadExtract::RemainderAfterPrefix,
      .payload_excludes = kExcludesA,
      .channel_gate = kAnyChannel}}};

// invariant: the same row SET in the REVERSE declared order — declared order is ruleset CONTENT,
// because the identity serializer walks each span in order.
// invariant: so it must be reported as a difference.
constexpr std::array<IntentMarkerRow, 2> kMarkersReordered{
    {{.prefix = "<AAA-step> ",
      .kind = IntentMarkerKind::Step,
      .child_order = ChildOrder::Ordered,
      .dialect_gate = "alpha",
      .extract = PayloadExtract::RemainderAfterPrefix,
      .payload_excludes = {},
      .channel_gate = "annotated"},
     {.prefix = "<AAA-job> ",
      .kind = IntentMarkerKind::Job,
      .child_order = ChildOrder::Unordered,
      .dialect_gate = "alpha",
      .extract = PayloadExtract::RemainderAfterPrefix,
      .payload_excludes = kExcludesA,
      .channel_gate = kAnyChannel}}};

constexpr std::array<IntentEmitRow, 2> kEmitsMut{
    // invariant: the GENERATION shape — the half that moved no digest at all before the emit rows
    // were wired into the manifest.
    {{.prefix = "<AAA-job> ",
      .kind = IntentMarkerKind::Job,
      .child_order = ChildOrder::Unordered,
      .dialect_gate = "alpha",
      .emit = PayloadEmit::PayloadThenClosingParen,
      .channel_gate = kAnyChannel},
     {.prefix = "<AAA-step> ",
      .kind = IntentMarkerKind::Step,
      .child_order = ChildOrder::Ordered,
      .dialect_gate = "alpha",
      .emit = PayloadEmit::PayloadAfterPrefix,
      .channel_gate = "annotated"}}};

constexpr std::array<LevelLiftRow, 1> kLiftsMut{
    {{.prefix = "<AAA-error>", .level = LogLevel::Warn, .dialect_gate = "alpha"}}};

constexpr std::array<std::string_view, 2> kLocExtMut{{"py", "pyx"}};
constexpr std::array<LocationRow, 1> kLocationsMut{{{.kind = LocationMatchKind::PrefixAndExtension,
                                                     .infixes = {},
                                                     .extensions = kLocExtMut,
                                                     .prefixes = kLocPrefixA,
                                                     .suffixes = {}}}};

constexpr std::array<ValueClassRow, 1> kValueClassesMut{
    // invariant: the only integer field on any row.
    {{.key = "alpha.duration_ms", .cls = ValueClass::None, .schedule_id = "", .scale = 1}}};

constexpr std::array<OutcomeTokenRow, 2> kTokensMut{
    // invariant: a verdict MAPPING change — invisible in every key, and the exact shape of the
    // desk failure this comparator was built to locate.
    {{.token = "AAA-PASSED", .outcome = RunOutcome::Success, .dialect_gate = "alpha"},
     {.token = "AAA-BROKEN", .outcome = RunOutcome::Aborted, .dialect_gate = "alpha"}}};

constexpr std::array<OutcomeMarkerRow, 1> kOutcomeMarkersMut{
    {{.prefix = "<AAA-done> ",
      .dialect_gate = "alpha",
      .shape = OutcomeMarkerShape::PrefixIsVerdict,
      .outcome = RunOutcome::Unknown}}};

constexpr std::array<std::string_view, 2> kChannelsMut{{"annotated", "raw"}};
constexpr std::array<std::string_view, 1> kRevisionsMut{{"v2"}};

// invariant: each mutant is the subject with exactly ONE member re-pointed, built in the test body
// from the arrays above.
// invariant: a manifest-shaped mutation HELPER would be a SECOND description of the manifest's
// shape, and the whole point of these fixtures is that no second description exists to drift.
struct MutationArm
{
    std::string_view label;
    std::string_view expect_check;
    std::string_view expect_detail;
    SemanticPackageManifest mutant;
};

[[nodiscard]] std::string identity_of(const SemanticPackageManifest& manifest)
{
    const std::array<SemanticPackageManifest, 1> one{manifest};
    return compose(one).identity_hex();
}

// invariant: the failing checks rendered as name-and-detail lines — the verbose-on-failure
// surface every assertion streams, so a red DIAGNOSES ITSELF without a debugger.
[[nodiscard]] std::string failures_of(const Report& report)
{
    std::string out{report.summary()};
    for (const CheckResult& check : report.checks)
    {
        if (check.passed)
            continue;
        out += "\n  FAILED [";
        out += check.name;
        out += "] ";
        out += check.detail;
    }
    return out;
}

[[nodiscard]] std::vector<std::string_view> failing_names(const Report& report)
{
    std::vector<std::string_view> names;
    for (const CheckResult& check : report.checks)
        if (!check.passed)
            names.push_back(check.name);
    return names;
}

// invariant: the members NAMED BY HAND — the ORACLE for one check per manifest member.
// invariant: written here and NOT derived from the report, so a comparator that dropped a member
// cannot also drop the expectation.
constexpr std::array<std::string_view, 14> kExpectedCheckNames{
    {"equivalence.name", "equivalence.version", "equivalence.roles", "equivalence.markers",
     "equivalence.emits", "equivalence.level_lifts", "equivalence.locations",
     "equivalence.value_classes", "equivalence.outcome_tokens", "equivalence.outcome_markers",
     "equivalence.channels", "equivalence.dialect_revisions", "equivalence.strategy_presence_only",
     "equivalence.echoed_source_presence_only"}};

} // namespace

TEST(ManifestEquivalence, IndependentTwinsAgreeOnEveryMember)
{
    // invariant: the digest is the INDEPENDENT oracle for these two really being the same ruleset
    // — if this fails, the FIXTURES diverged and the comparator is not on trial at all.
    ASSERT_EQ(identity_of(kAlpha), identity_of(kAlphaTwin))
        << "the twin fixtures are not content-equal, so the equality arm below proves nothing";

    const Report report{manifest_equivalence_report(kAlpha, kAlphaTwin)};
    EXPECT_TRUE(report.all_passed()) << failures_of(report);
    EXPECT_EQ(report.checks.size(), kExpectedCheckNames.size()) << report.summary();
}

TEST(ManifestEquivalence, ReportCarriesOneCheckPerManifestMember)
{
    const Report report{manifest_equivalence_report(kAlpha, kAlphaTwin)};
    ASSERT_EQ(report.checks.size(), kExpectedCheckNames.size())
        << "SemanticPackageManifest has " << kExpectedCheckNames.size()
        << " members and the report carries " << report.checks.size()
        << " checks — a member is uncompared, or a check is duplicated";
    for (std::size_t idx{0}; idx < kExpectedCheckNames.size(); ++idx)
        EXPECT_EQ(report.checks[idx].name, kExpectedCheckNames[idx])
            << "check " << idx << " is named \"" << report.checks[idx].name << "\", expected \""
            << kExpectedCheckNames[idx] << '"';
}

TEST(ManifestEquivalence, EveryManifestMemberDiscriminates)
{
    SemanticPackageManifest mut_name{kAlpha};
    mut_name.name = "alpha-renamed";
    SemanticPackageManifest mut_version{kAlpha};
    mut_version.version = "1.0.1";
    SemanticPackageManifest mut_roles{kAlpha};
    mut_roles.roles = kRolesMut;
    SemanticPackageManifest mut_markers{kAlpha};
    mut_markers.markers = kMarkersMut;
    SemanticPackageManifest mut_emits{kAlpha};
    mut_emits.emits = kEmitsMut;
    SemanticPackageManifest mut_lifts{kAlpha};
    mut_lifts.level_lifts = kLiftsMut;
    SemanticPackageManifest mut_locations{kAlpha};
    mut_locations.locations = kLocationsMut;
    SemanticPackageManifest mut_value_classes{kAlpha};
    mut_value_classes.value_classes = kValueClassesMut;
    SemanticPackageManifest mut_tokens{kAlpha};
    mut_tokens.outcome_tokens = kTokensMut;
    SemanticPackageManifest mut_outcome_markers{kAlpha};
    mut_outcome_markers.outcome_markers = kOutcomeMarkersMut;
    SemanticPackageManifest mut_channels{kAlpha};
    mut_channels.channels = kChannelsMut;
    SemanticPackageManifest mut_revisions{kAlpha};
    mut_revisions.dialect_revisions = kRevisionsMut;
    SemanticPackageManifest mut_strategy{kAlpha};
    mut_strategy.strategy = &make_probe_strategy;
    SemanticPackageManifest mut_hook{kAlpha};
    mut_hook.echoed_source = &probe_echoed_source;

    const std::array<MutationArm, 14> arms{
        {{"name: \"alpha\" -> \"alpha-renamed\"", "equivalence.name", "alpha-renamed", mut_name},
         {"version: \"1.0.0\" -> \"1.0.1\"", "equivalence.version", "1.0.1", mut_version},
         {"roles[0].role: GroupBegin -> GroupEnd", "equivalence.roles", "roles[0]", mut_roles},
         {"markers[1].child_order: Ordered -> Unordered", "equivalence.markers", "markers[1]",
          mut_markers},
         {"emits[0].emit: PayloadAfterPrefix -> PayloadThenClosingParen", "equivalence.emits",
          "emits[0]", mut_emits},
         {"level_lifts[0].level: Error -> Warn", "equivalence.level_lifts", "level_lifts[0]",
          mut_lifts},
         {"locations[0].extensions: \"pyi\" -> \"pyx\"", "equivalence.locations", "locations[0]",
          mut_locations},
         {"value_classes[0].scale: 1000 -> 1", "equivalence.value_classes", "value_classes[0]",
          mut_value_classes},
         {"outcome_tokens[1].outcome: Failure -> Aborted", "equivalence.outcome_tokens",
          "outcome_tokens[1]", mut_tokens},
         {"outcome_markers[0].shape: RemainderToken -> PrefixIsVerdict",
          "equivalence.outcome_markers", "outcome_markers[0]", mut_outcome_markers},
         {"channels: \"stripped\" -> \"raw\"", "equivalence.channels", "raw", mut_channels},
         {"dialect_revisions: \"v1\" -> \"v2\"", "equivalence.dialect_revisions", "v2",
          mut_revisions},
         {"strategy: absent -> present", "equivalence.strategy_presence_only", "PRESENCE",
          mut_strategy},
         {"echoed_source: absent -> present", "equivalence.echoed_source_presence_only", "PRESENCE",
          mut_hook}}};

    ASSERT_EQ(arms.size(), kExpectedCheckNames.size())
        << "one discrimination arm per manifest member is the contract of this test";

    const std::string subject_identity{identity_of(kAlpha)};
    for (const MutationArm& arm : arms)
    {
        SCOPED_TRACE(std::string{"mutation: "} + std::string{arm.label});

        // invariant: THE INDEPENDENT ORACLE — every manifest member enters the identity preimage,
        // so every arm here must MOVE the digest.
        // invariant: an arm whose digest does NOT move is a broken FIXTURE and not a comparator
        // finding.
        // invariant: asserting it FIRST is what keeps a silently-inert mutation from reading as a
        // comparator failure.
        EXPECT_NE(identity_of(arm.mutant), subject_identity)
            << "the mutation did not move semantic_identity — the fixture is inert, so this arm "
               "cannot judge the comparator";

        const Report report{manifest_equivalence_report(kAlpha, arm.mutant)};
        EXPECT_FALSE(report.all_passed())
            << "the comparator reported EQUAL for a manifest that differs in one member: "
            << report.summary();

        const std::vector<std::string_view> failed{failing_names(report)};
        ASSERT_EQ(failed.size(), 1U)
            << "exactly one check must go red for a one-member mutation; " << failures_of(report);
        EXPECT_EQ(failed.front(), arm.expect_check) << failures_of(report);

        const CheckResult* named{nullptr};
        for (const CheckResult& check : report.checks)
            if (check.name == arm.expect_check)
                named = &check;
        ASSERT_NE(named, nullptr) << "no check named " << arm.expect_check;
        EXPECT_NE(named->detail.find(arm.expect_detail), std::string::npos)
            << "the detail does not name the coordinate \"" << arm.expect_detail
            << "\" — a locator that cannot locate: " << named->detail;
    }
}

TEST(ManifestEquivalence, RowCountMismatchNamesBothCountsAndTheUnpairedRow)
{
    SemanticPackageManifest shorter{kAlpha};
    shorter.markers = kMarkersShort;

    const Report report{manifest_equivalence_report(kAlpha, shorter)};
    ASSERT_FALSE(report.all_passed()) << report.summary();

    const std::vector<std::string_view> failed{failing_names(report)};
    ASSERT_EQ(failed.size(), 1U) << failures_of(report);
    EXPECT_EQ(failed.front(), "equivalence.markers") << failures_of(report);

    const CheckResult& markers{report.checks[3]};
    ASSERT_EQ(markers.name, "equivalence.markers");
    EXPECT_NE(markers.detail.find("LHS declares 2 rows, RHS declares 1"), std::string::npos)
        << markers.detail;
    // invariant: the unpaired row must be NAMED.
    // invariant: the paired-prefix diff structurally cannot show what was REMOVED, so a report
    // giving only the two counts would leave the reader to find it by hand.
    EXPECT_NE(markers.detail.find("<AAA-step> "), std::string::npos) << markers.detail;
}

TEST(ManifestEquivalence, SameRowSetInADifferentOrderIsNotEquivalent)
{
    SemanticPackageManifest reordered{kAlpha};
    reordered.markers = kMarkersReordered;

    // invariant: the digest AGREES that a re-ordering is a difference, and it is the reason the
    // comparator pairs by INDEX rather than by key.
    // invariant: the serializer walks each span in DECLARED order, so a re-ordered declaration is a
    // different ruleset and the two must not read as equivalent.
    ASSERT_NE(identity_of(reordered), identity_of(kAlpha))
        << "the fixture did not move the digest — re-ordering is inert here";

    const Report report{manifest_equivalence_report(kAlpha, reordered)};
    EXPECT_FALSE(report.all_passed()) << report.summary();
    const std::vector<std::string_view> failed{failing_names(report)};
    ASSERT_EQ(failed.size(), 1U) << failures_of(report);
    EXPECT_EQ(failed.front(), "equivalence.markers") << failures_of(report);
}

// invariant: THE DEGENERATE BOUNDARY, stated as a POSITIVE assertion PLUS its discrimination arm.
// invariant: two empty manifests ARE equivalent — that is the correct answer, and it is exactly
// the answer a comparator that does nothing at all would also give.
// invariant: so the boundary is NEVER asserted alone: the second half asserts that an empty
// manifest and a populated one are NOT equivalent, which the do-nothing comparator fails.
TEST(ManifestEquivalence, EmptyManifestsAreEquivalentAndStillDiscriminate)
{
    constexpr SemanticPackageManifest kEmptyLhs{.name = "", .version = ""};
    constexpr SemanticPackageManifest kEmptyRhs{.name = "", .version = ""};

    const Report equal_report{manifest_equivalence_report(kEmptyLhs, kEmptyRhs)};
    EXPECT_TRUE(equal_report.all_passed()) << failures_of(equal_report);

    const Report split_report{manifest_equivalence_report(kEmptyLhs, kAlpha)};
    EXPECT_FALSE(split_report.all_passed())
        << "an empty manifest and a populated one compared EQUAL — the comparator is inert";
    // invariant: every POPULATED member must be reported, and the two code-tier members are absent
    // on BOTH sides so they legitimately stay green.
    EXPECT_EQ(failing_names(split_report).size(), 12U) << failures_of(split_report);
}
