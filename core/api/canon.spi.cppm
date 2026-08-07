// insight.canon.spi — the PROVIDER contract (ADR-17). The public, installed, versioned
// surface a semantic package implements: the closed rule grammar (versioned by
// kSemanticGrammarVersion), the format-strategy interface + its ParsedLine intermediate (the code
// tier, §2.3), the package manifest, and the curated scan primitives a dialect strategy needs.
// Consumers of insight.canon never import this (the facade does NOT re-export it); the sealed
// insight.canon.detail.* shards stay sealed. A package (`insight.semantic.github`, …) imports
// THIS + insight.canon.api, never a detail shard.
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

// ── EventTime — a timestamp AND where it came from, as ONE value (DN-29.D12 / DN-29.D14) ───────
//
// THE CLAUSE THIS TYPE EXISTS TO MAKE STRUCTURAL: *the timestamp and its provenance are assigned
// together, through one site, and are not independently settable.* A `bool` beside an
// `optional<Timestamp>` would be two fields, and the failure mode is then a future declared-time
// field read WITHOUT setting the flag — the same class as the `links[]` drop, discovered the same
// way, months later. Here there is nothing to forget: the provenance IS the value.
//
// THERE IS DELIBERATELY NO IMPLICIT CONVERSION from `std::optional<Timestamp>`. One would make
// `timestamp = parse_unix_nano(...)` compile and silently mean PARSED, which re-opens exactly the
// hole this closes. Every assignment names its provenance, and the compiler enforces it.
//
// The read surface forwards, so a consumer asking "is there a time?" is unchanged; only WRITERS
// were made to say more.
class EventTime
{
  public:
    EventTime() = default;

    // The strategy INFERRED this time from bytes whose authorship is ambiguous — a printf stamp,
    // a syslog prefix, a JSON field canon happens to know the name of. This is the default species
    // and what all nineteen representation strategies produce.
    [[nodiscard]] static EventTime parsed(std::optional<Timestamp> value) noexcept
    {
        EventTime out;
        out.value_ = value;
        return out;
    }

    // The PRODUCER declared this time in a schema field whose meaning is the event time — OTLP
    // `startTimeUnixNano` / `timeUnixNano`. Not content that resembles a time: the same species of
    // fact as `trace_id`, which is why it outranks a transport stamp (DN-29.D12 rung 1) where a
    // parsed one does not.
    [[nodiscard]] static EventTime declared(Timestamp value) noexcept
    {
        EventTime out;
        out.value_ = value;
        out.declared_ = true;
        return out;
    }

    [[nodiscard]] bool has_value() const noexcept
    {
        return value_.has_value();
    }
    explicit operator bool() const noexcept
    {
        return value_.has_value();
    }
    [[nodiscard]] Timestamp value_or(Timestamp fallback) const noexcept
    {
        return value_.value_or(fallback);
    }
    [[nodiscard]] std::optional<Timestamp> value() const noexcept
    {
        return value_;
    }
    // Deref forwards too, so `timestamp->time_since_epoch()` reads exactly as it did against the
    // bare optional. The point of this type is to constrain WRITERS; making readers rewrite would
    // have been churn with no guarantee attached to it. Precondition is the optional's own.
    [[nodiscard]] const Timestamp& operator*() const noexcept
    {
        return *value_;
    }
    [[nodiscard]] const Timestamp* operator->() const noexcept
    {
        return &*value_;
    }

    // True only for rung 1. A time that is absent is never declared, so this cannot disagree with
    // has_value() — one field, one truth.
    [[nodiscard]] bool is_declared() const noexcept
    {
        return declared_ && value_.has_value();
    }

  private:
    std::optional<Timestamp> value_;
    bool declared_{false};
};

// Intermediate representation produced by a format strategy.
//
// All string_view fields point into arena-managed storage. They remain valid until the owning
// ArenaAllocator is reset or destroyed.
//
// raw_line   — the original line, copied into the arena by LogParser before the strategy is
// invoked. component  — component / tag extracted by the strategy and stored into the arena via
//              ArenaAllocator::store_string().
// content    — message body fed to the masker, also arena-stored.
struct ParsedLine
{
    std::string_view raw_line;
    // The event time AND its provenance — see EventTime above. Assign with EventTime::parsed(...)
    // or EventTime::declared(...); there is no third way and no implicit one.
    EventTime timestamp;
    LogLevel level{LogLevel::Unknown};
    std::string_view component; // F3b: the low-card functional source (subsystem/daemon/job)
    std::string_view host;      // F3b: the high-card node/host identity (hors-cube)
    std::string_view content;
    // Echoed-source provenance (SRC-D-PROV-1): true when the RAW line was a CI command-echo of
    // run-step SCRIPT source (the GHA `\x1b[36;1m … \x1b[0m` command-echo wrapper), NOT an observed
    // runtime event. Set by the composed provenance hook (a package code-tier predicate) at the
    // parser layer, BEFORE any strategy sees the ANSI-stripped content. A per-line classification
    // attribute, NOT part of template identity: the parser uses it to demote `level` to Unknown so
    // a failure WORD in echoed shell source never confers an alerting level. `false` for every
    // non-echoed line.
    bool echoed_source{false};
    // ── The LEGIBILITY MARKER (DN-29.D16, L2 of DN-29.D15) ─────────────────────────────────────
    // EMPTY when the parse recognized at least one declared role (timestamp / level / component /
    // message, or an OTEL record). NON-EMPTY when it recognized NONE — and then it holds a WITNESS
    // KEY that WAS present in the input, so a consumer can see not merely THAT nothing was read but
    // WHAT arrived. A view into the raw line or the arena: no allocation, on any path.
    //
    // A DISTINCT SPECIES from `echoed_source` above, and the distinction is load-bearing rather
    // than pedantic. That one is OBSERVATION-provenance — how the line was observed. This is the
    // parse's own statement that it produced a value it could not interpret. Keeping it
    // schema-blind — it knows nothing about OTLP, ECS, or any format — is exactly what lets it
    // survive a schema move that defeats every format-aware check.
    //
    // ⚠ IT IS A STATEMENT, NEVER A VERDICT. A marked line is still emitted, still analysed, still
    // improvable — the marker MUST NOT be turned into a rejection. Rejecting the role-less
    // population would foreclose reading it better later, which is the direction DN-030 takes; and
    // `std::expected`'s error channel is the wrong home for it besides, because zero roles is a
    // SUCCESSFUL parse of low information, not a failure to produce a value.
    std::string_view no_role_witness_key;
    // OTEL trace context (SRC-D-OTEL-1), populated by a strategy that recognizes OTEL log records
    // (today: JsonStrategy on OTLP/JSON). Consumed downstream — trace-scoped n-gram grouping, and
    // the observed causal DAG for the declared vertex/edge (ADR-29.D2) — never serialized;
    // `present == false` for every non-OTEL input.
    OtelTraceContext trace{};
    // Declared ordinal observations (W1, SRC-D-W1-3), populated by a strategy that recognizes
    // declared structured numeric fields (today: JsonStrategy via kOrdinalFieldCatalog). A span
    // over arena-stable storage; empty for every non-ordinal line. Consumed metalog-side (W1
    // binning), never tokenized into the template.
    std::span<const OrdinalObservation> ordinals;
    // O4b Span Links (SRC-D-OTEL-9/SRC-D-OTEL-21): the span_ids this span declares a cross-trace
    // edge to (OTLP `links[]`), populated by the span strategy. A span over arena-stable storage;
    // empty for every line without links. Consumed metalog-side (distilled into the service
    // topology), never tokenized.
    std::span<const SpanId> linked_span_ids;
};

