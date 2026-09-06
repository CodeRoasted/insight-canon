// refs: ADR-3.D4
// invariant: `import insight.canon;` is the whole public surface: this unit re-exports the api, the
// composition and the transport vocabulary, and nothing else.
// invariant: the four `insight.canon.detail.*` shards are sealed and build-only — never
// re-exported, so no consumer can name one.
// invariant: the Tokenizer is declared HERE and not in api because its implementation needs the
// parser and the masker from the detail shards, which themselves import api.
// note: homing it above detail would close the api-detail cycle this facade seam exists to break
export module insight.canon;
import insight.canon.internal;
export import insight.canon.api;
// refs: ADR-17
// invariant: the composition surface — `compose()` and `ComposedSemantics`, which the Tokenizer
// takes by const reference.
// invariant: a pp-import is ONE logical line by [cpp.pre], so a module name wrapped across physical
// lines is ill-formed by the STANDARD and no compiler bump can retire it.
// note: keep this directive short: as a trailing comment the why wrapped the module NAME
export import insight.canon.compose;
// refs: ADR-23
// invariant: the transport vocabulary — `IngestDeclaration`, the catalogue and the stream-scoped
// peel — is re-exported because a CONSUMER declares the stack: it is the caller's provenance.
// invariant: same short-line discipline as the directive above, and for the same reason.
export import insight.canon.transport;

export namespace insight::tokenization
{

// refs: ADR-17, ADR-21.D3, ADR-22, DN-75
// invariant: canon owns the walker ALGORITHMS and the composed rows are the DATA; they are homed in
// the facade because `ComposedSemantics` lives in a module that imports api.
// invariant: NO walker takes a dialect or channel coordinate — `composed` is the RESOLVED
// stream's view, so a row that is in the table is a row that fires.
// note: a per-line format or dialect argument would restore the content dependence
// pre: `content` is a `NormalizedContent`, producible only by stage 1 followed by a suffix-taking
// stage 2, so a caller that has not normalized DOES NOT COMPILE at this boundary.
// invariant: the type does NOT prove the RIGHT stage 2 ran: the strip divergence stands and the
// eidos/canon reconciliation still rides its own gate.
// note: as prose this obligation was met by 1 of 3 consumers: 1 077 of 3 193 markers lost
// post: `classify` returns the longest-matching role row's `StructuralRole`, and None when no row
// of the view matches.
[[nodiscard]] StructuralRole
classify(NormalizedContent content, const insight::semantic::ComposedSemantics& composed) noexcept;

// refs: ADR-18
// post: `recognize` returns the longest-matching marker row's `IntentMarker`, whose payload is the
// content after the matched prefix, and None when no row of the view matches.
// invariant: the alignment class and the instance discriminant are derived by canon from that
// payload, never declared by a package.
// invariant: the returned marker's `name` and `discriminant` VIEW the handed content's bytes, so
// the caller's storage must outlive them.
[[nodiscard]] IntentMarker recognize(NormalizedContent content,
                                     const insight::semantic::ComposedSemantics& composed) noexcept;

// invariant: phase 1 of the pipeline: one raw log line in, one `CanonicalEvent` out.
// invariant: NOT thread-safe — one instance per thread or per strand.
class Tokenizer
{
  public:
    // refs: ADR-17
    // invariant: core ships NO default composition: every binary names its package set and threads
    // the resulting `ComposedSemantics` in here.
    // pre: `composed` outlives the Tokenizer, which does not own it.
    explicit Tokenizer(ArenaAllocator& arena, MaskConfig mask_config,
                       const insight::semantic::ComposedSemantics& composed);
    ~Tokenizer();

    Tokenizer(const Tokenizer&) = delete;
    Tokenizer& operator=(const Tokenizer&) = delete;
    Tokenizer(Tokenizer&&) noexcept;
    Tokenizer& operator=(Tokenizer&&) noexcept;

    // refs: ADR-21.D4, F-SRC-insight-canon:test_stable_door_does_not_normalize.cpp
    // post: `process_line` performs stage 1 itself, so it equals the stable door applied to the
    // normalized line — the equivalence the two doors are asserted on.
    [[nodiscard]] std::expected<CanonicalEvent, std::string>
    process_line(std::string_view raw_line);

