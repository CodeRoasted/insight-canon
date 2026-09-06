// invariant: the PROVIDER contract a semantic package implements: the closed rule grammar, the
// format-strategy interface with its ParsedLine intermediate, and the package manifest.
// invariant: the facade does NOT re-export this module; a package imports THIS plus
// insight.canon.api, never a sealed detail shard.
// refs: ADR-17
module;

export module insight.canon.spi;
import insight.canon.internal;
import insight.canon.api;

// invariant: the code tier is installed HERE so an external package can implement a dialect
// strategy against a versioned, installed contract.
// refs: ADR-17, ADR-17.D4
export namespace insight::tokenization
{

// invariant: the timestamp and its provenance are ONE value, assigned through one site and never
// independently settable.
// invariant: there is deliberately NO implicit conversion from std::optional<Timestamp>, so every
// assignment names its provenance and the compiler enforces it.
// refs: DN-29.D12, DN-29.D14
class EventTime
{
  public:
    EventTime() = default;

    // post: the INFERRED species — bytes whose authorship is ambiguous, and the default a
    // representation strategy produces.
    [[nodiscard]] static EventTime parsed(std::optional<Timestamp> value) noexcept
    {
        EventTime out;
        out.value_ = value;
        return out;
    }

    // post: the DECLARED species — a schema field whose MEANING is the event time, never content
    // that resembles one.
    // invariant: a declared time outranks a transport stamp where a parsed one does not.
    // refs: DN-29.D12
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
    // pre: the optional's own — these two reproduce std::optional::operator* and operator->
    // verbatim, precondition included.
    // note: the read surface forwards, so only WRITERS were made to say more.
    [[nodiscard]] const Timestamp& operator*() const noexcept
    {
        // note: the directive below is the FORWARDING contract, not a missing check.
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        return *value_;
    }
    [[nodiscard]] const Timestamp* operator->() const noexcept
    {
        // note: the directive below is the FORWARDING contract, not a missing check.
        // NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        return &*value_;
    }

    // invariant: true only for the declared species; an absent time is never declared, so this
    // cannot disagree with has_value().
    [[nodiscard]] bool is_declared() const noexcept
    {
        return declared_ && value_.has_value();
    }

  private:
    std::optional<Timestamp> value_;
    bool declared_{false};
};

// invariant: the intermediate a format strategy produces; every string_view field points into
// arena-managed storage.
// invariant: those views stay valid until the owning ArenaAllocator is reset or destroyed.
struct ParsedLine
{
    std::string_view raw_line;
    EventTime timestamp;
    // invariant: declared() when the bytes came from a position whose MEANING is the level;
    // inferred() when canon read it out of message content.
    // refs: DN-32.D3
    EventLevel level;
    // invariant: component is the low-cardinality functional source; host is the high-cardinality
    // node identity, and it is hors-cube.
    std::string_view component;
    std::string_view host;
    std::string_view content;
    // invariant: true when the RAW line was a CI command-echo of run-step SCRIPT source, never an
    // observed runtime event.
    // invariant: set by the composed provenance hook at the parser layer, BEFORE any strategy sees
    // the ANSI-stripped content.
    // invariant: a per-line classification attribute, NOT part of template identity; false for
    // every non-echoed line.
    // refs: SRC-D-PROV-1
    bool echoed_source{false};
    // invariant: EMPTY when the parse recognized at least one declared role; NON-EMPTY when it
    // recognized NONE.
    // invariant: when non-empty it holds a witness KEY that WAS present in the input, as a view
    // into the raw line or the arena — no allocation on any path.
    // invariant: a STATEMENT, never a verdict: a marked line is still emitted and still analysed,
    // and the marker MUST NOT become a rejection.
    // refs: DN-29.D15, DN-29.D16, DN-30
    std::string_view no_role_witness_key;
    // invariant: populated only by a strategy that recognizes OTEL log records; present is false
    // for every non-OTEL input.
    // invariant: consumed downstream, never serialized.
    // refs: SRC-D-OTEL-1, ADR-29.D2
    OtelTraceContext trace{};
    // invariant: a span over arena-stable storage, empty for every non-ordinal line, consumed
    // metalog-side and never tokenized into the template.
    // refs: SRC-D-W1-3
    std::span<const OrdinalObservation> ordinals;
    // invariant: the span_ids this span declares a cross-trace edge to, empty for every line
    // without links, consumed metalog-side and never tokenized.
    // refs: SRC-D-OTEL-9, SRC-D-OTEL-21
    std::span<const SpanId> linked_span_ids;
};

// invariant: a representation-format parser OR a package's dialect strategy, registered into the
// FormatDetector and probed once per line, sticky-latched.
// refs: ADR-17
class IFormatStrategy
{
  public:
    IFormatStrategy() = default;
    IFormatStrategy(const IFormatStrategy&) = delete;
    IFormatStrategy& operator=(const IFormatStrategy&) = delete;
    IFormatStrategy(IFormatStrategy&&) = delete;
    IFormatStrategy& operator=(IFormatStrategy&&) = delete;
    virtual ~IFormatStrategy() = default;