// The format-strategy interface — a representation-format parser (core) OR a dialect strategy
// shipped by a semantic package (§1.3/§2.3.1). Registered into the FormatDetector via the
// composition; probed once per line (sticky-latched). External packages implement this against the
// installed spi contract.
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
    // via ArenaAllocator::store_string(); their string_views remain valid until the arena is reset
    // or destroyed.
    [[nodiscard]] virtual std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const = 0;

    [[nodiscard]] virtual LogFormat format() const noexcept = 0;

    // Returns a [0,1] confidence score that this strategy matches the line. Used by FormatDetector
    // for majority-vote detection. Must be O(1).
    [[nodiscard]] virtual double confidence(std::string_view line) const noexcept = 0;
};

} // namespace insight::tokenization

// ════════════════════════════════════════════════════════════════════════════════════════════════
// The closed rule grammar (kSemanticGrammarVersion, §2.2) — POD rows, constexpr-constructible,
// canonically serializable for the composed identity hash. Canon owns every matcher ALGORITHM; a
// package owns only DATA (rows). A new matching capability is a grammar-version bump, never
// package-local parsing code.
// ════════════════════════════════════════════════════════════════════════════════════════════════
export namespace insight::semantic
{

// The grammar version — a component of the composed identity (§4). Bump on any grammar SHAPE change
// (a new row kind, a new closed-enum member, a serialization change). ASCII, versioned string id.
// grammar-2 (ADR-17, the first anticipated growth): the run-outcome row kinds (OutcomeTokenRow /
// OutcomeMarkerRow), the IntentMarkerRow payload-exclusion set, and the RemainderToClosingParen
// extractor — the shapes the Jenkins dialect genuinely needs.
// grammar-3 (ADR-23): the manifest gains `emits`, so the GENERATION projection enters the
// canonical serialization. A pure SERIALIZATION-COVERAGE change — no package's rows moved, no row
// kind is new (IntentEmitRow already existed and was already required by the DialectIntent concept)
// — but the serialization is part of the grammar shape, so it bumps here and the composed digest
// moves with it. That bump is DELIBERATE and documented (SRC-SID-3): before it, a generation-side
// change did not move semantic_identity at all, so two writers could claim one RulesetIdentity.
//
// The bump is NOT redundant with the `emits` content entering the digest, even though both move it
// here. The version is the SAFE guard: a future serialization change that adds no bytes (a field
// reorder, a widened enum) would move nothing on its own, and this string is the only thing that
// would. Keep bumping it on any serialization change, including one that also moves content.
//
// THIS TOKEN IS ASSIGNED AT SHIP, NEVER RESERVED — ADR-2, NORMATIVE, and it binds every
// token of this class (semantic-grammar-N, transport-catalog-N, canonicalization_version, wire
// versions). The value means "the Nth shape of this grammar", and WHICH feature causes the Nth
// shape is not knowable in advance: ship order is not plan order, so a growth can be gated,
// descoped or never built while an unrelated change lands tomorrow. ADR-17 had named "grammar-2 →
// grammar-3" for the gcc/make growth; that package does not exist, so the number was reserved by a
// plan and was never assignable. `emits` moved the shape and shipped, so it takes the token.
// ADR-2 carries the errata to 0026 (its substantive claim is untouched — it names the GROWTH,
// not a number).
//
// grammar-4 (ADR-22, T4): the per-row `format_gate : insight::LogFormat` becomes
// `dialect_gate : std::string_view` on all six gated row kinds, so six preimage sites move from a
// single `append_u8` to a length-prefixed `append_str`. That is a SERIALIZATION SHAPE change on the
// nose — the case the paragraph above says to keep bumping for, "including one that also moves
// content" — and ADR-2.3 sharpened exactly it for the sibling transport token.
//
// ⚠ ADR-22 says T4 "spends no version token". That reading is right about the two tokens
// it measured — `canonicalization_version` (the MASKING token; canon.api.cppm owns the value) and
// the MetaLog wire version, neither of which T4 touches — and it did not enumerate THIS one. The
// bump costs Eqya's sequencing nothing: `kSemanticGrammarVersion` appears at exactly one site,
// inside the `semantic_identity` preimage (compose.cpp), reaches no wire field and no MetaLog
// block, and the digest it feeds is moving anyway. It is not reserved by a plan — the shape
// shipped, so it takes the token (ADR-2).
//
// grammar-5 (ADR-17, the GitLab CI dialect): three shape changes, all forced by bytes GitLab
// emits and none expressible in grammar-4 — the `NumericFieldThenRemainder` extractor and its
// `NumericFieldThenPayload` emit dual (a marker payload preceded by a variable-length numeric
// field), and `OutcomeMarkerRow`'s shape discriminator + own verdict (a terminal line whose PREFIX
// carries the verdict and whose remainder is free-form). Two new closed-enum members, one new enum,
// and two new serialized row fields — every one of them a serialization shape change, so the token
// moves for the reason the paragraph above states.
inline constexpr std::string_view kSemanticGrammarVersion{"semantic-grammar-5"};

// ── The DIALECT coordinate (ADR-22 / ADR-22) ─────────────────────────────────────────────────────
// A DIALECT is a VOCABULARY over a HOST FORMAT (0064 clause 1): the format owns the LAYOUT rule
// (where one record's fields begin and end), the dialect owns the NAMES inside a layout another
// layer already delimited. `GitHub Actions` is `RawText` + the GHA workflow-command markers;
// `Jenkins` is `RawText` + the `[Pipeline]` markers. The host format is a `LogFormat`; the dialect
// is NOT, and never was.
//
// ⚠ THE GATE IS A NAME, NOT AN ENUM, and that is the whole point (0065 clause 1). A row's dialect
// gate is a COMPOSED PACKAGE NAME — "github", "jenkins" — checked against `ComposedPackage::name`.
// A canon-owned `Dialect` enum was refused in writing: it means every new dialect edits canon,
// which is the dependency this coordinate exists to invert, moved one type to the left and made
// HARDER to see because the new type's name would assert the problem was solved. A bare
// `bool requires_declared_dialect` was refused too — it is the right information content and the
// wrong encoding: `find_conflict`/`gates_intersect` would then collide two packages'
// identically-prefixed concrete rows and fatal composition on a duplicate that is not one. The name
// is redundant at the MANIFEST and load-bearing at the COMPOSED TABLE, because composition FLATTENS
// (`ComposedSemantics` exposes flat spans with no edge back to the contributing package).
//
// canon knows the FIELD and knows NO VALUE. The values arrive as package names, so canon knows
// there ARE dialects and must not know them.

// The "any dialect" sentinel: the rule fires regardless of the stream's declared dialect. The EMPTY
// string_view is that sentinel, exactly as `kAnyChannel` is on the channel axis — a package may
// never be named "" , so empty is unambiguously "any". A universal structural-role row
// (`##[group]`, fired on any content today) reproduces the pre-split UNGATED
// StructuralRoleRegistry::classify EXACTLY. A dialect-specific row (an intent marker, a level lift)
// names its OWN package (SRC-II-6 — a dialect never fires cross-dialect).
//
// NOTE the deliberate asymmetry with the CALLER's declaration, which is also empty when absent, and
// it is `kAnyChannel`'s verbatim: an empty *row* gate means "fires on any dialect"; an empty
// *declaration* means "the caller did not say" (Unspecified), which drops every concretely-gated
// row. Fail-closed on DEPTH, not on the run.
inline constexpr std::string_view kAnyDialect{};

// Does a row gated to `dialect_gate` fire on a stream whose caller declared `declared_dialect`?
// Deliberately a SECOND predicate beside `channel_admits` rather than one shared helper, on the
// house precedent set when `gates_intersect` and the since-retired `gate_matches` were kept
// apart with a comment saying why: the two coordinates answer different questions against different
// vocabularies (0029 D5's materialization vs 0064's vocabulary-over-a-host), each is documented
// against its own ADR, and a merged predicate would make a call site read as if the two axes were
// one.
//   * kAnyDialect row              → always fires (a universal role row is untouched)
//   * concrete row, same dialect   → fires
//   * concrete row, other dialect  → does NOT fire (a Jenkins row never fires on a GHA stream)
//   * concrete row, undeclared     → does NOT fire (fail-closed — "" matches no concrete gate)
// An UNKNOWN declared dialect (a typo) never reaches here: it is a HARD ERROR at stream resolution,
// listing the composed package names. An unknown dialect is a *mistake*; an absent one is a
// *choice* — they must not share a code path (ADR-23).
[[nodiscard]] constexpr bool dialect_admits(std::string_view dialect_gate,
                                            std::string_view declared_dialect) noexcept
{
    return dialect_gate == kAnyDialect || dialect_gate == declared_dialect;
}

// ── The INTENT CHANNEL coordinate (ADR-22) ───────────────────────────────────────────────────────
// `Medium = Dialect × IntentChannel` (ADR-22 — the first factor is the DIALECT, not the format;
// the medium gate below and the row-firing rule already read `dialect × channel`). The
// IntentChannel is the channel through which that intent was MATERIALIZED. Same intent, different
// channel ⇒ a new channel (not a new dialect). The channel vocabulary is package-declared DATA,
// exactly like the markers — core stays semantic-unaware (ADR-17): canon knows channel *markers*,
// it does not detect, route or map.
//
// It is NOT "the sink": a sink is LogCraft's output destination (console/file/http/SHM), a
// generator-side concept. This coordinate is a provenance fact about a STREAM, and a consumer that
// never heard of LogCraft still has to declare it (ADR-22).
//
// Why the coordinate exists (measured on REAL bytes, not theorized): canon receives the SAME GHA
// Step banner in two materializations under ONE dialect gate — the runner's `##[group]Run <cmd>`
// (GHA's real channel) and the workflow-command-stripped `Run <cmd>` (our own lattice-experiment
// ablation, which canon must also read). In the annotated channel a line starting with `Run ` is
// ordinary PROSE, so without a channel the stripped row mints a PHANTOM Step quantum out of
// prose: 9.05 % of 22 030 real annotated logs, and the shipped Action feeds exactly that form. The
// stripped row cannot simply be deleted — the ablation genuinely uses the bare prefix as its banner
// — so gating is the only fix.
//
// canon MUST NOT infer the channel: it is always TOLD (ADR-22). A CALLER may derive its own
// declaration (Acquisition peeks a whole file — deterministic given the bytes); that inference
// lives at the caller, never here.
//
// The channel is EXTRINSIC and that is the whole reason it is declared rather than detected: no
// byte carries it. A prose line is byte-identical across channels, and both GHA channels share one
// dialect gate — the discriminating evidence is absent from the very lines that get misrecognized.
// So someone must SAY, and only the caller who acquired the stream knows (ADR-22; never
// auto-detect — a prefix heuristic makes content non-deterministic under streaming).
//
// The "any channel" sentinel: the row fires regardless of the stream's declared channel — the
// DEGENERATE case every single-materialization dialect (Jenkins, test_frameworks) uses, so it is
// untouched by all of this (ADR-22, minimal blast radius). The EMPTY string_view is that
// sentinel, mirroring kAnyDialect = the empty view: a package may never declare an empty channel
// name (all_channels_named enforces it), so empty is unambiguously "any".
//
// NOTE the deliberate asymmetry with the CALLER's declaration, which is also empty when absent: an
// empty *row* gate means "fires on any channel"; an empty *declaration* means "the caller did not
// say" (Unspecified). channel_admits() below is where the two meet, and it is why an undeclared
// stream keeps its kAnyChannel rows but loses every concretely-gated one — fail-closed on DEPTH,
// not on the run. Never default an undeclared stream to a concrete channel: that is precisely the
// defect this killed (both GHA rows living at once).
inline constexpr std::string_view kAnyChannel{};

// Does a row gated to `channel_gate` fire on a stream whose caller declared `declared_channel`?
// The ONE predicate the whole coordinate reduces to. Total, pure, and deliberately tiny:
//   * kAnyChannel row              → always fires (the degenerate dialect is untouched)
//   * concrete row, same channel   → fires
//   * concrete row, other channel  → does NOT fire (this is the phantom fix)
//   * concrete row, undeclared     → does NOT fire (fail-closed — "" matches no concrete gate)
// An UNKNOWN declared channel (a typo) never reaches here: it is a HARD ERROR at composition,
// listing the declared vocabulary. An unknown channel is a *mistake*; an absent channel is a
// *choice* — they must not share a code path.
[[nodiscard]] constexpr bool channel_admits(std::string_view channel_gate,
                                            std::string_view declared_channel) noexcept
{
    return channel_gate == kAnyChannel || channel_gate == declared_channel;
}

// How a matched marker's payload is extracted (§2.2 — a CLOSED extractor enum; the algorithm lives
// in core). The alignment CLASS / discriminant are then derived in core by canonicalize_intent /
// discriminant_of. A new extractor is a grammar-version bump, part of the identity.
enum class PayloadExtract : std::uint8_t
{
    None = 0,             ///< no payload (structural markers carry none)
    RemainderAfterPrefix, ///< the content after the matched prefix, verbatim (intent markers)
    // grammar-2 (ADR-17 / studies/006): the content after the matched prefix up to a REQUIRED
    // line-final ')' — the Jenkins named-block-open form `[Pipeline] { (<name>)`. A line that does
    // not end with ')' does not match the row at all (strict — an un-named `[Pipeline] {` wrapper
    // is scaffold, not a quantum). Nested parens stay inside the payload (`{ (Branch: test (lts))`
    // → `Branch: test (lts)`): only the single final ')' is the delimiter.
    RemainderToClosingParen,
    // grammar-5 (ADR-17): the content after the matched prefix is a non-empty run of ASCII
    // digits, then a single ':', then the payload — the GitLab section marker
    // `section_start:<unix-ts>:<name>[<options>]`. A variable-length field to SKIP is what no other
    // extractor can express: `RemainderAfterPrefix` would put the epoch inside the payload, and
    // `IntentMarker::name` is the raw payload that `compare_skeletons` keys on (ADR-18), so every
    // run would carry a different name and nothing would ever align. Lengthening the prefix cannot
    // absorb a variable-length field.
    //
    // The payload ENDS AT THE FIRST '\r' and then loses a trailing `[…]` option group — two
    // producer-owned boundaries, bundled into the extractor exactly as RemainderToClosingParen
    // bundles its required ')':
    //   * '\r' TERMINATES the marker. GitLab closes it with `\r\x1b[0K` (CR + erase-line) and may
    //     continue the SAME line with the section's human-readable header.
    //     SRC-D-TID-11 — see canon.api.cppm (normalize()) for the contract.
    //     Local: the strip kills the escape and leaves the CR, so a rule that
    //     merely trimmed a TRAILING
    //     CR would name a section `build_tools_section\rTools build`. It is handled HERE rather
    //     than in the dialect strategy because the strategy is not the only recognition path —
    //     eidos's `strip_leading_timestamp` pre-pass calls `recognize()` with no strategy in the
    //     loop (ADR-22) — and the extractor is the one site both paths share, so
    //     agreement is by construction rather than by coordination.
    //   * a trailing `[…]` option group (`[collapsed=true]`, `[hide_duration=true,collapsed=true]`)
    //     — without the drop, a producer toggling `collapsed` RENAMES a section.
    // A shape failure (no digits, no ':', an empty payload) means the ROW DOES NOT MATCH, so a
    // malformed producer marker — the unexpanded `%s` / `$(date +%s)` class, 95 occurrences in
    // marker_corpus_v1 — is declined rather than mis-parsed. Digit-length is deliberately
    // unconstrained: a width window would mirror the study instrument that measured it, and it is
    // anchoring, not the stamp, that excludes the echoed phantoms.
    NumericFieldThenRemainder,
};

// How a WRITER row materializes an intent's payload into log bytes (studies/008,
// shared_intent_declaration §2.3/§3.2) — the CLOSED dual of PayloadExtract: each emit value is the
// exact inverse of one extractor, so recognize(render_row(row, payload)) recovers the payload. This
// is the generation side of "one declaration, two projections": canon RECOGNIZES via
// PayloadExtract, LogCraft GENERATES via PayloadEmit, both rows-as-data (SRC-SID-2 — never a
// render() callable, which is un-hashable and lets the two projections diverge). A new emit shape
// is a grammar-version bump, part of the identity, exactly as a new extractor is.
enum class PayloadEmit : std::uint8_t
{
    None = 0,           ///< dual of PayloadExtract::None — the prefix alone (structural markers)
    PayloadAfterPrefix, ///< dual of RemainderAfterPrefix — the prefix, then the payload verbatim
    PayloadThenClosingParen, ///< dual of RemainderToClosingParen — prefix, payload, then the final
                             ///< ')'
    // grammar-5 (ADR-17): dual of NumericFieldThenRemainder — prefix, a single PLACEHOLDER digit
    // `0`, ':', then the payload. The placeholder is deliberate and its consequence is declared: a
    // generated GitLab marker carries no wall-clock, so a synthetic stream has no section duration.
    // Emitting a VARYING stamp is a step_duration capability, not a writer detail.
    //
    // The two reader-side suffix drops have no generation dual — the writer never emits a producer
    // option group and never emits the CR terminator — the same asymmetry `payload_excludes`
    // already carries. So the round trip closes on every payload the writer can produce, which is
    // what G2 asserts.
    PlaceholderNumericFieldThenPayload,
};

// Which core location-matching ALGORITHM a LocationRow selects + parameterizes (§2.2 — a CLOSED
// enum; the three families location_recognizer implements today). A new dialect needing a new
// family is a grammar-version bump, part of the identity.
enum class LocationMatchKind : std::uint8_t
{
    // `<base>.test.<ext>` / `<base>.spec.<ext>` with ext ∈ params.extensions
    // (jest/vitest/playwright).
    TestSpecExtension = 0,
    // basename `<prefix>*` or `*<prefix-reversed>` + a fixed extension: pytest
    // `test_*.py`/`*_test.py`.
    PrefixAndExtension,
    // any of a set of fixed suffixes, word-boundary-terminated: go `_test.go`, ruby
    // `_spec.rb`/`_test.rb`.
    SuffixSet,
};

// ── Row kinds (one per knowledge surface, §2.2) ──────────────────────────────────────────────────

// A structural-role rule: a line-anchored prefix announces a role (§1.2 — `##[group]` →
// GroupBegin).
struct StructuralRoleRow
{
    std::string_view prefix;
    insight::StructuralRole role;
    // kAnyDialect = fire on any dialect (the pre-split ungated behavior); otherwise the OWNING
    // package's name. Filtered into the stream view once, at resolution — never consulted per line.
    std::string_view dialect_gate{kAnyDialect};
};

// An intent-marker rule: a prefix opens a behavioural quantum (§1.2 — `Complete job name: ` → Job).
// Carries the dialect's HIERARCHY (kind + child_order — the ADR-18 level-typed alignment
// declaration) and the payload extractor. FORMAT-GATED by construction (SRC-II-6).
struct IntentMarkerRow
{
    std::string_view prefix;
    insight::tokenization::IntentMarkerKind kind;
    insight::tokenization::ChildOrder child_order;
    // ADR-22 — the DIALECT gate: the owning package's name (an intent marker is always
    // concretely gated by construction, SRC-II-6). Filtered into the stream view at resolution.
    std::string_view dialect_gate{kAnyDialect};
    PayloadExtract extract;
    // grammar-2 (ADR-17 / studies/006): a CLOSED exclusion set over the extracted payload — the
    // row does NOT fire when the payload's leading token matches an entry (entry == payload, or
    // payload starts with entry followed by a space). The Jenkins step form `[Pipeline] <verb>`
    // needs it: the verb set is open (any pipeline step), the structural tokens that share the
    // prefix (`{`, `}`, `stage`, `node`, `parallel`, `//`, `End of Pipeline`) are closed dialect
    // data. Empty for rows without exclusions (every pre-grammar-2 row). The span points at
    // package-static constexpr storage (SRC-SP-7 lifetime); serialized into semantic_identity.
    std::span<const std::string_view> payload_excludes;
    // ADR-22 — the CHANNEL gate: this row fires only on a stream the caller declared as this
    // IntentChannel. kAnyChannel (the default) = fires on any channel, so every
    // single-materialization dialect is untouched. REQUIRED exactly when one channel's marker
    // occurs as ordinary CONTENT in a sibling channel (the GHA `Run ` case) — which is why `Run `
    // needs a gate and `##[error]` does not. Serialized into semantic_identity, so a channel-row
    // change moves the digest exactly as a prefix change does.
    std::string_view channel_gate{kAnyChannel};
};

// A generation-template rule (studies/008, shared_intent_declaration §3.2) — the WRITER dual of
// IntentMarkerRow. Carries the SAME dialect hierarchy (kind + child_order, the ADR-18
// declaration) and the SAME medium gate (dialect_gate × channel_gate = the Medium the line
// materializes into — the O2 medium axis; the two GHA Step media `Run ` / `##[group]Run ` are two
// emit rows sharing kind, differing in prefix). No payload_excludes: the writer only ever emits a
// real intent, never an excluded structural token, so the exclusion set is a reader-side concern
// with no generation dual. Rows-as-data (SRC-SID-2): the emit shape is the closed PayloadEmit enum,
// never a callable. Content-hashable exactly as IntentMarkerRow is, so a generation-side change
// moves semantic_identity as a recognition change does (G4 — the hash wiring lands with the ADR at
// ratification).
struct IntentEmitRow
{
    std::string_view prefix;
    insight::tokenization::IntentMarkerKind kind;
    insight::tokenization::ChildOrder child_order;
    // ADR-22 — the DIALECT gate, symmetric to IntentMarkerRow's: the Medium the line
    // materializes into is `dialect × channel`, so both projections name the same pair.
    std::string_view dialect_gate{kAnyDialect};
    PayloadEmit emit;
    // ADR-22 — the CHANNEL gate, symmetric to IntentMarkerRow's. The writer's dual of the
    // reader's question: not "which prefix do I match" but "which IntentChannel am I materializing
    // into". This is what dissolves the apparent C2 contradiction — the writer never inspects
    // prefixes (the SRC-SID-1 smell C2 exists to kill), it applies the channel adaptation the
    // Medium names. Paired with the reader's gate by paired_writer_row, so the two projections
    // cannot drift onto different channels.
    //
    // It is also the MEDIUM SELECTOR's input (ADR-22): a writer picks the emit row whose
    // channel_gate matches the channel it was told to render, never the first row that matches by
    // array order — so `##[group]Run ` is reachable and the output does not depend on canon's row
    // ordering.
    std::string_view channel_gate{kAnyChannel};
};

// A level-lift rule: a prefix lifts the line's LogLevel (§1.2 — `##[error]` → Error). Consumed by
// the dialect strategy that owns the rows (level lift happens inside parse(), before raw-text
// inference).
struct LevelLiftRow
{
    std::string_view prefix;
    insight::LogLevel level;
    std::string_view dialect_gate{kAnyDialect}; // ADR-22 — the owning package's name
};

// A location rule: recognizes a test-file WHERE coordinate (§5.3/SRC-II-8). `kind` selects the core
// matching algorithm; the params are the dialect-independent file-naming vocabulary it walks. The
// spans point at package-static constexpr arrays (SRC-SP-7 immutable-release lifetime). Not every
// param is used by every kind — `extensions` for TestSpecExtension/PrefixAndExtension, `suffixes`
// for SuffixSet, `prefixes` for PrefixAndExtension (the basename `test_`/`_test` forms).
struct LocationRow
{
    LocationMatchKind kind;
    std::span<const std::string_view> infixes; // `.test.` / `.spec.` (TestSpecExtension)
    std::span<const std::string_view>
        extensions; // ts/tsx/js/… (TestSpec) or `.py` (PrefixAndExtension)
    std::span<const std::string_view>
        prefixes; // `test_` / `_test` basename forms (PrefixAndExtension)
    std::span<const std::string_view> suffixes; // `_test.go` / `_spec.rb` / `_test.rb` (SuffixSet)
};

// ── Run-outcome rows (grammar-2, ADR-17 / insight_run_outcome_model.md §4) ──
// The dialect's run-verdict MAPPING — native verdict string → the core-owned RunOutcome — plus the
// console-tail fallback marker. Matcher algorithms live in core (map_outcome_token /
// scan_run_outcome / resolve_run_outcome, the SRC-D-OUT-RUN-1 precedence); a package ships only
// rows, and either set may be empty (GHA ships tokens but no marker — it has no run-verdict console
// line).

// One native verdict token → RunOutcome (byte-exact, format-gated). Consumed on BOTH resolution
// rungs: the authoritative `--*-outcome` side-input token and the console-tail marker's extracted
// remainder map through the SAME set.
struct OutcomeTokenRow
{
    std::string_view token; // the native dialect verdict string, verbatim ("UNSTABLE", "cancelled")
    insight::RunOutcome outcome;
    std::string_view dialect_gate{kAnyDialect}; // ADR-22 — the owning package's name
};

// How a matched OutcomeMarkerRow yields its verdict (grammar-5, ADR-17 — a CLOSED shape enum, the
// PayloadExtract discipline applied to the outcome walker). Two dialects, two genuinely different
// terminal-line grammars, and no prefix choice unifies them.
enum class OutcomeMarkerShape : std::uint8_t
{
    // The verdict is the line's REMAINDER after the prefix and must be a single ASCII word, mapped
    // through the composed OutcomeTokenRow set (Jenkins `Finished: SUCCESS`).
    RemainderToken = 0,
    // The PREFIX itself announces the verdict; the remainder is free-form and is not read. GitLab's
    // failure line is `ERROR: Job failed: exit code 1` / `… : exit status 137` /
    // `… (system failure): <reason>` — 144 of 619 traces in marker_corpus_v1, and unrecognizable
    // under RemainderToken for any prefix: `ERROR: Job failed: ` leaves `exit code 1`,
    // `ERROR: Job failed` leaves `: exit code 1`, `ERROR: Job ` leaves `failed: exit code 1`.
    PrefixIsVerdict,
};

// The console-tail terminal-verdict line (Jenkins: `Finished: `; GitLab: `Job succeeded` /
// `ERROR: Job failed`). Core matches it line-anchored on the resolved view; the LONGEST matching
// prefix wins within a line and the LAST match in line order wins across lines (a run has one
// terminal verdict; deterministic integer index, no dependence on declaration order).
struct OutcomeMarkerRow
{
    std::string_view prefix;
    std::string_view dialect_gate{kAnyDialect}; // ADR-22 — the owning package's name
    OutcomeMarkerShape shape{OutcomeMarkerShape::RemainderToken};
    // The verdict this row's PREFIX announces. READ ONLY under PrefixIsVerdict — a RemainderToken
    // row's verdict comes from its remainder, through the token rows, and this field is inert for
    // it. That conditionality is what the type cannot express, which is why it is written here:
    // RunOutcome has no "absent" member (Unknown is a real verdict a row may legitimately carry —
    // the Jenkins NOT_BUILT precedent), so a sentinel would be a lie rather than a guard.
    insight::RunOutcome outcome{insight::RunOutcome::Unknown};
};

// A value-class rule (the grammar SEAT for package value classes, §5). No package ships domain
// value classes — the composed ValueClassRegistry is the VIEW over the core catalogs
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
    std::string_view
        schedule_id;    // the ordinal schedule id when cls is an ordinal class (else empty)
    std::int64_t scale; // unit→canonical factor when applicable (else 0)
};

