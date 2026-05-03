// Test file: magic numbers are intentional test values; short identifiers follow
// test-helper conventions; literal suffixes are test-style.
// NOLINTBEGIN(readability-magic-numbers,readability-identifier-length,readability-uppercase-literal-suffix)
#include "insight/sequence/sequence_engine.hpp"

#include <algorithm>
#include <gtest/gtest.h>

#include <cstddef>
#include <initializer_list>
#include <vector>

#include "insight/tokenization/canonical_event.hpp"

namespace insight::sequence
{
namespace
{
    tokenization::CanonicalEvent make_event(EventID id)
    {
        tokenization::CanonicalEvent event;
        event.id = id;
        return event;
    }

    void ingest_ids(SequenceEngine& engine, std::initializer_list<EventID> ids)
    {
        for (EventID id : ids)
            engine.ingest(make_event(id));
    }
} // namespace

// ── Basic ingestion ──────────────────────────────────────────────────────────

TEST(SequenceEngineBasic, SizeTracksTotal)
{
    SequenceEngine engine;
    EXPECT_EQ(engine.size(), 0U);
    ingest_ids(engine, {1, 2, 3});
    EXPECT_EQ(engine.size(), 3U);
}

TEST(SequenceEngineBasic, UniqueEventsCount)
{
    SequenceEngine engine;
    ingest_ids(engine, {1, 2, 1, 3, 1});
    EXPECT_EQ(engine.unique_events(), 3U);
}

TEST(SequenceEngineBasic, ClearResetsAll)
{
    SequenceEngine engine;
    ingest_ids(engine, {1, 2, 3, 1, 2});
    engine.clear();

    EXPECT_EQ(engine.size(), 0U);
    EXPECT_EQ(engine.unique_events(), 0U);
    EXPECT_EQ(engine.edge_count(), 0U);
    EXPECT_TRUE(engine.transitions().empty());
    EXPECT_TRUE(engine.top_ngrams(2, 10).empty());

    // Re-ingest after clear should work cleanly.
    ingest_ids(engine, {4, 5});
    EXPECT_EQ(engine.size(), 2U);
    EXPECT_EQ(engine.edge_count(), 1U);
}

TEST(SequenceEngineBasic, SizeUnboundedAfterManyIngests)
{
    // Verifies the ring buffer never causes size() to saturate at 3.
    SequenceEngine engine;
    constexpr std::size_t kN{1000};
    for (std::size_t i = 0; i < kN; ++i)
        engine.ingest(make_event(static_cast<EventID>(i % 10)));
    EXPECT_EQ(engine.size(), kN);
}

// ── Transition matrix ────────────────────────────────────────────────────────

TEST(SequenceEngineTransitions, CountsEdges)
{
    SequenceEngine engine;
    // Edges: 1→2, 2→3, 3→1  (distinct 3)
    ingest_ids(engine, {1, 2, 3, 1, 2, 3, 1});
    EXPECT_EQ(engine.edge_count(), 3U);
}

TEST(SequenceEngineTransitions, ProbabilitiesSumToOne)
{
    SequenceEngine engine;
    // from=1 goes to 2 twice and to 3 once → P(1→2)=2/3, P(1→3)=1/3
    ingest_ids(engine, {1, 2, 1, 3, 1, 2});

    const auto edges{engine.transitions()};
    double sum_from_1{0.0};
    for (const auto& e : edges)
        if (e.from == 1)
            sum_from_1 += e.probability;
    EXPECT_NEAR(sum_from_1, 1.0, 1e-9);
}

TEST(SequenceEngineTransitions, SortedByCountDesc)
{
    SequenceEngine engine;
    ingest_ids(engine, {1, 2, 1, 2, 1, 2, 1, 3}); // 1→2 ×3, 2→1 ×2, 1→3 ×1

    const auto edges{engine.transitions()};
    ASSERT_FALSE(edges.empty());
    for (std::size_t i = 1; i < edges.size(); ++i)
        EXPECT_GE(edges[i - 1].count, edges[i].count);
}

TEST(SequenceEngineTransitions, HonorsTransitionCap)
{
    SequenceConfig config;
    config.max_transitions = 2;
    SequenceEngine engine{config};

    ingest_ids(engine, {1, 2, 3, 4, 5}); // would produce 4 distinct edges

    EXPECT_LE(engine.edge_count(), 2U);
}

TEST(SequenceEngineTransitions, SingleEventProducesNoEdges)
{
    SequenceEngine engine;
    engine.ingest(make_event(42));
    EXPECT_EQ(engine.edge_count(), 0U);
    EXPECT_TRUE(engine.transitions().empty());
}

// ── N-grams ──────────────────────────────────────────────────────────────────

TEST(SequenceEngineNGrams, CountsBigramsAndTrigrams)
{
    SequenceEngine engine;
    ingest_ids(engine, {1, 2, 3, 1, 2, 3});

    const auto bigrams{engine.top_ngrams(2, 2)};
    ASSERT_EQ(bigrams.size(), 2U);
    EXPECT_EQ(bigrams[0].sequence, (std::vector<EventID>{1, 2}));
    EXPECT_EQ(bigrams[0].count, 2U);
    EXPECT_DOUBLE_EQ(bigrams[0].probability, 0.4);
    EXPECT_EQ(bigrams[1].sequence, (std::vector<EventID>{2, 3}));
    EXPECT_EQ(bigrams[1].count, 2U);

    const auto trigrams{engine.top_ngrams(3, 1)};
    ASSERT_EQ(trigrams.size(), 1U);
    EXPECT_EQ(trigrams[0].sequence, (std::vector<EventID>{1, 2, 3}));
    EXPECT_EQ(trigrams[0].count, 2U);
    EXPECT_DOUBLE_EQ(trigrams[0].probability, 0.5);
}

TEST(SequenceEngineNGrams, HonorsDistinctKeyCap)
{
    SequenceConfig config;
    config.max_ngram_keys = 1;
    SequenceEngine engine{config};

    ingest_ids(engine, {1, 2, 3, 4});

    const auto bigrams{engine.top_ngrams(2, 10)};
    ASSERT_EQ(bigrams.size(), 1U);
    EXPECT_EQ(bigrams[0].sequence, (std::vector<EventID>{1, 2}));
    EXPECT_EQ(bigrams[0].count, 1U);
    EXPECT_DOUBLE_EQ(bigrams[0].probability, 1.0);
}

TEST(SequenceEngineNGrams, ClampsUnsupportedHigherOrders)
{
    SequenceConfig config;
    config.max_ngram_size = 8;
    SequenceEngine engine{config};

    ingest_ids(engine, {1, 2, 3, 4});

    EXPECT_FALSE(engine.top_ngrams(3, 10).empty());
    EXPECT_TRUE(engine.top_ngrams(4, 10).empty());
}

TEST(SequenceEngineNGrams, TopKLimitsOutput)
{
    SequenceEngine engine;
    ingest_ids(engine, {1, 2, 3, 1, 2, 3, 1, 2}); // 3 distinct bigrams

    const auto bigrams{engine.top_ngrams(2, 2)};
    EXPECT_EQ(bigrams.size(), 2U);
}

TEST(SequenceEngineNGrams, NgramProbabilitiesSumToOne)
{
    SequenceEngine engine;
    ingest_ids(engine, {1, 2, 3, 1, 3, 2}); // 5 distinct bigrams

    const auto bigrams{engine.top_ngrams(2, 100)};
    const double total = std::ranges::fold_left(bigrams, 0.0, [](double acc, const auto& e)
                                                { return acc + e.probability; });
    EXPECT_NEAR(total, 1.0, 1e-9);
}

TEST(SequenceEngineNGrams, BigramsRequireTwoEvents)
{
    SequenceEngine engine;
    engine.ingest(make_event(1));
    EXPECT_TRUE(engine.top_ngrams(2, 10).empty());

    engine.ingest(make_event(2));
    EXPECT_EQ(engine.top_ngrams(2, 10).size(), 1U);
}

TEST(SequenceEngineNGrams, RingBufferCorrectAfterManyIngests)
{
    // After > 3 events the ring wraps; verify the last trigram is still right.
    SequenceEngine engine;
    ingest_ids(engine, {9, 8, 7, 1, 2, 3});

    // Only {1,2,3} should appear as a trigram — the early ones don't repeat.
    const auto trigrams{engine.top_ngrams(3, 10)};
    bool found_123{false};
    for (const auto& t : trigrams)
        if (t.sequence == std::vector<EventID>{1, 2, 3})
            found_123 = true;
    EXPECT_TRUE(found_123);

    // {9,8,7} must appear exactly once.
    bool found_987{false};
    for (const auto& t : trigrams)
        if (t.sequence == std::vector<EventID>{9, 8, 7})
            found_987 = true;
    EXPECT_TRUE(found_987);
}

// ── Dominant path ─────────────────────────────────────────────────────────────

TEST(SequenceEngineDominantPath, EmptyEngineReturnsEmptyPath)
{
    SequenceEngine engine;
    const auto path{engine.reconstruct_dominant_path()};
    EXPECT_TRUE(path.nodes.empty());
}

TEST(SequenceEngineDominantPath, FollowsMostLikelyEdge)
{
    SequenceEngine engine;
    // from 1: → 2 (×3), → 3 (×1) → path should go 1→2
    ingest_ids(engine, {1, 2, 1, 2, 1, 2, 1, 3});

    const auto path{engine.reconstruct_dominant_path(4)};
    ASSERT_GE(path.nodes.size(), 2U);
    EXPECT_EQ(path.nodes[0], 1U);
    EXPECT_EQ(path.nodes[1], 2U);
}

TEST(SequenceEngineDominantPath, DetectsCycle)
{
    SequenceEngine engine;
    ingest_ids(engine, {1, 2, 1, 2, 1, 2}); // 1→2→1→2… strict cycle

    const auto path{engine.reconstruct_dominant_path(8)};
    EXPECT_TRUE(path.truncated_by_cycle);
    // Should have visited 1 and 2 before stopping.
    EXPECT_GE(path.nodes.size(), 2U);
}

TEST(SequenceEngineDominantPath, StopsAtSink)
{
    SequenceEngine engine;
    ingest_ids(engine, {1, 2, 3}); // 3 has no outgoing edge

    const auto path{engine.reconstruct_dominant_path(10)};
    EXPECT_FALSE(path.truncated_by_cycle);
    EXPECT_EQ(path.nodes.back(), 3U);
}

TEST(SequenceEngineDominantPath, RespectsMaxSteps)
{
    SequenceEngine engine;
    ingest_ids(engine, {1, 2, 3, 4, 5, 6}); // linear chain of 5 edges

    const auto path{engine.reconstruct_dominant_path(2)};
    // start node + at most 2 steps = at most 3 nodes
    EXPECT_LE(path.nodes.size(), 3U);
}

TEST(SequenceEngineDominantPath, CumulativeProbabilityDecreases)
{
    SequenceEngine engine;
    // Deterministic chain: 1→2→3 with certainty each time.
    ingest_ids(engine, {1, 2, 3, 1, 2, 3});

    const auto path{engine.reconstruct_dominant_path(5)};
    EXPECT_NEAR(path.cumulative_probability, 1.0, 1e-9);
}

} // namespace insight::sequence
// NOLINTEND(readability-magic-numbers,readability-identifier-length,readability-uppercase-literal-suffix)
