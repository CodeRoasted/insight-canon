// refs: ADR-3.D4
// invariant: SEALED — never re-exported by the facade and never installed (a PRIVATE file set),
// so nothing outside canon can import it.
export module insight.canon.detail.parse;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;
import insight.canon.compose;
import insight.canon.detail.strategy;
import insight.canon.detail.scan;

export namespace insight::tokenization
{

class FormatDetector
{
  public:
    // refs: ADR-17, SRC-SP-1
    // invariant: only REPRESENTATION strategies are named here; every DIALECT strategy arrives as a
    // factory carried by `composed`, so canon core names no dialect.
    explicit FormatDetector(const insight::semantic::ComposedSemantics& composed);

    void register_strategy(std::unique_ptr<IFormatStrategy> strategy);

    // post: the highest-confidence strategy, or the raw-text fallback on a non-empty line no
    // structured strategy claims; O(C + U) in candidates and custom strategies.
    [[nodiscard]] IFormatStrategy* detect(std::string_view line) const;

    // post: the strategy with the highest CUMULATIVE confidence over the sample — a sum, so one
    // strong line can outweigh a numerical majority; O((C + U) * N).
    [[nodiscard]] IFormatStrategy*
    detect_from_batch(std::span<const std::string_view> sample) const;

    [[nodiscard]] std::span<const std::unique_ptr<IFormatStrategy>> strategies() const noexcept;

  private:
    static constexpr std::size_t kFormatSlotCount =
        static_cast<std::size_t>(LogFormat::Unknown) + 1U;

    std::vector<std::unique_ptr<IFormatStrategy>> strategies_;
    std::vector<IFormatStrategy*> custom_strategies_;
    std::array<IFormatStrategy*, kFormatSlotCount> by_format_{};

    // invariant: reached only when no structured strategy scores on a NON-EMPTY line, so
    // unstructured text is templated rather than dropped; an empty line stays dropped.
    std::unique_ptr<IFormatStrategy> fallback_;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{
// note: declared here so the passkey's friend binds this module-attached class.
class LogParser;
} // namespace insight::tokenization

// refs: ADR-21.D4, SRC-D-TID-11, F-SRC-insight-canon:test_normalized_content_doors.cpp
// invariant: THE one non-public producer of `NormalizedContent` — the passkey's friend list is
// pinned at ONE by the door census, and growing it deletes the mechanism.
// invariant: the conformance kit must never mint here; its probes are escape-free by construction,
// so `normalize()` is a fixed point on them and the public factory serves.
// note: `extern "C++"` puts the class on the GLOBAL module so api and this shard name ONE entity.
extern "C++"
{
    namespace insight::tokenization
    {
        class LogParserPasskey
        {
          public:
            [[nodiscard]] NormalizedContent mint(std::string_view stage1_bytes) const noexcept
            {
                return NormalizedContent{stage1_bytes};
            }

          private:
            constexpr LogParserPasskey() noexcept = default;
            friend class LogParser;
        };
    } // namespace insight::tokenization
}

export namespace insight::tokenization
{

// invariant: NOT thread-safe — one instance per thread or strand.
class LogParser
{
  public:
    // refs: ADR-17
    // pre: `composed` is borrowed — the composed vocabulary must outlive the parser.
    LogParser(ArenaAllocator& arena, const insight::semantic::ComposedSemantics& composed);

    // post: auto-detection is off; a format no registered strategy carries re-enables it instead.
    void set_format(LogFormat fmt);

    void set_auto_detect(bool enabled);

    // post: the line is copied into the arena, so every string_view on the returned ParsedLine
    // stays valid until the arena is reset.
    [[nodiscard]] std::expected<ParsedLine, std::string> parse_line(std::string_view line);

    // pre: `stable_line` and every view sliced from it stay valid for the arena's lifetime; this
    // door makes no arena copy.
    [[nodiscard]] std::expected<ParsedLine, std::string> parse_stable(std::string_view stable_line);

    [[nodiscard]] std::vector<std::expected<ParsedLine, std::string>>
    parse_batch(std::span<const std::string_view> lines);

    // refs: ADR-21.D4, SRC-D-TID-11
    // pre: `stage1_content` is strategy-produced; any other bytes go through `normalize()`.
    // post: attests WHO minted, never WHAT ran before — via `parse_line` the bytes carry stage-1
    // performance, via `parse_stable` door provenance only.
    // note: non-static on purpose — the caller must hold the performer, not name its class.
    // NOLINTNEXTLINE(readability-convert-member-functions-to-static)
    [[nodiscard]] NormalizedContent attest(std::string_view stage1_content) const noexcept
    {
        return LogParserPasskey{}.mint(stage1_content);
    }

    [[nodiscard]] std::size_t lines_parsed() const noexcept;
    [[nodiscard]] std::size_t lines_failed() const noexcept;
    [[nodiscard]] LogFormat detected_format() const noexcept;
    // post: the format the last line ROUTED to, Unknown until one routes — detected_format()
    // reports only an explicit set_format() and is Unknown under auto-detect.
    [[nodiscard]] LogFormat routed_format() const noexcept;

  private:
    [[nodiscard]] IFormatStrategy* select_strategy(std::string_view line);
    // note: a non-owning facade over a caller-managed arena, which outlives the parser.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    ArenaAllocator& arena_;
    // note: borrowed — the composed vocabulary must outlive the parser.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const insight::semantic::ComposedSemantics& composed_;
    FormatDetector detector_;
    IFormatStrategy* active_strategy_{nullptr};
    // invariant: tried first on every line to short-circuit the O(strategies) detection scan; full
    // detection resumes when its confidence returns 0.0.
    IFormatStrategy* sticky_strategy_{nullptr};
    bool auto_detect_{true};
    LogFormat last_format_{LogFormat::Unknown};
    std::size_t parsed_count_{0};
    std::size_t failed_count_{0};
    // refs: F-SRC-insight-canon:test_transport_peel_equivalence_gate.cpp
    // invariant: lines carrying NO EVENT — empty, or all escape bytes — are counted here and
    // never in failed_count_, which gates the failure warns and the failure rate.
    std::size_t skipped_count_{0};
    // refs: SRC-D-TID-11
    // invariant: the strip's result is never longer than its input, so the retained capacity makes
    // stage 1 allocation-free in steady state.
    std::string escape_scratch_;
};

} // namespace insight::tokenization