    // pre: the input string_view outlives the call — raw_line in the result borrows from it.
    // post: component and content are copied into the supplied arena and stay valid until it is
    // reset or destroyed.
    // post: content is a TOTAL projection — content.empty() implies the line has no message bytes
    // beyond the header this strategy parsed.
    // invariant: a strategy that cannot satisfy projection totality on a line MUST NOT claim that
    // line.
    // invariant: removing bytes from content needs ENTITLEMENT BY GRAMMAR — the claim predicate
    // structurally validated that byte range.
    // invariant: and ENTITLEMENT BY LAYER — that the range belongs to the RECORD rather than to a
    // delivery ENVELOPE is decidable from the line's own bytes.
    // invariant: a leading field before an unconstrained remainder is a STAMP, not a header, and a
    // stamp leaves content only by a declared transport peel.
    // invariant: a strategy that recognizes a stamp MAY read it into a typed ParsedLine field and
    // MUST leave its bytes in content.
    // refs: ADR-16.D9, DN-43.D6, DN-43.D11, ADR-23.D3, ADR-23.D5
    [[nodiscard]] virtual std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const = 0;

    [[nodiscard]] virtual LogFormat format() const noexcept = 0;

    // post: a [0,1] confidence that this strategy matches the line, majority-voted by
    // FormatDetector.
    // invariant: non-zero only if parse() is structurally committed to succeeding on that line.
    // invariant: prefix-shaped confidence is admissible only where the format's grammar is FULLY
    // DETERMINED by that prefix.
    // invariant: bounded by the line's HEADER and never by the line — this runs on every line of
    // every stream.
    // refs: DN-43.D1
    [[nodiscard]] virtual double confidence(std::string_view line) const noexcept = 0;
};

} // namespace insight::tokenization

