// invariant: the inner-step grammar over SYNTHETIC rows whose literals are not GitHub's, so the
// mechanism is vocabulary-free.
// invariant: the arms cover kind InnerStep, role Ends, extractor KeyedFields and its dual, each
// row's revision_since and rank, the revision fence and the identity's trailing section.
// invariant: the github declaration's rows are proven in the github suite.
// refs: DN-162.D1, DN-162.D4, DN-162.D5, DN-162.O3
#include <gtest/gtest.h>

import insight.canon.test;

using insight::semantic::all_row_revisions_declared;
using insight::semantic::compose;
using insight::semantic::ComposedSemantics;
using insight::semantic::IntentEmitRow;
using insight::semantic::IntentMarkerRow;
using insight::semantic::KeyedFieldRoles;
using insight::semantic::MarkerRole;
using insight::semantic::OutcomeMarkerRow;
using insight::semantic::PayloadEmit;
using insight::semantic::PayloadExtract;
using insight::semantic::SemanticPackageManifest;
using insight::semantic::unit_role_row_admitted;
using insight::tokenization::ChildOrder;
using insight::tokenization::IntentMarker;
using insight::tokenization::IntentMarkerKind;
using insight::tokenization::recognize;
using insight::tokenization::recognize_end;
using insight::tokenization::UnitEnd;

