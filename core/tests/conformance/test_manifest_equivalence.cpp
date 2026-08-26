// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_manifest_equivalence.cpp — the conformance kit's THIRD entry point,
// `manifest_equivalence_report(lhs, rhs)`: "do these two semantic packages agree, field for field?"
// (DN-17.D21 §5). The comparator exists to say WHERE two rulesets differ; `semantic_identity`
// already says THAT they differ, better than any comparator can. So this suite proves the two
// things a locator can silently fail at, and neither is provable by a green run alone:
//
//   • DISCRIMINATION, per manifest MEMBER. Fourteen arms, one per member of
//     SemanticPackageManifest, each a manifest identical to the subject except in that one member.
//     Every arm asserts the report goes red, that EXACTLY ONE check fails, and that the failing
//     check is the one named for the mutated member. Without these arms the suite would be
//     degenerate-satisfiable: a comparator that returns "equal" unconditionally passes every
//     equality assertion ever written, and two empty manifests are genuinely equal.
//   • COVERAGE — that the members compared are the manifest's members, not "the members someone
//     remembered". Two independent instruments: the comparator binds all fourteen members with a
//     structured binding (a fifteenth member is a COMPILE error there, not a silently uncompared
//     one), and every arm here cross-checks against `compose({m}).identity()`, an oracle written in
//     another file by another hand — `compose.cpp`'s `serialize_manifest`, hashed with SHA-256. An
//     uncompared member shows up as "the digest moved and the report stayed green", which is loud.
//
// The oracle is never the comparator (no SUT==ORACLE): expected check names and expected detail
// coordinates are written by hand below, and the equality oracle is the digest.
//
// The two sides are always INDEPENDENT objects over INDEPENDENT storage — kAlpha and kAlphaTwin are
// declared from two disjoint sets of arrays with equal content, never one manifest passed twice.
// A comparator that read one side twice would pass an lhs==rhs suite trivially (can't-FAIL).
//
// Determinism: byte-only, no RNG, no clock, no float. Every fixture is constexpr.
#include <gtest/gtest.h>

import insight.canon.test; // facade + spi (the row grammar) + the conformance kit

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

// ── The code tier. Two real symbols, not placeholders: the comparator compares PRESENCE, and the
// identity serializer writes one presence byte, so what these DO is irrelevant to both — but a
// factory that returned nullptr would be a stub standing where a package's real hook stands, and
// the suite would be asserting over a shape no package ships. JsonStrategy is a core-owned strategy
// the test target already links.
[[nodiscard]] std::unique_ptr<insight::tokenization::IFormatStrategy> make_probe_strategy()
{
    return std::make_unique<insight::tokenization::JsonStrategy>();
}

[[nodiscard]] bool probe_echoed_source(std::string_view raw_line) noexcept
{
    return raw_line.starts_with("+ ");
}

// ══ Subject: package "alpha", content in EVERY member ══════════════════════════════════════════
// Every member is non-empty except the two code-tier hooks, which are absent so their mutation arm
// runs in the absent→present direction (a nullptr subject and a present mutant are two distinguish-
// able states; two present hooks are not, by construction — see the comparator's contract).

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

// ══ The TWIN: the same CONTENT, declared over a disjoint set of arrays ══════════════════════════
// Not `kAlpha` again, and not spans into kAlpha's arrays: the equality arm has to be able to fail.
// A comparator that compared span POINTERS (or that read one argument twice) would report these two
// as different (or as equal without looking), and either way the arm below catches it.

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

// ══ The fourteen mutations — ONE member each, everything else spliced from the subject ══════════

constexpr std::array<StructuralRoleRow, 2> kRolesMut{
    // roles[0].role: GroupBegin → GroupEnd. A NON-key field on purpose: a comparator that keyed on
    // the prefix alone would call these two row sets equal.
    {{.prefix = "<AAA-group>", .role = StructuralRole::GroupEnd, .dialect_gate = "alpha"},
     {.prefix = "<AAA-end>", .role = StructuralRole::GroupEnd, .dialect_gate = "alpha"}}};

