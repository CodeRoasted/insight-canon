// insight.semantic.gitlab — the GitLab CI job-trace dialect semantic package (ADR 0024/0025/0069,
// studies/012). VOCABULARY as DATA in the closed canon rule grammar (intent markers + run-outcome
// rows) + the CODE tier (the dialect format strategy). Fully self-contained: imports only
// insight.canon.api (types) + insight.canon.spi (the provider contract) — never a sealed detail
// shard. The composition (insight::semantic::compose) statically links this package's kManifest
// into a binary.
//
// The depth claim this vocabulary carries is scoped to the MODERN leg — GitLab runner >= 18.9, the
// generation whose own stamper splits a packed `\r` line into one line per marker. Pre-18.8 output
// packs several markers onto one `\n`-line separated by `\r`, canon splits lines on `\n` only, and
// `recognize()` returns one marker per line — so a line-anchored row sees the leading `section_end:`
// and the `section_start:` behind it is invisible. Measured recall over marker_corpus_v1: modern
// 3193/3231 = 98.8%, old 294/1054 = 27.9%. Lifting the old leg means splitting lines on `\r`, which
// is line DELIMITATION — delivery, not vocabulary — and belongs to the transport axis, never here.
//
// Ships NO structural-role / level-lift / location / value-class rows and NO channel vocabulary:
// GitLab has one materialization (the degenerate kAnyChannel case, ADR 0029 D5), and studies/012
// surfaced no role or location vocabulary — we do not build dormant rows (rip-dormant discipline).
// No `echoed_source` hook either: GitLab's marker phantom is killed by ANCHORING alone (see
// kMarkers).
module;

export module insight.semantic.gitlab;
import insight.canon.internal; // std + global C fixed-width types
import insight.canon.api;      // LogFormat, RunOutcome, IntentMarkerKind, ChildOrder
export import insight.canon.spi;