namespace
{
[[nodiscard]] insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

constexpr std::string_view kSynth{"synth_inner"};

// invariant: literals no platform uses — a `/` segment, a `#` ordinal, a `~anon` anonymous
// segment — so a core that held GitHub's would fail every naming arm below.
constexpr KeyedFieldRoles kNaming{
    .name = "key", .display = "title", .segment = "/", .ordinal = "#", .anonymous = "~anon"};
constexpr KeyedFieldRoles kEnding{.outcome = "own", .propagated_outcome = "counted"};

constexpr std::array<std::string_view, 2> kRevisions{"r1", "r2"};
constexpr std::array<std::string_view, 1> kChannels{"marked"};

// invariant: a channel-gated r1 Step row FIRST, so a view that drops it shifts every later row's
// index, and a rank read at the wrong index is visible.
constexpr std::array<IntentMarkerRow, 3> kMarkers{{
    {.prefix = "== step ",
     .kind = IntentMarkerKind::Step,
     .child_order = ChildOrder::Ordered,
     .dialect_gate = kSynth,
     .extract = PayloadExtract::RemainderAfterPrefix,
     .channel_gate = "marked",
     .revision_since = "r1"},
    {.prefix = ">> begin ",
     .kind = IntentMarkerKind::InnerStep,
     .child_order = ChildOrder::Ordered,
     .dialect_gate = kSynth,
     .extract = PayloadExtract::KeyedFields,
     .fields = kNaming,
     .revision_since = "r2"},
    {.prefix = "<< end ",
     .kind = IntentMarkerKind::InnerStep,
     .child_order = ChildOrder::Ordered,
     .dialect_gate = kSynth,
     .extract = PayloadExtract::KeyedFields,
     .role = MarkerRole::Ends,
     .fields = kEnding,
     .revision_since = "r2"},
}};

constexpr std::array<IntentEmitRow, 3> kEmits{{
    {.prefix = "== step ",
     .kind = IntentMarkerKind::Step,
     .child_order = ChildOrder::Ordered,
     .dialect_gate = kSynth,
     .emit = PayloadEmit::PayloadAfterPrefix,
     .channel_gate = "marked",
     .revision_since = "r1"},
    {.prefix = ">> begin ",
     .kind = IntentMarkerKind::InnerStep,
     .child_order = ChildOrder::Ordered,
     .dialect_gate = kSynth,
     .emit = PayloadEmit::KeyedFieldsThenClosingBracket,
     .fields = kNaming,
     .revision_since = "r2"},
    {.prefix = "<< end ",
     .kind = IntentMarkerKind::InnerStep,
     .child_order = ChildOrder::Ordered,
     .dialect_gate = kSynth,
     .emit = PayloadEmit::KeyedFieldsThenClosingBracket,
     .role = MarkerRole::Ends,
     .fields = kEnding,
     .revision_since = "r2"},
}};

constexpr SemanticPackageManifest kManifest{
    .name = kSynth,
    .version = "1.0.0",
    .markers = kMarkers,
    .emits = kEmits,
    .channels = kChannels,
    .dialect_revisions = kRevisions,
};

static_assert(all_row_revisions_declared(kManifest),
              "the synthetic two-revision package names a declared revision on every row");
static_assert(insight::semantic::all_intents_paired(kMarkers, kEmits),
              "every synthetic reader row has its writer dual");

[[nodiscard]] ComposedSemantics view_of(std::string_view channel)
{
    const std::array manifests{kManifest};
    return compose(manifests).for_stream(kSynth, channel);
}

[[nodiscard]] std::string show(const IntentMarker& mark)
{
    return "{kind=" + std::to_string(static_cast<int>(mark.kind)) + ", name=\"" +
           std::string{mark.name} + "\", display=\"" + std::string{mark.display} +
           "\", discriminant=\"" + std::string{mark.discriminant} + "\", since=\"" +
           std::string{mark.revision_since} + "\", rank=" + std::to_string(mark.revision_rank) +
           "}";
}

[[nodiscard]] std::string show(const UnitEnd& ended)
{
    return "{kind=" + std::to_string(static_cast<int>(ended.kind)) + ", outcome=\"" +
           std::string{ended.outcome} + "\", propagated_outcome=\"" +
           std::string{ended.propagated_outcome} + "\", since=\"" +
           std::string{ended.revision_since} + "\", rank=" + std::to_string(ended.revision_rank) +
           "}";
}

template <typename Row, std::size_t Count>
[[nodiscard]] constexpr std::array<Row, Count> with_since(std::array<Row, Count> rows,
                                                          std::string_view since)
{
    for (Row& row : rows)
        row.revision_since = since;
    return rows;
}

constexpr std::array<std::string_view, 1> kOneRevision{"r1"};
constexpr auto kUnownedMarkers{with_since(kMarkers, {})};
constexpr auto kUnownedEmits{with_since(kEmits, {})};
constexpr auto kOwnedMarkers{with_since(kMarkers, "r1")};
constexpr auto kOwnedEmits{with_since(kEmits, "r1")};
constexpr auto kUndeclaredMarkers{with_since(kMarkers, "r3")};
constexpr auto kUndeclaredEmits{with_since(kEmits, "r3")};
constexpr std::array<OutcomeMarkerRow, 1> kOwnerless{{{.prefix = "verdict: "}}};

constexpr SemanticPackageManifest kSingleton{.name = kSynth,
                                             .version = "1.0.0",
                                             .markers = kUnownedMarkers,
                                             .emits = kUnownedEmits,
                                             .channels = kChannels,
                                             .dialect_revisions = kOneRevision};
constexpr SemanticPackageManifest kSingletonOwned{.name = kSynth,
                                                  .version = "1.0.0",
                                                  .markers = kOwnedMarkers,
                                                  .emits = kOwnedEmits,
                                                  .channels = kChannels,
                                                  .dialect_revisions = kOneRevision};
constexpr SemanticPackageManifest kMultiUnowned{.name = kSynth,
                                                .version = "1.0.0",
                                                .markers = kUnownedMarkers,
                                                .emits = kUnownedEmits,
                                                .channels = kChannels,
                                                .dialect_revisions = kRevisions};
constexpr SemanticPackageManifest kMultiUndeclared{.name = kSynth,
                                                   .version = "1.0.0",
                                                   .markers = kUndeclaredMarkers,
                                                   .emits = kUndeclaredEmits,
                                                   .channels = kChannels,
                                                   .dialect_revisions = kRevisions};
constexpr SemanticPackageManifest kMultiOwnerless{.name = kSynth,
                                                  .version = "1.0.0",
                                                  .markers = kMarkers,
                                                  .emits = kEmits,
                                                  .outcome_markers = kOwnerless,
                                                  .channels = kChannels,
                                                  .dialect_revisions = kRevisions};

[[nodiscard]] std::string identity_of(const SemanticPackageManifest& manifest)
{
    const std::array manifests{manifest};
    return compose(manifests).identity_hex();
}
} // namespace

// refs: DN-162.D1, DN-162.O3
// invariant: the fence's SINGLETON half: one revision owns every row, so no row names one.
TEST(InnerStepGrammar, TheFenceRefusesAnOwnerInASingleRevisionPackage)
{
    static_assert(all_row_revisions_declared(kSingleton));
    static_assert(!all_row_revisions_declared(kSingletonOwned),
                  "a singleton package whose rows name its one revision must be refused: one "
                  "meaning, one spelling");
}

