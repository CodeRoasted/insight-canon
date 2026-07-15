// insight.canon.spi — the PROVIDER contract (ADR 0024 §2.4). The public, installed, versioned
// surface a semantic package implements: the closed rule grammar (semantic-grammar-1), the format-
// strategy interface + its ParsedLine intermediate (the code tier, §2.3), the package manifest, and
// the curated scan primitives a dialect strategy needs. Consumers of insight.canon never import this
// (the facade does NOT re-export it); the sealed insight.canon.detail.* shards stay sealed. A package
// (`insight.semantic.github`, …) imports THIS + insight.canon.api, never a detail shard.
//
// Homed in api/ (public, installed) alongside the facade. Imports api (the enums/types the grammar
// rows reference) + internal (std). Provider types (IFormatStrategy, ParsedLine) live HERE — they
// were formerly in the sealed detail.strategy shard, which an external package cannot import; the
// shard now re-exports them so the 19 core strategies are unchanged.
module;

export module insight.canon.spi;
import insight.canon.internal; // std + global C fixed-width types
import insight.canon.api;      // LogFormat, LogLevel, StructuralRole, IntentMarkerKind, ChildOrder,
                               // ArenaAllocator, Timestamp, OtelTraceContext, OrdinalObservation

// ════════════════════════════════════════════════════════════════════════════════════════════════
// The format-strategy code tier (§2.3.1) — relocated from insight.canon.detail.strategy so a
// semantic package can implement a dialect strategy against an INSTALLED contract.
// ════════════════════════════════════════════════════════════════════════════════════════════════
export namespace insight::tokenization
{

// Intermediate representation produced by a format strategy.
//
// All string_view fields point into arena-managed storage. They remain valid until the owning
// ArenaAllocator is reset or destroyed.
//
// raw_line   — the original line, copied into the arena by LogParser before the strategy is invoked.
// component  — component / tag extracted by the strategy and stored into the arena via
//              ArenaAllocator::store_string().
// content    — message body fed to the masker, also arena-stored.
struct ParsedLine
{
    std::string_view raw_line;
    std::optional<Timestamp> timestamp;
    LogLevel level{LogLevel::Unknown};
    std::string_view component; // F3b: the low-card functional source (subsystem/daemon/job)
    std::string_view host;      // F3b: the high-card node/host identity (hors-cube)
    std::string_view content;
    // Echoed-source provenance (D-PROV-1): true when the RAW line was a CI command-echo of run-step
    // SCRIPT source (the GHA `\x1b[36;1m … \x1b[0m` command-echo wrapper), NOT an observed runtime
    // event. Set by the composed provenance hook (a package code-tier predicate) at the parser layer,
    // BEFORE any strategy sees the ANSI-stripped content. A per-line classification attribute, NOT
    // part of template identity: the parser uses it to demote `level` to Unknown so a failure WORD in
    // echoed shell source never confers an alerting level. `false` for every non-echoed line.
    bool echoed_source{false};
    // OTEL trace context (D-OTEL-1), populated by a strategy that recognizes OTEL log records (today:
    // JsonStrategy on OTLP/JSON). Consumed downstream (O2 grouping; O3 DAG), never serialized;
    // `present == false` for every non-OTEL input.
    OtelTraceContext trace{};
    // Declared ordinal observations (W1, D-W1-3), populated by a strategy that recognizes declared
    // structured numeric fields (today: JsonStrategy via kOrdinalFieldCatalog). A span over
    // arena-stable storage; empty for every non-ordinal line. Consumed metalog-side (W1 binning),
    // never tokenized into the template.
    std::span<const OrdinalObservation> ordinals{};
    // O4b Span Links (D-OTEL-9/21): the span_ids this span declares a cross-trace edge to (OTLP
    // `links[]`), populated by the span strategy. A span over arena-stable storage; empty for every
    // line without links. Consumed metalog-side (distilled into the service topology), never tokenized.
    std::span<const SpanId> linked_span_ids{};
};

// The format-strategy interface — a representation-format parser (core) OR a dialect strategy shipped
// by a semantic package (§1.3/§2.3.1). Registered into the FormatDetector via the composition; probed
// once per line (sticky-latched). External packages implement this against the installed spi contract.
class IFormatStrategy
{
  public:
    IFormatStrategy() = default;
    IFormatStrategy(const IFormatStrategy&) = delete;
    IFormatStrategy& operator=(const IFormatStrategy&) = delete;
    IFormatStrategy(IFormatStrategy&&) = delete;
    IFormatStrategy& operator=(IFormatStrategy&&) = delete;
    virtual ~IFormatStrategy() = default;