namespace insight::semantic::gitlab
{

// ── The code-tier seam (defined in gitlab_strategy.cpp, this module's impl unit) ──
// The dialect format strategy factory (matches spi::StrategyFactory).
export std::unique_ptr<insight::tokenization::IFormatStrategy> make_strategy();

// The dialect NAME every gated row below carries, and the name a caller declares
// (`IngestDeclaration::dialect` / `--dialect`). ADR 0065 clause 1: the gate is a composed package
// name, never an enum, so canon knows the field and knows no value.
export inline constexpr std::string_view kDialect{"gitlab"};

// ── Intent-marker rows ──
// ONE row, ONE level, flat. DIALECT-GATED to this package (II-6 — `section_start:` is
// GitLab-runner-specific).
//
// `Step`, not `Job`, and the FOLD is why: eidos treats a `Job` marker as opening a new job node and
// immediately opens a synthetic setup quantum under it, while a `Step` marker opens a quantum under
// whatever job is current. A GitLab trace IS one job and its job identity is exogenous — there is no
// job banner anywhere in a trace (studies/012 G-GL-P6). Mapping sections to `Job` would mint one
// "job" per runner phase (`prepare_executor` as a job) and leave every one of them step-less.
// Mapping to `Step` yields one implicit job whose steps are its phases, which is what the bytes say;
// the content before the first section falls into the existing preamble quantum, which is correct —
// it is the runner banner block.
//
// `Ordered`, not `Unordered`: GitLab job phases are strictly sequential by construction of the
// runner state machine — one runner, one job, one phase at a time. Unlike GHA matrix jobs and
// Jenkins parallel branches they never co-occur, so a transposition IS a signal.
//
// `section_end:` is NOT a row. `IntentMarkerKind` has no close kind and the fold is open-marker
// driven, so a section's quantum runs until the next section opens. For the 92.9% of starts at
// depth 1 that is exact; for a nested section the parent's tail is attributed to the last child, a
// declared limitation (303 of 4285 starts, 94 of 619 traces).
//
// NO `payload_excludes`, and the reason is a rot argument rather than a taste one. Section names
// split by depth almost perfectly — depth 1 is the runner's own phase vocabulary (99.7%), depth >=2
// is user sections (91.7%) — but the only in-grammar discriminator is a CLOSED exclusion set, and
// the runner vocabulary contains the OPEN `step_*` family (579 depth-1 occurrences, `step_script`
// and `step_release` observed). Enumerating an open family in an exclusion list is a mirror of the
// producer's source that can only rot.
//
// The ANTI-PHANTOM GUARD IS POSITION, NOT THE STAMP. GitLab scripts echo their own markers — the
// runner's command echo and bash `set -x` xtrace — and the xtrace form carries a fully-expanded,
// strictly-VALID stamp, so a stamp-shape guard does not reject it. What rejects it is that a genuine
// marker sits at offset 0 of the peeled, ANSI-stripped content while an echoed one is preceded by
// literal ASCII `++ echo -e '\e[0K`. `recognize()` matches with `starts_with`, so the guard is free:
// 8486 of 8545 corpus markers are segment-leading, and 23 of the 59 exceptions are exactly this echo
// class, correctly excluded.
inline constexpr std::array<IntentMarkerRow, 1> kMarkers{{
    {.prefix = "section_start:",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .dialect_gate = kDialect,
     .extract = PayloadExtract::NumericFieldThenRemainder},
}};

// ── Generation-template rows — the WRITER dual ──
// One emit row per recognition row, paired by (prefix, kind, dialect_gate). The emit shape renders
// `section_start:0:<payload>` — a single PLACEHOLDER digit — so `recognize(render_row(row, p))`
// recovers `p` exactly and the G2 round-trip closes.
//
// The placeholder's consequence is declared, not hidden: a LogCraft-generated GitLab marker carries
// no wall-clock and therefore no section duration. Making the writer emit a VARYING stamp is a
// step_duration capability, not a package detail.
inline constexpr std::array<IntentEmitRow, 1> kEmitMarkers{{
    {.prefix = "section_start:",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .dialect_gate = kDialect,
     .emit = PayloadEmit::PlaceholderNumericFieldThenPayload},
}};

// The C2 bidirectionality obligation: this dialect exposes BOTH projections, and every recognition
// row is paired with a generation row. DialectIntent fails to compile if a reader ships without a
// writer.
export struct Dialect
{
    static constexpr std::span<const IntentMarkerRow> markers{kMarkers};
    static constexpr std::span<const IntentEmitRow> emit_markers{kEmitMarkers};
};
static_assert(
    insight::semantic::DialectIntent<Dialect>,
    "gitlab: a recognition marker has no paired generation row (reader without a writer)");

// ── Run-outcome rows (ADR 0025 §4) ──
// The API `status` vocabulary — what an authoritative side-input actually carries. `skipped` /
// `manual` map to Unknown DELIBERATELY, on the Jenkins NOT_BUILT precedent: an explicit row says
// "we know this token and it carries no verdict", where an absent row produces a fail-closed
// resolution note about a token the dialect does in fact define.
//
// NO `Unstable` row. GitLab has no native UNSTABLE; composing one out of `allow_failure` is a
// product decision about what a partial success MEANS, never a silent mapping in a row table.
inline constexpr std::array<OutcomeTokenRow, 5> kOutcomeTokens{{
    {.token = "success", .outcome = insight::RunOutcome::Success, .dialect_gate = kDialect},
    {.token = "failed", .outcome = insight::RunOutcome::Failure, .dialect_gate = kDialect},
    {.token = "canceled", .outcome = insight::RunOutcome::Aborted, .dialect_gate = kDialect},
    {.token = "skipped", .outcome = insight::RunOutcome::Unknown, .dialect_gate = kDialect},
    {.token = "manual", .outcome = insight::RunOutcome::Unknown, .dialect_gate = kDialect},
}};

// The console-tail rows — all three of the PrefixIsVerdict shape, because GitLab's terminal line
// carries the verdict in its PREFIX and a free-form remainder behind it (`ERROR: Job failed: exit
// code 1`, `… : exit status 137`, `… (system failure): <reason>`). GitLab ships NO remainder-token
// row: no GitLab terminal line has a single-word remainder.
//
// THE THIRD ROW IS A MEASURED FINDING, not symmetry. GitLab announces a CANCELLATION with the
// FAILURE prefix — `ERROR: Job failed: canceled`, 17 of the 25 cancelled jobs in marker_corpus_v1.
// A Jenkins-shaped row set reads all 17 as Failure: a WRONG verdict, not a missing one. The row is a
// strict extension of the failure row and is resolved by LONGEST PREFIX, never by array order — the
// grammar-5 tie-break exists precisely so this verdict does not depend on where these three rows sit
// in this array.
//
// The console tail stays the DEGENERATE fallback; the API result is authoritative (adr/0025,
// D-OUT-RUN-1). Measured divergence exists and is exactly what that precedence is for: 2 cancelled
// jobs end on `Job succeeded`.
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

// ── The manifest (§2.5) — the package's single composed contribution ──
// name "gitlab", version "1.0.0" (SP-7 immutable-release discipline).
export inline constexpr SemanticPackageManifest kManifest{
    .name = "gitlab",
    .version = "1.0.0",
    .roles = {},
    .markers = kMarkers,
    .emits = kEmitMarkers, // ADR 0044 §7 — the generation projection is identity-bearing
    .level_lifts = {},
    .locations = {},
    .value_classes = {},
    .outcome_tokens = kOutcomeTokens,
    .outcome_markers = kOutcomeMarkers,
    .channels = {}, // one materialization — the degenerate kAnyChannel case (ADR 0029 D5)
    .strategy = &make_strategy,
    .echoed_source = nullptr,
};

// ADR 0065 clause 1 — every gated row names THIS package or kAnyDialect, checked at COMPILE time,
// here. A gate naming another package would reach across a boundary this package does not own, and
// a typo would produce a row that silently never fires under any declaration.
static_assert(insight::semantic::all_dialect_gates_owned(kManifest),
              "gitlab: a row's dialect_gate is neither kAnyDialect nor this package's own name (a "
              "typo in .dialect_gate, or a row reaching for another package's vocabulary?)");
static_assert(kManifest.name == kDialect,
              "gitlab: kDialect and the manifest name must be the same string — kDialect is what a "
              "caller declares and what every gated row carries");

} // namespace insight::semantic::gitlab
