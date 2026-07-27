// insight.canon — public facade (1.5.2 domain decomposition, §11.9.11). Consumers
// `import insight.canon;` unchanged. Re-exports the public api surface; the
// insight.canon.detail.{scan,strategy,mask,parse} shards are NOT re-exported (sealed, build-only).
// Tokenizer lives HERE (not in api): its impl needs LogParser/the masker from the detail shards,
// which import api — homing it above detail breaks the api↔detail cycle (the ADR-0002 facade seam).
// The Tokenizer decl uses only api types (ArenaAllocator/MaskConfig/CanonicalEvent) + std;
// tokenizer_engine.cpp (module insight.canon) imports detail.{strategy,mask,parse} in its purview.
export module insight.canon;
import insight.canon.internal; // std (expected/unique_ptr/vector/span/string for the Tokenizer decl)
export import insight.canon.api; // public surface (types, det_math, arena, ...)
// compose()/ComposedSemantics (ADR 0024 §3/§4) — Tokenizer takes it. The comment sits ABOVE the
// directive deliberately: as a trailing comment it pushed the line past the column limit, and the
// formatter then wrapped the MODULE NAME across lines. gcc-15 rejects that (a module-import
// directive is one logical line); clang-21 accepts it, so the break reaches only the ship
// toolchain. Keep this line short enough that no formatter has a reason to touch it.
export import insight.canon.compose;
// The transport vocabulary (ADR 0044): IngestDeclaration, the catalogue, and the stream-scoped
// peel. Re-exported because a CONSUMER declares the stack — it is the caller's provenance, not a
// package's data. Same short-line discipline as the directive above (gcc-15 rejects a module name
// wrapped across lines).
export import insight.canon.transport;

// ──────── from api/insight/tokenization/tokenizer_engine.hpp ────────
export namespace insight::tokenization
{

// ── Composed-semantics walkers (ADR 0024 §3/§4) ──────────────────────────────────────────────────
// The dialect-recognition mechanisms, homed in the facade (not api) because they consume the
// composed tables — ComposedSemantics lives in insight.canon.compose, which imports api, so a
// walker in api would close a cycle. Canon owns the ALGORITHM; the composed rows are the DATA.
// Byte-for-byte equivalent to the pre-split hardcoded StructuralRoleRegistry / IntentMarkerRegistry
// / recognize_location.

// Classify a LINE's structural role from the composed role rows (longest-match; a row fires when
// its gate is kAnyFormat or equals `format`). None when no row matches.
[[nodiscard]] StructuralRole
classify(std::string_view content, LogFormat format,
         const insight::semantic::ComposedSemantics& composed) noexcept;

// Recognize an intent marker from the composed marker rows (format-gated, longest-match). The
// payload is the content after the matched prefix; the alignment class + instance discriminant are
// derived by canon's canonicalize_intent / discriminant_of. None when no gated row matches.
[[nodiscard]] IntentMarker recognize(std::string_view content, LogFormat format,
                                     const insight::semantic::ComposedSemantics& composed) noexcept;

// Phase 1 facade: raw log line → CanonicalEvent.
// Thread-safety: NOT thread-safe; use one instance per thread / strand.
class Tokenizer
{
  public:
    // No default composition in core (ADR 0024 §3): every binary declares its package set and
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

// ── Run-outcome recognition + resolution (ADR 0025 / insight_run_outcome_model.md §3–§4) ────────
// Canon owns the ALGORITHMS (the format-gated token map, the console-tail scan, the D-OUT-RUN-1
// precedence resolver); the composed OutcomeTokenRow/OutcomeMarkerRow sets are the DATA. Homed in
// the facade (they consume ComposedSemantics; the scan drives the sealed LogParser).

// Map a native verdict token through the composed OutcomeTokenRow set, gated to `format` (a Jenkins
// token resolves against Jenkins rows only — II-6). nullopt = no row claims the token under that
// gate (distinct from a row that maps it TO RunOutcome::Unknown, e.g. Jenkins NOT_BUILT).
[[nodiscard]] std::optional<RunOutcome>
map_outcome_token(std::string_view token, LogFormat format,
                  const insight::semantic::ComposedSemantics& composed) noexcept;

// The whole-log console-tail scan (§3.2 — the degenerate "only a console log" source). One
// parse-only pass (no masking): per line, the routed format gates the composed OutcomeMarkerRow
// walk; a match extracts the remainder token (a single ASCII word, strict) and the LAST match wins.
// Also latches the log's outcome-bearing DIALECT: the first routed format any OutcomeTokenRow is
// gated on — the gate the side-input token resolves under.
struct RunOutcomeScan
{
    LogFormat dialect{LogFormat::Unknown}; // first outcome-bearing routed format; Unknown = none
    LogFormat marker_format{LogFormat::Unknown}; // the matched marker line's routed format
    bool marker_present{false};
    std::string token; // the last console-tail verdict token (empty when !marker_present)
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

// Location recognition (intent_identity_model.md §5.3/§5.4, II-8) — the test-file WHERE coordinate,
// homed in the facade because it walks the composed location rows (ComposedSemantics is in
// insight.canon.compose). Canon owns the three LocationMatchKind algorithms; the composed rows are
// the dialect-independent file-naming vocabulary. Returns a view into `content`, or empty.
// Byte-for-byte equivalent to the pre-split hardcoded recognize_location.
[[nodiscard]] std::string_view
recognize_location(std::string_view content,
                   const insight::semantic::ComposedSemantics& composed) noexcept;

} // namespace insight
