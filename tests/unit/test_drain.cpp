// NOLINTBEGIN
// Unit tests: allow short identifiers and test-specific patterns
// tests/1_tokenization/test_drain.cpp
//
// Unit tests for the Drain log-template mining algorithm.
//
// Coverage:
//   - New cluster creation and template assignment
//   - Template refinement on repeated matches
//   - Wildcard extraction and param reporting
//   - Similarity threshold gating
//   - Cluster count and total_matched bookkeeping
//   - get_template() lookup
//   - prune() LRU eviction
//   - reset() teardown
//   - Edge cases: empty content, max_clusters cap

#include <gtest/gtest.h>

#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/drain.hpp"
#include "insight/tokenization/drain_config.hpp"

using namespace insight::tokenization;

// ─────────────────────────────────────────────────────────────────────────────
// Test arena: reset before every match call so each result owns its memory.
// Params/template_str remain valid until the next do_match() call.
// ─────────────────────────────────────────────────────────────────────────────
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static ArenaAllocator g_arena{256U * 1024U};

static Drain::ArenaMatchResult do_match(Drain& d, std::string_view s)
{
    g_arena.reset();
    return d.match_into_arena(s, g_arena);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static DrainConfig tight_config()
{
    DrainConfig cfg;
    cfg.similarity_threshold = 0.5;
    cfg.max_depth = 4;
    cfg.max_clusters = 1000;
    return cfg;
}

// ─────────────────────────────────────────────────────────────────────────────
// Basic creation and identification
// ─────────────────────────────────────────────────────────────────────────────

TEST(Drain_Cluster, FirstLineCreatesNewCluster)
{
    Drain drain{tight_config()};
    auto r{do_match(drain, "User alice logged in from 192.168.1.1")};
    EXPECT_TRUE(r.new_cluster);
    EXPECT_EQ(drain.cluster_count(), 1u);
}

TEST(Drain_Cluster, IdenticalLinesShareCluster)
{
    Drain drain{tight_config()};
    auto r1{do_match(drain, "Connection established to db01")};
    auto r2{do_match(drain, "Connection established to db01")};
    EXPECT_NE(r1.template_id, 0u);
    EXPECT_EQ(r1.template_id, r2.template_id);
    EXPECT_FALSE(r2.new_cluster);
}

TEST(Drain_Cluster, DifferentLengthsCreateDifferentClusters)
{
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "short line"));
    static_cast<void>(do_match(drain, "a line with more tokens here"));
    EXPECT_EQ(drain.cluster_count(), 2u);
}

TEST(Drain_Cluster, TotalMatchedIncrementsPerCall)
{
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "line one"));
    static_cast<void>(do_match(drain, "line two"));
    static_cast<void>(do_match(drain, "line three"));
    EXPECT_EQ(drain.total_matched(), 3u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Template refinement / wildcard extraction
// ─────────────────────────────────────────────────────────────────────────────

TEST(Drain_Template, VariablePositionBecomesWildcard)
{
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "User alice logged in"));
    static_cast<void>(do_match(drain, "User bob logged in"));

    auto tmpl{drain.get_template(1)}; // ID 1 = first cluster
    ASSERT_TRUE(tmpl.has_value());
    // "User" and "logged" and "in" should be stable; second token should be <*>
    EXPECT_NE(tmpl->find("<*>"), std::string::npos);
    EXPECT_NE(tmpl->find("User"), std::string::npos);
}

TEST(Drain_Template, ConstantPositionsPreserved)
{
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "Connected to port 443"));
    static_cast<void>(do_match(drain, "Connected to port 8080"));
    static_cast<void>(do_match(drain, "Connected to port 80"));

    auto tmpl{drain.get_template(1)};
    ASSERT_TRUE(tmpl.has_value());
    EXPECT_NE(tmpl->find("Connected"), std::string::npos);
    EXPECT_NE(tmpl->find("to"), std::string::npos);
    EXPECT_NE(tmpl->find("port"), std::string::npos);
    EXPECT_NE(tmpl->find("<*>"), std::string::npos);
}

