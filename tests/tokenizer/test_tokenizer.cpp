// NOLINTBEGIN
// Unit tests: allow short identifiers and test-specific patterns
// tests/1_tokenization/test_tokenizer.cpp
//
// Integration tests for the full Phase 1 pipeline:
//   raw log line → Tokenizer → CanonicalEvent
//
// Tests validate end-to-end correctness for all four log formats, event
// identity, template grouping, param extraction, and batch processing.

#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

// ─────────────────────────────────────────────────────────────────────────────
// Fixture: a fresh arena + tokenizer per test
// ─────────────────────────────────────────────────────────────────────────────

class TokenizerTest : public ::testing::Test
{
  protected:
    static constexpr std::size_t kArenaSize{1u << 20}; // 1 MiB

    ArenaAllocator arena{kArenaSize};
    // Semantic-unaware: the universal formats tokenize with a degenerate (zero-package) composition.
    // `composed` is declared BEFORE `tokenizer` so it outlives the const-ref the Tokenizer holds.
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed};
};

// ─────────────────────────────────────────────────────────────────────────────
// Basic pipeline correctness per format
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, ProcessesBSDSyslogLine)
{
    constexpr std::string_view line{"Jan 15 08:03:22 myhost sshd[1]: Accepted password for alice"};
    auto result{tokenizer.process_line(line)};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_EQ(ev.component, "sshd");
    EXPECT_EQ(ev.level, LogLevel::Unknown); // BSD syslog has no inline level
    EXPECT_FALSE(ev.template_str.empty());
}

TEST_F(TokenizerTest, ProcessesJSONLine)
{
    constexpr std::string_view line{
        R"({"ts":"2024-01-15T10:30:00Z","level":"ERROR","component":"db","message":"Query timeout"})"};
    auto result{tokenizer.process_line(line)};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_EQ(ev.level, LogLevel::Error);
    EXPECT_EQ(ev.component, "db");
    EXPECT_FALSE(ev.template_str.empty());
}

TEST_F(TokenizerTest, ProcessesKVLine)
{
    constexpr std::string_view line = "ts=2024-01-15T10:30:00Z level=WARN component=cache "
                                      "msg=eviction_threshold_reached";
    auto result{tokenizer.process_line(line)};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_EQ(ev.level, LogLevel::Warn);
    EXPECT_EQ(ev.component, "cache");
}

// The cube-dimension precondition for the kv (logfmt) flow path: a kv line carrying
// `level=` + `component=<service>` + a benign prose `msg=` tokenizes to a CLEAN
// (level, component, role) tuple — component = the declared service (non-empty),
// role None (the message announces no structural marker), and level taken verbatim
// from the `level=` field (across the Info/Error band). The cube axes (insight::cube
// kNumDims=3) read exactly these three CanonicalEvent fields, so this is their
// canon-level guarantee.
//
// Re-homed from the former e2e do-operator substrate precondition (28-31's
// `KvCanonPopulatesFlowCubeDimsCleanly`, ROADMAP Topic-H): that test asserted the
// same tuple on LIVE LogCraft-generated kv flow records, but the wiring it guards is
// a single-component canon property — proven here on hand-built kv lines, decoupled
// from the generator. The do-axis collapse claim itself stays in the playground
// do-operator contract fixtures (28-31), which rely on this precondition.
TEST_F(TokenizerTest, KvFlowRecordsPopulateCubeDimsCleanly)
{
    struct KvCase
    {
        std::string_view line;
        LogLevel level;
        std::string_view component;
    };
    // Representative benign flow records — the declared services across the Info/Error
    // band, plain prose messages (no announced ##[...] / ::...:: structural marker).
    const KvCase cases[]{
        {R"(level=info component=gateway msg="accept settle request")", LogLevel::Info, "gateway"},
        {R"(level=info component=payments msg="settle order total")", LogLevel::Info, "payments"},
        {R"(level=error component=payments msg="settle order total")", LogLevel::Error, "payments"},
        {R"(level=info component=notifier msg="publish settlement event")", LogLevel::Info,
         "notifier"},
        {R"(level=error component=notifier msg="publish settlement event")", LogLevel::Error,
         "notifier"},
    };

    for (const auto& kase : cases)
    {
        const auto event{tokenizer.process_line(kase.line)};
        ASSERT_TRUE(event.has_value()) << "kv flow line failed to tokenize: " << kase.line;
        EXPECT_EQ(event->component, kase.component)
            << "component= field must populate the WHERE axis cleanly: " << kase.line;
        EXPECT_FALSE(event->component.empty()) << "empty component on: " << kase.line;
        EXPECT_EQ(event->level, kase.level)
            << "level= field must populate the SEVERITY axis verbatim: " << kase.line;
        EXPECT_EQ(event->structural_role, StructuralRole::None)
            << "a benign prose message announces no role (KIND axis = None): " << kase.line;
    }
}

