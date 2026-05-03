#include "insight/sequence/sequence_engine.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace insight::sequence
{

namespace
{
    constexpr std::size_t kHashLeftShift{6U};
    constexpr std::size_t kHashRightShift{2U};

    [[nodiscard]] std::size_t mix(std::size_t seed, EventID value) noexcept
    {
        constexpr std::size_t kGoldenRatio = 0x9e3779b97f4a7c15ULL;
        seed ^= static_cast<std::size_t>(value) + kGoldenRatio + (seed << kHashLeftShift) +
                (seed >> kHashRightShift);
        return seed;
    }

} // namespace

SequenceEngine::SequenceEngine() : SequenceEngine{SequenceConfig{}} {}

SequenceEngine::SequenceEngine(SequenceConfig config) : config_{config}
{
    config_.max_ngram_size =
        std::clamp<std::size_t>(config_.max_ngram_size, 2, kMaxTrackedNgramSize);
    // Slots [0, max_ngram_size]; index 0 and 1 unused.
    ngram_counts_.assign(config_.max_ngram_size + 1, {});
    ngram_totals_.assign(config_.max_ngram_size + 1, 0);
}

std::size_t SequenceEngine::NGramKeyHash::operator()(const NGramKey& key) const noexcept
{
    std::size_t seed = key.size;
    for (std::size_t i = 0; i < key.size; ++i)
        seed = mix(seed, key.ids[i]);
    return seed;
}

std::size_t SequenceEngine::EdgeKeyHash::operator()(const EdgeKey& key) const noexcept
{
    return mix(mix(0, key.from), key.to);
}

void SequenceEngine::clear()
{
    total_ingested_ = 0;
    ring_fill_ = 0;
    last_.reset();
    event_counts_.clear();
    transitions_.clear();
    edge_count_ = 0;
    for (auto& ngram_map : ngram_counts_)
        ngram_map.clear();
    std::ranges::fill(ngram_totals_, 0);
}

void SequenceEngine::account_event(EventID event_id)
{
    ++event_counts_[event_id];
}

void SequenceEngine::account_transition(EventID from, EventID dst)
{
    const EdgeKey key{from, dst};
    auto iter{transitions_.find(key)};
    if (iter != transitions_.end())
    {
        ++iter->second;
        return;
    }
    if (edge_count_ >= config_.max_transitions)
        return; // bounded: drop new edges past the cap
    transitions_.emplace(key, 1U);
    ++edge_count_;
}

void SequenceEngine::account_ngram(std::size_t ngram_size)
{
    if (ngram_size < 2 || ngram_size > config_.max_ngram_size ||
        static_cast<std::size_t>(ring_fill_) < ngram_size)
        return;

    NGramKey key{.size = static_cast<std::uint8_t>(ngram_size)};
    const std::size_t start{static_cast<std::size_t>(ring_fill_) - ngram_size};
    for (std::size_t index = 0; index < ngram_size; ++index)
        key.ids[index] = ngram_ring_[start + index];

    auto& bucket{ngram_counts_[ngram_size]};
    auto bucket_it{bucket.find(key)};
    if (bucket_it == bucket.end())
    {
        if (bucket.size() >= config_.max_ngram_keys)
            return; // bounded: drop new keys past the cap
        bucket.emplace(key, 1);
    }
    else
    {
        ++bucket_it->second;
    }
    ++ngram_totals_[ngram_size];
}

void SequenceEngine::ingest(const tokenization::CanonicalEvent& event)
{
    const EventID event_id = event.id;

    if (last_)
        account_transition(*last_, event_id);

    // Push into ring buffer (shift-left; kMaxTrackedNgramSize == 3 → at most 2 copies).
    if (static_cast<std::size_t>(ring_fill_) < kMaxTrackedNgramSize)
    {
        ngram_ring_[ring_fill_++] = event_id;
    }
    else
    {
        for (std::size_t i{1}; i < kMaxTrackedNgramSize; ++i)
            ngram_ring_[i - 1U] = ngram_ring_[i];
        ngram_ring_[kMaxTrackedNgramSize - 1U] = event_id;
    }
    ++total_ingested_;

    account_event(event_id);

    // n-gram updates need the new event already in the ring.
    for (std::size_t ngram_size = 2; ngram_size <= config_.max_ngram_size; ++ngram_size)
        account_ngram(ngram_size);

    last_ = event_id;
}

std::size_t SequenceEngine::size() const noexcept
{
    return static_cast<std::size_t>(total_ingested_);
}

std::size_t SequenceEngine::unique_events() const noexcept
{
    return event_counts_.size();
}

std::size_t SequenceEngine::edge_count() const noexcept
{
    return edge_count_;
}

std::vector<EventTransition> SequenceEngine::transitions() const
{
    std::vector<EventTransition> out;
    out.reserve(edge_count_);

    // Two-pass: accumulate per-row totals, then emit probabilities.
    std::unordered_map<EventID, std::uint64_t> row_totals;
    row_totals.reserve(event_counts_.size());
    for (const auto& [key, count] : transitions_)
        row_totals[key.from] += count;

    for (const auto& [key, count] : transitions_)
    {
        const std::uint64_t row_total{row_totals[key.from]};
        out.push_back({.from = key.from,
                       .to = key.to,
                       .count = count,
                       .probability = row_total > 0 ? static_cast<double>(count) /
                                                          static_cast<double>(row_total)
                                                    : 0.0});
    }
    std::ranges::sort(out,
                      [](const EventTransition& lhs, const EventTransition& rhs)
                      {
                          if (lhs.count != rhs.count)
                              return lhs.count > rhs.count;
                          if (lhs.from != rhs.from)
                              return lhs.from < rhs.from;
                          return lhs.to < rhs.to;
                      });
    return out;
}

std::vector<NGramEntry> SequenceEngine::top_ngrams(std::size_t ngram_order, std::size_t top_k) const
{
    if (ngram_order < 2 || ngram_order >= ngram_counts_.size())
        return {};
    const auto& bucket{ngram_counts_[ngram_order]};
    const std::uint64_t total = ngram_totals_[ngram_order];
    const double inv_total = total > 0 ? 1.0 / static_cast<double>(total) : 0.0;

    std::vector<NGramEntry> out;
    out.reserve(bucket.size());
    for (const auto& [key, count] : bucket)
    {
        std::vector<EventID> sequence;
        sequence.reserve(key.size);
        for (std::size_t index = 0; index < key.size; ++index)
            sequence.push_back(key.ids[index]);

        out.push_back({.sequence = std::move(sequence),
                       .count = count,
                       .probability = static_cast<double>(count) * inv_total});
    }

    std::ranges::sort(out,
                      [](const NGramEntry& lhs, const NGramEntry& rhs)
                      {
                          if (lhs.count != rhs.count)
                              return lhs.count > rhs.count;
                          return lhs.sequence < rhs.sequence;
                      });
    if (out.size() > top_k)
        out.resize(top_k);
    return out;
}

DominantPath SequenceEngine::reconstruct_dominant_path(std::size_t max_steps) const
{
    DominantPath path;
    if (event_counts_.empty())
        return path;

    // Pick the most-frequent EventID as the entry point. Tie-broken
    // by lower id for determinism.
    auto start_it{event_counts_.begin()};
    for (auto it{std::next(event_counts_.begin())}; it != event_counts_.end(); ++it)
    {
        if (it->second > start_it->second ||
            (it->second == start_it->second && it->first < start_it->first))
            start_it = it;
    }

    // Build per-node outgoing adjacency from the flat edge map (cold query path).
    std::unordered_map<EventID, std::unordered_map<EventID, std::uint64_t>> adj;
    adj.reserve(event_counts_.size());
    for (const auto& [key, count] : transitions_)
        adj[key.from].emplace(key.to, count);

    // Cycle detection: small reserved vector — no hash overhead for ≤ max_steps nodes.
    std::vector<EventID> seen;
    seen.reserve(max_steps + 1U);
    EventID current = start_it->first;
    path.nodes.push_back(current);
    seen.push_back(current);

    for (std::size_t step = 0; step < max_steps; ++step)
    {
        auto out_it{adj.find(current)};
        if (out_it == adj.end() || out_it->second.empty())
            break;

        // Pick the highest-count outgoing edge; ties → lower target id.
        const auto& outgoing{out_it->second};
        std::uint64_t row_total = 0;
        for (const auto& [to_event, count] : outgoing)
            row_total += count;

        auto best{outgoing.begin()};
        for (auto it{std::next(outgoing.begin())}; it != outgoing.end(); ++it)
        {
            if (it->second > best->second ||
                (it->second == best->second && it->first < best->first))
                best = it;
        }
        if (std::ranges::find(seen, best->first) != seen.end())
        {
            path.truncated_by_cycle = true;
            break;
        }

        const double edge_prob =
            row_total > 0 ? static_cast<double>(best->second) / static_cast<double>(row_total)
                          : 0.0;
        path.cumulative_probability *= edge_prob;
        current = best->first;
        path.nodes.push_back(current);
        seen.push_back(current);
    }

    return path;
}

std::vector<BranchingEntry> SequenceEngine::branching(std::size_t top_k) const
{
    if (transitions_.empty())
        return {};

    // Build per-node {to -> count} histograms from the flat edge map.
    // Cold path; allocates a small map per source node.
    std::unordered_map<EventID, std::vector<std::uint64_t>> outgoing;
    outgoing.reserve(event_counts_.size());
    for (const auto& [key, count] : transitions_)
        outgoing[key.from].push_back(count);

    std::vector<BranchingEntry> out;
    out.reserve(outgoing.size());
    for (const auto& [node, counts] : outgoing)
    {
        std::uint64_t total = 0;
        for (auto cnt : counts)
            total += cnt;
        double entropy = 0.0;
        if (total > 0)
        {
            const double inv_total = 1.0 / static_cast<double>(total);
            for (auto cnt : counts)
            {
                if (cnt == 0)
                    continue;
                const double prob = static_cast<double>(cnt) * inv_total;
                entropy -= prob * std::log2(prob);
            }
        }
        out.push_back({.node = node,
                       .fanout = static_cast<std::uint64_t>(counts.size()),
                       .total_outgoing = total,
                       .entropy_bits = entropy});
    }

    std::ranges::sort(out,
                      [](const BranchingEntry& lhs, const BranchingEntry& rhs)
                      {
                          if (lhs.entropy_bits != rhs.entropy_bits)
                              return lhs.entropy_bits > rhs.entropy_bits;
                          if (lhs.total_outgoing != rhs.total_outgoing)
                              return lhs.total_outgoing > rhs.total_outgoing;
                          return lhs.node < rhs.node;
                      });
    if (top_k > 0 && out.size() > top_k)
        out.resize(top_k);
    return out;
}

} // namespace insight::sequence