constexpr std::array<IntentMarkerRow, 2> kMarkersMut{
    // markers[1].child_order: Ordered → Unordered. Non-key, and it is the ADR-18 alignment
    // declaration — the field whose silent divergence would mis-align every comparison downstream.
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

// markers, one row SHORT — the length-mismatch arm (an appended/removed row, not a mutated one).
constexpr std::array<IntentMarkerRow, 1> kMarkersShort{
    {{.prefix = "<AAA-job> ",
      .kind = IntentMarkerKind::Job,
      .child_order = ChildOrder::Unordered,
      .dialect_gate = "alpha",
      .extract = PayloadExtract::RemainderAfterPrefix,
      .payload_excludes = kExcludesA,
      .channel_gate = kAnyChannel}}};

// markers, the same SET in the reverse declared ORDER — declared order is ruleset content (the
// identity serializer walks each span in order), so this must be reported as a difference.
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
    // emits[0].emit: PayloadAfterPrefix → PayloadThenClosingParen — the generation shape, the half
    // that moved no digest at all before grammar-3 wired `emits` into the manifest.
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
    // level_lifts[0].level: Error → Warning.
    {{.prefix = "<AAA-error>", .level = LogLevel::Warn, .dialect_gate = "alpha"}}};

constexpr std::array<std::string_view, 2> kLocExtMut{{"py", "pyx"}};
constexpr std::array<LocationRow, 1> kLocationsMut{
    // locations[0].extensions: the file-naming vocabulary, one entry changed.
    {{.kind = LocationMatchKind::PrefixAndExtension,
      .infixes = {},
      .extensions = kLocExtMut,
      .prefixes = kLocPrefixA,
      .suffixes = {}}}};

constexpr std::array<ValueClassRow, 1> kValueClassesMut{
    // value_classes[0].scale: the only int64 field on any row.
    {{.key = "alpha.duration_ms", .cls = ValueClass::None, .schedule_id = "", .scale = 1}}};

constexpr std::array<OutcomeTokenRow, 2> kTokensMut{
    // outcome_tokens[1].outcome: Failure → Cancelled. A verdict MAPPING change — invisible in every
    // key, and the exact shape of the desk failure this comparator was built to locate.
    {{.token = "AAA-PASSED", .outcome = RunOutcome::Success, .dialect_gate = "alpha"},
     {.token = "AAA-BROKEN", .outcome = RunOutcome::Aborted, .dialect_gate = "alpha"}}};

constexpr std::array<OutcomeMarkerRow, 1> kOutcomeMarkersMut{
    // outcome_markers[0].shape: RemainderToken → PrefixIsVerdict.
    {{.prefix = "<AAA-done> ",
      .dialect_gate = "alpha",
      .shape = OutcomeMarkerShape::PrefixIsVerdict,
      .outcome = RunOutcome::Unknown}}};

constexpr std::array<std::string_view, 2> kChannelsMut{{"annotated", "raw"}};
constexpr std::array<std::string_view, 1> kRevisionsMut{{"v2"}};

// One arm per member. Each mutant is the subject with exactly one member re-pointed, built in the
// test body from the arrays above: a manifest-shaped mutation HELPER would be a second description
// of the manifest's shape, and the whole point of these fixtures is that no second description
// exists to drift out of step with the first.
struct MutationArm
{
    std::string_view label;         // the mutation, in words — printed on failure
    std::string_view expect_check;  // the ONE check that must go red
    std::string_view expect_detail; // a coordinate the detail must name
    SemanticPackageManifest mutant;
};

[[nodiscard]] std::string identity_of(const SemanticPackageManifest& manifest)
{
    const std::array<SemanticPackageManifest, 1> one{manifest};
    return compose(one).identity_hex();
}

// The failing checks of a report, as "name: detail" lines — the verbose-on-failure surface every
// assertion below streams, so a red diagnoses itself without a debugger.
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

// The fourteen members, named by hand — the ORACLE for "one check per manifest member". Written
// here and not derived from the report, so a comparator that dropped a member cannot also drop the
// expectation.
constexpr std::array<std::string_view, 14> kExpectedCheckNames{
    {"equivalence.name", "equivalence.version", "equivalence.roles", "equivalence.markers",
     "equivalence.emits", "equivalence.level_lifts", "equivalence.locations",
     "equivalence.value_classes", "equivalence.outcome_tokens", "equivalence.outcome_markers",
     "equivalence.channels", "equivalence.dialect_revisions", "equivalence.strategy_presence_only",
     "equivalence.echoed_source_presence_only"}};

} // namespace