// ── The strategy factory (code tier)
// ────────────────────────────────────────────────────────────── A package that ships a dialect
// format strategy exports a factory: nullable (data-only packages return an empty
// manifest.strategy). The composition registers the produced strategy into the FormatDetector via
// the existing register_strategy seam.
using StrategyFactory = std::unique_ptr<insight::tokenization::IFormatStrategy> (*)();

// A raw-line provenance hook (the echoed-source code tier, §2.3.2 — a discouraged escape-hatch
// recognizer, signature-constrained to a pure `bool(std::string_view)` byte function). Consulted by
// LogParser on the RAW (ANSI-bearing) line, independent of the routed strategy, so it reproduces
// the pre-split strategy-independent is_echoed_source_line EXACTLY. Nullable.
using ProvenanceHook = bool (*)(std::string_view raw_line) noexcept;

// ── The manifest (§2.5) ─────────────────────────────────────────────────────────────────────────
// Each package exports one `constexpr SemanticPackageManifest kManifest` from its own named module.
// No register_*() methods, no mutable registration state — the composition is a pure function of
// the manifest SET.
struct SemanticPackageManifest
{
    std::string_view name;    // "github"
    std::string_view version; // "1.0.0" — immutable release discipline (SRC-SP-7)
    std::span<const StructuralRoleRow> roles;
    std::span<const IntentMarkerRow> markers;
    // grammar-3 (ADR-23): the GENERATION projection — the writer dual of `markers`, declared
    // here so it enters `semantic_identity`. `IntentEmitRow` and the DialectIntent concept already
    // made a package ship these rows; what was missing is that the manifest — the object compose()
    // serializes into the identity — had no member for them, so a generation-side change did NOT
    // move the comparability hash and two documents from DIFFERENT writers could claim the same
    // RulesetIdentity. That is the silent divergence SRC-SID-2 exists to forbid. EMPTY for a
    // package that ships no markers (test_frameworks: pure location data, no intents to recognize
    // OR generate). Every package with markers declares the SAME span its Dialect type exposes as
    // `emit_markers` — one array, two views, so the pairing the concept static_asserts is the
    // pairing the identity hashes.
    std::span<const IntentEmitRow> emits;
    std::span<const LevelLiftRow> level_lifts;
    std::span<const LocationRow> locations;
    std::span<const ValueClassRow> value_classes;
    // grammar-2 (ADR-17): the run-outcome vocabulary. Both may be empty (§3.2 — a package
    // declares exactly the outcome surfaces its dialect actually has).
    std::span<const OutcomeTokenRow> outcome_tokens;
    std::span<const OutcomeMarkerRow> outcome_markers;
    // ADR-22 — the package's declared INTENT CHANNEL vocabulary: every materialization this
    // dialect's intents can be rendered into ("annotated" / "stripped" for GHA). EMPTY for a
    // single-materialization dialect (Jenkins, test_frameworks), whose rows are all kAnyChannel —
    // the degenerate case. Only the dialect that HAS multiple materializations declares channels.
    // This is the vocabulary a caller's `--channel` is validated against: declared ⇒ fires; unknown
    // ⇒ HARD ERROR listing these names. The span points at package-static constexpr storage
    // (SRC-SP-7 lifetime); serialized into semantic_identity alongside the rows it gates.
    std::span<const std::string_view> channels;
    StrategyFactory strategy{nullptr};     // nullable — the dialect format-strategy code tier
    ProvenanceHook echoed_source{nullptr}; // nullable — the raw-line echoed-source code tier
};