    // refs: ADR-21.D4, DN-75.D2
    // invariant: THE STABLE DOOR performs NO stage 1 at all, deliberately, so its answers — the
    // projection, the level lift, the role, the marker — are functions of the caller's bytes.
    // invariant: it exists so the echoed-source demotion can read the SGR command-echo wrapper that
    // stage 1 destroys, on a path that holds ONE view and hands it to strategy and detector alike.
    // note: routing this through `process_line` restores stage 1, silently killing echoed-source
    // invariant: the two preconditions do not compose on one view: `normalize` writes into a
    // scratch buffer the next line reuses, so a normalized view is not stable.
    // pre: every read of the returned `CanonicalEvent` happens-before the caller's bytes die or the
    // arena resets, whichever comes first.
    // invariant: that is the condition itself; "valid for the arena's lifetime" is only a PROXY for
    // it, and a caller holding an event past its line satisfies the proxy while breaking the door.
    // invariant: the event and every `string_view` the strategy sliced from `stable_line` view the
    // caller's bytes; canon copies none of them.
    // assert: the violation class is empty BY MEASUREMENT, not by construction: all four production
    // call sites reset the arena in the same call, after the last read.
    // refs: F-SRC-insight-eidos:insight_pipeline.cpp
    // invariant: canon PERFORMS stage 1 and can attest it, but cannot know a caller-side lifetime,
    // so a token carrying this precondition would be an attestation the CALLER mints.
    // note: it would compile whether or not it were true, and prove strictly less than the type
    [[nodiscard]] std::expected<CanonicalEvent, std::string>
    process_stable_line(std::string_view stable_line);

    [[nodiscard]] std::vector<std::expected<CanonicalEvent, std::string>>
    process_batch(std::span<const std::string_view> lines);

    // refs: DN-29.D6, DN-29.D15
    // post: true with `records` replaced by the N canonical flat-span records the document carries
    // when `raw_line` is an OTLP span-export DOCUMENT; false with `records` untouched otherwise.
    // pre: FOR THE ENTRY THAT HOLDS THE WHOLE INPUT — a file, a CLI read, a receiver body — and
    // NEVER for a frame-oriented streaming path.
    // invariant: the SHM plane's line frame carries a bounded payload while an export has no
    // declared size, so a document crossing it arrives truncated.
    // note: the record entry REFUSES a document instead, so this is a separate over-triggering door
    // invariant: it yields RECORDS, not events: window closure resets the caller's arena, so the
    // caller must be free to tokenize record k only once it is done with k-1.
    // pre: tokenize each record with `process_line`, never `process_stable_line` — `records` is
    // caller scratch that the next document reuses.
    [[nodiscard]] static bool unpack_span_document(std::string_view raw_line,
                                                   std::vector<std::string>& records);

    [[nodiscard]] std::size_t events_produced() const noexcept;
    [[nodiscard]] std::size_t lines_parsed() const noexcept;

