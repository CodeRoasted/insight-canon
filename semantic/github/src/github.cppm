// insight.semantic.github — the GitHub Actions / Azure Pipelines dialect semantic package
// (ADR 0024). VOCABULARY as DATA in the closed semantic-grammar-1 (structural roles, intent markers,
// level lifts) + the CODE tier (the dialect format strategy + the echoed-source provenance hook).
// Fully self-contained: imports only insight.canon.api (types + utils) + insight.canon.spi (the
// provider contract) — never a sealed detail shard. The composition (insight::semantic::compose)
// statically links this package's kManifest into a binary.
//
// `export import insight.canon.spi` so a consumer that imports this module can name
// SemanticPackageManifest (the type of kManifest) without a separate spi import.
module;

export module insight.semantic.github;
import insight.canon.internal; // std + global C fixed-width types
import insight.canon.api;      // StructuralRole, LogLevel, LogFormat, IntentMarkerKind, ChildOrder
export import insight.canon.spi;

namespace insight::semantic::github
{

// ── The code-tier seams (defined in github_strategy.cpp, this module's impl unit) ──
// The dialect format strategy factory (matches spi::StrategyFactory) and the echoed-source raw-line
// provenance hook (matches spi::ProvenanceHook — pure bool(string_view) noexcept). Declared here so
// kManifest can take their addresses; kManifest is the module's single export.
export std::unique_ptr<insight::tokenization::IFormatStrategy> make_strategy();
export bool is_echoed_source(std::string_view raw_line) noexcept;

// ── Structural-role rows (§1.2) ──
// The announced GitHub-Actions/Azure markers. format_gate = kAnyFormat: these fire regardless of the
// routed format, reproducing the pre-split UNGATED StructuralRoleRegistry::classify EXACTLY (a
// `##[group]` on a RawText CI line still classifies — byte-identity, G-SP-1).
inline constexpr std::array<StructuralRoleRow, 6> kRoles{{
    {.prefix = "##[group]", .role = insight::StructuralRole::GroupBegin, .format_gate = kAnyFormat},
    {.prefix = "::group::", .role = insight::StructuralRole::GroupBegin, .format_gate = kAnyFormat},
    {.prefix = "##[endgroup]", .role = insight::StructuralRole::GroupEnd, .format_gate = kAnyFormat},
    {.prefix = "::endgroup::", .role = insight::StructuralRole::GroupEnd, .format_gate = kAnyFormat},
    {.prefix = "##[error]", .role = insight::StructuralRole::Terminator, .format_gate = kAnyFormat},
    {.prefix = "::error::", .role = insight::StructuralRole::Terminator, .format_gate = kAnyFormat},
}};

// ── Intent-marker rows (§1.2/§2.2) ──
// FORMAT-GATED to GitHubActions (II-6 — `Run ` is GHA-runner-specific and would misfire elsewhere).
// The hierarchy rides the rows: Job = Unordered (jobs parallel-by-construction), Step = Ordered (steps
// sequential-by-YAML) — the ADR 0023 level-typed alignment declaration. The payload is the content
// after the prefix, verbatim (core's canonicalize_intent/discriminant_of derive the class + instance).
//
// A step banner appears in TWO materializations of the same GHA stream: the runner's raw form wraps
// it as `##[group]Run <cmd>`; the §5.3 workflow-command strip (crawl corpus) exposes the bare
// `Run <cmd>`. Both are corpus-attested (bare in the stripped slices, wrapped in every raw job log;
// `::group::Run ` is NOT attested — the runner rewrites `::` to `##[…]` — so no such row). Shipping
// BOTH prefixes with RemainderAfterPrefix makes the recognized step name materialization-INVARIANT
// (`##[group]Run yarn lint` and `Run yarn lint` both yield payload `yarn lint`) — so the stripped and
// raw streams segment identically, which is what dissolves the "which form did Sift get fed" hazard
// without any binary-side ingest strip (the strip is dialect grammar, and lives here, not in an
// adapter — ADR 0024 §3, II-8 one-recognizer-layer). The two Step prefixes never shadow: neither is a
// proper prefix of the other, so compose's longest-match note stays empty.
inline constexpr std::array<IntentMarkerRow, 3> kMarkers{{
    {.prefix = "Complete job name: ",
     .kind = insight::tokenization::IntentMarkerKind::Job,
     .child_order = insight::tokenization::ChildOrder::Unordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .extract = PayloadExtract::RemainderAfterPrefix},
    {.prefix = "Run ",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .extract = PayloadExtract::RemainderAfterPrefix},
    {.prefix = "##[group]Run ", // the raw (un-stripped) materialization of the same step banner
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .extract = PayloadExtract::RemainderAfterPrefix},
}};

// ── Generation-template rows (studies/008, shared_intent_declaration §3.2) — the WRITER dual ──
// One emit row per recognition row, paired by (prefix, kind, format_gate). Both Step media are present
// (the O2 medium axis): `Run <cmd>` is the stripped-command materialization, `##[group]Run <cmd>` the
// runner-wrapped one — one Step intent, two media, canon reads both to the same identity, so the writer
// generates either and the round-trip closes (G2). Each emit is PayloadAfterPrefix, the exact inverse
// of the reader's RemainderAfterPrefix: render_row(row, "yarn lint") reproduces the banner canon
// segments back to Step "yarn lint".
inline constexpr std::array<IntentEmitRow, 3> kEmitMarkers{{
    {.prefix = "Complete job name: ",
     .kind = insight::tokenization::IntentMarkerKind::Job,
     .child_order = insight::tokenization::ChildOrder::Unordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .emit = PayloadEmit::PayloadAfterPrefix},
    {.prefix = "Run ",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .emit = PayloadEmit::PayloadAfterPrefix},
    {.prefix = "##[group]Run ", // the runner-wrapped medium of the same step banner
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .emit = PayloadEmit::PayloadAfterPrefix},
}};

// The C2 bidirectionality obligation: this dialect exposes BOTH projections, and every recognition row
// is paired with a generation row. DialectIntent fails to compile if a reader ships without a writer.
export struct Dialect
{
    static constexpr std::span<const IntentMarkerRow> markers{kMarkers};
    static constexpr std::span<const IntentEmitRow> emit_markers{kEmitMarkers};
};
static_assert(insight::semantic::DialectIntent<Dialect>,
              "github: a recognition marker has no paired generation row (reader without a writer)");

// ── Level-lift rows (§1.2) ──
// The GHA workflow-command level lift. FORMAT-GATED to GitHubActions; consumed by THIS package's
// dialect strategy (level_from_message walks these, inside parse(), before raw-text inference — the
// pre-split ordering, byte-identical). Also serialized into semantic_identity.
inline constexpr std::array<LevelLiftRow, 8> kLevelLifts{{
    {.prefix = "##[error]", .level = insight::LogLevel::Error, .format_gate = insight::LogFormat::GitHubActions},
    {.prefix = "::error::", .level = insight::LogLevel::Error, .format_gate = insight::LogFormat::GitHubActions},
    {.prefix = "##[warning]", .level = insight::LogLevel::Warn, .format_gate = insight::LogFormat::GitHubActions},
    {.prefix = "::warning::", .level = insight::LogLevel::Warn, .format_gate = insight::LogFormat::GitHubActions},
    {.prefix = "##[debug]", .level = insight::LogLevel::Debug, .format_gate = insight::LogFormat::GitHubActions},
    {.prefix = "::debug::", .level = insight::LogLevel::Debug, .format_gate = insight::LogFormat::GitHubActions},
    {.prefix = "##[notice]", .level = insight::LogLevel::Info, .format_gate = insight::LogFormat::GitHubActions},
    {.prefix = "::notice::", .level = insight::LogLevel::Info, .format_gate = insight::LogFormat::GitHubActions},
}};

// ── Run-outcome token rows (grammar-2, ADR 0025 §4 — the GitHub reshape) ──
// The GHA native job/run conclusion strings (`${{ needs.<job>.result }}` / the API `conclusion`),
// mapped into the core four-class vocabulary. This RETIRES the render-side `deriveBuildStatus`
// binary at its root: the verdict now enters the ENGINE four-class-aware through the same channel
// as Jenkins (the Action forwards the native token to `sift --*-outcome`; Argos rewires the JS).
// GHA has no native UNSTABLE string today — the category stays core, this dialect simply ships no
// row for it. skipped/neutral/action_required carry no pass↔fail verdict → Unknown (honest, never
// a guess). NO OutcomeMarkerRow: GHA emits no single run-verdict console line (`Process completed
// with exit code N` is per-step) — the degenerate console path is correctly Unknown (§3.2).
inline constexpr std::array<OutcomeTokenRow, 7> kOutcomeTokens{{
    {.token = "success", .outcome = insight::RunOutcome::Success, .format_gate = insight::LogFormat::GitHubActions},
    {.token = "failure", .outcome = insight::RunOutcome::Failure, .format_gate = insight::LogFormat::GitHubActions},
    {.token = "cancelled", .outcome = insight::RunOutcome::Aborted, .format_gate = insight::LogFormat::GitHubActions},
    {.token = "timed_out", .outcome = insight::RunOutcome::Aborted, .format_gate = insight::LogFormat::GitHubActions},
    {.token = "skipped", .outcome = insight::RunOutcome::Unknown, .format_gate = insight::LogFormat::GitHubActions},
    {.token = "neutral", .outcome = insight::RunOutcome::Unknown, .format_gate = insight::LogFormat::GitHubActions},
    {.token = "action_required", .outcome = insight::RunOutcome::Unknown, .format_gate = insight::LogFormat::GitHubActions},
}};

// ── The manifest (§2.5) — the package's single composed contribution ──
// name "github", version "1.2.0" — bumped from 1.1.0 for the grammar-2 outcome-token rows (SP-7
// immutable-release discipline: a released version's rows are frozen, a content change is a new
// version; the bump also rides the II-7 semantic_identity hash, an honest comparability boundary).
// Ships no locations (that is the test_frameworks package) and no value classes (none has a consumer
// yet). Code tier: the dialect strategy + the echoed-source hook.
export inline constexpr SemanticPackageManifest kManifest{
    .name = "github",
    .version = "1.2.0",
    .roles = kRoles,
    .markers = kMarkers,
    .level_lifts = kLevelLifts,
    .locations = {},
    .value_classes = {},
    .outcome_tokens = kOutcomeTokens,
    .outcome_markers = {},
    .strategy = &make_strategy,
    .echoed_source = &is_echoed_source,
};

} // namespace insight::semantic::github