// ════════════════════════════════════════════════════════════════════════════════════════════════
// The generation projection API (studies/008, shared_intent_declaration §3.2) — the WRITER half of
// "one declaration, two projections". render_row is the pure inverse of core's payload extraction;
// paired_writer_row + all_intents_paired + the DialectIntent concept make bidirectionality a
// compile-time obligation (a package that ships a recognition row without its paired generation row
// does not satisfy DialectIntent → does not compile). This is the C2 mechanism (§3.2): the
// round-trip obligation turned into a TYPE obligation. render_row/paired_writer_row are the surface
// the studies/008 G2 closure kit round-trips against; the concept is the structural guarantee
// behind it.
// ════════════════════════════════════════════════════════════════════════════════════════════════

// The numeric field a PlaceholderNumericFieldThenPayload row materializes, separator included. One
// digit, and the value is not a stamp: the reader skips the field whatever it holds, so the writer
// owes only a SHAPE-VALID field, and the shortest valid one makes the absence of a wall-clock
// legible in the rendered bytes rather than hidden behind a plausible-looking epoch.
inline constexpr std::string_view kPlaceholderNumericField{"0:"};

// render_row — the writer-row expansion. PURE: a function of (row, payload) ONLY — no RNG, no
// envelope, no engine state, no wall-clock — so it lives on the canon (recognition) side and
// LogCraft merely calls it to materialize. The exact inverse of the PayloadExtract algorithm: for
// every emit shape, recognize(render_row(row, payload), composed) recovers
// (row.kind, row.child_order, payload) — the G2 round-trip. Allocates the result string (the only
// allocation; caller-owned).
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
    case PayloadEmit::PlaceholderNumericFieldThenPayload:
        out.reserve(row.prefix.size() + kPlaceholderNumericField.size() + payload.size());
        out.append(row.prefix);
        out.append(kPlaceholderNumericField);
        out.append(payload);
        break;
    }
    return out;
}