// invariant: canon owns every matcher ALGORITHM; a package owns only DATA rows.
// invariant: a new matching capability is a grammar-version bump, never package-local parsing code.
// refs: ADR-17, ADR-17.D4
export namespace insight::semantic
{

// invariant: a component of the composed identity; bump on any grammar SHAPE change — a new row
// kind, a new closed-enum member, or a serialization change.
// invariant: the token is ASSIGNED AT SHIP and never reserved by a plan: it names the Nth shape,
// and which feature causes it is unknowable in advance.
// invariant: keep bumping on a serialization change that moves no bytes — a field reorder or a
// widened enum moves nothing else, and this string is the only guard.
// invariant: it reaches exactly one non-comment site, the semantic_identity preimage in
// compose.cpp, and no wire field and no MetaLog block.
// invariant: distinct from the MASKING token kCanonicalizationVersion, whose value canon.api.cppm
// owns; this one names the GRAMMAR's shape.
// refs: ADR-2.D5, ADR-17.D4, ADR-22.D8, SRC-SID-3
inline constexpr std::string_view kSemanticGrammarVersion{"semantic-grammar-6"};

// invariant: a DIALECT is a VOCABULARY over a HOST FORMAT — the format owns the layout rule, the
// dialect owns the names inside a layout another layer already delimited.
// invariant: canon knows the FIELD and knows NO VALUE: the gate is a composed PACKAGE NAME, never a
// canon-owned enum, because an enum makes every new dialect edit canon.
// invariant: kAnyDialect is the EMPTY string_view, and a package may never be named "", so empty is
// unambiguously "any".
// invariant: an empty ROW gate means "fires on any dialect"; an empty DECLARATION means the caller
// did not say, which drops every concretely-gated row.
// invariant: fail-closed on DEPTH, never on the run.
// refs: ADR-22.D2, ADR-22.D6, SRC-II-6
inline constexpr std::string_view kAnyDialect{};

// post: kAnyDialect always fires; a concrete gate fires on the same declared dialect and on no
// other, and an undeclared stream matches no concrete gate.
// invariant: an UNKNOWN declared dialect never reaches here — it is a HARD ERROR at stream
// resolution, listing the composed package names.
// invariant: deliberately a SECOND predicate beside channel_admits: the two coordinates answer
// different questions against different vocabularies.
// refs: ADR-22.D5, ADR-22.D6
[[nodiscard]] constexpr bool dialect_admits(std::string_view dialect_gate,
                                            std::string_view declared_dialect) noexcept
{
    return dialect_gate == kAnyDialect || dialect_gate == declared_dialect;
}

// invariant: Medium is dialect times IntentChannel; the channel is the channel through which an
// intent was MATERIALIZED, and the same intent in another channel is a new channel.
// invariant: the channel vocabulary is package-declared DATA — canon knows channel markers and
// never detects, routes or maps.
// invariant: it is a provenance fact about a STREAM, never a sink, which is a generator-side output
// destination.
// invariant: canon MUST NOT infer the channel — it is always TOLD; a caller may derive its own
// declaration, and that inference lives at the caller.
// invariant: the channel is EXTRINSIC: no byte carries it, and a prose line is byte-identical
// across channels.
// invariant: kAnyChannel is the EMPTY string_view, mirroring kAnyDialect; all_channels_named
// forbids an empty declared name, so empty is unambiguously "any".
// invariant: an empty DECLARATION means the caller did not say; never default an undeclared stream
// to a concrete channel.
// refs: ADR-22.D5, ADR-22.D6
inline constexpr std::string_view kAnyChannel{};

// post: kAnyChannel always fires; a concrete gate fires on the same declared channel and on no
// other, and an undeclared stream matches no concrete gate.
// invariant: an UNKNOWN declared channel never reaches here — it is a HARD ERROR at composition,
// listing the declared vocabulary.
// refs: ADR-22.D5
[[nodiscard]] constexpr bool channel_admits(std::string_view channel_gate,
                                            std::string_view declared_channel) noexcept
{
    return channel_gate == kAnyChannel || channel_gate == declared_channel;
}

// invariant: a CLOSED extractor enum — the algorithm lives in core and a new extractor is a
// grammar-version bump, part of the identity.
// invariant: the alignment class and the discriminant are derived in core, by canonicalize_intent
// and discriminant_of.
// refs: ADR-2.D7, ADR-17.D4
enum class PayloadExtract : std::uint8_t
{
    // invariant: no payload — the structural markers carry none.
    None = 0,
    // invariant: the content after the matched prefix, verbatim — the intent markers.
    RemainderAfterPrefix,
    // invariant: the content after the matched prefix up to a REQUIRED line-final ')'; a line that
    // does not end with ')' does not match the row at all.
    // invariant: nested parens stay inside the payload — only the single final ')' is the
    // delimiter.
    // refs: STU-6, ADR-17
    RemainderToClosingParen,
    // invariant: the content after the prefix is a non-empty run of ASCII digits, then a single
    // ':', then the payload — a variable-length field to SKIP.
    // invariant: the payload ENDS AT THE FIRST '\r' and then loses a trailing bracketed option
    // group — two producer-owned boundaries bundled into the extractor.
    // invariant: a shape failure — no digits, no ':', an empty payload — means the ROW DOES NOT
    // MATCH, so a malformed producer marker is declined rather than mis-parsed.
    // invariant: digit-length is deliberately unconstrained; anchoring, not the stamp, is what
    // excludes the echoed phantoms.
    // refs: SRC-D-TID-11, STU-12, ADR-17
    NumericFieldThenRemainder,
};

// invariant: the CLOSED dual of PayloadExtract — each emit value is the exact inverse of one
// extractor, so recognize(render_row(row, payload)) recovers the payload.
// invariant: both projections are rows-as-data, never a render() callable, which would be
// un-hashable and would let the two projections diverge.
// invariant: a new emit shape is a grammar-version bump, part of the identity, exactly as a new
// extractor is.
// refs: SRC-SID-2, STU-8, DN-17.D15
enum class PayloadEmit : std::uint8_t
{
    // invariant: dual of PayloadExtract::None — the prefix alone.
    None = 0,
    // invariant: dual of RemainderAfterPrefix — the prefix, then the payload verbatim.
    PayloadAfterPrefix,
    // invariant: dual of RemainderToClosingParen — prefix, payload, then the final ')'.
    PayloadThenClosingParen,
    // invariant: dual of NumericFieldThenRemainder — prefix, a single PLACEHOLDER digit 0, ':',
    // then the payload.
    // invariant: the placeholder's consequence is declared — a generated marker carries no
    // wall-clock, so a synthetic stream has no section duration.
    // invariant: the two reader-side suffix drops have no generation dual: the writer emits neither
    // a producer option group nor the CR terminator.
    PlaceholderNumericFieldThenPayload,
};

// post: the extractor-to-emitter map, TOTAL over the declared pairs, and the ONE site the pairing
// is written down.
// invariant: -Werror=switch and MSVC /we4062 are set PRIVATE on the insight_canon and
// insight_canon_tests targets, so an unhandled enumerator is a compile ERROR in this file.
// invariant: there is deliberately no default: label — a default is what would silence that
// diagnostic and turn the break into a wrong answer.
// refs: DN-17.D15, ADR-2.D7
[[nodiscard]] constexpr PayloadEmit dual(PayloadExtract extract) noexcept
{
    switch (extract)
    {
    case PayloadExtract::None:
        return PayloadEmit::None;
    case PayloadExtract::RemainderAfterPrefix:
        return PayloadEmit::PayloadAfterPrefix;
    case PayloadExtract::RemainderToClosingParen:
        return PayloadEmit::PayloadThenClosingParen;
    case PayloadExtract::NumericFieldThenRemainder:
        return PayloadEmit::PlaceholderNumericFieldThenPayload;
    }
    return PayloadEmit::None;
}

// invariant: a CLOSED enum selecting and parameterizing one core location-matching ALGORITHM; a new
// family is a grammar-version bump, part of the identity.
// refs: SRC-II-8, ADR-2.D7
enum class LocationMatchKind : std::uint8_t
{
    // invariant: <base>.test.<ext> or <base>.spec.<ext> with ext in params.extensions.
    TestSpecExtension = 0,
    // invariant: basename <prefix>* or *<prefix-reversed>, plus a fixed extension.
    PrefixAndExtension,
    // invariant: any of a set of fixed suffixes, word-boundary-terminated.
    SuffixSet,
};

// invariant: one row kind per knowledge surface; a line-anchored prefix announces a StructuralRole.
// refs: ADR-17
struct StructuralRoleRow
{
    std::string_view prefix;
    insight::StructuralRole role;
    // invariant: kAnyDialect fires on any dialect — the pre-split ungated behaviour; otherwise
    // the OWNING package's name.
    // invariant: filtered into the stream view once, at resolution, and never consulted per line.
    // refs: ADR-22.D6
    std::string_view dialect_gate{kAnyDialect};
};

// invariant: a prefix opens a behavioural quantum, carrying the dialect's kind and child_order —
// the level-typed alignment declaration — and the payload extractor.
// invariant: DIALECT-gated by construction: an intent marker names its own package and never fires
// cross-dialect.
// refs: SRC-II-6, ADR-18, ADR-22.D6
struct IntentMarkerRow
{
    std::string_view prefix;
    insight::tokenization::IntentMarkerKind kind;
    insight::tokenization::ChildOrder child_order;
    // refs: ADR-22.D6, SRC-II-6
    std::string_view dialect_gate{kAnyDialect};
    PayloadExtract extract;
    // invariant: a CLOSED exclusion set over the extracted payload — the row does NOT fire when
    // an entry equals the payload, or the payload starts with an entry followed by a space.
    // invariant: empty for rows without exclusions; the span points at package-static constexpr
    // storage and is serialized into semantic_identity.
    // refs: SRC-SP-7, STU-6
    std::span<const std::string_view> payload_excludes;
    // invariant: the row fires only on a stream the caller declared as this IntentChannel;
    // kAnyChannel, the default, fires on any.
    // invariant: REQUIRED exactly when one channel's marker occurs as ordinary CONTENT in a sibling
    // channel of the same dialect.
    // invariant: serialized into semantic_identity, so a channel-row change moves the digest
    // exactly as a prefix change does.
    // refs: ADR-22.D6
    std::string_view channel_gate{kAnyChannel};
};

// invariant: the WRITER dual of IntentMarkerRow — the same kind and child_order, and the same
// Medium, which is dialect times channel.
// invariant: no payload_excludes: the writer only ever emits a real intent, so the exclusion set is
// a reader-side concern with no generation dual.
// invariant: rows-as-data — the emit shape is the closed PayloadEmit enum, never a callable.
// invariant: content-hashable exactly as IntentMarkerRow is, so a generation-side change moves
// semantic_identity as a recognition change does.
// refs: SRC-SID-2, STU-8, ADR-18
struct IntentEmitRow
{
    std::string_view prefix;
    insight::tokenization::IntentMarkerKind kind;
    insight::tokenization::ChildOrder child_order;
    // refs: ADR-22.D6
    std::string_view dialect_gate{kAnyDialect};
    PayloadEmit emit;
    // invariant: the writer's dual of the reader's question — not which prefix do I match, but
    // which IntentChannel am I materializing into.
    // invariant: paired with the reader's gate by paired_writer_row, so the two projections cannot
    // drift onto different channels.
    // invariant: it is the MEDIUM SELECTOR's input — a writer picks the emit row whose
    // channel_gate matches, never the first row that matches by array order.
    // refs: ADR-22.D6, SRC-SID-1
    std::string_view channel_gate{kAnyChannel};
};

// invariant: a prefix lifts the line's LogLevel, inside parse() and before raw-text inference,
// consumed by the dialect strategy that owns the rows.
// refs: ADR-22.D6
struct LevelLiftRow
{
    std::string_view prefix;
    insight::LogLevel level;
    std::string_view dialect_gate{kAnyDialect};
};

// invariant: recognizes a test-file WHERE coordinate; kind selects the core matching algorithm and
// the params are the dialect-independent file-naming vocabulary it walks.
// invariant: infixes and extensions serve TestSpecExtension; SuffixSet reads suffixes alone; and
// PrefixAndExtension reads extensions plus prefixes OR suffixes, so suffixes serve two kinds.
// invariant: the spans point at package-static constexpr arrays.
// refs: SRC-II-8, SRC-SP-7, BIB:intent_identity
struct LocationRow
{
    LocationMatchKind kind;
    std::span<const std::string_view> infixes;
    std::span<const std::string_view> extensions;
    std::span<const std::string_view> prefixes;
    std::span<const std::string_view> suffixes;
};

// invariant: the dialect's run-verdict MAPPING — a native verdict string to the core-owned
// RunOutcome — plus the console-tail fallback marker.
// invariant: the matcher algorithms live in core; a package ships only rows, and either set may be
// empty.
// invariant: one native verdict token to RunOutcome, byte-exact and dialect-gated.
// invariant: consumed on BOTH resolution rungs — the authoritative side-input token and the
// console-tail marker's extracted remainder map through the SAME set.
// refs: SRC-D-OUT-RUN-1, ADR-17.D5
struct OutcomeTokenRow
{
    std::string_view token;
    insight::RunOutcome outcome;
    std::string_view dialect_gate{kAnyDialect};
};

// invariant: a CLOSED shape enum — how a matched OutcomeMarkerRow yields its verdict; two
// dialects, two terminal-line grammars, and no prefix choice unifies them.
// refs: ADR-17.D5, ADR-2.D7
enum class OutcomeMarkerShape : std::uint8_t
{
    // invariant: the verdict is the line's REMAINDER after the prefix, must be a single ASCII word,
    // and is mapped through the composed OutcomeTokenRow set.
    RemainderToken = 0,
    // invariant: the PREFIX itself announces the verdict; the remainder is free-form and is not
    // read.
    // invariant: the shape exists because its lines are unrecognizable under RemainderToken for ANY
    // prefix choice — every cut leaves a multi-word remainder.
    // refs: STU-12
    PrefixIsVerdict,
};

// invariant: the console-tail terminal-verdict line, matched line-anchored on the resolved view.
// invariant: the LONGEST matching prefix wins within a line, and the LAST matching line wins across
// lines — a run has one terminal verdict.
// invariant: no dependence on row declaration order.
// refs: SRC-D-OUT-RUN-1
struct OutcomeMarkerRow
{
    std::string_view prefix;
    std::string_view dialect_gate{kAnyDialect};
    OutcomeMarkerShape shape{OutcomeMarkerShape::RemainderToken};
    // invariant: READ ONLY under PrefixIsVerdict — a RemainderToken row's verdict comes from its
    // remainder through the token rows, and this field is inert for it.
    // note: RunOutcome has no absent member, so a sentinel would be a lie rather than a guard.
    insight::RunOutcome outcome{insight::RunOutcome::Unknown};
};

// invariant: the grammar SEAT for package value classes; no shipped package declares one, and the
// composed ValueClassRegistry is the VIEW over the core catalogs.
// invariant: present so the manifest shape is final and the identity hash slot is stable.
enum class ValueClass : std::uint8_t
{
    None = 0,
};

// invariant: schedule_id is the ordinal schedule id when cls is an ordinal class, empty otherwise.
// invariant: scale is the unit-to-canonical factor when applicable, zero otherwise.
struct ValueClassRow
{
    std::string_view key;
    ValueClass cls;
    std::string_view schedule_id;
    std::int64_t scale;
};

// invariant: a package shipping a dialect format strategy exports a factory; a data-only package
// leaves manifest.strategy empty.
// invariant: the composition registers the produced strategy into the FormatDetector through the
// existing register_strategy seam.
// refs: ADR-17
using StrategyFactory = std::unique_ptr<insight::tokenization::IFormatStrategy> (*)();

// invariant: a raw-line provenance hook, signature-constrained to a pure bool(string_view) byte
// function — a discouraged escape-hatch recognizer.
// invariant: consulted by LogParser on the RAW, ANSI-bearing line, independent of the routed
// strategy.
// refs: SRC-D-PROV-1, ADR-17
using ProvenanceHook = bool (*)(std::string_view raw_line) noexcept;

// invariant: each package exports one constexpr SemanticPackageManifest from its own named module.
// invariant: no register_*() methods and no mutable registration state — the composition is a
// pure function of the manifest SET.
// refs: ADR-17
struct SemanticPackageManifest
{
    std::string_view name;
    // invariant: version moves when what this package RECOGNIZES or EMITS moves — its rows, their
    // gates, or its code tier — and never for a grammar-shape change or a rename.
    // refs: SRC-SP-7, ADR-17.D3, DN-17.D22
    std::string_view version;
    std::span<const StructuralRoleRow> roles;
    std::span<const IntentMarkerRow> markers;
    // invariant: the GENERATION projection, declared here so it enters semantic_identity and a
    // generation-side change moves the comparability hash.
    // invariant: EMPTY for a package that ships no markers.
    // invariant: every package with markers declares the SAME span its Dialect type exposes as
    // emit_markers — one array, two views.
    // refs: SRC-SID-2, ADR-23, DN-17.D15
    std::span<const IntentEmitRow> emits;
    std::span<const LevelLiftRow> level_lifts;
    std::span<const LocationRow> locations;
    std::span<const ValueClassRow> value_classes;
    // invariant: the run-outcome vocabulary; both spans may be empty, a package declaring exactly
    // the outcome surfaces its dialect actually has.
    // refs: SRC-D-OUT-RUN-1, ADR-17.D5
    std::span<const OutcomeTokenRow> outcome_tokens;
    std::span<const OutcomeMarkerRow> outcome_markers;
    // invariant: the package's declared INTENT CHANNEL vocabulary — every materialization this
    // dialect's intents can be rendered into.
    // invariant: EMPTY for a single-materialization dialect, whose rows are all kAnyChannel; only a
    // dialect with several materializations declares channels.
    // invariant: a caller's declared channel is validated against this vocabulary — declared
    // fires, unknown is a HARD ERROR listing these names.
    // invariant: the span points at package-static constexpr storage and is serialized into
    // semantic_identity alongside the rows it gates.
    // refs: ADR-22.D5, ADR-22.D6, SRC-SP-7
    std::span<const std::string_view> channels;
    // invariant: the package's declared DIALECT REVISION vocabulary — which VENDOR generation of
    // the dialect these rows recognize.
    // invariant: the vendor owns the referent and we own the declaration; .version above is OUR
    // ruleset identity and a reader must never substitute one for the other.
    // invariant: NON-EMPTY, with names unique and non-empty — a package that recognizes nothing
    // in particular is not a state the grammar admits.
    // invariant: cardinality ONE is a SCHEMA bound enforced by the declaration tool, never here:
    // core stays general so a later schema bump re-opens it without reshaping this member.
    // invariant: NOT a gate — nothing filters on it and no row carries it; the identity serializer
    // is its only RUNTIME reader, and the conformance kit's equivalence check compares it too.
    // invariant: deliberately NOT propagated into ComposedPackage or the metalog RulesetIdentity
    // wire surface, which has a different owner and external implementers.
    // invariant: the span points at package-static constexpr storage and is serialized at the END
    // of the manifest preimage.
    // refs: ADR-17.D9, ADR-22.D8, SRC-SP-7
    std::span<const std::string_view> dialect_revisions;
    // invariant: both code-tier hooks are nullable; a data-only package leaves each null.
    StrategyFactory strategy{nullptr};
    ProvenanceHook echoed_source{nullptr};
};

// invariant: the WRITER half of one declaration, two projections: render_row is the pure inverse of
// core's payload extraction.
// invariant: paired_writer_row, all_intents_paired and the DialectIntent concept together make
// bidirectionality a COMPILE-TIME obligation.
// invariant: the numeric field a PlaceholderNumericFieldThenPayload row materializes, separator
// included.
// invariant: the value is not a stamp — the reader skips the field whatever it holds, so the
// writer owes only a SHAPE-VALID field.
// refs: STU-8, DN-17.D15
inline constexpr std::string_view kPlaceholderNumericField{"0:"};

// post: PURE — a function of (row, payload) only: no RNG, no envelope, no engine state, no
// wall-clock.
// post: the exact inverse of the PayloadExtract algorithm — recognize(render_row(row, payload))
// recovers the row's kind and child_order and the payload.
// post: allocates the result string, caller-owned, and it is the only allocation.
// refs: STU-8
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

// post: PURE, the same contract as render_row — a function of (row, token) only, no RNG and no
// clock, homed canon-side so the epilogue's byte shape has ONE owner.
// post: token is read only under RemainderToken; a PrefixIsVerdict row renders the prefix alone,
// which is its minimal shape-valid rendering.
// invariant: no new row kind is needed — the outcome rows are already symmetric literals, and the
// conformance kit asserts that scanning a rendered line recovers that row's own verdict.
// refs: SRC-D-OUT-RUN-1, BIB:jenkins_dialect
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

// post: the generation row that materializes into a line THAT reader row recognizes — the same
// prefix, kind and MEDIUM.
// invariant: the Medium is dialect times IntentChannel, so the pairing matches on BOTH gates — a
// reader row gated to one channel pairs only with a writer row gated to the same one.
// post: nullptr when unpaired, and well-defined iff every reader row has exactly one paired writer
// row, which is what all_intents_paired enforces.
// refs: ADR-22.D6, STU-8
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

// invariant: a package's channel vocabulary and its rows' gates must agree, and the check is
// consteval so a disagreement is a build error in the package's own TU.
// invariant: every declared channel name is non-empty — the empty name IS kAnyChannel, so
// declaring it would collapse "any" and "this one" onto one value.
// refs: ADR-22.D6
[[nodiscard]] consteval bool all_channels_named(std::span<const std::string_view> channels) noexcept
{
    return std::ranges::all_of(channels,
                               [](std::string_view channel) noexcept { return !channel.empty(); });
}

// invariant: the declared revision vocabulary is non-empty, every name is non-empty, and no name
// repeats — the same seat and posture as all_channels_named.
// invariant: non-emptiness is load-bearing here where the channel vocabulary's is not: an empty
// channel span is the honest degenerate case, an empty revision span is not.
// refs: ADR-17.D9
[[nodiscard]] consteval bool
all_revisions_named(std::span<const std::string_view> revisions) noexcept
{
    if (revisions.empty())
        return false;
    for (std::size_t i{0}; i < revisions.size(); ++i)
    {
        if (revisions[i].empty())
            return false;
        for (std::size_t j{i + 1}; j < revisions.size(); ++j)
            if (revisions[i] == revisions[j])
                return false;
    }
    return true;
}

// invariant: every row's channel_gate is kAnyChannel or one of the package's DECLARED channels, so
// a channel-name typo is a compile error in the package that made it.
// refs: ADR-22.D6
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

// invariant: every gated row's dialect_gate is either kAnyDialect or the package's OWN name, so a
// dialect-name typo is a build error in the package that made it.
// invariant: a gate naming a package this manifest is not reaches across a boundary it does not
// own, and ComposedSemantics flattens, so nothing downstream could tell it apart.
// invariant: takes the MANIFEST rather than six spans, so a package cannot add a row kind and
// silently forget it here.
// refs: ADR-22.D6
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

// invariant: a manifest name MUST be non-empty.
// invariant: kAnyDialect IS the empty string, so a manifest named "" makes all_dialect_gates_owned
// succeed VACUOUSLY for every ungated row.
// invariant: set-valued because it is asserted at the COMPOSITION site, beside find_conflict —
// one place where a binary declares its package list is one place to fence that list.
// refs: DN-17.D17
[[nodiscard]] consteval bool
all_packages_named(std::span<const SemanticPackageManifest> packages) noexcept
{
    return std::ranges::all_of(packages, [](const SemanticPackageManifest& package) noexcept
                               { return !package.name.empty(); });
}

// invariant: every recognition marker has a paired generation row — no reader without a writer;
// consteval, so a package static_asserts it over its constexpr rows.
// refs: SRC-SID-2, STU-8
[[nodiscard]] consteval bool all_intents_paired(std::span<const IntentMarkerRow> markers,
                                                std::span<const IntentEmitRow> emits) noexcept
{
    return std::ranges::all_of(markers, [emits](const IntentMarkerRow& reader) noexcept
                               { return paired_writer_row(reader, emits) != nullptr; });
}

// invariant: a type models DialectIntent iff it exposes BOTH projections as constexpr row spans and
// every reader row is paired.
// invariant: a dialect whose type ships a recognition row without its generation row does NOT
// compile where the concept is required.
// refs: SRC-SID-2, STU-8
template <typename Dialect>
concept DialectIntent = requires {
    { Dialect::markers } -> std::convertible_to<std::span<const IntentMarkerRow>>;
    { Dialect::emit_markers } -> std::convertible_to<std::span<const IntentEmitRow>>;
} && all_intents_paired(Dialect::markers, Dialect::emit_markers);

} // namespace insight::semantic
