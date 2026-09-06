// refs: ADR-17, ADR-22, ADR-23.D1, STU-12
// invariant: VOCABULARY as data (intent rows + run-outcome rows) plus ONE code tier, the format
// strategy; imports are canon's api and spi only, never a sealed detail shard.
// assert: the recall claim is cut on the STAMPED axis — the 32-byte runner prefix is present:
// stamped 3193/3231 = 98.8 %, unstamped 294/1054 = 27.9 %.
// assert: re-cut on the runner-BANNER axis (>= 18.9) it holds at 2963/3001 = 98.7 %: every
// banner-modern trace is stamped, and 37 stamped traces carry an old banner.
// invariant: the unstamped leg's loss would be lifted by splitting lines on `\r`, which is line
// DELIMITATION — delivery, not vocabulary — so it belongs to the transport axis, never here.
// note: three populations wear the words "modern leg", so every figure here names its axis
module;

export module insight.semantic.gitlab;
import insight.canon.internal;
import insight.canon.api;
export import insight.canon.spi;

namespace insight::semantic::gitlab
{

export std::unique_ptr<insight::tokenization::IFormatStrategy> make_strategy();

// refs: ADR-22.D6
// invariant: one string is both what a caller declares and what every gated row below carries, so
// canon knows the field and knows no value — a package NAME, never an enum.
export inline constexpr std::string_view kDialect{"gitlab"};

// refs: SRC-II-6, STU-12
// invariant: NO `payload_excludes` — a CLOSED exclusion set cannot name the runner's OPEN
// `step_*` family (579 depth-1 occurrences), so it would mirror the producer and rot.
// assert: a genuine marker sits at offset 0 of the peeled, ANSI-stripped content and `recognize`
// matches with `starts_with`, so ANCHORING is the whole anti-phantom guard.
// invariant: no close kind and an open-marker fold, so a nested section's tail is attributed to its
// last child — 347 of 4 285 starts, across 94 of the 619 content-bearing traces.
inline constexpr std::array<IntentMarkerRow, 1> kMarkers{{
    {.prefix = "section_start:",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .dialect_gate = kDialect,
     .extract = PayloadExtract::NumericFieldThenRemainder},
}};

// post: `recognize(render_row(row, p))` recovers `p` exactly — the emit shape renders one
// PLACEHOLDER digit where the producer's epoch sits.
// invariant: so a generated marker carries no wall-clock and therefore no section duration; a
// VARYING stamp would be a step_duration capability, not a package detail.
inline constexpr std::array<IntentEmitRow, 1> kEmitMarkers{{
    {.prefix = "section_start:",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .dialect_gate = kDialect,
     .emit = PayloadEmit::PlaceholderNumericFieldThenPayload},
}};

// refs: ADR-18.D4
export struct Dialect
{
    static constexpr std::span<const IntentMarkerRow> markers{kMarkers};
    static constexpr std::span<const IntentEmitRow> emit_markers{kEmitMarkers};
};
static_assert(
    insight::semantic::DialectIntent<Dialect>,
    "gitlab: a recognition marker has no paired generation row (reader without a writer)");

// invariant: `skipped` and `manual` map to Unknown as EXPLICIT rows — a known token carrying no
// verdict, where an ABSENT row would raise a fail-closed note about a token this dialect defines.
inline constexpr std::array<OutcomeTokenRow, 5> kOutcomeTokens{{
    {.token = "success", .outcome = insight::RunOutcome::Success, .dialect_gate = kDialect},
    {.token = "failed", .outcome = insight::RunOutcome::Failure, .dialect_gate = kDialect},
    {.token = "canceled", .outcome = insight::RunOutcome::Aborted, .dialect_gate = kDialect},
    {.token = "skipped", .outcome = insight::RunOutcome::Unknown, .dialect_gate = kDialect},
    {.token = "manual", .outcome = insight::RunOutcome::Unknown, .dialect_gate = kDialect},
}};

// refs: ADR-17, SRC-D-OUT-RUN-1
// assert: all three rows resolve by LONGEST PREFIX, never by array order: the cancel row is a
// strict extension of the failure row, and it matches on 17 of the 25 cancelled jobs.
// invariant: the console tail is the DEGENERATE fallback and the API result is authoritative; the
// divergence it exists for is measured — 2 of the 25 cancelled jobs end on `Job succeeded`.
// note: the third row is MEASURED, not symmetry: GitLab announces a cancel with the failure prefix
inline constexpr std::array<OutcomeMarkerRow, 3> kOutcomeMarkers{{
    {.prefix = "Job succeeded",
     .dialect_gate = kDialect,
     .shape = OutcomeMarkerShape::PrefixIsVerdict,
     .outcome = insight::RunOutcome::Success},
    {.prefix = "ERROR: Job failed",
     .dialect_gate = kDialect,
     .shape = OutcomeMarkerShape::PrefixIsVerdict,
     .outcome = insight::RunOutcome::Failure},
    {.prefix = "ERROR: Job failed: canceled",
     .dialect_gate = kDialect,
     .shape = OutcomeMarkerShape::PrefixIsVerdict,
     .outcome = insight::RunOutcome::Aborted},
}};

// refs: ADR-17.D9, ADR-22.D8
// invariant: this names the VENDOR syntax generation the rows recognize and moves when GITLAB ships
// a new one; `.version` above moves when WE edit the ruleset.
export inline constexpr std::array<std::string_view, 1> kDialectRevisions{{"v1"}};

// refs: SRC-SP-7, ADR-23, ADR-22
// invariant: `.version` is immutable-release discipline, `.emits` makes the generation projection
// identity-bearing, and `.channels = {}` is the degenerate kAnyChannel case.
// note: no `echoed_source` hook is needed: GitLab's marker phantom dies to anchoring alone
export inline constexpr SemanticPackageManifest kManifest{
    .name = "gitlab",
    .version = "1.0.0",
    .roles = {},
    .markers = kMarkers,
    .emits = kEmitMarkers,
    .level_lifts = {},
    .locations = {},
    .value_classes = {},
    .outcome_tokens = kOutcomeTokens,
    .outcome_markers = kOutcomeMarkers,
    .channels = {},
    .dialect_revisions = kDialectRevisions,
    .strategy = &make_strategy,
    .echoed_source = nullptr,
};

// refs: ADR-22
// assert: every gated row names THIS package or kAnyDialect, at COMPILE time and here — a typo
// would ship a row that silently never fires under any declaration.
static_assert(insight::semantic::all_dialect_gates_owned(kManifest),
              "gitlab: a row's dialect_gate is neither kAnyDialect nor this package's own name (a "
              "typo in .dialect_gate, or a row reaching for another package's vocabulary?)");
static_assert(kManifest.name == kDialect,
              "gitlab: kDialect and the manifest name must be the same string — kDialect is what a "
              "caller declares and what every gated row carries");

// refs: ADR-17.D9
static_assert(insight::semantic::all_revisions_named(kDialectRevisions),
              "gitlab: the declared dialect-revision vocabulary must be non-empty, with unique, "
              "non-empty names (grammar-6 — the coordinate is what a reader compares generations "
              "on, so an unnamed or repeated one is not a declaration)");

} // namespace insight::semantic::gitlab
