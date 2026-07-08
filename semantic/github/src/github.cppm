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
inline constexpr std::array<IntentMarkerRow, 2> kMarkers{{
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
}};

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

// ── The manifest (§2.5) — the package's single composed contribution ──
// name "github", version "1.0.0" (SP-7 immutable-release discipline). Ships no locations (that is
// the test_frameworks package) and no value classes (none has a consumer in 1.7.5). Code tier: the
// dialect strategy + the echoed-source hook.
export inline constexpr SemanticPackageManifest kManifest{
    .name = "github",
    .version = "1.0.0",
    .roles = kRoles,
    .markers = kMarkers,
    .level_lifts = kLevelLifts,
    .locations = {},
    .value_classes = {},
    .strategy = &make_strategy,
    .echoed_source = &is_echoed_source,
};

} // namespace insight::semantic::github