    // refs: ADR-16.D9, DN-43.D14
    // post: the PROJECTION-TOTALITY count for this stream — lines that had bytes and projected to
    // an empty `content`; a monotonic per-stream total, reset by nothing, read after the walk.
    // invariant: it is NOT a defect count. The population has two members: a genuinely empty body,
    // which is the CORRECT identity for a content-less line, and a projection bug.
    // invariant: this counter is their sum, only a per-strategy expectation separates them, and the
    // figure is pinned per corpus by a gate that RUNS.
    // note: before this accessor the number left canon only through a rate-limited warning
    [[nodiscard]] std::size_t empty_projections() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace insight::tokenization

export namespace insight
{

// refs: ADR-17.D5, SRC-D-OUT-RUN-1, SRC-II-6
// invariant: canon owns the run-outcome ALGORITHMS — the token map, the console-tail scan and the
// precedence resolver; the composed outcome rows are the DATA.
// post: `map_outcome_token` maps a native verdict token through the RESOLVED VIEW's
// `OutcomeTokenRow` set, and nullopt means no row in this view claims the token.
// invariant: the dialect gate was applied once at stream resolution, so an undeclared stream
// carries no concretely-gated row and nothing resolves — fail-closed on DEPTH.
// note: nullopt differs from a row that maps the token TO Unknown, as Jenkins NOT_BUILT does
[[nodiscard]] std::optional<RunOutcome>
map_outcome_token(std::string_view token,
                  const insight::semantic::ComposedSemantics& composed) noexcept;

// refs: ADR-17.D1, ADR-22.D1, DN-32.D6
// invariant: a caller-declared verdict is a PAIR — `(vocabulary, token)` — never a bare string,
// and the two halves answer different questions the authorship test keeps apart.
// invariant: the STREAM's dialect answers who WROTE the bytes, and "no dialect" is a fact rather
// than a gap; the SIDE INPUT's vocabulary answers who SUPPLIED the verdict.
// note: requiring a raw cmake log to carry GitHub's vocabulary is a category error
// invariant: a bare token is NOT self-interpreting — `failure`, `failed`, `FAILURE`, and
// `UNSTABLE` has no universal meaning — so resolving one would force a spelling list into CORE.
struct SideInputVerdict
{
    // refs: DN-32.D7
    // invariant: the producer's own spelling, verbatim, never a pre-resolved `RunOutcome`; empty
    // means the caller declared nothing, which is a CHOICE and asserts nothing.
    std::string_view token;
    // refs: DN-32.D6, DN-32.D7
    // invariant: the name of the package whose `outcome_tokens` interpret that spelling, REQUIRED
    // whenever `token` is non-empty: the two fields are one declaration.
    // invariant: a non-empty token beside an empty vocabulary TERMINATES — half a declaration is
    // a wiring mistake, not a weak assertion.
    // note: resolving nothing in silence is how the crawl shipped 63 pairs of unbounded claims
    // invariant: declaring NOTHING — both fields empty — stays a first-class choice and
    // degrades.
    std::string_view vocabulary;
};

// refs: ADR-22, ADR-22.D5, DN-32.D6
// post: maps a native verdict token through a NAMED vocabulary's `outcome_tokens`, INDEPENDENTLY of
// any stream's resolved dialect.
// pre: `composed` is the FULL composition — every package's rows — and never a stream view.
// invariant: passing a STREAM VIEW is a silent no-op rather than an error, because a view has
// already been filtered and re-filtering it removes nothing.
// invariant: passing a FRESH composition is the doubly-Unspecified view, in which every
// concretely-gated row is already dropped, so nothing maps.
// invariant: the body re-derives through `for_stream`, the one ratified evaluation point of the
// dialect gate, which reads the UNFILTERED tables — so this cannot be fixed by walking the view.
// invariant: the gate coordinate is a caller's declaration, fixed before the first line and read
// once per side per diff, so no DECLARED row's gate is ever a function of CONTENT.
// post: nullopt = no row in the named vocabulary claims the token. That is the one non-fatal miss,
// and it stays non-fatal because it is a VALUE error under correct wiring.
// invariant: an UNKNOWN vocabulary NAME and an EMPTY vocabulary beside a non-empty token both
// TERMINATE: both are WIRING errors, unreachable from any log byte.
// note: not noexcept — `for_stream` allocates; cold path, once per side per diff, never per line
[[nodiscard]] std::optional<RunOutcome>
map_outcome_token_in(std::string_view token, std::string_view vocabulary,
                     const insight::semantic::ComposedSemantics& composed);

// refs: ADR-17, ADR-17.D5, ADR-22
// post: the whole-log console-tail scan is ONE parse-only pass with no masking: per line the
// resolved view's `OutcomeMarkerRow` set is walked.
// invariant: the LONGEST matching prefix wins within a line and the LAST such line wins across the
// log.
// invariant: longest-prefix-wins replaces a walk whose winner was a function of DECLARATION ORDER,
// so a package's row order can no longer decide a verdict.
// note: GitLab needs `ERROR: Job failed: canceled` to beat `ERROR: Job failed`
// invariant: the scan LATCHES no dialect and carries no `LogFormat` at all: the vocabulary is fixed
// before the first line by the declaration.
struct RunOutcomeScan
{
    bool marker_present{false};
    // invariant: the winning `RemainderToken` row's extracted verdict word, which the resolver maps
    // through the view's `OutcomeTokenRow` set.
    // invariant: empty when no marker matched, and empty when the winner was a `PrefixIsVerdict`
    // row — that shape has no remainder token by construction.
    std::string token;
    // refs: ADR-17
    // invariant: the winning `PrefixIsVerdict` row's own `RunOutcome`, engaged EXACTLY when the
    // console tail resolved off the row rather than off a token.
    // invariant: the resolver prefers it and falls back to mapping `token`, so the fail-closed note
    // stays reachable for the shape that can actually produce it.
    std::optional<RunOutcome> verdict;
};

[[nodiscard]] RunOutcomeScan scan_run_outcome(std::span<const std::string> lines,
                                              const insight::semantic::ComposedSemantics& composed);

// refs: ADR-17.D5, SRC-D-OUT-RUN-1
// invariant: the resolution order is STRICT and TOTAL, never a reconciliation: the authoritative
// side-input token if it maps, else the console tail's last match if it maps, else Unknown.
// invariant: when rung 1 resolves, a present-but-DISAGREEING console tail is NOT consulted — it
// can be a local, nested or caught outcome rather than a competing whole-run verdict.
// invariant: that divergence is surfaced as a kept trace-level log plus the `divergent` flag, and
// the authoritative value stands.
// invariant: a token that is provided but does not map is never a silent misclassification: it
// surfaces in `note` and resolution falls down the ladder.
// refs: F-SRC-insight-canon:test_run_outcome.cpp
struct RunOutcomeResolution
{
    // invariant: `console` carries the console candidate's mapped value, and Unknown when there was
    // none.
    // invariant: `authoritative` is true exactly when rung 1 resolved; `divergent` is true when
    // rung 1 resolved AND a mapped console tail disagrees.
    // invariant: `note` carries the surfaced fail-closed note, and empty means clean.
    RunOutcome outcome{RunOutcome::Unknown};
    RunOutcome console{RunOutcome::Unknown};
    bool authoritative{false};
    bool divergent{false};
    std::string note;
};

// refs: DN-32.D6
// pre: TWO compositions, and they are not interchangeable: `stream_view` is the resolved view of
// the stream being diffed, `vocabularies` is the FULL composition.
// invariant: rung 2 reads `stream_view` and must — a console marker came out of THESE bytes, so a
// Jenkins marker may not fire on a GHA stream.
// invariant: rung 1 reads `vocabularies`, and only when the side input NAMES its vocabulary: the
// declarer's vocabulary has nothing to do with who wrote the bytes.
// invariant: when the side input names no vocabulary the pair is incomplete and rung 1 falls back
// to `stream_view`, resolving exactly what it resolved before.
[[nodiscard]] RunOutcomeResolution
resolve_run_outcome(SideInputVerdict side_input, const RunOutcomeScan& scan,
                    const insight::semantic::ComposedSemantics& stream_view,
                    const insight::semantic::ComposedSemantics& vocabularies);

// refs: BIB:intent_identity, SRC-II-8
// post: returns the test-file WHERE coordinate as a view into the content's bytes, or empty when no
// composed location row matches.
// invariant: canon owns the three `LocationMatchKind` algorithms; the composed rows are the
// dialect-independent file-naming vocabulary, which is why this is homed in the facade.
// invariant: the view is the LOCATION ALONE — a family fixes its end, and the same byte class
// walked backwards fixes its start.
// note: an annotation glued to the path with no separator stays OUT of the label
// pre: `content` carries the same type-borne ingest precondition as `classify` and `recognize`.
[[nodiscard]] std::string_view
recognize_location(insight::tokenization::NormalizedContent content,
                   const insight::semantic::ComposedSemantics& composed) noexcept;

} // namespace insight