TEST_F(TokenizerTest, ProcessesCLFLine)
{
    constexpr std::string_view line{
        R"(192.168.1.5 - bob [15/Jan/2024:10:30:00 +0000] "GET /api/health HTTP/1.1" 200 42)"};
    auto result{tokenizer.process_line(line)};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_EQ(ev.level, LogLevel::Info);
    EXPECT_EQ(ev.component, "192.168.1.5");
    EXPECT_NE(ev.template_str.find("GET"), std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Event identity
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, EventIDMonotonicallyIncreases)
{
    constexpr std::string_view line{R"({"level":"INFO","message":"tick"})"};
    auto r1{tokenizer.process_line(line)};
    auto r2{tokenizer.process_line(line)};
    ASSERT_TRUE(r1.has_value());
    ASSERT_TRUE(r2.has_value());
    EXPECT_LT(r1.value().id, r2.value().id);
}

TEST_F(TokenizerTest, EventsProducedCounterIncrements)
{
    EXPECT_EQ(tokenizer.events_produced(), 0u);
    static_cast<void>(tokenizer.process_line(R"({"msg":"one"})"));
    EXPECT_EQ(tokenizer.events_produced(), 1u);
    static_cast<void>(tokenizer.process_line(R"({"msg":"two"})"));
    EXPECT_EQ(tokenizer.events_produced(), 2u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Template grouping
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, SameStructuredLinesSameTemplateID)
{
    auto r1{tokenizer.process_line(R"({"msg":"User alice connected"})")};
    auto r2{tokenizer.process_line(R"({"msg":"User alice connected"})")};
    ASSERT_TRUE(r1.has_value() && r2.has_value());
    // Identity is the content-deterministic template_str (the downstream SHA-256 of it
    // is template_id). Same line → same template_str, statelessly.
    EXPECT_EQ(r1.value().template_str, r2.value().template_str);
}

TEST_F(TokenizerTest, VariablePartBecomesWildcardInTemplate)
{
    // The IPv4 token is masked to "<*>" per-line (stateless); the kept literal "User"
    // anchors the template. (The names alice/bob are letter-leading words → KEPT, not
    // masked — the D-TID-14 boundary; only syntactic high-card classes mask.)
    auto r{tokenizer.process_line(R"({"msg":"User bob logged in from 10.0.0.2"})")};
    ASSERT_TRUE(r.has_value());
    EXPECT_NE(r.value().template_str.find("User"), std::string::npos);
    EXPECT_NE(r.value().template_str.find("<*>"), std::string::npos); // the IP masked
}

TEST_F(TokenizerTest, DifferentFormatLinesDifferentTemplates)
{
    // Two lines with different content → different masked templates.
    auto rSyslog{tokenizer.process_line("Jan 15 08:03:22 host proc[1]: kernel startup completed")};
    auto rJSON{
        tokenizer.process_line(R"({"level":"INFO","message":"database connection established"})")};
    ASSERT_TRUE(rSyslog.has_value() && rJSON.has_value());
    // The two lines go through different strategies → different content → different templates.
    EXPECT_NE(rSyslog.value().template_str, rJSON.value().template_str);
}

// ─────────────────────────────────────────────────────────────────────────────
// Params
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, ParamsExtractedAfterTemplateStabilises)
{
    // Three identical-structure lines; after the second, wildcards appear.
    static_cast<void>(tokenizer.process_line(R"({"msg":"Retry attempt 1 of 3"})"));
    static_cast<void>(tokenizer.process_line(R"({"msg":"Retry attempt 2 of 3"})"));
    auto r{tokenizer.process_line(R"({"msg":"Retry attempt 5 of 10"})")};
    ASSERT_TRUE(r.has_value());
    // Should have at least one param (the variable numeric fields).
    EXPECT_GE(r.value().params.size(), 1u);
}

TEST_F(TokenizerTest, ParamsAreArenaOwned)
{
    static_cast<void>(tokenizer.process_line(R"({"msg":"connect from 10.0.0.1 ok"})"));
    auto r{tokenizer.process_line(R"({"msg":"connect from 10.0.0.2 ok"})")};
    ASSERT_TRUE(r.has_value());
    for (auto sv : r.value().params)
        EXPECT_TRUE(arena.owns(sv.data()));
}

// ─────────────────────────────────────────────────────────────────────────────
// Batch processing
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, BatchReturnsOneResultPerLine)
{
    const std::vector<std::string_view> lines = {
        R"({"msg":"line one"})",
        R"({"msg":"line two"})",
        R"({"msg":"line three"})",
    };
    auto results{tokenizer.process_batch(lines)};
    EXPECT_EQ(results.size(), lines.size());
}

