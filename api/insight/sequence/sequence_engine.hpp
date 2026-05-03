#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "insight/core/types.hpp"
#include "insight/tokenization/canonical_event.hpp"

namespace insight::sequence
{

// ── Configuration ──────────────────────────────────────────────

struct SequenceConfig
{
    static constexpr std::size_t kDefaultMaxTransitions = 100'000;
    static constexpr std::size_t kDefaultMaxNgramKeys = 50'000;

    // Maximum n-gram order tracked (bigrams = 2, trigrams = 3).
    std::size_t max_ngram_size{3};

    // Soft cap on the number of distinct edges retained in the
    // transition matrix. New edges past this cap are dropped (counts
    // on existing edges keep updating). Bounds memory at the cost of
    // long-tail accuracy.
    std::size_t max_transitions{kDefaultMaxTransitions};

    // Soft cap on the number of distinct n-gram keys tracked per
    // order. Same dropping policy as max_transitions.
    std::size_t max_ngram_keys{kDefaultMaxNgramKeys};
};

// ── Public views ───────────────────────────────────────────────

struct EventTransition
{
    EventID from{};
    EventID to{};
    std::uint64_t count{};
    double probability{}; // P(to | from)
};

struct NGramEntry
{
    std::vector<EventID> sequence; // size == n (2 or 3)
    std::uint64_t count{};
    double probability{}; // count / total observations of this n
};

struct DominantPath
{
    std::vector<EventID> nodes;
    double cumulative_probability{1.0};
    bool truncated_by_cycle{false};
};

// Per-node fanout / outgoing-distribution entropy. Mirrors MetaLog
// spec v0.2.0 §4.2 `behavior.branching` entries with EventID stand-ins
// for the spec's template_id strings (the metalog stage substitutes
// the content-derived hash on serialisation).
struct BranchingEntry
{
    EventID node{};
    std::uint64_t fanout{0};
    std::uint64_t total_outgoing{0};
    double entropy_bits{0.0};
};

// ── Engine ─────────────────────────────────────────────────────
//
// Streaming sequence model. Single-threaded. Bounded memory once
// max_transitions / max_ngram_keys are reached.
//
// size() returns total ingested event count. Primary API:
// transitions(), top_ngrams(n, k), reconstruct_dominant_path().
class SequenceEngine
{
  public:
    SequenceEngine();
    explicit SequenceEngine(SequenceConfig config);

    void ingest(const tokenization::CanonicalEvent& event);
    void clear();

    // Total number of events ingested (not a ring-window size).
    [[nodiscard]] std::size_t size() const noexcept;

    // ── Real Phase 2 surface ──
    [[nodiscard]] std::size_t unique_events() const noexcept;
    [[nodiscard]] std::size_t edge_count() const noexcept;

    // All edges with their probabilities, sorted by count desc then
    // (from, to) asc for determinism.
    [[nodiscard]] std::vector<EventTransition> transitions() const;

    // Top-k n-grams of order `ngram_order` (must be 2 or 3), sorted by count
    // desc then key asc. Returns at most `top_k` entries.
    [[nodiscard]] std::vector<NGramEntry> top_ngrams(std::size_t ngram_order,
                                                     std::size_t top_k) const;

    static constexpr std::size_t kDefaultMaxSteps = 8;
    // Greedy dominant-path reconstruction:
    //   * starts at the EventID with the highest event count;
    //   * at each step follows the most likely outgoing transition;
    //   * stops on a sink, on a revisited node (cycle), or after
    //     `max_steps` hops.
    // Returns an empty path if the engine is empty.
    [[nodiscard]] DominantPath
    reconstruct_dominant_path(std::size_t max_steps = kDefaultMaxSteps) const;

    // Per-node fanout statistics (MetaLog SPEC §4.2). Cold query — one
    // sweep over the flat transition table, O(edge_count). Sorted by
    // entropy_bits desc then total_outgoing desc then node asc for
    // determinism. `top_k` caps the result; pass 0 for "all nodes".
    [[nodiscard]] std::vector<BranchingEntry> branching(std::size_t top_k = 0) const;

  private:
    static constexpr std::size_t kMaxTrackedNgramSize = 3;

    struct NGramKey
    {
        std::array<EventID, kMaxTrackedNgramSize> ids{};
        std::uint8_t size{};

        [[nodiscard]] bool operator==(const NGramKey& other) const noexcept = default;
    };

    struct NGramKeyHash
    {
        [[nodiscard]] std::size_t operator()(const NGramKey& key) const noexcept;
    };

    // Flat edge key — single O(1) lookup per ingest vs two for the old
    // nested unordered_map<EventID, unordered_map<EventID, uint64_t>>.
    struct EdgeKey
    {
        EventID from{};
        EventID to{};
        [[nodiscard]] bool operator==(const EdgeKey&) const noexcept = default;
    };

    struct EdgeKeyHash
    {
        [[nodiscard]] std::size_t operator()(const EdgeKey& key) const noexcept;
    };

    void account_event(EventID event_id);
    void account_transition(EventID from, EventID dst);
    void account_ngram(std::size_t ngram_size);

    SequenceConfig config_{};

    // Bounded rolling window for n-gram construction.
    // Replaces unbounded events_ vector — ingest runs for hours without OOM.
    std::uint64_t total_ingested_{0};
    std::array<EventID, kMaxTrackedNgramSize> ngram_ring_{};
    std::uint8_t ring_fill_{0};

    std::optional<EventID> last_;

    // event_id -> total observations
    std::unordered_map<EventID, std::uint64_t> event_counts_;

    // Flat (from, to) -> count. Single hash lookup per ingest.
    std::unordered_map<EdgeKey, std::uint64_t, EdgeKeyHash> transitions_;
    std::size_t edge_count_{0};

    // n-gram key -> count. Fixed-size keys avoid per-event heap
    // allocation on the hot ingest path for supported bigrams/trigrams.
    // Indexed by order [2..max_ngram_size]. Slot 0 and 1 are unused.
    std::vector<std::unordered_map<NGramKey, std::uint64_t, NGramKeyHash>> ngram_counts_;
    std::vector<std::uint64_t> ngram_totals_;
};

} // namespace insight::sequence