// render_outcome — the writer dual of the run-outcome console-tail scan (T5 §3.1: the Jenkins
// epilogue `Finished: <RESULT>`). PURE, same contract as render_row: a function of (row, token)
// only — no RNG, no clock — homed canon-side so the epilogue's byte shape has ONE owner (the same
// rows scan_run_outcome matches; a LogCraft-side spelling of the prefix would be the third-spelling
// defect the retrofit killed). NO new row kind: the outcome rows are already symmetric literals.
//
// `token` is read only under RemainderToken — the exact conditionality OutcomeMarkerRow::outcome
// already documents, mirrored: a RemainderToken row's verdict rides its remainder (prefix + the
// native token), while a PrefixIsVerdict row's PREFIX announces the verdict and its remainder is
// free-form and unread, so the minimal shape-valid rendering is the prefix alone. The conformance
// law (canon.conformance, outcome.round_trip): scan_run_outcome over the rendered line recovers
// the row/token's own verdict.
[[nodiscard]] inline std::string render_outcome(const OutcomeMarkerRow& row, std::string_view token)
{
    std::string out;
    switch (row.shape)
    {
    case OutcomeMarkerShape::RemainderToken:
        out.reserve(row.prefix.size() + token.size());
        out.append(row.prefix);
        out.append(token);
        break;
    case OutcomeMarkerShape::PrefixIsVerdict:
        out.reserve(row.prefix.size());
        out.append(row.prefix);
        break;
    }
    return out;
}

