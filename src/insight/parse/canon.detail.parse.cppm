// insight.canon.detail.parse — SEALED detection/parsing domain (1.5.2 domain decomposition,
// §11.9.11). FormatDetector (strategy registry + majority vote) and LogParser (arena + detector +
// sticky active-strategy state). Top of the strategy chain: imports detail.strategy for the
// IFormatStrategy/ParsedLine contract it routes. Never re-exported by the facade and never
// installed (PRIVATE file set).
export module insight.canon.detail.parse;
import insight.canon.internal;        // std + global C types
import insight.canon.api;             // LogFormat, ArenaAllocator
import insight.canon.detail.strategy; // IFormatStrategy, ParsedLine

// ──────── from src/insight/tokenization/format_detector.hpp ────────
export namespace insight::tokenization
{

class FormatDetector
{
  public:
    FormatDetector();

    void register_strategy(std::unique_ptr<IFormatStrategy> strategy);

    // Returns best-matching strategy for a given line
    [[nodiscard]] IFormatStrategy* detect(std::string_view line) const;

    // Detect from a sample batch (majority vote)
    [[nodiscard]] IFormatStrategy*
    detect_from_batch(std::span<const std::string_view> sample) const;

    // Get all registered strategies
    [[nodiscard]] std::span<const std::unique_ptr<IFormatStrategy>> strategies() const noexcept;

  private:
    static constexpr std::size_t kFormatSlotCount =
        static_cast<std::size_t>(LogFormat::Unknown) + 1U;

    std::vector<std::unique_ptr<IFormatStrategy>> strategies_;
    std::vector<IFormatStrategy*> custom_strategies_;
    std::array<IFormatStrategy*, kFormatSlotCount> by_format_{};

    // Last-resort catch-all. Used only when no structured strategy scores on a
    // non-empty line, so unstructured text is templated rather than dropped.
    std::unique_ptr<IFormatStrategy> fallback_;
};

} // namespace insight::tokenization

export namespace insight::tokenization
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
    [[nodiscard]] std::expected<ParsedLine, std::string> parse_line(std::string_view line);

    // Like parse_line() but skips the arena store_string() copy.
    // The caller guarantees that `stable_line` and all string_views sliced from
    // it remain valid for the arena's lifetime (e.g. mmap'd or pre-stored buffers).
    [[nodiscard]] std::expected<ParsedLine, std::string> parse_stable(std::string_view stable_line);

    [[nodiscard]] std::vector<std::expected<ParsedLine, std::string>>
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
