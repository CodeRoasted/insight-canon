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
// Canon owns the ALGORITHMS (the format-gated token map, the console-tail scan, the D-OUT-RUN-1
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

// The whole-log console-tail scan (§3.2 — the degenerate "only a console log" source). One
// parse-only pass (no masking): per line, the resolved view's OutcomeMarkerRow set is walked and the
// LONGEST matching prefix wins within the line; the LAST such line wins across the log.
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

// D-OUT-RUN-1 — the strict total resolution order, NEVER a reconciliation:
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

[[nodiscard]] RunOutcomeResolution
resolve_run_outcome(std::string_view side_input_token, const RunOutcomeScan& scan,
                    const insight::semantic::ComposedSemantics& composed);

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
