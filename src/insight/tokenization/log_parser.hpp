#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

#include "insight/core/types.hpp"
#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/format_detector.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include "insight/utils/result.hpp"

namespace insight::tokenization
{

// LogParser wraps arena + FormatDetector + active strategy.
// Thread-safety: NOT thread-safe; use one instance per thread / strand.
class LogParser
{
  public:
    explicit LogParser(ArenaAllocator& arena);

    // Force a specific format; disables auto-detection.
    void set_format(LogFormat fmt);

    // Enable / disable per-line auto-detection (default: enabled).
    void set_auto_detect(bool enabled);

    // Parse a single line. Once a strategy is selected, the line is copied into
    // the arena so string_views inside the returned ParsedLine are stable.
    [[nodiscard]] insight::Result<ParsedLine> parse_line(std::string_view line);

    // Like parse_line() but skips the arena store_string() copy.
    // The caller guarantees that `stable_line` and all string_views sliced from
    // it remain valid for the arena's lifetime (e.g. mmap'd or pre-stored buffers).
    [[nodiscard]] insight::Result<ParsedLine> parse_stable(std::string_view stable_line);

    [[nodiscard]] std::vector<insight::Result<ParsedLine>>
    parse_batch(std::span<const std::string_view> lines);

    [[nodiscard]] std::size_t lines_parsed() const noexcept;
    [[nodiscard]] std::size_t lines_failed() const noexcept;
    [[nodiscard]] LogFormat detected_format() const noexcept;

  private:
    // Selects the active strategy for the given line, updating sticky/active
    // state as a side-effect. Returns nullptr if no strategy matches.
    [[nodiscard]] IFormatStrategy* select_strategy(std::string_view line);
    ArenaAllocator& arena_; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members): parser is
                            // a non-owning facade over a caller-managed arena.
    FormatDetector detector_;
    IFormatStrategy* active_strategy_{nullptr};
    // Sticky: remembers the last auto-detected strategy. Tried first on each
    // line to short-circuit the O(strategies) detection scan for homogeneous
    // streams (the common case). Falls back to full detection when confidence
    // returns 0.0 (format change) or on the first line.
    IFormatStrategy* sticky_strategy_{nullptr};
    bool auto_detect_{true};
    std::size_t parsed_count_{0};
    std::size_t failed_count_{0};
};

} // namespace insight::tokenization
