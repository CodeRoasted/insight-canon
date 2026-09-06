// refs: ADR-8, ADR-17, ADR-22, ADR-23, STU-6, BIB:jenkins_dialect
// invariant: the package is VOCABULARY AS DATA — intent-marker rows plus run-outcome rows — and
// ships NO code tier: the format strategy died at the T5 identity cut and nothing replaced it.
// invariant: it imports canon's api and spi only, never a sealed detail shard, and a binary gets
// its rows by statically linking `kManifest` through the composition.
// invariant: the Timestamper bracket stamp is DECLARED catalogue transport
// (`bracket-rfc3339-line-prefix`), so peeling it is canon's job and never this package's.
// invariant: the rows are dialect-gated data walked by core under a `jenkins` declaration — the
// rows plus canon's walkers ARE the parser, and there is no detection step left.
// invariant: no structural-role, level-lift, location or value-class rows: the frozen spike
// surfaced none of those depths on real consoles, and dormant vocabulary is not built.
// assert: the peel is certified equal to the deleted strategy's strip by the bracket
// peel-equivalence gate over the 12 whole-stream traces.
// refs: F-SRC-insight-canon:test_bracket_peel_equivalence_gate.cpp
// assert: on the 82 bare traces the equivalence is certified for the 75 rows still carrying the
// pre-cut emission; the other 7 were re-emitted 2026-08-26 and carry NO before/after null.
// refs: F-SRC-insight-canon:test_jenkins_bare_null_gate.cpp
module;

export module insight.semantic.jenkins;
import insight.canon.internal;
import insight.canon.api;
export import insight.canon.spi;

