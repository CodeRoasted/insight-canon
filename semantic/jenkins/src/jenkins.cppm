// insight.semantic.jenkins — the Jenkins Pipeline dialect semantic package (ADR-17,
// studies/006). VOCABULARY as DATA in the closed canon rule grammar (intent markers + run-outcome
// rows); the code tier is EMPTY since T5 5.2 (see below). Fully self-contained: imports only
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

// ── The code tier: NONE (T5 5.2 — the GHA T4 precedent, one dialect over) ──
// There is NO strategy factory any more: `JenkinsStrategy` detected and peeled the Timestamper
// bracket stamp, claimed `[Pipeline] ` lines and the `Finished: ` epilogue at 0.92, and every one
// of those legs is now DECLARED rather than detected — the bracket stamp is catalogue transport
// (`bracket-rfc3339-line-prefix`, canon.transport; peel-equivalence certified by G-T5-PEEL against
// the strip frozen per adr/0062), and the marker/outcome rows are dialect-gated data walked by
// core under `--dialect jenkins` (adr/0065 clause 5: rows plus canon's walkers ARE the parser).
// The strategy's parse of a bare claimed line was RawText's parse (level via the same
// infer_leading_log_level, empty component, content unmoved) — certified as exactly that by
// G-T5-BARE's byte-identity over the 82 bare traces. Jenkins has no echoed-source wrapper either
// (that is the GHA SGR command-echo), so the whole code tier is empty.
//
// THE 19-LOG PAYLOAD-STAMPED RE-BASELINE, recorded here because this is the strategy's successor
// surface (adr/0046 Part 2 clause 1; T5 §4 item 5): that class is NOT declarable (the stamp is a
// payload-determined subset, adr/0044 §1), so post-purification its stamps stay CONTENT and those
// lines template with the stamp under D-MSK-5's `[<*>]` normal form. Template IDs move; the count
// is stable — measured ±strip 3 337 vs 3 339, the +2 fully attributed (one dual-occurrence twin +
// the bare-`[<*>]` cell from 134 timestamp-only lines; the §6.5 prefix-image triangle returned
// REPAIRED 2026-07-30, per stamped line template(unstripped) == "[<*>]" ⧺ M(rest), zero exceptions
// on 6 416 stamped lines). A re-baseline, not a regression.

// The dialect NAME every gated row below carries, and the name a caller declares
// (`IngestDeclaration::dialect` / `--dialect`). ADR-22: the gate is a composed package
// name, never an enum, so canon knows the field and knows no value. Exported so a caller can name
// this dialect without spelling a literal; `all_dialect_gates_owned` static_asserts it against the
// manifest's `.name`.
export inline constexpr std::string_view kDialect{"jenkins"};

// ── Intent-marker rows (studies/006 §Reproduction, grammar-2) ──
// DIALECT-GATED to this package (SRC-II-6 — `[Pipeline] ` is Jenkins-runner-specific). The
// hierarchy rides the rows (ADR-18): STAGE is the container level (kind=Job — declared stages AND
// parallel/matrix `Branch:` legs co-occur, so the level matches UNORDERED, exactly like GHA matrix
// jobs); STEP is the leaf level (Ordered — steps are sequential within their stage). The two
// prefixes nest, so longest-VALID-match resolves a named block open to STAGE and everything else
// `[Pipeline] `- prefixed to STEP — minus the closed structural-token exclusion set (the spike's
// STRUCT set + the `// …` block-close annotations; `End of Pipeline` is the run epilogue).
inline constexpr std::array<std::string_view, 7> kStepExcludes{
    "{", "}", "stage", "node", "parallel", "//", "End of Pipeline"};

inline constexpr std::array<IntentMarkerRow, 2> kMarkers{{
    {.prefix = "[Pipeline] { (", // the NAMED block open: `{ (<name>)` — stage or Branch: leg
     .kind = insight::tokenization::IntentMarkerKind::Job,
     .child_order = insight::tokenization::ChildOrder::Unordered,
     .dialect_gate = kDialect,
     .extract = PayloadExtract::RemainderToClosingParen},
    {.prefix = "[Pipeline] ", // the step annotation: `<verb>` (sh, echo, junit, checkout, …)
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .dialect_gate = kDialect,
     .extract = PayloadExtract::RemainderAfterPrefix,
     .payload_excludes = kStepExcludes},
}};