// paired_writer_row — the reader→writer pairing. Given a recognition row, returns the generation
// row that materializes into a line THAT row recognizes: same prefix, kind, and MEDIUM. The Medium
// is `dialect × IntentChannel` (ADR-22 / ADR-22), so the pairing matches on BOTH
// gates — a reader
// row gated to one channel must pair with the writer row gated to the SAME channel, or the two
// projections would silently describe different materializations (a C2 violation the pairing exists
// to make impossible). Well-defined iff every reader row has exactly one paired writer row — the
// property all_intents_paired enforces. Returns nullptr when unpaired (a violation the static check
// rejects; exposed so a runtime closure kit can assert on it too). constexpr — usable in the
// consteval check and at runtime.
[[nodiscard]] constexpr const IntentEmitRow*
paired_writer_row(const IntentMarkerRow& reader, std::span<const IntentEmitRow> emits) noexcept
{
    for (const IntentEmitRow& emit : emits)
    {
        if (emit.prefix == reader.prefix && emit.kind == reader.kind &&
            emit.dialect_gate == reader.dialect_gate && emit.channel_gate == reader.channel_gate)
        {
            return &emit;
        }
    }
    return nullptr;
}

// ── The IntentChannel static checks (ADR-22 — fail-closed at COMPILE time) ──
// A package's channel vocabulary and its rows' gates must agree, and the check is consteval so the
// disagreement is a build error in the package's own TU — the same seat, and the same posture, as
// the DialectIntent concept. This is the compile-time half of "an unknown channel is a hard error":
// a gate naming a channel the package never declared cannot ship at all.

