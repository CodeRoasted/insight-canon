// insight.canon — public facade (ADR-3.D4). Consumers
// `import insight.canon;` unchanged. Re-exports the public api surface; the
// insight.canon.detail.{scan,strategy,mask,parse} shards are NOT re-exported (sealed, build-only).
// Tokenizer lives HERE (not in api): its impl needs LogParser/the masker from the detail shards,
// which import api — homing it above detail breaks the api↔detail cycle (the ADR-3 facade seam).
// The Tokenizer decl uses only api types (ArenaAllocator/MaskConfig/CanonicalEvent) + std;
// tokenizer_engine.cpp (module insight.canon) imports detail.{strategy,mask,parse} in its purview.
export module insight.canon;
import insight.canon.internal; // std (expected/unique_ptr/vector/span/string for the Tokenizer decl)
export import insight.canon.api; // public surface (types, det_math, arena, ...)
// compose()/ComposedSemantics (ADR-17) — Tokenizer takes it. The comment sits ABOVE the
// directive deliberately: as a trailing comment it pushed the line past the column limit, and the
// formatter then wrapped the MODULE NAME across lines. gcc-15 rejects that (a module-import
// directive is one logical line); clang-21 accepts it, so the break reaches only the ship
// toolchain. Keep this line short enough that no formatter has a reason to touch it.
export import insight.canon.compose;
// The transport vocabulary (ADR-23): IngestDeclaration, the catalogue, and the stream-scoped
// peel. Re-exported because a CONSUMER declares the stack — it is the caller's provenance, not a
// package's data. Same short-line discipline as the directive above (gcc-15 rejects a module name
// wrapped across lines).
export import insight.canon.transport;

// ──────── from api/insight/tokenization/tokenizer_engine.hpp ────────
export namespace insight::tokenization
{

// ── Composed-semantics walkers (ADR-17) ──────────────────────────────────────────────────────────
// The dialect-recognition mechanisms, homed in the facade (not api) because they consume the
// composed tables — ComposedSemantics lives in insight.canon.compose, which imports api, so a
// walker in api would close a cycle. Canon owns the ALGORITHM; the composed rows are the DATA.
// Byte-for-byte equivalent to the pre-split hardcoded StructuralRoleRegistry / IntentMarkerRegistry
// / recognize_location.

// ⚠ NO DIALECT COORDINATE, ON ANY OF THEM (ADR-22). `composed` is the RESOLVED STREAM's
// view — `resolve_stream` filtered the declared dialect and channel into it once, before the first
// line — so a row that is in the table is a row that fires. Re-introducing a per-line format or
// dialect argument here would restore the content dependence T4 removed: the argument used to be
// `LogParser::routed_format()`, the per-line detector winner under a sticky-strategy fast path.

// ⚠⚠ THE PRECONDITION ON `content` IS THE TYPE (ADR-21.D3 — carried by the type, so an
// unnormalized caller outside canon fails to compile). `NormalizedContent` is producible only by
// stage 1 (`normalize`, insight.canon.api) followed by a suffix-taking stage 2 — the declared
// `TransportStack::peel(NormalizedLine)` or the caller's own `undeclared_suffix` — so a caller
// that has not normalized DOES NOT COMPILE at canon's public boundary. What used to stand here
// was the same obligation as prose; it was satisfied by one of three consumers and cost 1 077 of
// 3 193 GitLab markers, silently, across two call sites that both looked correct. The type does
// NOT prove the RIGHT stage 2 ran (§12.3) — ADR-22's strip divergence stands, and the
// eidos/canon reconciliation still rides T5.

// Classify a LINE's structural role from the resolved view's role rows (longest-match). None when
// no row matches.
[[nodiscard]] StructuralRole
classify(NormalizedContent content, const insight::semantic::ComposedSemantics& composed) noexcept;

// Recognize an intent marker from the resolved view's marker rows (longest-match). The payload is
// the content after the matched prefix; the alignment class + instance discriminant are derived by
// canon's canonicalize_intent / discriminant_of. None when no row matches. The returned marker's
// `name`/`discriminant` VIEW the handed content's bytes (ADR-18) — the caller's storage must
// outlive them.
[[nodiscard]] IntentMarker recognize(NormalizedContent content,
                                     const insight::semantic::ComposedSemantics& composed) noexcept;

// Phase 1 facade: raw log line → CanonicalEvent.
// Thread-safety: NOT thread-safe; use one instance per thread / strand.
class Tokenizer
{
  public:
    // No default composition in core (ADR-17): every binary declares its package set and
    // threads the ComposedSemantics (which the Tokenizer does NOT own — it must outlive the
    // Tokenizer).
    explicit Tokenizer(ArenaAllocator& arena, MaskConfig mask_config,
                       const insight::semantic::ComposedSemantics& composed);
    ~Tokenizer();

