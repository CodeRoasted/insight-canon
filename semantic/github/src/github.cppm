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

// ── The declared SINK vocabulary (ADR 0028 D1) ──
// `Medium = IntentFormat × Sink`. GHA is ONE IntentFormat with TWO Sinks — the same job⊃step intent
// materialized into two output environments:
//
//   annotated — the runner's raw job log (what the shipped Action fetches via
//               downloadJobLogsForWorkflowRun): step banners are `##[group]Run <cmd>`, and the
//               workflow-command markers (`##[error]`, `##[group]`) are present.
//   stripped  — the §5.3 workflow-command-stripped form (the crawl corpus): step banners are the bare
//               `Run <cmd>`, and no `##[…]` marker survives.
//
// MEASURED, and the partition is clean (22 030 real annotated/stripped pairs): the annotated form NEVER
// uses bare `Run ` as a banner, and the stripped form never contains `##[`. `::group::Run ` is not
// attested — the runner rewrites `::` to `##[…]` — so there is no third Sink and no both-variants-in-
// one-Sink case to model.
// EXPORTED: a package's Sink vocabulary is part of its public declaration. Every caller that acquires a
// GHA stream must name the Sink it acquired (ADR 0028 D2 — the Sink is caller-declared provenance), so
// the names have to be nameable: the CLI validates `--sink` against them, the Action declares
// `annotated` because it chose downloadJobLogsForWorkflowRun, and a test says which form it fixtures.
export inline constexpr std::string_view kSinkAnnotated{"annotated"};
export inline constexpr std::string_view kSinkStripped{"stripped"};
export inline constexpr std::array<std::string_view, 2> kSinks{{kSinkAnnotated, kSinkStripped}};

// ── Intent-marker rows (§1.2/§2.2) ──
// FORMAT-GATED to GitHubActions (II-6 — `Run ` is GHA-runner-specific and would misfire elsewhere).
// The hierarchy rides the rows: Job = Unordered (jobs parallel-by-construction), Step = Ordered (steps
// sequential-by-YAML) — the ADR 0023 level-typed alignment declaration. The payload is the content
// after the prefix, verbatim (core's canonicalize_intent/discriminant_of derive the class + instance).
//
// SINK-GATED per Step (ADR 0028 D1 — this is the phantom fix, and it REPLACES the reasoning that used
// to sit here). The two Step prefixes are the same intent in two Sinks, so each gates to ITS Sink:
//
//   `Run `         → stripped   — REQUIRED. In the ANNOTATED Sink a line starting with `Run ` is
//                                 ordinary PROSE, so an ungated row mints a phantom Step quantum out of
//                                 it. Measured: 9.05 % of 22 030 real annotated logs, 7 752 lines, 62
//                                 distinct payloads, EVERY ONE prose (`` `npm audit` for details. ``
//                                 dominates). Confirmed end-to-end to fabricate a VanishedPhase.
//   `##[group]Run ` → annotated — self-gating in principle (`##[` never occurs in a stripped log), but
//                                 gated anyway so the declared Sink selects EXACTLY ONE Step row: a
//                                 multi-Sink tree becomes unrepresentable rather than merely unattested
//                                 (D3), and D5's fail-closed is symmetric across both forms.
//
// The superseded claim, recorded so it is not re-derived: shipping BOTH prefixes ungated was believed to
// make the step name "materialization-INVARIANT" and to dissolve the which-form-was-Sift-fed hazard. It
// does the opposite — invariance of the PAYLOAD is real (both yield `yarn lint`), but ungated
// recognition of the *bare* prefix in the annotated Sink is exactly the defect. Invariance is preserved
// here, by the two rows extracting the same payload under their own Sinks (G-SINK-1/G-SINK-3).
//
// `Complete job name: ` stays kAnySink: it is the banner in BOTH Sinks (the strip does not touch it), so
// it has no Sink-dependent reading and needs no gate (D1's "does not metastasize" rule).
// The two Step prefixes never shadow: neither is a proper prefix of the other, so compose's
// longest-match note stays empty.
inline constexpr std::array<IntentMarkerRow, 3> kMarkers{{
    {.prefix = "Complete job name: ",
     .kind = insight::tokenization::IntentMarkerKind::Job,
     .child_order = insight::tokenization::ChildOrder::Unordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .extract = PayloadExtract::RemainderAfterPrefix,
     .sink_gate = kAnySink}, // the job banner is identical in both Sinks
    {.prefix = "Run ",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .extract = PayloadExtract::RemainderAfterPrefix,
     .sink_gate = kSinkStripped}, // in the annotated Sink this prefix is PROSE
    {.prefix = "##[group]Run ", // the runner-wrapped materialization of the same step banner
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .extract = PayloadExtract::RemainderAfterPrefix,
     .sink_gate = kSinkAnnotated},
}};