// Every declared channel name is non-empty. The empty name IS kAnyChannel, so declaring it would
// make "any" and "this specific one" the same value — the sentinel must stay unambiguous.
[[nodiscard]] consteval bool all_channels_named(std::span<const std::string_view> channels) noexcept
{
    return std::ranges::all_of(channels,
                               [](std::string_view channel) noexcept { return !channel.empty(); });
}

// Every row's channel_gate is kAnyChannel or one of the package's DECLARED channels. Catches the
// typo
// (`.channel_gate = "anotated"`) at compile time, in the package that made it.
[[nodiscard]] consteval bool
all_channel_gates_declared(std::span<const IntentMarkerRow> markers,
                           std::span<const IntentEmitRow> emits,
                           std::span<const std::string_view> channels) noexcept
{
    const auto declared{[channels](std::string_view gate) noexcept
                        { return gate == kAnyChannel || std::ranges::contains(channels, gate); }};
    return std::ranges::all_of(markers, [&declared](const IntentMarkerRow& row) noexcept
                               { return declared(row.channel_gate); }) &&
           std::ranges::all_of(emits, [&declared](const IntentEmitRow& row) noexcept
                               { return declared(row.channel_gate); });
}

// ── The DIALECT static check (ADR-22 — fail-closed at COMPILE time) ──
// Every gated row's `dialect_gate` is either kAnyDialect or the package's OWN name. Both halves are
// load-bearing:
//   * a gate naming a package this manifest is not — even a real, composed one — is a package
//     reaching across a boundary it does not own, and `ComposedSemantics` flattens, so nothing
//     downstream could tell that apart from a legitimate row;
//   * a TYPO (`.dialect_gate = "gihub"`) becomes a build error in the package that made it, rather
//     than a row that silently never fires under any declaration.
// Symmetric with `all_channel_gates_declared`, and for its reason: an unknown NAME must fail at the
// earliest seat that can see it, and for a package's own rows that seat is its own TU.
//
// Takes the MANIFEST rather than six spans: the check's whole content is "every gated row against
// `.name`", and threading the six spans by hand would let a package add a row kind to the manifest
// and forget it here — the omission being invisible, which is the failure mode the check exists to
// remove.
[[nodiscard]] consteval bool
all_dialect_gates_owned(const SemanticPackageManifest& manifest) noexcept
{
    const auto owned{[&manifest](std::string_view gate) noexcept
                     { return gate == kAnyDialect || gate == manifest.name; }};
    return std::ranges::all_of(manifest.roles, [&owned](const StructuralRoleRow& row) noexcept
                               { return owned(row.dialect_gate); }) &&
           std::ranges::all_of(manifest.markers, [&owned](const IntentMarkerRow& row) noexcept
                               { return owned(row.dialect_gate); }) &&
           std::ranges::all_of(manifest.emits, [&owned](const IntentEmitRow& row) noexcept
                               { return owned(row.dialect_gate); }) &&
           std::ranges::all_of(manifest.level_lifts, [&owned](const LevelLiftRow& row) noexcept
                               { return owned(row.dialect_gate); }) &&
           std::ranges::all_of(manifest.outcome_tokens,
                               [&owned](const OutcomeTokenRow& row) noexcept
                               { return owned(row.dialect_gate); }) &&
           std::ranges::all_of(manifest.outcome_markers,
                               [&owned](const OutcomeMarkerRow& row) noexcept
                               { return owned(row.dialect_gate); });
}