// refs: DN-162.D1, DN-162.O3
// invariant: the fence's MULTI-REVISION half: every row names a declared revision, and a section
// whose rows carry no owner is empty.
TEST(InnerStepGrammar, TheFenceRequiresADeclaredOwnerOnEveryRowOfAMultiRevisionPackage)
{
    static_assert(all_row_revisions_declared(kManifest));
    static_assert(!all_row_revisions_declared(kMultiUnowned),
                  "a multi-revision row that names no revision must be refused");
    static_assert(!all_row_revisions_declared(kMultiUndeclared),
                  "a row naming a revision the span does not declare must be refused");
    static_assert(!all_row_revisions_declared(kMultiOwnerless),
                  "a multi-revision package declaring a row that cannot name its owner (an "
                  "outcome marker) must be refused");
}

// refs: DN-162.D4, DN-162.D5
// invariant: the name is the id's last segment, its trailing ordinal removed, and the display
// under the declared anonymous segment; the display rides apart from the name, raw.
// invariant: an inner step declares no discriminant, and every literal is the ROW's, none core's.
TEST(InnerStepGrammar, ANamingRowNamesTheUnitFromItsDeclaredFields)
{
    const ComposedSemantics view{view_of("marked")};
    struct Case
    {
        std::string_view line;
        std::string_view name;
        std::string_view display;
    };
    for (const Case& probe : {
             Case{">> begin title=Build it;key=root/setup]", "setup", "Build it"},
             Case{">> begin key=root/outer/lint#2;title=Lint]", "lint", "Lint"},
             Case{">> begin title=Run a%3Bb%5D;key=root/~anon]", "Run a%3Bb%5D", "Run a%3Bb%5D"},
             Case{">> begin title=Run again;key=root/~anon#3]", "Run again", "Run again"},
             Case{">> begin title=T;key=solo]", "solo", "T"},
             Case{">> begin title=T;key=root/v#x]", "v#x", "T"},
             Case{">> begin title=T;key=root/a.b_2]", "a.b_2", "T"},
         })
    {
        const IntentMarker got{recognize(norm_probe(probe.line), view)};
        EXPECT_EQ(got.kind, IntentMarkerKind::InnerStep) << probe.line << " -> " << show(got);
        EXPECT_EQ(got.name, probe.name) << probe.line << " -> " << show(got);
        EXPECT_EQ(got.display, probe.display) << probe.line << " -> " << show(got);
        EXPECT_TRUE(got.discriminant.empty())
            << "an inner step declares no instance key (DN-162.D5): " << probe.line << " -> "
            << show(got);
        EXPECT_EQ(got.child_order, ChildOrder::Ordered) << probe.line;
    }
}

// refs: DN-162.D4
// invariant: KeyedFields requires the line-final ']' and every key its row names; a line short of
// either does not match the row, and an empty name declines it too.
TEST(InnerStepGrammar, TheKeyedExtractorDeclinesALineWithoutItsFinalBracketOrAKey)
{
    const ComposedSemantics view{view_of("marked")};
    for (const std::string_view line : {
             std::string_view{">> begin title=Build;key=root/setup"},
             std::string_view{">> begin title=Build;key=root/setup] "},
             std::string_view{">> begin title=Build]"},
             std::string_view{">> begin key=root/setup]"},
             std::string_view{">> begin title=Build;key=]"},
             std::string_view{">> begin titles=Build;keys=root/setup]"},
         })
        EXPECT_EQ(recognize(norm_probe(line), view).kind, IntentMarkerKind::None)
            << "\"" << line
            << "\" matched the naming row: " << show(recognize(norm_probe(line), view));
    for (const std::string_view line : {
             std::string_view{"<< end own=failure;counted=success"},
             std::string_view{"<< end own=failure]"},
             std::string_view{"<< end counted=success]"},
         })
        EXPECT_EQ(recognize_end(norm_probe(line), view).kind, IntentMarkerKind::None)
            << "\"" << line
            << "\" matched the ending row: " << show(recognize_end(norm_probe(line), view));
}