    // Parse a single log line.
    //
    // The input string_view must remain valid for the duration of the call (raw_line in the result
    // borrows from it). Owned scalar fields (component, content) are copied into the supplied arena
    // via ArenaAllocator::store_string(); their string_views remain valid until the arena is reset or
    // destroyed.
    [[nodiscard]] virtual std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const = 0;

    [[nodiscard]] virtual LogFormat format() const noexcept = 0;

    // Returns a [0,1] confidence score that this strategy matches the line. Used by FormatDetector
    // for majority-vote detection. Must be O(1).
    [[nodiscard]] virtual double confidence(std::string_view line) const noexcept = 0;
};

} // namespace insight::tokenization

// ════════════════════════════════════════════════════════════════════════════════════════════════
// The closed rule grammar (semantic-grammar-1, §2.2) — POD rows, constexpr-constructible, canonically
// serializable for the composed identity hash. Canon owns every matcher ALGORITHM; a package owns only
// DATA (rows). A new matching capability is a grammar-version bump, never package-local parsing code.
// ════════════════════════════════════════════════════════════════════════════════════════════════
export namespace insight::semantic
{

// The grammar version — a component of the composed identity (§4). Bump on any grammar SHAPE change
// (a new row kind, a new closed-enum member, a serialization change). ASCII, versioned string id.
// grammar-2 (ADR 0025, the first anticipated growth): the run-outcome row kinds (OutcomeTokenRow /
// OutcomeMarkerRow), the IntentMarkerRow payload-exclusion set, and the RemainderToClosingParen
// extractor — the shapes the Jenkins dialect genuinely needs.
inline constexpr std::string_view kSemanticGrammarVersion{"semantic-grammar-2"};

// The "any format" sentinel for a row's format gate: the rule fires regardless of the line's routed
// format. `LogFormat::Unknown` is that sentinel (no real routed line carries Unknown — RawText is the
// catch-all), so a universal structural-role row (`##[group]`, fired on any content today) reproduces
// the pre-split UNGATED StructuralRoleRegistry::classify EXACTLY. A dialect-specific row (an intent
// marker, a level lift) uses its concrete LogFormat (II-6 — a dialect never fires cross-format).
inline constexpr insight::LogFormat kAnyFormat{insight::LogFormat::Unknown};

// How a matched marker's payload is extracted (§2.2 — a CLOSED extractor enum; the algorithm lives in
// core). The alignment CLASS / discriminant are then derived in core by canonicalize_intent /
// discriminant_of. A new extractor is a grammar-version bump, part of the identity.
enum class PayloadExtract : std::uint8_t
{
    None = 0,             ///< no payload (structural markers carry none)
    RemainderAfterPrefix, ///< the content after the matched prefix, verbatim (intent markers)
    // grammar-2 (ADR 0025 / studies/006): the content after the matched prefix up to a REQUIRED
    // line-final ')' — the Jenkins named-block-open form `[Pipeline] { (<name>)`. A line that does
    // not end with ')' does not match the row at all (strict — an un-named `[Pipeline] {` wrapper
    // is scaffold, not a quantum). Nested parens stay inside the payload (`{ (Branch: test (lts))`
    // → `Branch: test (lts)`): only the single final ')' is the delimiter.
    RemainderToClosingParen,
};

// How a WRITER row materializes an intent's payload into log bytes (studies/008,
// shared_intent_declaration §2.3/§3.2) — the CLOSED dual of PayloadExtract: each emit value is the
// exact inverse of one extractor, so recognize(render_row(row, payload)) recovers the payload. This is
// the generation side of "one declaration, two projections": canon RECOGNIZES via PayloadExtract,
// LogCraft GENERATES via PayloadEmit, both rows-as-data (SID-2 — never a render() callable, which is
// un-hashable and lets the two projections diverge). A new emit shape is a grammar-version bump, part
// of the identity, exactly as a new extractor is.
enum class PayloadEmit : std::uint8_t
{
    None = 0,                ///< dual of PayloadExtract::None — the prefix alone (structural markers)
    PayloadAfterPrefix,      ///< dual of RemainderAfterPrefix — the prefix, then the payload verbatim
    PayloadThenClosingParen, ///< dual of RemainderToClosingParen — prefix, payload, then the final ')'
};

// Which core location-matching ALGORITHM a LocationRow selects + parameterizes (§2.2 — a CLOSED enum;
// the three families location_recognizer implements today). A new dialect needing a new family is a
// grammar-version bump, part of the identity.
enum class LocationMatchKind : std::uint8_t
{
    // `<base>.test.<ext>` / `<base>.spec.<ext>` with ext ∈ params.extensions (jest/vitest/playwright).
    TestSpecExtension = 0,
    // basename `<prefix>*` or `*<prefix-reversed>` + a fixed extension: pytest `test_*.py`/`*_test.py`.
    PrefixAndExtension,
    // any of a set of fixed suffixes, word-boundary-terminated: go `_test.go`, ruby `_spec.rb`/`_test.rb`.
    SuffixSet,
};

// ── Row kinds (one per knowledge surface, §2.2) ──────────────────────────────────────────────────

// A structural-role rule: a line-anchored prefix announces a role (§1.2 — `##[group]` → GroupBegin).
struct StructuralRoleRow
{
    std::string_view prefix;
    insight::StructuralRole role;
    insight::LogFormat format_gate; // kAnyFormat = fire on any format (the pre-split ungated behavior)
};

// An intent-marker rule: a prefix opens a behavioural quantum (§1.2 — `Complete job name: ` → Job).
// Carries the dialect's HIERARCHY (kind + child_order — the ADR 0023 level-typed alignment
// declaration) and the payload extractor. FORMAT-GATED by construction (II-6).
struct IntentMarkerRow
{
    std::string_view prefix;
    insight::tokenization::IntentMarkerKind kind;
    insight::tokenization::ChildOrder child_order;
    insight::LogFormat format_gate;
    PayloadExtract extract;
    // grammar-2 (ADR 0025 / studies/006): a CLOSED exclusion set over the extracted payload — the
    // row does NOT fire when the payload's leading token matches an entry (entry == payload, or
    // payload starts with entry followed by a space). The Jenkins step form `[Pipeline] <verb>`
    // needs it: the verb set is open (any pipeline step), the structural tokens that share the
    // prefix (`{`, `}`, `stage`, `node`, `parallel`, `//`, `End of Pipeline`) are closed dialect
    // data. Empty for rows without exclusions (every pre-grammar-2 row). The span points at
    // package-static constexpr storage (SP-7 lifetime); serialized into semantic_identity.
    std::span<const std::string_view> payload_excludes{};
};

// A generation-template rule (studies/008, shared_intent_declaration §3.2) — the WRITER dual of
// IntentMarkerRow. Carries the SAME dialect hierarchy (kind + child_order, the ADR 0023 declaration)
// and the SAME medium gate (format_gate = the format+sink the line materializes into — the O2 medium
// axis; the two GHA Step media `Run ` / `##[group]Run ` are two emit rows sharing kind, differing in
// prefix). No payload_excludes: the writer only ever emits a real intent, never an excluded structural
// token, so the exclusion set is a reader-side concern with no generation dual. Rows-as-data (SID-2):
// the emit shape is the closed PayloadEmit enum, never a callable. Content-hashable exactly as
// IntentMarkerRow is, so a generation-side change moves semantic_identity as a recognition change does
// (G4 — the hash wiring lands with the ADR at ratification).
struct IntentEmitRow
{
    std::string_view prefix;
    insight::tokenization::IntentMarkerKind kind;
    insight::tokenization::ChildOrder child_order;
    insight::LogFormat format_gate;
    PayloadEmit emit;
};

// A level-lift rule: a prefix lifts the line's LogLevel (§1.2 — `##[error]` → Error). Consumed by the
// dialect strategy that owns the rows (level lift happens inside parse(), before raw-text inference).
struct LevelLiftRow
{
    std::string_view prefix;
    insight::LogLevel level;
    insight::LogFormat format_gate;
};

// A location rule: recognizes a test-file WHERE coordinate (§5.3/II-8). `kind` selects the core
// matching algorithm; the params are the dialect-independent file-naming vocabulary it walks. The
// spans point at package-static constexpr arrays (SP-7 immutable-release lifetime). Not every param is
// used by every kind — `extensions` for TestSpecExtension/PrefixAndExtension, `suffixes` for
// SuffixSet, `prefixes` for PrefixAndExtension (the basename `test_`/`_test` forms).
struct LocationRow
{
    LocationMatchKind kind;
    std::span<const std::string_view> infixes;    // `.test.` / `.spec.` (TestSpecExtension)
    std::span<const std::string_view> extensions; // ts/tsx/js/… (TestSpec) or `.py` (PrefixAndExtension)
    std::span<const std::string_view> prefixes;   // `test_` / `_test` basename forms (PrefixAndExtension)
    std::span<const std::string_view> suffixes;   // `_test.go` / `_spec.rb` / `_test.rb` (SuffixSet)
};

// ── Run-outcome rows (grammar-2, ADR 0025 / insight_run_outcome_model.md §4) ──
// The dialect's run-verdict MAPPING — native verdict string → the core-owned RunOutcome — plus the
// console-tail fallback marker. Matcher algorithms live in core (map_outcome_token /
// scan_run_outcome / resolve_run_outcome, the D-OUT-RUN-1 precedence); a package ships only rows,
// and either set may be empty (GHA ships tokens but no marker — it has no run-verdict console line).

// One native verdict token → RunOutcome (byte-exact, format-gated). Consumed on BOTH resolution
// rungs: the authoritative `--*-outcome` side-input token and the console-tail marker's extracted
// remainder map through the SAME set.
struct OutcomeTokenRow
{
    std::string_view token; // the native dialect verdict string, verbatim ("UNSTABLE", "cancelled")
    insight::RunOutcome outcome;
    insight::LogFormat format_gate;
};

// The console-tail terminal-verdict line (Jenkins: `Finished: `). Core matches it line-anchored on
// the routed format, extracts the remainder token (which must be a single ASCII word), and maps it
// through the composed OutcomeTokenRow set; the LAST match in line order wins (a run has one
// terminal verdict; deterministic integer index).
struct OutcomeMarkerRow
{
    std::string_view prefix;
    insight::LogFormat format_gate;
};

// A value-class rule (the grammar SEAT for package value classes, §5). No package ships domain value
// classes in 1.7.5 — the composed ValueClassRegistry is the VIEW over the core catalogs
// (kOrdinalFieldCatalog/kOtelFieldCatalog/KEEP). Present so the manifest shape is final and the
// identity hash slot is stable.
enum class ValueClass : std::uint8_t
{
    None = 0,
};

struct ValueClassRow
{
    std::string_view key;
    ValueClass cls;
    std::string_view schedule_id; // the ordinal schedule id when cls is an ordinal class (else empty)
    std::int64_t scale;           // unit→canonical factor when applicable (else 0)
};

// ── The strategy factory (code tier) ──────────────────────────────────────────────────────────────
// A package that ships a dialect format strategy exports a factory: nullable (data-only packages
// return an empty manifest.strategy). The composition registers the produced strategy into the
// FormatDetector via the existing register_strategy seam.
using StrategyFactory = std::unique_ptr<insight::tokenization::IFormatStrategy> (*)();

// A raw-line provenance hook (the echoed-source code tier, §2.3.2 — a discouraged escape-hatch
// recognizer, signature-constrained to a pure `bool(std::string_view)` byte function). Consulted by
// LogParser on the RAW (ANSI-bearing) line, independent of the routed strategy, so it reproduces the
// pre-split strategy-independent is_echoed_source_line EXACTLY. Nullable.
using ProvenanceHook = bool (*)(std::string_view raw_line) noexcept;

// ── The manifest (§2.5) ─────────────────────────────────────────────────────────────────────────
// Each package exports one `constexpr SemanticPackageManifest kManifest` from its own named module.
// No register_*() methods, no mutable registration state — the composition is a pure function of the
// manifest SET.
struct SemanticPackageManifest
{
    std::string_view name;    // "github"
    std::string_view version; // "1.0.0" — immutable release discipline (SP-7)
    std::span<const StructuralRoleRow> roles;
    std::span<const IntentMarkerRow> markers;
    std::span<const LevelLiftRow> level_lifts;
    std::span<const LocationRow> locations;
    std::span<const ValueClassRow> value_classes;
    // grammar-2 (ADR 0025): the run-outcome vocabulary. Both may be empty (§3.2 — a package
    // declares exactly the outcome surfaces its dialect actually has).
    std::span<const OutcomeTokenRow> outcome_tokens;
    std::span<const OutcomeMarkerRow> outcome_markers;
    StrategyFactory strategy{nullptr};  // nullable — the dialect format-strategy code tier
    ProvenanceHook echoed_source{nullptr}; // nullable — the raw-line echoed-source code tier
};

// ════════════════════════════════════════════════════════════════════════════════════════════════
// The generation projection API (studies/008, shared_intent_declaration §3.2) — the WRITER half of
// "one declaration, two projections". render_row is the pure inverse of core's payload extraction;
// paired_writer_row + all_intents_paired + the DialectIntent concept make bidirectionality a
// compile-time obligation (a package that ships a recognition row without its paired generation row
// does not satisfy DialectIntent → does not compile). This is the C2 mechanism (§3.2): the round-trip
// obligation turned into a TYPE obligation. render_row/paired_writer_row are the surface the studies/008
// G2 closure kit round-trips against; the concept is the structural guarantee behind it.
// ════════════════════════════════════════════════════════════════════════════════════════════════

// render_row — the writer-row expansion. PURE: a function of (row, payload) ONLY — no RNG, no envelope,
// no engine state, no wall-clock — so it lives on the canon (recognition) side and LogCraft merely
// calls it to materialize. The exact inverse of the PayloadExtract algorithm: for every emit shape,
// recognize(render_row(row, payload), row.format_gate, composed) recovers (row.kind, row.child_order,
// payload) — the G2 round-trip. Allocates the result string (the only allocation; caller-owned).
[[nodiscard]] inline std::string render_row(const IntentEmitRow& row, std::string_view payload)
{
    std::string out;
    switch (row.emit)
    {
    case PayloadEmit::None:
        out.reserve(row.prefix.size());
        out.append(row.prefix);
        break;
    case PayloadEmit::PayloadAfterPrefix:
        out.reserve(row.prefix.size() + payload.size());
        out.append(row.prefix);
        out.append(payload);
        break;
    case PayloadEmit::PayloadThenClosingParen:
        out.reserve(row.prefix.size() + payload.size() + 1);
        out.append(row.prefix);
        out.append(payload);
        out.push_back(')');
        break;
    }
    return out;
}

// paired_writer_row — the reader→writer pairing. Given a recognition row, returns the generation row
// that materializes into a line THAT row recognizes: same prefix, kind, and medium (format_gate).
// Well-defined iff every reader row has exactly one paired writer row — the property all_intents_paired
// enforces. Returns nullptr when unpaired (a violation the static check rejects; exposed so a runtime
// closure kit can assert on it too). constexpr — usable in the consteval check and at runtime.
[[nodiscard]] constexpr const IntentEmitRow*
paired_writer_row(const IntentMarkerRow& reader, std::span<const IntentEmitRow> emits) noexcept
{
    for (const IntentEmitRow& emit : emits)
    {
        if (emit.prefix == reader.prefix && emit.kind == reader.kind &&
            emit.format_gate == reader.format_gate)
        {
            return &emit;
        }
    }
    return nullptr;
}

// all_intents_paired — the bidirectionality predicate (SID: no reader without a writer). consteval so a
// package static_asserts it over its constexpr rows: every recognition marker has a paired generation
// row. The value half of the C2 type obligation (§3.2).
[[nodiscard]] consteval bool all_intents_paired(std::span<const IntentMarkerRow> markers,
                                                std::span<const IntentEmitRow> emits) noexcept
{
    for (const IntentMarkerRow& reader : markers)
    {
        if (paired_writer_row(reader, emits) == nullptr)
        {
            return false;
        }
    }
    return true;
}

// The DialectIntent concept (§3.2) — bidirectionality as a TYPE obligation. A type models DialectIntent
// iff it exposes BOTH projections as constexpr row spans (recognition `markers` + generation
// `emit_markers`) AND every reader row is paired (all_intents_paired). A dialect whose type ships a
// recognition row without its generation row does NOT satisfy the concept → does not compile where the
// concept is required. This is the compile-time half of G2; render_row + the runtime round-trip are the
// value half.
template <typename Dialect>
concept DialectIntent = requires {
    { Dialect::markers } -> std::convertible_to<std::span<const IntentMarkerRow>>;
    { Dialect::emit_markers } -> std::convertible_to<std::span<const IntentEmitRow>>;
} && all_intents_paired(Dialect::markers, Dialect::emit_markers);

} // namespace insight::semantic