// all_intents_paired — the bidirectionality predicate (SID: no reader without a writer). consteval
// so a package static_asserts it over its constexpr rows: every recognition marker has a paired
// generation row. The value half of the C2 type obligation (§3.2).
[[nodiscard]] consteval bool all_intents_paired(std::span<const IntentMarkerRow> markers,
                                                std::span<const IntentEmitRow> emits) noexcept
{
    return std::ranges::all_of(markers, [emits](const IntentMarkerRow& reader) noexcept
                               { return paired_writer_row(reader, emits) != nullptr; });
}

// The DialectIntent concept (§3.2) — bidirectionality as a TYPE obligation. A type models
// DialectIntent iff it exposes BOTH projections as constexpr row spans (recognition `markers` +
// generation `emit_markers`) AND every reader row is paired (all_intents_paired). A dialect whose
// type ships a recognition row without its generation row does NOT satisfy the concept → does not
// compile where the concept is required. This is the compile-time half of G2; render_row + the
// runtime round-trip are the value half.
template <typename Dialect>
concept DialectIntent = requires {
    { Dialect::markers } -> std::convertible_to<std::span<const IntentMarkerRow>>;
    { Dialect::emit_markers } -> std::convertible_to<std::span<const IntentEmitRow>>;
} && all_intents_paired(Dialect::markers, Dialect::emit_markers);

} // namespace insight::semantic
