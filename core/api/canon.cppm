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
export import insight.canon.compose; // compose()/ComposedSemantics (ADR 0024 §3/§4) — Tokenizer takes it

// ──────── from api/insight/tokenization/tokenizer_engine.hpp ────────
export namespace insight::tokenization
{

// ── Composed-semantics walkers (ADR 0024 §3/§4) ──────────────────────────────────────────────────
// The dialect-recognition mechanisms, homed in the facade (not api) because they consume the composed
// tables — ComposedSemantics lives in insight.canon.compose, which imports api, so a walker in api
// would close a cycle. Canon owns the ALGORITHM; the composed rows are the DATA. Byte-for-byte
// equivalent to the pre-split hardcoded StructuralRoleRegistry / IntentMarkerRegistry / recognize_location.

// Classify a LINE's structural role from the composed role rows (longest-match; a row fires when its
// gate is kAnyFormat or equals `format`). None when no row matches.
[[nodiscard]] StructuralRole classify(std::string_view content, LogFormat format,
                                      const insight::semantic::ComposedSemantics& composed) noexcept;

// Recognize an intent marker from the composed marker rows (format-gated, longest-match). The payload
// is the content after the matched prefix; the alignment class + instance discriminant are derived by
// canon's canonicalize_intent / discriminant_of. None when no gated row matches.
[[nodiscard]] IntentMarker recognize(std::string_view content, LogFormat format,
                                     const insight::semantic::ComposedSemantics& composed) noexcept;

// Phase 1 facade: raw log line → CanonicalEvent.
// Thread-safety: NOT thread-safe; use one instance per thread / strand.
class Tokenizer
{
  public:
    // No default composition in core (ADR 0024 §3): every binary declares its package set and threads
    // the ComposedSemantics (which the Tokenizer does NOT own — it must outlive the Tokenizer).
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

// Location recognition (intent_identity_model.md §5.3/§5.4, II-8) — the test-file WHERE coordinate,
// homed in the facade because it walks the composed location rows (ComposedSemantics is in
// insight.canon.compose). Canon owns the three LocationMatchKind algorithms; the composed rows are the
// dialect-independent file-naming vocabulary. Returns a view into `content`, or empty. Byte-for-byte
// equivalent to the pre-split hardcoded recognize_location.
[[nodiscard]] std::string_view recognize_location(std::string_view content,
                                                  const insight::semantic::ComposedSemantics& composed) noexcept;

} // namespace insight