    Tokenizer(const Tokenizer&) = delete;
    Tokenizer& operator=(const Tokenizer&) = delete;
    Tokenizer(Tokenizer&&) noexcept;
    Tokenizer& operator=(Tokenizer&&) noexcept;

    [[nodiscard]] std::expected<CanonicalEvent, std::string>
    process_line(std::string_view raw_line);

    // Like process_line() but skips the arena copy of the raw line.
    // The caller guarantees that stable_line (and all string_views sliced from
    // it by the format strategy) remain valid for the arena's lifetime, e.g.
    // lines from a mmap'd file or a pre-stored arena buffer.
    [[nodiscard]] std::expected<CanonicalEvent, std::string>
    process_stable_line(std::string_view stable_line);

    [[nodiscard]] std::vector<std::expected<CanonicalEvent, std::string>>
    process_batch(std::span<const std::string_view> lines);

    // ── The ACQUISITION-tier record-source unpack (DN-29.D6, L3 of DN-29.D15) ─────────────────
    // If `raw_line` is recognised as an OTLP span-export DOCUMENT, replace `records` with the N
    // canonical flat-span records it carries and return true; otherwise return false with
    // `records` untouched and the caller stays on its ordinary 1:1 path.
    //
    // ⚠ FOR THE ENTRY THAT HOLDS THE WHOLE INPUT — a file, a CLI read, a receiver body — and NEVER
    // for a frame-oriented streaming path. The SHM plane carries a fixed 4096-byte payload while
    // an export has no declared size, so a document crossing it arrives truncated; document mode
    // there would put an unbounded object inside a bounded-memory instrument. That is why the
    // record entry REFUSES a document rather than unpacking one, and why this is a separate,
    // deliberately over-triggering door rather than a widening of that one.
    //
    // It yields RECORDS, not events: window closure resets the caller's arena, so the caller must
    // be free to tokenize record k only once it is done with k-1. Tokenize each with process_line
    // — never process_stable_line — because `records` is caller scratch the next document reuses.
    [[nodiscard]] static bool unpack_span_document(std::string_view raw_line,
                                                   std::vector<std::string>& records);