namespace insight::semantic::jenkins
{

// refs: ADR-23.D1, ADR-23.D6, SRC-D-MSK-5
// invariant: the payload-stamped class is NOT declarable as transport — the stamp is a
// payload-determined subset, not a stream property — so its stamps stay CONTENT.
// assert: those lines therefore template WITH the stamp, collapsed to the `[<*>]` bracket normal
// form, and that template consequence is ruled real and correct rather than honesty-only.
// assert: measured over the 19 payload-stamped logs: 3 337 distinct templates with the peel against
// 3 339 without, and both extra templates are attributed rather than residual.
// assert: the two are one dual-occurrence twin plus the bare `[<*>]` cell that the 134
// timestamp-only lines produce; the exit gate reads REPAIRED on 6 416 stamped lines.
// note: a re-baseline with a closed account of its delta, never a precision regression
// refs: F-SRC-insight-canon:payload_stamp_template_count_measurement_test.cpp
// refs: ADR-22
// invariant: `kDialect` is BOTH what a caller declares and what every gated row below carries, so
// canon knows the field and knows no value — a composed package NAME, never an enum.
// invariant: it is exported so a caller never spells the literal, and the manifest gate below
// static_asserts it against `kManifest.name`.
export inline constexpr std::string_view kDialect{"jenkins"};

// refs: SRC-II-6, STU-6
// invariant: the rows are DIALECT-GATED to this package: `[Pipeline] ` is Jenkins-runner-specific
// and must fire on nothing else.
// invariant: the exclusion set is CLOSED and small because the structural tokens are a fixed
// vocabulary of the runner's own block syntax, not an open verb family.
// note: `End of Pipeline` is the run epilogue and `//` opens a block-close annotation
inline constexpr std::array<std::string_view, 7> kStepExcludes{
    "{", "}", "stage", "node", "parallel", "//", "End of Pipeline"};

// refs: ADR-18
// invariant: the hierarchy rides the rows — STAGE is the container level (Job) and STEP the leaf
// level, so the level a marker opens is declared data and never inferred at the call site.
// invariant: STAGE is UNORDERED because declared stages and parallel/matrix `Branch:` legs
// co-occur, so their sequence is not a structural fact; STEP is Ordered within its stage.
// assert: the two prefixes NEST, so resolution is longest-VALID-match: a named block open reaches
// STAGE and every other `[Pipeline] `-prefixed line reaches STEP, minus the excludes.
inline constexpr std::array<IntentMarkerRow, 2> kMarkers{{
    {.prefix = "[Pipeline] { (",
     .kind = insight::tokenization::IntentMarkerKind::Job,
     .child_order = insight::tokenization::ChildOrder::Unordered,
     .dialect_gate = kDialect,
     .extract = PayloadExtract::RemainderToClosingParen},
    {.prefix = "[Pipeline] ",
     .kind = insight::tokenization::IntentMarkerKind::Step,
     .child_order = insight::tokenization::ChildOrder::Ordered,
     .dialect_gate = kDialect,
     .extract = PayloadExtract::RemainderAfterPrefix,
     .payload_excludes = kStepExcludes},
}};

// refs: STU-8, BIB:intent_library
// invariant: one emit row per recognition row, paired by prefix, kind and dialect gate — the
// WRITER dual of the reader above.
// post: `PayloadThenClosingParen` is the exact inverse of the reader's `RemainderToClosingParen`,
// so rendering a stage payload reproduces `[Pipeline] { (<name>)`.
// invariant: the STEP emit needs no dual for the exclusion set: the writer only ever emits a real
// step verb, never a structural token, so the round trip closes on every declared row.
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

export struct Dialect
{
    static constexpr std::span<const IntentMarkerRow> markers{kMarkers};
    static constexpr std::span<const IntentEmitRow> emit_markers{kEmitMarkers};
};
static_assert(
    insight::semantic::DialectIntent<Dialect>,
    "jenkins: a recognition marker has no paired generation row (reader without a writer)");

// refs: ADR-17, STU-6, SRC-D-OUT-RUN-1
// invariant: the five native `result` strings map into the core four-class vocabulary, and
// NOT_BUILT maps to Unknown because the run never produced a verdict — honest, not a guess.
// invariant: the console-tail `Finished: <RESULT>` epilogue is the DEGENERATE fallback source only:
// it is truncation-fragile and can be present-but-divergent from the API result.
// assert: on a divergence the authoritative side-input always wins and the disagreement is flagged;
// one real counterexample stands in the marker corpus and is pinned by name.
inline constexpr std::array<OutcomeTokenRow, 5> kOutcomeTokens{{
    {.token = "SUCCESS", .outcome = insight::RunOutcome::Success, .dialect_gate = kDialect},
    {.token = "FAILURE", .outcome = insight::RunOutcome::Failure, .dialect_gate = kDialect},
    {.token = "UNSTABLE", .outcome = insight::RunOutcome::Unstable, .dialect_gate = kDialect},
    {.token = "ABORTED", .outcome = insight::RunOutcome::Aborted, .dialect_gate = kDialect},
    {.token = "NOT_BUILT", .outcome = insight::RunOutcome::Unknown, .dialect_gate = kDialect},
}};

inline constexpr std::array<OutcomeMarkerRow, 1> kOutcomeMarkers{{
    {.prefix = "Finished: ", .dialect_gate = kDialect},
}};

// refs: ADR-17.D9
// invariant: this is the VENDOR generation the rows recognize — the Declarative Pipeline console
// syntax, of which Jenkins has shipped one.
// invariant: it is NOT `.version`: that one moves when WE edit the ruleset, this one moves when
// Jenkins ships a new console syntax generation.
export inline constexpr std::array<std::string_view, 1> kDialectRevisions{{"v1"}};

// refs: SRC-SP-7, ADR-23
// invariant: `.version` is immutable-release discipline, and the generation projection `.emits` is
// identity-bearing, so both are serialized into the semantic identity.
// assert: the DEPTH claim this vocabulary carries is scoped to DECLARATIVE Pipeline — 100 % stage
// and step recall on that carrier; scripted and matrix-pipe corroborate, never widen it.
// invariant: freestyle and classic MatrixProject emit no `[Pipeline]` skeleton at all, so they are
// the degenerate-closure floor for STRUCTURE.
// assert: they still get a correct four-class VERDICT, because the `result` is always present and
// `Finished:` is emitted on every Jenkins job type.
// refs: SRC-II-4, BIB:intent_identity
export inline constexpr SemanticPackageManifest kManifest{
    .name = "jenkins",
    .version = "1.1.0",
    .roles = {},
    .markers = kMarkers,
    .emits = kEmitMarkers,
    .level_lifts = {},
    .locations = {},
    .value_classes = {},
    .outcome_tokens = kOutcomeTokens,
    .outcome_markers = kOutcomeMarkers,
    .dialect_revisions = kDialectRevisions,
    .echoed_source = nullptr,
};

// refs: ADR-22
// invariant: every gated row names THIS package or the any-dialect wildcard, checked at COMPILE
// time in the package that declares the rows.
// invariant: a gate naming another package would reach across a boundary this package does not own,
// and a typo would mint a row that silently never fires under any declaration.
static_assert(insight::semantic::all_dialect_gates_owned(kManifest),
              "jenkins: a row's dialect_gate is neither kAnyDialect nor this package's own name (a "
              "typo in .dialect_gate, or a row reaching for another package's vocabulary?)");
static_assert(
    kManifest.name == kDialect,
    "jenkins: kDialect and the manifest name must be the same string — kDialect is what a "
    "caller declares and what every gated row carries");

// refs: ADR-17.D9
// invariant: the declared revision vocabulary is checked at the same seat and for the same reason
// as the gate above — the coordinate is what a reader compares generations on.
static_assert(insight::semantic::all_revisions_named(kDialectRevisions),
              "jenkins: the declared dialect-revision vocabulary must be non-empty, with unique, "
              "non-empty names (grammar-6 — the coordinate is what a reader compares generations "
              "on, so an unnamed or repeated one is not a declaration)");

} // namespace insight::semantic::jenkins