// refs: DN-162.D4, DN-162.D8
// invariant: an ending row yields the ended unit's two declared tokens verbatim, keyed by its own
// fields in whatever order the line prints them; `recognize` never returns it.
TEST(InnerStepGrammar, AnEndingRowYieldsBothDeclaredOutcomeTokens)
{
    const ComposedSemantics view{view_of("marked")};
    const UnitEnd ended{
        recognize_end(norm_probe("<< end key=root/x;counted=success;own=failure;ms=12]"), view)};
    EXPECT_EQ(ended.kind, IntentMarkerKind::InnerStep) << show(ended);
    EXPECT_EQ(ended.outcome, "failure") << show(ended);
    EXPECT_EQ(ended.propagated_outcome, "success") << show(ended);
    EXPECT_EQ(recognize(norm_probe("<< end own=failure;counted=success]"), view).kind,
              IntentMarkerKind::None)
        << "`recognize` returned an ending row";
    EXPECT_EQ(recognize_end(norm_probe(">> begin title=T;key=root/x]"), view).kind,
              IntentMarkerKind::None)
        << "`recognize_end` returned a naming row";
}

// refs: DN-162.D1, DN-162.D7
// invariant: a fired row reports its revision and its RANK in its package's ordered span, and the
// ranks stay parallel to the rows through a view that drops a row before them.
TEST(InnerStepGrammar, AFiredRowReportsItsRevisionAndRankThroughAFilteredView)
{
    for (const std::string_view channel : {std::string_view{"marked"}, std::string_view{}})
    {
        const ComposedSemantics view{view_of(channel)};
        const IntentMarker opened{recognize(norm_probe(">> begin title=T;key=a]"), view)};
        EXPECT_EQ(opened.revision_since, "r2") << "channel \"" << channel << "\": " << show(opened);
        EXPECT_EQ(opened.revision_rank, 1U) << "channel \"" << channel << "\": " << show(opened);
        const UnitEnd ended{recognize_end(norm_probe("<< end own=success;counted=success]"), view)};
        EXPECT_EQ(ended.revision_since, "r2") << "channel \"" << channel << "\": " << show(ended);
        EXPECT_EQ(ended.revision_rank, 1U) << "channel \"" << channel << "\": " << show(ended);
    }
    const IntentMarker step{recognize(norm_probe("== step build"), view_of("marked"))};
    EXPECT_EQ(step.kind, IntentMarkerKind::Step) << show(step);
    EXPECT_EQ(step.revision_since, "r1") << show(step);
    EXPECT_EQ(step.revision_rank, 0U) << show(step);
}

// refs: DN-162.D4, ADR-17.D6
// invariant: the dual renders a line its extractor reads back: the naming row's name and display,
// the ending row's two tokens.
TEST(InnerStepGrammar, TheKeyedDualRoundTrips)
{
    const ComposedSemantics view{view_of("marked")};
    const std::string named{insight::semantic::render_row(kEmits[1], "probe")};
    EXPECT_EQ(named, ">> begin key=probe;title=probe]");
    const IntentMarker got{recognize(norm_probe(named), view)};
    EXPECT_EQ(got.kind, IntentMarkerKind::InnerStep) << named << " -> " << show(got);
    EXPECT_EQ(got.name, "probe") << named << " -> " << show(got);
    const std::string ending{insight::semantic::render_row(kEmits[2], "probe")};
    EXPECT_EQ(ending, "<< end own=probe;counted=probe]");
    const UnitEnd ended{recognize_end(norm_probe(ending), view)};
    EXPECT_EQ(ended.kind, IntentMarkerKind::InnerStep) << ending << " -> " << show(ended);
    EXPECT_EQ(ended.outcome, "probe") << show(ended);
    EXPECT_EQ(ended.propagated_outcome, "probe") << show(ended);
}