    [[nodiscard]] std::size_t events_produced() const noexcept;
    [[nodiscard]] std::size_t lines_parsed() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace insight::tokenization

export namespace insight
{

// ── Run-outcome recognition + resolution (ADR-17 / insight_run_outcome_model.md §3–§4) ──────────
// Canon owns the ALGORITHMS (the format-gated token map, the console-tail scan, the SRC-D-OUT-RUN-1
// precedence resolver); the composed OutcomeTokenRow/OutcomeMarkerRow sets are the DATA. Homed in
// the facade (they consume ComposedSemantics; the scan drives the sealed LogParser).

// Map a native verdict token through the RESOLVED VIEW's OutcomeTokenRow set. The dialect gate was
// applied at stream resolution (SRC-II-6 still holds — a Jenkins token is simply not in a GHA
// stream's view), so an undeclared stream carries no concretely-gated row and nothing resolves:
// fail-closed on depth. nullopt = no row in this view claims the token (distinct from a row that
// maps it TO RunOutcome::Unknown, e.g. Jenkins NOT_BUILT).
[[nodiscard]] std::optional<RunOutcome>
map_outcome_token(std::string_view token,
                  const insight::semantic::ComposedSemantics& composed) noexcept;

// ── A CALLER-DECLARED verdict (DN-32.D6) ────────────────────────────────────────────────────────
//
// A verdict supplied by the caller is a PAIR — `(vocabulary, token)` — never a bare string, and the
// two halves answer different questions that ADR-22.D1's authorship test keeps apart:
//
//   * the STREAM's dialect answers *who wrote the bytes*. For a raw cmake/ninja build log the true
//     answer is "no dialect", and that is a fact, not a gap.
//   * the SIDE INPUT's vocabulary answers *who supplied the verdict* — a CI Action reading its own
//     platform's job status.
//
// Requiring cmake's bytes to carry GitHub's vocabulary is a category error, and it is what made the
// engine fail closed on our own dogfood: every `outcome_tokens` row is gated to its owning package,
// the gate was evaluated against the STREAM's resolved dialect, and a dialect-free build log
// therefore resolved nothing — *"this stream's resolved vocabulary carries no run-outcome tokens"*
// — even though the vocabulary needed already existed and already covered the token exactly.
//
// ⚠ THIS IS NOT "resolve without a vocabulary". A bare token is NOT self-interpreting: GHA says
// `failure`, GitLab says `failed`, Jenkins says `FAILURE` and also `UNSTABLE`, which has no
// universal meaning at all. Resolving a bare string would force a native→canonical spelling list
// into CORE. Naming the vocabulary keeps the mapping as package data (ADR-17.D1) and core learns
// nothing.
struct SideInputVerdict
{
    // The producer's own spelling, verbatim — never a pre-resolved RunOutcome. Empty = the caller
    // declared nothing, which is a CHOICE and asserts nothing (DN-32.D7).
    std::string_view token;
    // The name of the package whose `outcome_tokens` interpret that spelling. Empty = not named,
    // and then the token resolves against the STREAM's view exactly as before — an incomplete pair
    // gains nothing, which is the fail-safe direction.
    std::string_view vocabulary;
};

// Map a native verdict token through a NAMED vocabulary's `outcome_tokens`, INDEPENDENTLY of any
// stream's resolved dialect. `composed` is the FULL composition (every package's rows), not a
// stream view — the whole point is that the stream's dialect is not consulted.
//
// ⚠ IT DOES NOT REINTRODUCE A PER-LINE GATE INPUT, and that distinction is what makes it safe.
// The determinism fix ADR-22 records is that a DECLARED ROW's gate must not be a function of
// CONTENT — the old `LogParser::routed_format()` was the per-line detector winner, so which rows
// fired depended on the bytes. This gate coordinate is a caller's declaration, fixed before the
// first line and consulted exactly ONCE per side per diff, never on the hot path. Nothing here
// walks a line.
//
// nullopt = no row in the named vocabulary claims the token (distinct from a row that maps it TO
// RunOutcome::Unknown, e.g. Jenkins NOT_BUILT), or the vocabulary was not named at all. An UNKNOWN
// vocabulary NAME terminates, for the same reason and through the same door as an unknown dialect
// (ADR-22.D5): a typo that silently disabled the verdict would disarm every rule that reads it.
//
// ⚠ `composed` MUST be the FULL composition, and passing a stream view is a silent no-op rather
// than an error: a view has already been filtered, and a FRESH composition is the
// doubly-Unspecified view in which every concretely-gated row is already dropped. The
// implementation re-derives through `for_stream`, the one ratified evaluation point of the dialect
// gate, which reads the UNFILTERED tables — so this cannot be "fixed" by walking the view.
//
// Not noexcept: `for_stream` allocates. Cold path — once per side per diff, never per line.
[[nodiscard]] std::optional<RunOutcome>
map_outcome_token_in(std::string_view token, std::string_view vocabulary,
                     const insight::semantic::ComposedSemantics& composed);

// The whole-log console-tail scan (§3.2 — the degenerate "only a console log" source). One
// parse-only pass (no masking): per line, the resolved view's OutcomeMarkerRow set is walked and
// the LONGEST matching prefix wins within the line; the LAST such line wins across the log.
//
// Longest-prefix-wins is grammar-5 (ADR-17) and it replaces "the last row that matched overwrites"
// — a walk whose winner was a function of DECLARATION ORDER. GitLab needs
// `ERROR: Job failed: canceled` (Aborted) to beat `ERROR: Job failed` (Failure), and resolving that
// by where the rows sit in an array is a silent coupling of a verdict to a package's formatting.
// `classify` and `recognize` already guarantee longest-match; this makes the third walker agree.
// Behaviour-preserving for every shipped package that predates it: Jenkins declares one marker row,
// GHA and test_frameworks declare none, so no stream had two matching rows to order.
//
// It no longer LATCHES a dialect. It used to carry two `LogFormat` fields — the first
// outcome-bearing routed format, and the matched marker line's routed format — whose only job was
// to gate `map_outcome_token` afterwards. Both were per-line detector outputs, so the resolution a
// side-input token got depended on the stream's CONTENT; under a declared dialect the vocabulary is
// fixed before the first line and the fields have nothing left to carry (ADR-22).
struct RunOutcomeScan
{
    bool marker_present{false};
    // The winning RemainderToken row's extracted verdict word, which the resolver maps through the
    // view's OutcomeTokenRow set. Empty when no marker matched, and empty when the winner was a
    // PrefixIsVerdict row — that shape has no remainder token by construction, and `verdict` below
    // carries its answer instead.
    std::string token;
    // The winning PrefixIsVerdict row's own RunOutcome (grammar-5, ADR-17). Engaged EXACTLY when
    // the console tail resolved off the row rather than off a token, so the resolver never has to
    // ask which shape won — it prefers this and falls back to mapping `token`, and the
    // "console verdict is not in the composed vocabulary" fail-closed note stays reachable for the
    // shape that can actually produce it.
    std::optional<RunOutcome> verdict;
};

[[nodiscard]] RunOutcomeScan scan_run_outcome(std::span<const std::string> lines,
                                              const insight::semantic::ComposedSemantics& composed);

// SRC-D-OUT-RUN-1 — the strict total resolution order, NEVER a reconciliation:
//   1. the authoritative side-input token, if provided AND it maps in the detected dialect;
//   2. else the console-tail marker's last match, if present AND it maps;
//   3. else Unknown.
// When rung 1 resolves, a present-but-DISAGREEING console tail is NOT consulted (Accumulo #498 —
// a local/nested/caught outcome, not a competing whole-run verdict): the divergence is surfaced as
// a kept trace-level log + the `divergent` flag, the authoritative value stands. A token that is
// provided but does not map is never a silent misclassification: it surfaces in `note`
// (fail-closed, the SP-3/SP-4 discipline applied to values) and resolution falls down the ladder.
struct RunOutcomeResolution
{
    RunOutcome outcome{RunOutcome::Unknown};
    RunOutcome console{
        RunOutcome::Unknown};  // the console candidate's mapped value (Unknown otherwise)
    bool authoritative{false}; // rung 1 resolved
    bool divergent{false};     // rung 1 resolved AND a mapped console tail disagrees
    std::string note;          // surfaced fail-closed note ("" = clean)
};

//
// TWO compositions, and they are not interchangeable (DN-32.D6):
//   * `stream_view` — the resolved view of the stream being diffed. Rung 2 reads it, and must:
//     a console marker came out of THESE bytes, so a Jenkins marker may not fire on a GHA stream.
//   * `vocabularies` — the FULL composition. Rung 1 reads it, and only when the side input NAMES
//     its vocabulary; the declarer's vocabulary has nothing to do with who wrote the bytes.
// When the side input names no vocabulary the pair is incomplete and rung 1 falls back to
// `stream_view`, which resolves exactly what it resolved before and gains nothing it did not.
[[nodiscard]] RunOutcomeResolution
resolve_run_outcome(SideInputVerdict side_input, const RunOutcomeScan& scan,
                    const insight::semantic::ComposedSemantics& stream_view,
                    const insight::semantic::ComposedSemantics& vocabularies);

// Location recognition (bibles/intent_identity.md §8, SRC-II-8) — the test-file WHERE coordinate,
// homed in the facade because it walks the composed location rows (ComposedSemantics is in
// insight.canon.compose). Canon owns the three LocationMatchKind algorithms; the composed rows are
// the dialect-independent file-naming vocabulary. Returns a view into the content's bytes, or
// empty. Byte-for-byte equivalent to the pre-split hardcoded recognize_location. `content`
// carries the same type-borne ingest precondition as `classify`/`recognize` above.
[[nodiscard]] std::string_view
recognize_location(insight::tokenization::NormalizedContent content,
                   const insight::semantic::ComposedSemantics& composed) noexcept;

} // namespace insight
