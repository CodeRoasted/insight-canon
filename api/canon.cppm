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

// Phase 1 facade: raw log line → CanonicalEvent.
// Thread-safety: NOT thread-safe; use one instance per thread / strand.
class Tokenizer
{
  public:
    explicit Tokenizer(ArenaAllocator& arena, MaskConfig mask_config = {});
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
