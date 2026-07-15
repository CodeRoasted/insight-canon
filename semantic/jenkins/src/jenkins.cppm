// insight.semantic.jenkins — the Jenkins Pipeline dialect semantic package (ADR 0024/0025,
// studies/006). VOCABULARY as DATA in the closed semantic-grammar-2 (intent markers + run-outcome
// rows) + the CODE tier (the dialect format strategy). Fully self-contained: imports only
// insight.canon.api (types) + insight.canon.spi (the provider contract) — never a sealed detail
// shard. The composition (insight::semantic::compose) statically links this package's kManifest
// into a binary.
//
// The rows graduate the studies/006 §Reproduction spike VERBATIM (the G1-ratified recognizers):
//   STAGE  = a NAMED `[Pipeline] { (<name>)` block open — a declared stage OR a parallel/matrix
//            `Branch:` branch (un-named `[Pipeline] {` wrappers are scaffold, not quanta);
//   STEP   = `[Pipeline] <verb>`, excluding the closed structural-token set;
//   OUTCOME = the four-class token map + the console-tail `Finished: ` epilogue (truncation-fragile
//            fallback — the API result side-input is the authoritative source, D-OUT-RUN-1).
// Ships NO structural-role / level-lift / location / value-class rows: studies/006 surfaced none —
// we do not build dormant vocabulary (rip-dormant discipline).
//
// `export import insight.canon.spi` so a consumer that imports this module can name
// SemanticPackageManifest (the type of kManifest) without a separate spi import.
module;

export module insight.semantic.jenkins;
import insight.canon.internal; // std + global C fixed-width types
import insight.canon.api;      // LogFormat, RunOutcome, IntentMarkerKind, ChildOrder
export import insight.canon.spi;