// refs: DN-162.D4, ADR-17.D6
// invariant: role Ends is legal on kind InnerStep with the exact ending set; an inner step is read
// by KeyedFields alone; a field role on any other row is refused.
TEST(InnerStepGrammar, CompositionAdmitsAnInnerStepRowOnlyInItsExactShape)
{
    EXPECT_TRUE(unit_role_row_admitted(kMarkers[1]));
    EXPECT_TRUE(unit_role_row_admitted(kMarkers[2]));
    IntentMarkerRow ends_off_inner{kMarkers[2]};
    ends_off_inner.kind = IntentMarkerKind::Step;
    EXPECT_FALSE(unit_role_row_admitted(ends_off_inner)) << "role Ends on kind Step was admitted";
    IntentMarkerRow ends_one_token{kMarkers[2]};
    ends_one_token.fields.propagated_outcome = {};
    EXPECT_FALSE(unit_role_row_admitted(ends_one_token))
        << "an Ends row reading its own outcome alone was admitted";
    IntentMarkerRow ends_one_key{kMarkers[2]};
    ends_one_key.fields.propagated_outcome = ends_one_key.fields.outcome;
    EXPECT_FALSE(unit_role_row_admitted(ends_one_key))
        << "an Ends row reading both tokens from one key was admitted";
    IntentMarkerRow naming_short{kMarkers[1]};
    naming_short.fields.anonymous = {};
    EXPECT_FALSE(unit_role_row_admitted(naming_short))
        << "a naming row without its anonymous literal was admitted";
    IntentMarkerRow inner_unkeyed{kMarkers[1]};
    inner_unkeyed.extract = PayloadExtract::RemainderAfterPrefix;
    inner_unkeyed.fields = {};
    EXPECT_FALSE(unit_role_row_admitted(inner_unkeyed))
        << "an InnerStep row read by another extractor was admitted";
    IntentMarkerRow step_with_fields{kMarkers[0]};
    step_with_fields.fields = kNaming;
    EXPECT_FALSE(unit_role_row_admitted(step_with_fields))
        << "a Step row declaring field roles was admitted";
    IntentMarkerRow keyed_step{kMarkers[1]};
    keyed_step.kind = IntentMarkerKind::Step;
    EXPECT_FALSE(unit_role_row_admitted(keyed_step)) << "a KeyedFields Step row was admitted";

    static constexpr std::array<IntentMarkerRow, 1> kRefused{
        {{.prefix = "<< end ",
          .kind = IntentMarkerKind::Step,
          .extract = PayloadExtract::KeyedFields,
          .role = MarkerRole::Ends,
          .fields = kEnding}}};
    static constexpr SemanticPackageManifest kRefusedManifest{
        .name = "synth_refused", .version = "1.0.0", .markers = kRefused};
    EXPECT_DEATH(
        {
            const std::array manifests{kRefusedManifest};
            (void)compose(manifests);
        },
        "role Ends, legal on kind InnerStep only");
}

// refs: DN-162.D1, ADR-17.D9
// invariant: the trailing ownership section is written only above one revision: a singleton's rows
// move nothing by naming an owner, and a multi-revision row's owner moves the identity.
// invariant: a KeyedFields row's field role moves the identity too.
TEST(InnerStepGrammar, TheIdentityCarriesOwnersAboveOneRevisionAndTheFieldRoles)
{
    EXPECT_EQ(identity_of(kSingleton), identity_of(kSingletonOwned))
        << "a single-revision package's preimage took owner bytes: every existing package's "
           "identity would move with the section";

    static constexpr auto kMoved{[]
                                 {
                                     auto rows{kMarkers};
                                     rows[1].revision_since = "r1";
                                     return rows;
                                 }()};
    static constexpr auto kMovedEmits{[]
                                      {
                                          auto rows{kEmits};
                                          rows[1].revision_since = "r1";
                                          return rows;
                                      }()};
    static constexpr SemanticPackageManifest kMovedManifest{.name = kSynth,
                                                            .version = "1.0.0",
                                                            .markers = kMoved,
                                                            .emits = kMovedEmits,
                                                            .channels = kChannels,
                                                            .dialect_revisions = kRevisions};
    EXPECT_NE(identity_of(kManifest), identity_of(kMovedManifest))
        << "moving a row's owner from r2 to r1 did not move the identity";

    static constexpr auto kResegmented{[]
                                       {
                                           auto rows{kMarkers};
                                           rows[1].fields.segment = ".";
                                           return rows;
                                       }()};
    static constexpr auto kResegmentedEmits{[]
                                            {
                                                auto rows{kEmits};
                                                rows[1].fields.segment = ".";
                                                return rows;
                                            }()};
    static constexpr SemanticPackageManifest kResegmentedManifest{.name = kSynth,
                                                                  .version = "1.0.0",
                                                                  .markers = kResegmented,
                                                                  .emits = kResegmentedEmits,
                                                                  .channels = kChannels,
                                                                  .dialect_revisions = kRevisions};
    EXPECT_NE(identity_of(kManifest), identity_of(kResegmentedManifest))
        << "changing a KeyedFields row's segment literal did not move the identity";
}
