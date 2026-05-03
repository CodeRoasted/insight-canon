#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "arena_allocator.hpp"
#include "canonical_event.hpp"
#include "drain_config.hpp"
#include "insight/utils/result.hpp"

namespace insight::tokenization
{

// Phase 1 facade: raw log line → CanonicalEvent.
// Thread-safety: NOT thread-safe; use one instance per thread / strand.
class Tokenizer
{
  public:
    explicit Tokenizer(ArenaAllocator& arena, DrainConfig drain_config = {});
    ~Tokenizer();

    Tokenizer(const Tokenizer&) = delete;
    Tokenizer& operator=(const Tokenizer&) = delete;
    Tokenizer(Tokenizer&&) noexcept;
    Tokenizer& operator=(Tokenizer&&) noexcept;

    [[nodiscard]] insight::Result<CanonicalEvent> process_line(std::string_view raw_line);

    // Like process_line() but skips the arena copy of the raw line.
    // The caller guarantees that stable_line (and all string_views sliced from
    // it by the format strategy) remain valid for the arena's lifetime, e.g.
    // lines from a mmap'd file or a pre-stored arena buffer.
    [[nodiscard]] insight::Result<CanonicalEvent> process_stable_line(std::string_view stable_line);

    [[nodiscard]] std::vector<insight::Result<CanonicalEvent>>
    process_batch(std::span<const std::string_view> lines);

    [[nodiscard]] std::size_t events_produced() const noexcept;
    [[nodiscard]] std::size_t lines_parsed() const noexcept;
    [[nodiscard]] std::size_t cluster_count() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace insight::tokenization