TEST_F(TokenizerTest, BatchCountsEventsProduced)
{
    const std::vector<std::string_view> lines = {
        R"({"msg":"a"})",
        R"({"msg":"b"})",
        R"({"msg":"c"})",
    };
    static_cast<void>(tokenizer.process_batch(lines));
    EXPECT_EQ(tokenizer.events_produced(), lines.size());
}

TEST_F(TokenizerTest, BatchErrorLineDoeNotCountAsProduced)
{
    const std::vector<std::string_view> lines = {
        R"({"msg":"valid"})",
        "", // empty → error
        R"({"msg":"also valid"})",
    };
    auto results{tokenizer.process_batch(lines)};
    ASSERT_EQ(results.size(), 3u);
    EXPECT_TRUE(results[0].has_value());
    EXPECT_FALSE(results[1].has_value());
    EXPECT_TRUE(results[2].has_value());
    EXPECT_EQ(tokenizer.events_produced(), 2u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Error handling
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, EmptyLineReturnsError)
{
    auto result{tokenizer.process_line("")};
    EXPECT_FALSE(result.has_value());
    EXPECT_EQ(tokenizer.events_produced(), 0u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Accessor delegation
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, ReportsParsedLineCount)
{
    static_cast<void>(tokenizer.process_line(R"({"msg":"test"})"));
    EXPECT_GE(tokenizer.lines_parsed(), 1u);
}

// (ReportsClusterCount retired — the stateless masker has no cluster state; cluster_count()
//  was removed with the Drain clustering, stateless_template_id.md D-TID-3.)

// ─────────────────────────────────────────────────────────────────────────────
// Edge cases / robustness
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, ShortGarbageLineParsesAsRawText)
{
    // "xyz" matches no structured strategy, so the raw-text fallback templates
    // it rather than dropping it (the wedge ingests unstructured CI logs).
    auto result{tokenizer.process_line("xyz")};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_EQ(ev.level, LogLevel::Unknown);
    EXPECT_FALSE(ev.template_str.empty());
}

TEST_F(TokenizerTest, WhitespaceOnlyLineReturnsError)
{
    auto result{tokenizer.process_line("   ")};
    EXPECT_FALSE(result.has_value());
}

TEST_F(TokenizerTest, UTF8ContentEndToEnd)
{
    // UTF-8 in a JSON message field should flow through without corruption
    // and not cause any crash.
    auto result{tokenizer.process_line(
        R"({"level":"INFO","message":"connexion \u00e9tablie avec succ\u00e8s"})")};
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().template_str.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// Batch with all four formats
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, BatchAllFourFormatsAllSucceed)
{
    const std::vector<std::string_view> lines = {
        // Syslog (BSD)
        "Jan 15 08:03:22 myhost sshd[1]: Accepted password for user",
        // JSON
        R"({"level":"INFO","component":"api","message":"request served"})",
        // KV
        "level=WARN component=cache msg=eviction_triggered",
        // CLF
        R"(10.0.0.1 - - [15/Jan/2024:10:30:00 +0000] "GET /status HTTP/1.1" 200 64)",
    };
    auto results{tokenizer.process_batch(lines)};
    ASSERT_EQ(results.size(), 4u);
    for (std::size_t i{0}; i < results.size(); ++i)
    {
        EXPECT_TRUE(results[i].has_value())
            << "Line " << i << " failed: " << (!results[i].has_value() ? results[i].error() : "");
    }
    EXPECT_EQ(tokenizer.events_produced(), 4u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Interleaving errors must not corrupt template IDs
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, InterleaveErrorsDoNotCorruptTemplateIds)
{
    // Process a valid line, an error, then another structurally identical valid
    // line. The third line's template_str must equal the first's (same structure) —
    // the intervening error does not perturb the stateless masker.
    auto r1{tokenizer.process_line(R"({"msg":"worker job started"})")};
    ASSERT_TRUE(r1.has_value());

    auto rErr{tokenizer.process_line("")}; // forced error
    EXPECT_FALSE(rErr.has_value());

    auto r3{tokenizer.process_line(R"({"msg":"worker job started"})")};
    ASSERT_TRUE(r3.has_value());
    EXPECT_EQ(r1.value().template_str, r3.value().template_str);
}

// ─────────────────────────────────────────────────────────────────────────────
// JSON message containing KV-like content (nested format)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, JSONWithKVContentMaskedStatelessly)
{
    // The JSON strategy extracts the message; the stateless masker classifies each KV
    // token by its OWN content. A `key=value` pair is a single letter-leading token →
    // KEPT literal (the D-TID-14 boundary: a varying value-WORD is not a syntactic
    // high-card class; masking it needs the deferred SemanticClassRegistry). So two
    // lines differing only in a KV value-word are DISTINCT templates — the accepted
    // stateless over-split (D-TID-8), NOT Drain's old cross-line wildcard.
    auto ra{tokenizer.process_line(R"({"msg":"action=login user=alice status=ok"})")};
    auto rb{tokenizer.process_line(R"({"msg":"action=login user=bob status=ok"})")};
    ASSERT_TRUE(ra.has_value() && rb.has_value());
    EXPECT_NE(ra.value().template_str.find("action=login"), std::string::npos);
    EXPECT_NE(ra.value().template_str, rb.value().template_str)
        << "a varying KV value-word stays literal (no cross-line wildcard): "
        << ra.value().template_str << " vs " << rb.value().template_str;
    // A digit-leading value as its own token still masks per-line.
    auto rn{tokenizer.process_line(R"({"msg":"served 500 status=ok"})")};
    ASSERT_TRUE(rn.has_value());
    EXPECT_NE(rn.value().template_str.find("<*>"), std::string::npos); // "500" masked
}

// ─────────────────────────────────────────────────────────────────────────────
// Param extraction for variable (digit-leading) positions
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, HighVolumeTemplateStabilisesParams)
{
    // Send 8 lines in the pattern "fetched NNN rows returned" with varying NNN.
    // The numeric position is a VARIABLE count (not a status value), so it masks
    // to <*> per-line and a param is extracted. (A status value behind code/status/
    // exit/signal would instead be KEPT distinct; see the StatusValueKeptDistinct
    // masker test. "rows" is not a status keyword, so masking applies.)
    const std::vector<std::string_view> lines = {
        R"({"msg":"fetched 200 rows returned"})", R"({"msg":"fetched 201 rows returned"})",
        R"({"msg":"fetched 400 rows returned"})", R"({"msg":"fetched 404 rows returned"})",
        R"({"msg":"fetched 500 rows returned"})", R"({"msg":"fetched 503 rows returned"})",
        R"({"msg":"fetched 301 rows returned"})", R"({"msg":"fetched 204 rows returned"})",
    };
    auto results{tokenizer.process_batch(lines)};
    ASSERT_EQ(results.size(), 8u);

    // After stabilisation the results should have a non-empty param for the
    // variable-count position.
    bool any_param_found{false};
    for (auto& r : results)
    {
        if (r.has_value() && !r.value().params.empty())
        {
            any_param_found = true;
            break;
        }
    }
    EXPECT_TRUE(any_param_found);
}

// ─────────────────────────────────────────────────────────────────────────────
// Unicode extremes — non-Latin scripts and emoji
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(TokenizerTest, NonLatinUnicodeEndToEnd)
{
    // Arabic, Cyrillic and CJK codepoints are valid UTF-8 and must pass through
    // simdjson and the masker without corruption or crash.
    // \u062a\u0633\u062c\u064a\u0644  = Arabic "tasjiil" (registration)
    // \u0432\u0445\u043e\u0434        = Cyrillic "vkhod" (login)
    // \u767b\u5f55                    = CJK "denglu" (login)
    auto r1{tokenizer.process_line(
        R"({"level":"INFO","message":"\u062a\u0633\u062c\u064a\u0644 \u0645\u0633\u062a\u062e\u062f\u0645"})")};
    ASSERT_TRUE(r1.has_value());
    EXPECT_FALSE(r1.value().template_str.empty());

    auto r2{tokenizer.process_line(
        R"({"level":"INFO","message":"\u0432\u0445\u043e\u0434 \u043f\u043e\u043b\u044c\u0437\u043e\u0432\u0430\u0442\u0435\u043b\u044f"})")};
    ASSERT_TRUE(r2.has_value());
    EXPECT_FALSE(r2.value().template_str.empty());

    auto r3{tokenizer.process_line(R"({"level":"INFO","message":"\u767b\u5f55\u6210\u529f"})")};
    ASSERT_TRUE(r3.has_value());
    EXPECT_FALSE(r3.value().template_str.empty());
}

TEST_F(TokenizerTest, EmojiContentEndToEnd)
{
    // Emoji codepoints (encoded as JSON \uXXXX surrogate pairs or direct
    // UTF-8) must survive simdjson → the masker without crashing.
    // \ud83d\ude80 = U+1F680 ROCKET (surrogate pair)
    // \u2705      = U+2705  WHITE HEAVY CHECK MARK
    auto r1{tokenizer.process_line(
        R"({"level":"INFO","message":"deploy \ud83d\ude80 succeeded \u2705"})")};
    ASSERT_TRUE(r1.has_value());
    EXPECT_FALSE(r1.value().template_str.empty());

    // Two structurally identical emoji lines must receive the same template.
    auto r2{tokenizer.process_line(
        R"({"level":"INFO","message":"deploy \ud83d\ude80 succeeded \u2705"})")};
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r1.value().template_str, r2.value().template_str);
}

// NOLINTEND