TEST(Drain_Template, ParamsExtractedAtWildcardPositions)
{
    Drain drain{tight_config()};
    // First match: no wildcards yet.
    static_cast<void>(do_match(drain, "Request from 10.0.0.1 completed"));
    // Second match: position 2 becomes wildcard.
    auto r{do_match(drain, "Request from 10.0.0.2 completed")};
    EXPECT_FALSE(r.new_cluster);
    // The wildcard position should yield the value from the second match.
    ASSERT_FALSE(r.params.empty());
    EXPECT_EQ(r.params[0], "10.0.0.2");
}

TEST(Drain_Template, MultipleWildcardsYieldMultipleParams)
{
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "User alice connected from 192.168.0.1"));
    auto r{do_match(drain, "User bob connected from 10.0.0.5")};
    EXPECT_EQ(r.params.size(), 2u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Similarity threshold
// ─────────────────────────────────────────────────────────────────────────────

TEST(Drain_Threshold, BelowThresholdCreatesNewCluster)
{
    DrainConfig cfg;
    cfg.similarity_threshold = 0.9; // very tight
    cfg.max_depth = 4;
    cfg.max_clusters = 1000;
    Drain drain{cfg};

    // These share only "connected" → similarity well below 0.9.
    static_cast<void>(do_match(drain, "connected successfully 127 0 0 1 port"));
    auto r{do_match(drain, "error   timeout 127 0 0 1 port")};
    EXPECT_TRUE(r.new_cluster);
}

// ─────────────────────────────────────────────────────────────────────────────
// Lookup and accessors
// ─────────────────────────────────────────────────────────────────────────────

TEST(Drain_Lookup, GetTemplateReturnsCorrectString)
{
    Drain drain{tight_config()};
    auto r{do_match(drain, "Disk usage at 95 percent")};
    auto t{drain.get_template(r.template_id)};
    ASSERT_TRUE(t.has_value());
    EXPECT_EQ(*t, r.template_str);
}

TEST(Drain_Lookup, GetTemplateReturnNulloptForUnknownID)
{
    Drain drain{tight_config()};
    EXPECT_FALSE(drain.get_template(9999u).has_value());
}

TEST(Drain_Lookup, AllTemplatesContainsAllClusters)
{
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "alpha beta gamma"));
    static_cast<void>(do_match(drain, "x y z w v")); // different length → different cluster
    auto map{drain.all_templates()};
    EXPECT_GE(map.size(), 2u);
}

// ─────────────────────────────────────────────────────────────────────────────
// prune()
// ─────────────────────────────────────────────────────────────────────────────

TEST(Drain_Prune, ReducesClusterCountToTarget)
{
    Drain drain{tight_config()};
    // Create 5 distinct cluster lengths.
    static_cast<void>(do_match(drain, "a"));
    static_cast<void>(do_match(drain, "b b"));
    static_cast<void>(do_match(drain, "c c c"));
    static_cast<void>(do_match(drain, "d d d d"));
    static_cast<void>(do_match(drain, "e e e e e"));
    ASSERT_GE(drain.cluster_count(), 3u);

    drain.prune(2);
    EXPECT_LE(drain.cluster_count(), 2u);
}

TEST(Drain_Prune, DoesNothingIfAlreadyBelowMax)
{
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "hello world"));
    auto before{drain.cluster_count()};
    drain.prune(100);
    EXPECT_EQ(drain.cluster_count(), before);
}

// ─────────────────────────────────────────────────────────────────────────────
// reset()
// ─────────────────────────────────────────────────────────────────────────────

TEST(Drain_Reset, ClearsAllClusters)
{
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "some log line"));
    static_cast<void>(do_match(drain, "another log line longer"));
    drain.reset();
    EXPECT_EQ(drain.cluster_count(), 0u);
}

TEST(Drain_Reset, TotalMatchedResetToZero)
{
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "one"));
    static_cast<void>(do_match(drain, "two"));
    drain.reset();
    EXPECT_EQ(drain.total_matched(), 0u);
}