// ── Generation-template rows (studies/008, shared_intent_declaration §3.2) — the WRITER dual ──
// One emit row per recognition row, paired by (prefix, kind, format_gate, sink_gate) — the MEDIUM is
// `IntentFormat × Sink` since ADR 0028, so each projection names the same Sink as its dual. Both Step
// media are present: `Run <cmd>` materializes into the STRIPPED Sink, `##[group]Run <cmd>` into the
// ANNOTATED one — one Step intent, two Sinks, each read back to the same identity under its own Sink,
// so the round-trip closes per Sink (G2). Each emit is PayloadAfterPrefix, the exact inverse of the
// reader's RemainderAfterPrefix: render_row(row, "yarn lint") reproduces the banner canon segments back
// to Step "yarn lint".
inline constexpr std::array<IntentEmitRow, 3> kEmitMarkers{{
    {.prefix = "Complete job name: ",
     .kind = insight::tokenization::IntentMarkerKind::Job,
     .child_order = insight::tokenization::ChildOrder::Unordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .emit = PayloadEmit::PayloadAfterPrefix,
     .sink_gate = kAnySink},
    {.prefix = "Run ",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .emit = PayloadEmit::PayloadAfterPrefix,
     .sink_gate = kSinkStripped},
    {.prefix = "##[group]Run ", // the runner-wrapped medium of the same step banner
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .format_gate = insight::LogFormat::GitHubActions,
     .emit = PayloadEmit::PayloadAfterPrefix,
     .sink_gate = kSinkAnnotated},
}};

// The C2 bidirectionality obligation: this dialect exposes BOTH projections, and every recognition row
// is paired with a generation row. DialectIntent fails to compile if a reader ships without a writer.
export struct Dialect
{
    static constexpr std::span<const IntentMarkerRow> markers{kMarkers};
    static constexpr std::span<const IntentEmitRow> emit_markers{kEmitMarkers};
};
static_assert(insight::semantic::DialectIntent<Dialect>,
              "github: a recognition marker has no paired generation row (reader without a writer), or "
              "a reader/writer pair straddles two Sinks (ADR 0028 D1 — the projections must name the "
              "same Medium)");

// ADR 0028 D1 — the Sink vocabulary and the rows that gate to it must agree, at COMPILE time, here.
static_assert(insight::semantic::all_sinks_named(kSinks),
              "github: a declared Sink name is empty — the empty name IS kAnySink, so it may not also "
              "name a concrete Sink");
static_assert(insight::semantic::all_sink_gates_declared(kMarkers, kEmitMarkers, kSinks),
              "github: a row gates to a Sink this package never declared (a typo in .sink_gate?) — the "
              "declared vocabulary is kSinks");

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
// name "github", version "1.3.0" — bumped from 1.2.0 for the ADR 0028 Sink coordinate (the declared
// Sink vocabulary + the sink-gated Step rows). SP-7 immutable-release discipline: a released version's
// rows are frozen, a content change is a new version; the bump also rides the II-7 semantic_identity
// hash, an honest comparability boundary — a diff across this boundary is comparing two different
// recognition rulesets, and the digest says so.
// Ships no locations (that is the test_frameworks package) and no value classes (none has a consumer
// yet). Code tier: the dialect strategy + the echoed-source hook.
export inline constexpr SemanticPackageManifest kManifest{
    .name = "github",
    .version = "1.3.0",
    .roles = kRoles,
    .markers = kMarkers,
    .level_lifts = kLevelLifts,
    .locations = {},
    .value_classes = {},
    .outcome_tokens = kOutcomeTokens,
    .outcome_markers = {},
    .sinks = kSinks, // ADR 0028 D1 — the two GHA materializations
    .strategy = &make_strategy,
    .echoed_source = &is_echoed_source,
};

} // namespace insight::semantic::github