namespace insight::semantic::jenkins
{

// ── The code-tier seam (defined in jenkins_strategy.cpp, this module's impl unit) ──
// The dialect format strategy factory (matches spi::StrategyFactory). Jenkins has no echoed-source
// wrapper (that is the GHA SGR command-echo), so no provenance hook.
export std::unique_ptr<insight::tokenization::IFormatStrategy> make_strategy();

// ── Intent-marker rows (studies/006 §Reproduction, grammar-2) ──
// FORMAT-GATED to Jenkins (II-6 — `[Pipeline] ` is Jenkins-runner-specific). The hierarchy rides
// the rows (ADR 0023): STAGE is the container level (kind=Job — declared stages AND parallel/matrix
// `Branch:` legs co-occur, so the level matches UNORDERED, exactly like GHA matrix jobs); STEP is
// the leaf level (Ordered — steps are sequential within their stage). The two prefixes nest, so
// longest-VALID-match resolves a named block open to STAGE and everything else `[Pipeline] `-
// prefixed to STEP — minus the closed structural-token exclusion set (the spike's STRUCT set +
// the `// …` block-close annotations; `End of Pipeline` is the run epilogue).
inline constexpr std::array<std::string_view, 7> kStepExcludes{
    "{", "}", "stage", "node", "parallel", "//", "End of Pipeline"};

inline constexpr std::array<IntentMarkerRow, 2> kMarkers{{
    {.prefix = "[Pipeline] { (", // the NAMED block open: `{ (<name>)` — stage or Branch: leg
     .kind = insight::tokenization::IntentMarkerKind::Job,
     .child_order = insight::tokenization::ChildOrder::Unordered,
     .format_gate = insight::LogFormat::Jenkins,
     .extract = PayloadExtract::RemainderToClosingParen},
    {.prefix = "[Pipeline] ", // the step annotation: `<verb>` (sh, echo, junit, checkout, …)
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .format_gate = insight::LogFormat::Jenkins,
     .extract = PayloadExtract::RemainderAfterPrefix,
     .payload_excludes = kStepExcludes},
}};

// ── Generation-template rows (studies/008, shared_intent_declaration §3.2) — the WRITER dual ──
// One emit row per recognition row, paired by (prefix, kind, format_gate). The STAGE emit is
// PayloadThenClosingParen, the exact inverse of the reader's RemainderToClosingParen:
// render_row(stage_row, "Build") reproduces `[Pipeline] { (Build)`, which canon segments back to the
// named STAGE "Build". The STEP emit is PayloadAfterPrefix — the writer only ever emits a REAL step
// verb (`sh`, `echo`, …), never a kStepExcludes structural token, so the reader's exclusion set has no
// generation dual and the round-trip closes (G2).
inline constexpr std::array<IntentEmitRow, 2> kEmitMarkers{{
    {.prefix = "[Pipeline] { (",
     .kind = insight::tokenization::IntentMarkerKind::Job,
     .child_order = insight::tokenization::ChildOrder::Unordered,
     .format_gate = insight::LogFormat::Jenkins,
     .emit = PayloadEmit::PayloadThenClosingParen},
    {.prefix = "[Pipeline] ",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .format_gate = insight::LogFormat::Jenkins,
     .emit = PayloadEmit::PayloadAfterPrefix},
}};

// The C2 bidirectionality obligation: this dialect exposes BOTH projections, and every recognition row
// is paired with a generation row. DialectIntent fails to compile if a reader ships without a writer.
struct Dialect
{
    static constexpr std::span<const IntentMarkerRow> markers{kMarkers};
    static constexpr std::span<const IntentEmitRow> emit_markers{kEmitMarkers};
};
static_assert(insight::semantic::DialectIntent<Dialect>,
              "jenkins: a recognition marker has no paired generation row (reader without a writer)");

// ── Run-outcome rows (ADR 0025 §4, studies/006 Table 4) ──
// The five native Jenkins `result` strings → the core four-class vocabulary. NOT_BUILT maps to
// Unknown (the run never produced a verdict — honest, not a guess). The console-tail marker is the
// `Finished: <RESULT>` epilogue — the DEGENERATE fallback source only (truncation-fragile, and it
// can be present-but-divergent: Accumulo #498 — the authoritative side-input always wins).
inline constexpr std::array<OutcomeTokenRow, 5> kOutcomeTokens{{
    {.token = "SUCCESS", .outcome = insight::RunOutcome::Success, .format_gate = insight::LogFormat::Jenkins},
    {.token = "FAILURE", .outcome = insight::RunOutcome::Failure, .format_gate = insight::LogFormat::Jenkins},
    {.token = "UNSTABLE", .outcome = insight::RunOutcome::Unstable, .format_gate = insight::LogFormat::Jenkins},
    {.token = "ABORTED", .outcome = insight::RunOutcome::Aborted, .format_gate = insight::LogFormat::Jenkins},
    {.token = "NOT_BUILT", .outcome = insight::RunOutcome::Unknown, .format_gate = insight::LogFormat::Jenkins},
}};

inline constexpr std::array<OutcomeMarkerRow, 1> kOutcomeMarkers{{
    {.prefix = "Finished: ", .format_gate = insight::LogFormat::Jenkins},
}};

// ── The manifest (§2.5) — the package's single composed contribution ──
// name "jenkins", version "1.0.0" (SP-7 immutable-release discipline). The depth claim this
// vocabulary carries is scoped to DECLARATIVE Pipeline (studies/006: 100% stage + step recall on
// the claim carrier; scripted/matrix-pipe corroborate); freestyle + classic MatrixProject are the
// II-4 floor for STRUCTURE but still get a correct four-class VERDICT (outcome recognition works on
// every Jenkins job type — the `result` is always present, `Finished:` is emitted).
export inline constexpr SemanticPackageManifest kManifest{
    .name = "jenkins",
    .version = "1.0.0",
    .roles = {},
    .markers = kMarkers,
    .level_lifts = {},
    .locations = {},
    .value_classes = {},
    .outcome_tokens = kOutcomeTokens,
    .outcome_markers = kOutcomeMarkers,
    .strategy = &make_strategy,
    .echoed_source = nullptr,
};

} // namespace insight::semantic::jenkins