TEST(Drain_Reset, CanMatchAfterReset)
{
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "before reset"));
    drain.reset();
    auto r{do_match(drain, "after reset")};
    EXPECT_TRUE(r.new_cluster);
    EXPECT_EQ(drain.cluster_count(), 1u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Edge cases
// ─────────────────────────────────────────────────────────────────────────────

TEST(Drain_EdgeCase, EmptyContentHandled)
{
    Drain drain{tight_config()};
    EXPECT_NO_THROW(static_cast<void>(do_match(drain, "")));
    EXPECT_EQ(drain.total_matched(), 1u);
}

TEST(Drain_EdgeCase, SingleTokenLine)
{
    Drain drain{tight_config()};
    auto r1{do_match(drain, "ERR")};
    auto r2{do_match(drain, "ERR")};
    EXPECT_FALSE(r2.new_cluster);
    EXPECT_EQ(r1.template_id, r2.template_id);
}

TEST(Drain_EdgeCase, MaxClustersCapEnforced)
{
    DrainConfig cfg;
    cfg.max_clusters = 3;
    cfg.similarity_threshold = 0.99; // very tight: content variations won't auto-merge
    cfg.max_depth = 4;
    Drain drain{cfg};

    // All lines are 4 tokens so they share the same leaf node.
    // First 3 create distinct clusters; lines 4 and 5 must be absorbed
    // into existing ones rather than growing beyond the cap.
    static_cast<void>(do_match(drain, "service one started ok"));
    static_cast<void>(do_match(drain, "service two started ok"));
    static_cast<void>(do_match(drain, "service three stopped err"));
    static_cast<void>(
        do_match(drain, "service four stopped err")); // cap reached → absorb into nearest
    static_cast<void>(do_match(drain, "service five started ok")); // still at cap → absorb

    EXPECT_LE(drain.cluster_count(), cfg.max_clusters);
    EXPECT_EQ(drain.total_matched(), 5u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Numeric token routing
// ─────────────────────────────────────────────────────────────────────────────

TEST(Drain_Routing, NumericFirstTokenRoutedAsWildcard)
{
    // Drain replaces purely-numeric tokens with <*> during routing.
    // Two lines whose only difference is their numeric leading token should
    // land in the same leaf and merge into one cluster.
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "123 server started successfully"));
    auto r{do_match(drain, "456 server started successfully")};
    // Both have the same non-numeric tokens in positions 1-3 → same leaf.
    EXPECT_FALSE(r.new_cluster);
    EXPECT_EQ(drain.cluster_count(), 1u);
}

TEST(Drain_Routing, NumericTokensInMixedLineYieldParams)
{
    // Lines with constant text and varying numeric values should cluster together
    // (similarity >= threshold) and produce wildcard params once the template
    // stabilises.
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "processed 100 items in 500ms"));
    auto r{do_match(drain, "processed 250 items in 120ms")};
    // "processed", "items", "in" are stable (3/5 = 0.60 >= threshold 0.5) → same
    // cluster.
    ASSERT_FALSE(r.new_cluster);
    // The two differing positions ("100"→"250", "500ms"→"120ms") become <*> →
    // params.
    EXPECT_GE(r.params.size(), 1u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Configuration preservation
// ─────────────────────────────────────────────────────────────────────────────

TEST(Drain_Reset, ResetsConfigIsPreserved)
{
    // Verify that reset() preserves config (max_clusters in particular).
    // Strategy: configure max_clusters=5, then attempt to create 10 distinct
    // clusters after reset. If config is preserved the count stays <= 5; if
    // config reverts to default (10000) the count would reach 10 and the test
    // would fail.
    DrainConfig cfg;
    cfg.max_depth = 4;
    cfg.similarity_threshold = 0.99; // near-exact: different suffix always creates new cluster
    cfg.max_clusters = 5;
    Drain drain{cfg};

    static_cast<void>(do_match(drain, "health check passed"));
    drain.reset();

    // All 4-token lines start with "service" → same routing leaf.
    // With threshold=0.99, each unique numeric suffix tries to create its own
    // cluster; the cap (5) aborts that after the 5th.
    for (int i{0}; i < 10; ++i)
        static_cast<void>(do_match(drain, "service " + std::to_string(i) + " alpha beta"));

    EXPECT_LE(drain.cluster_count(), cfg.max_clusters);
}

// ─────────────────────────────────────────────────────────────────────────────
// Stress: many unique lines respect max_clusters cap
// ─────────────────────────────────────────────────────────────────────────────

TEST(Drain_Stress, HundredsOfUniqueLinesStayAtCap)
{
    DrainConfig cfg;
    cfg.max_clusters = 20;
    cfg.similarity_threshold = 0.99; // force separate clusters per content
    cfg.max_depth = 4;
    Drain drain{cfg};

    for (int i{1}; i <= 200; ++i)
        static_cast<void>(do_match(drain, "anomaly event type " + std::to_string(i) + " detected"));

    EXPECT_LE(drain.cluster_count(), cfg.max_clusters);
    EXPECT_EQ(drain.total_matched(), 200u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Token masking
// ─────────────────────────────────────────────────────────────────────────────

// IPv4 addresses are wildcarded on the very first occurrence so the template
// never contains a literal IP.
TEST(Drain_Masking, IPv4WildcardedOnFirstOccurrence)
{
    Drain drain{tight_config()};
    auto r{do_match(drain, "Received disconnect from 103.99.0.122: 14: No more auth methods")};
    ASSERT_TRUE(r.new_cluster);
    EXPECT_NE(r.template_str.find("<*>"), std::string::npos);
    EXPECT_EQ(r.template_str.find("103.99.0.122"), std::string::npos);
}

// Two lines with different IPs must share the same template cluster from the
// second line onward; no variation is needed for the IP position to merge.
TEST(Drain_Masking, DifferentIPsShareClusterImmediately)
{
    Drain drain{tight_config()};
    auto r1{do_match(drain, "Failed password from 1.2.3.4")};
    auto r2{do_match(drain, "Failed password from 5.6.7.8")};
    ASSERT_FALSE(r2.new_cluster);
    EXPECT_EQ(r1.template_id, r2.template_id);
}

// The param returned must contain the real IP, not the "<*>" placeholder.
TEST(Drain_Masking, IPv4ParamIsOriginalValue)
{
    Drain drain{tight_config()};
    static_cast<void>(do_match(drain, "Connect from 10.0.0.1 accepted"));
    auto r{do_match(drain, "Connect from 10.0.0.2 accepted")};
    ASSERT_FALSE(r.params.empty());
    EXPECT_EQ(r.params[0], "10.0.0.2");
}

// An IP with a trailing colon (common in sshd logs: "103.99.0.122: 14:") is
// also masked so the colon-suffixed token does not become a literal constant.
TEST(Drain_Masking, IPv4WithTrailingColonIsMasked)
{
    Drain drain{tight_config()};
    auto r1{do_match(drain, "disconnect from 10.0.0.1: reason 11")};
    auto r2{do_match(drain, "disconnect from 192.168.1.5: reason 11")};
    EXPECT_FALSE(r2.new_cluster);
    EXPECT_EQ(r1.template_id, r2.template_id);
}

// Hex addresses are wildcarded on the first occurrence.
TEST(Drain_Masking, HexAddressWildcardedOnFirstOccurrence)
{
    Drain drain{tight_config()};
    auto r{do_match(drain, "segfault at 0xdeadbeef in module foo")};
    EXPECT_NE(r.template_str.find("<*>"), std::string::npos);
    EXPECT_EQ(r.template_str.find("0xdeadbeef"), std::string::npos);
}

// Disabling token_masks restores legacy behaviour: the first occurrence of an
// IP stores it literally in the template (only wildcarded after inter-line var).
TEST(Drain_Masking, EmptyMaskListPreservesLiteralTokens)
{
    DrainConfig cfg{tight_config()};
    cfg.mask_ip_addresses = false; // disable all masking
    cfg.mask_hex_addresses = false;
    Drain drain{cfg};
    auto r{do_match(drain, "Received disconnect from 192.168.1.1")};
    // Without masking the IP appears verbatim in the first-occurrence template.
    EXPECT_NE(r.template_str.find("192.168.1.1"), std::string::npos);
    // With default masks the same line would produce "<*>" at that position.
    Drain drain_with_masks{tight_config()};
    auto r2{do_match(drain_with_masks, "Received disconnect from 192.168.1.1")};
    EXPECT_EQ(r2.template_str.find("192.168.1.1"), std::string::npos);
    EXPECT_NE(r2.template_str.find("<*>"), std::string::npos);
}

// NOLINTEND
