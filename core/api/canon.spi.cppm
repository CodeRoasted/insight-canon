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
inline constexpr std::string_view kSemanticGrammarVersion{"semantic-grammar-1"};

// The "any format" sentinel for a row's format gate: the rule fires regardless of the line's routed
// format. `LogFormat::Unknown` is that sentinel (no real routed line carries Unknown — RawText is the
// catch-all), so a universal structural-role row (`##[group]`, fired on any content today) reproduces
// the pre-split UNGATED StructuralRoleRegistry::classify EXACTLY. A dialect-specific row (an intent
// marker, a level lift) uses its concrete LogFormat (II-6 — a dialect never fires cross-format).
inline constexpr insight::LogFormat kAnyFormat{insight::LogFormat::Unknown};

// How a matched marker's payload is extracted (§2.2 — a CLOSED extractor enum; the algorithm lives in
// core). Today one shape suffices: the payload is the content AFTER the matched prefix, verbatim (the
// alignment CLASS / discriminant are then derived in core by canonicalize_intent/discriminant_of).
enum class PayloadExtract : std::uint8_t
{
    None = 0,             ///< no payload (structural markers carry none)
    RemainderAfterPrefix, ///< the content after the matched prefix, verbatim (intent markers)
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
    StrategyFactory strategy{nullptr};  // nullable — the dialect format-strategy code tier
    ProvenanceHook echoed_source{nullptr}; // nullable — the raw-line echoed-source code tier
};

} // namespace insight::semantic