// ── The equality arm: two independently declared manifests with equal content ────────────────────
TEST(ManifestEquivalence, IndependentTwinsAgreeOnEveryMember)
{
    // The digest is the independent oracle for "these two really are the same ruleset": if this
    // fails, the FIXTURES diverged and the comparator is not on trial at all.
    ASSERT_EQ(identity_of(kAlpha), identity_of(kAlphaTwin))
        << "the twin fixtures are not content-equal, so the equality arm below proves nothing";

    const Report report{manifest_equivalence_report(kAlpha, kAlphaTwin)};
    EXPECT_TRUE(report.all_passed()) << failures_of(report);
    EXPECT_EQ(report.checks.size(), kExpectedCheckNames.size()) << report.summary();
}

// ── The coverage arm: one check per manifest member, in a fixed order, always ────────────────────
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

// ── The discrimination arms: one mutated member each ─────────────────────────────────────────────
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

        // The INDEPENDENT oracle (compose.cpp's serialize_manifest + SHA-256, another file, another
        // author): every manifest member enters the identity preimage, so every arm here must move
        // the digest. An arm whose digest does NOT move is a broken FIXTURE, not a comparator
        // finding — and asserting it first is what keeps a silently-inert mutation from reading as
        // a comparator failure.
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

// ── Length mismatch: a removed row, not a mutated one ────────────────────────────────────────────
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
    // The unpaired row must be NAMED: the paired-prefix diff structurally cannot show what was
    // removed, so a report that only said "2 vs 1" would leave the reader to find it by hand.
    EXPECT_NE(markers.detail.find("<AAA-step> "), std::string::npos) << markers.detail;
}

// ── Declared ORDER is ruleset content ────────────────────────────────────────────────────────────
TEST(ManifestEquivalence, SameRowSetInADifferentOrderIsNotEquivalent)
{
    SemanticPackageManifest reordered{kAlpha};
    reordered.markers = kMarkersReordered;

    // The digest agrees, and it is the reason the comparator pairs by index rather than by key:
    // the serializer walks each span in DECLARED order, so a re-ordered declaration is a different
    // ruleset and the two must not read as equivalent.
    ASSERT_NE(identity_of(reordered), identity_of(kAlpha))
        << "the fixture did not move the digest — re-ordering is inert here";

    const Report report{manifest_equivalence_report(kAlpha, reordered)};
    EXPECT_FALSE(report.all_passed()) << report.summary();
    const std::vector<std::string_view> failed{failing_names(report)};
    ASSERT_EQ(failed.size(), 1U) << failures_of(report);
    EXPECT_EQ(failed.front(), "equivalence.markers") << failures_of(report);
}

// ── The degenerate boundary, stated as a POSITIVE assertion plus its discrimination arm ──────────
// Two empty manifests ARE equivalent — that is the correct answer, and it is exactly the answer a
// comparator that does nothing at all would also give. So the boundary is never asserted alone: the
// second half asserts that an empty manifest and a populated one are NOT equivalent, which the
// do-nothing comparator fails.
TEST(ManifestEquivalence, EmptyManifestsAreEquivalentAndStillDiscriminate)
{
    constexpr SemanticPackageManifest kEmptyLhs{.name = "", .version = ""};
    constexpr SemanticPackageManifest kEmptyRhs{.name = "", .version = ""};

    const Report equal_report{manifest_equivalence_report(kEmptyLhs, kEmptyRhs)};
    EXPECT_TRUE(equal_report.all_passed()) << failures_of(equal_report);

    const Report split_report{manifest_equivalence_report(kEmptyLhs, kAlpha)};
    EXPECT_FALSE(split_report.all_passed())
        << "an empty manifest and a populated one compared EQUAL — the comparator is inert";
    // Every populated member must be reported: name, version, roles, markers, emits, level_lifts,
    // locations, value_classes, outcome_tokens, outcome_markers, channels, dialect_revisions. The
    // two code-tier members are absent on BOTH sides, so they legitimately stay green.
    EXPECT_EQ(failing_names(split_report).size(), 12U) << failures_of(split_report);
}

// NOLINTEND