// ── Generation-template rows (studies/008, shared_intent_declaration §3.2) — the WRITER dual ──
// One emit row per recognition row, paired by (prefix, kind, dialect_gate). The STAGE emit is
// PayloadThenClosingParen, the exact inverse of the reader's RemainderToClosingParen:
// render_row(stage_row, "Build") reproduces `[Pipeline] { (Build)`, which canon segments back to
// the named STAGE "Build". The STEP emit is PayloadAfterPrefix — the writer only ever emits a REAL
// step verb (`sh`, `echo`, …), never a kStepExcludes structural token, so the reader's exclusion
// set has no generation dual and the round-trip closes (G2).
inline constexpr std::array<IntentEmitRow, 2> kEmitMarkers{{
    {.prefix = "[Pipeline] { (",
     .kind = insight::tokenization::IntentMarkerKind::Job,
     .child_order = insight::tokenization::ChildOrder::Unordered,
     .dialect_gate = kDialect,
     .emit = PayloadEmit::PayloadThenClosingParen},
    {.prefix = "[Pipeline] ",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .dialect_gate = kDialect,
     .emit = PayloadEmit::PayloadAfterPrefix},
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
    "jenkins: a recognition marker has no paired generation row (reader without a writer)");

// ── Run-outcome rows (ADR-17, studies/006 Table 4) ──
// The five native Jenkins `result` strings → the core four-class vocabulary. NOT_BUILT maps to
// Unknown (the run never produced a verdict — honest, not a guess). The console-tail marker is the
// `Finished: <RESULT>` epilogue — the DEGENERATE fallback source only (truncation-fragile, and it
// can be present-but-divergent: Accumulo #498 — the authoritative side-input always wins).
inline constexpr std::array<OutcomeTokenRow, 5> kOutcomeTokens{{
    {.token = "SUCCESS",
     .outcome = insight::RunOutcome::Success,
     .dialect_gate = kDialect},
    {.token = "FAILURE",
     .outcome = insight::RunOutcome::Failure,
     .dialect_gate = kDialect},
    {.token = "UNSTABLE",
     .outcome = insight::RunOutcome::Unstable,
     .dialect_gate = kDialect},
    {.token = "ABORTED",
     .outcome = insight::RunOutcome::Aborted,
     .dialect_gate = kDialect},
    {.token = "NOT_BUILT",
     .outcome = insight::RunOutcome::Unknown,
     .dialect_gate = kDialect},
}};

inline constexpr std::array<OutcomeMarkerRow, 1> kOutcomeMarkers{{
    {.prefix = "Finished: ", .dialect_gate = kDialect},
}};

// ── The manifest (§2.5) — the package's single composed contribution ──
// name "jenkins", version "1.1.0" (SP-7 immutable-release discipline; bumped from 1.0.0 for T5
// 5.2: the package's CODE TIER lost its format strategy — `.strategy` is serialized as a presence
// byte, so the manifest's content genuinely moved; the GHA 1.3.0→1.4.0 precedent at T4). The
// depth claim this vocabulary carries is scoped to DECLARATIVE Pipeline (studies/006: 100% stage
// + step recall on the claim carrier; scripted/matrix-pipe corroborate); freestyle + classic
// MatrixProject are the SRC-II-4 floor for STRUCTURE but still get a correct four-class VERDICT
// (outcome recognition works on every Jenkins job type — the `result` is always present,
// `Finished:` is emitted).
export inline constexpr SemanticPackageManifest kManifest{
    .name = "jenkins",
    .version = "1.1.0",
    .roles = {},
    .markers = kMarkers,
    .emits = kEmitMarkers, // ADR-23 — the generation projection is identity-bearing
    .level_lifts = {},
    .locations = {},
    .value_classes = {},
    .outcome_tokens = kOutcomeTokens,
    .outcome_markers = kOutcomeMarkers,
    .echoed_source = nullptr,
};

// ADR-22 — every gated row names THIS package or kAnyDialect, checked at COMPILE time,
// here. A gate naming another package would reach across a boundary this package does not own, and
// a typo would produce a row that silently never fires under any declaration.
static_assert(insight::semantic::all_dialect_gates_owned(kManifest),
              "jenkins: a row's dialect_gate is neither kAnyDialect nor this package's own name (a "
              "typo in .dialect_gate, or a row reaching for another package's vocabulary?)");
static_assert(kManifest.name == kDialect,
              "jenkins: kDialect and the manifest name must be the same string — kDialect is what a "
              "caller declares and what every gated row carries");

} // namespace insight::semantic::jenkins
