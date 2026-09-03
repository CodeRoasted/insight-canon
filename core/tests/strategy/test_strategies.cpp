// Unit tests: allow short identifiers and test-specific patterns
// tests/strategy/test_strategies.cpp
//
// Unit tests for the nineteen IFormatStrategy implementations:
//   SyslogStrategy, JsonStrategy, KVStrategy, CLFStrategy,
//   Log4jStrategy, SparkHDFSStrategy, BGLStrategy, AndroidLogcatStrategy,
//   ApacheErrorLogStrategy, WindowsCBSStrategy, HealthAppStrategy,
//   ProxifierStrategy, HPCStrategy, NginxErrorStrategy, RFC5424Strategy,
//   IISW3CStrategy, CloudWatchStrategy, SystemdJournalStrategy, RawTextStrategy.
//
// Layout: per-strategy TEST_F sections carry the strategy-specific claims (happy-path
// field extraction, edge cases, per-format confidence shapes). The four cross-strategy
// claim families that were once copy-pasted per strategy live at the END of the file as
// value-parameterized suites over one descriptor table (see "Table-driven families").

#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

// ─────────────────────────────────────────────────────────────────────────────
// Test data
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kBSDLine =
    "Jan 15 08:03:22 myhost sshd[1234]: Accepted password for alice from "
    "192.168.1.1 port 22";

static constexpr std::string_view kRFC3339Line{
    "2024-01-15T10:30:00Z myhost app[42]: User alice logged in"};

static constexpr std::string_view kJSONLine{
    R"({"ts":"2024-01-15T10:30:00Z","level":"INFO","component":"auth","message":"User logged in"})"};

static constexpr std::string_view kJSONMinimal{R"({"msg":"hello world"})"};

static constexpr std::string_view kJSONNoMsg{R"({"level":"ERROR","component":"db"})"};

static constexpr std::string_view kKVLine{
    R"(ts=2024-01-15T10:30:00Z level=INFO component=auth msg="User logged in" user_id=42)"};

static constexpr std::string_view kKVMinimal{"level=WARN msg=timeout"};

static constexpr std::string_view kCLFLine{
    R"(127.0.0.1 - frank [10/Oct/2000:13:55:36 -0700] "GET /apache_pb.gif HTTP/1.0" 200 2326)"};

static constexpr std::string_view kCombinedLine{
    R"(127.0.0.1 - frank [10/Oct/2000:13:55:36 -0700] "POST /login HTTP/1.1" 401 512 "http://example.com/" "Mozilla/5.0")"};

static constexpr std::string_view kCLF5xx{
    R"(10.0.0.1 - - [01/Jan/2024:00:00:01 +0000] "GET /crash HTTP/1.1" 500 0)"};

// ─────────────────────────────────────────────────────────────────────────────
// SyslogStrategy
// ─────────────────────────────────────────────────────────────────────────────

class SyslogStrategyTest : public ::testing::Test
{
  protected:
    SyslogStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(SyslogStrategyTest, ParsesBSDLine)
{
    auto result{strategy.parse(kBSDLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_FALSE(pl.content.empty());
    EXPECT_EQ(pl.component, "sshd");
    EXPECT_NE(pl.content.find("Accepted"), std::string::npos);
}

TEST_F(SyslogStrategyTest, ParsesBSDLineTimestamp)
{
    auto result{strategy.parse(kBSDLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().timestamp.has_value());
}

TEST_F(SyslogStrategyTest, ParsesRFC3339Line)
{
    auto result{strategy.parse(kRFC3339Line, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.component, "app");
    EXPECT_NE(pl.content.find("User"), std::string::npos);
}

TEST_F(SyslogStrategyTest, RejectsJSONLine)
{
    auto result{strategy.parse(kJSONLine, arena)};
    EXPECT_FALSE(result.has_value());
}

TEST_F(SyslogStrategyTest, RejectsKVLine)
{
    auto result{strategy.parse(kKVLine, arena)};
    EXPECT_FALSE(result.has_value());
}

TEST_F(SyslogStrategyTest, ConfidenceHighForBSD)
{
    EXPECT_GT(strategy.confidence(kBSDLine), 0.5);
}

TEST_F(SyslogStrategyTest, ConfidenceHighForRFC3339)
{
    EXPECT_GT(strategy.confidence(kRFC3339Line), 0.5);
}

TEST_F(SyslogStrategyTest, ConfidenceZeroForJSON)
{
    EXPECT_EQ(strategy.confidence(kJSONLine), 0.0);
}

TEST_F(SyslogStrategyTest, ConfidenceZeroForCLF)
{
    EXPECT_EQ(strategy.confidence(kCLFLine), 0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonStrategy
// ─────────────────────────────────────────────────────────────────────────────

class JsonStrategyTest : public ::testing::Test
{
  protected:
    JsonStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(JsonStrategyTest, ParsesAllFields)
{
    auto result{strategy.parse(kJSONLine, arena)};
    if (!result.has_value())
    {
        std::cerr << "DIAG error: " << result.error() << "\n";
    }
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    EXPECT_EQ(pl.component, "auth");
    EXPECT_EQ(pl.content, "User logged in");
}

TEST_F(JsonStrategyTest, ParsesMinimalJSON)
{
    auto result{strategy.parse(kJSONMinimal, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().content, "hello world");
}

TEST_F(JsonStrategyTest, FallsBackToJSONDumpWhenNoMessageKey)
{
    auto result{strategy.parse(kJSONNoMsg, arena)};
    ASSERT_TRUE(result.has_value());
    // Content should be the re-serialised JSON (non-empty).
    EXPECT_FALSE(result.value().content.empty());
}

// ── SRC-D-MSK-3 — nested-`fields` component/level descent ───────────────────────────
// App loggers (and LogCraft) nest custom fields under "fields":{…}; the top-level
// component/level lookups miss, so the cube WHERE axis went blind on JSON. When the
// top-level lookup misses, descend ONE level into "fields" and read {component, level}.
TEST_F(JsonStrategyTest, NestedFieldsComponentExtracted)
{
    auto result{strategy.parse(R"({"msg":"User logged in","fields":{"component":"auth"}})", arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result.value().component, "auth")
        << "component nested under \"fields\" must be recovered (was blind → cube WHERE empty)";
    EXPECT_EQ(result.value().content, "User logged in");
}

TEST_F(JsonStrategyTest, NestedFieldsLevelAndComponentExtracted)
{
    auto result{strategy.parse(
        R"({"msg":"connection lost","fields":{"level":"ERROR","component":"db"}})", arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result.value().level, LogLevel::Error) << "level nested under \"fields\" recovered";
    EXPECT_EQ(result.value().component, "db");
}

// A `source` synonym under "fields" resolves too (kComponentKeys: component/source/logger/…).
TEST_F(JsonStrategyTest, NestedFieldsSourceSynonymExtracted)
{
    auto result{
        strategy.parse(R"({"msg":"job scheduled","fields":{"source":"worker-pool"}})", arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result.value().component, "worker-pool");
}

// The descent is a FALLBACK only — a top-level component is authoritative and a nested
// one must NOT override it (the fallback fires only when the top-level lookup missed).
TEST_F(JsonStrategyTest, TopLevelComponentWinsOverNested)
{
    auto result{strategy.parse(
        R"({"component":"gateway","msg":"x","fields":{"component":"nested"}})", arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result.value().component, "gateway")
        << "top-level component is authoritative; the nested fallback must not override it";
}

TEST_F(JsonStrategyTest, RejectsNonJSONLine)
{
    auto result{strategy.parse(kBSDLine, arena)};
    EXPECT_FALSE(result.has_value());
}

TEST_F(JsonStrategyTest, RejectsPlainText)
{
    auto result{strategy.parse("this is not json at all", arena)};
    EXPECT_FALSE(result.has_value());
}

TEST_F(JsonStrategyTest, ConfidenceOneForJSON)
{
    EXPECT_EQ(strategy.confidence(kJSONLine), 1.0);
}

TEST_F(JsonStrategyTest, LevelDebugParsed)
{
    auto result{strategy.parse(R"({"level":"debug","message":"trace event"})", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Debug);
}

TEST_F(JsonStrategyTest, LevelErrorParsed)
{
    auto result{strategy.parse(R"({"severity":"ERROR","msg":"something failed"})", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Error);
}

// ── OTEL/OTLP ingestion (SRC-D-OTEL-1) ───────────────────────────────────────────
// One OTLP/JSON LogRecord as the LogCraft producer emits it: nested body.stringValue,
// numeric severityNumber, top-level traceId(32 hex)/spanId(16 hex)/parentSpanId.
static constexpr std::string_view kOtelLine{
    R"({"timeUnixNano":"1705312200000000000","observedTimeUnixNano":"1705312200000000000",)"
    R"("severityNumber":17,"severityText":"ERROR","body":{"stringValue":"GET /api/users -> 500"},)"
    R"("attributes":[{"key":"service.name","value":{"stringValue":"api"}}],)"
    R"("traceId":"0123456789abcdeffedcba9876543210","spanId":"00000000000000ff",)"
    R"("parentSpanId":"0000000000000001"})"};

TEST_F(JsonStrategyTest, OtelExtractsTraceContextAndBands)
{
    auto result{strategy.parse(kOtelLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    // OTLP timeUnixNano → event-time (without it the pipeline never closes a window). 1.7053122e18
    // ns = 1705312200 s; round-trip through the parsed Timestamp must recover that epoch second.
    ASSERT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::seconds>(pl.timestamp->time_since_epoch()).count(),
        1705312200);
    // severityNumber 17 → band (17-1)/4 = 4 → Error (declared severity wins).
    EXPECT_EQ(pl.level, LogLevel::Error);
    // body.stringValue becomes the content (the masker templates the message, NOT the raw JSON).
    EXPECT_EQ(pl.content, "GET /api/users -> 500");
    // Trace context consumed; trace_id is the non-zero hash of the OTEL hex.
    EXPECT_TRUE(pl.trace.present);
    EXPECT_EQ(pl.trace.trace_id, trace_id_from_hex("0123456789abcdeffedcba9876543210"));
    EXPECT_NE(pl.trace.trace_id.value, 0U);
    EXPECT_EQ(pl.trace.span_id, span_id_from_hex("00000000000000ff"));
    EXPECT_TRUE(pl.trace.has_parent);
    EXPECT_EQ(pl.trace.parent_span_id, span_id_from_hex("0000000000000001"));
    // OR1: the high-card trace ids are DROPPED from the content (never tokenized).
    EXPECT_EQ(pl.content.find("0123456789"), std::string_view::npos);
    EXPECT_EQ(pl.content.find("traceId"), std::string_view::npos);
}

TEST_F(JsonStrategyTest, OtelRootSpanHasNoParent)
{
    // No parentSpanId → root span. Still OTEL (severityNumber + traceId present).
    auto result{strategy.parse(
        R"({"severityNumber":9,"body":{"stringValue":"root step"},"traceId":"aa","spanId":"bb"})",
        arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_EQ(pl.level, LogLevel::Info); // 9 → band 2 → Info
    EXPECT_TRUE(pl.trace.present);
    EXPECT_FALSE(pl.trace.has_parent);
    EXPECT_EQ(pl.trace.parent_span_id.value, 0U);
    EXPECT_EQ(pl.content, "root step");
}

TEST_F(JsonStrategyTest, NonOtelJsonHasNoTraceContext)
{
    // A plain JSON log carries no trace context — present == false (the byte-identity basis).
    auto result{strategy.parse(kJSONLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().trace.present);
    EXPECT_EQ(result.value().trace.trace_id.value, 0U);
}

// ── OTEL span ingestion (D-OTEL-10 shape 2 / SRC-D-OTEL-18) ──────────────────────────────────────
// The canonical flat-span record the lab emits (name / start+end times / status / service.name),
// distinct from the OTLP log record above. Detected by the span-specific startTimeUnixNano key.
static constexpr std::string_view kSpanLine{
    R"({"traceId":"0123456789abcdeffedcba9876543210","spanId":"00000000000000ff",)"
    R"("parentSpanId":"0000000000000001","name":"checkout","kind":"SPAN_KIND_INTERNAL",)"
    R"("startTimeUnixNano":"1705312200000000000","endTimeUnixNano":"1705312200000500000",)"
    R"("status":{"code":"STATUS_CODE_UNSET"},)"
    R"("attributes":[{"key":"service.name","value":{"stringValue":"api-gateway"}},)"
    R"({"key":"method","value":{"stringValue":"GET"}}]})"};

TEST_F(JsonStrategyTest, OtelSpanMapsAllFields)
{
    auto result{strategy.parse(kSpanLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};

    // startTimeUnixNano → event time (a span is a POINT event at its start — D-OTEL-10).
    ASSERT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(
        std::chrono::duration_cast<std::chrono::seconds>(pl.timestamp->time_since_epoch()).count(),
        1705312200);
    // name → content (the templated operation, NOT the raw JSON); status UNSET → Info (declared).
    EXPECT_EQ(pl.content, "checkout");
    EXPECT_EQ(pl.level, LogLevel::Info);
    // service.name (from attributes[]) → component (the WHERE tier).
    EXPECT_EQ(pl.component, "api-gateway");
    // Consumed trace context (same hashing as the log path).
    EXPECT_TRUE(pl.trace.present);
    EXPECT_EQ(pl.trace.trace_id, trace_id_from_hex("0123456789abcdeffedcba9876543210"));
    EXPECT_EQ(pl.trace.span_id, span_id_from_hex("00000000000000ff"));
    EXPECT_TRUE(pl.trace.has_parent);
    EXPECT_EQ(pl.trace.parent_span_id, span_id_from_hex("0000000000000001"));
    // end − start → the span_duration_ns ordinal on the DurationLog2Ns ladder (SRC-D-OTEL-12).
    ASSERT_EQ(pl.ordinals.size(), 1U);
    EXPECT_EQ(pl.ordinals[0].field_name, "span_duration_ns");
    EXPECT_EQ(pl.ordinals[0].schedule, OrdinalSchedule::DurationLog2Ns);
    EXPECT_EQ(pl.ordinals[0].value, 500000); // 1705312200000500000 − 1705312200000000000
    // OR1: the high-card trace ids never enter the content.
    EXPECT_EQ(pl.content.find("0123456789"), std::string_view::npos);
}

TEST_F(JsonStrategyTest, OtelSpanErrorStatusLiftsLevel)
{
    // status STATUS_CODE_ERROR → Error (declared > inferred).
    auto result{strategy.parse(
        R"({"traceId":"aa","spanId":"bb","name":"db_query","startTimeUnixNano":"1705312200000000000",)"
        R"("endTimeUnixNano":"1705312200000000000","status":{"code":"STATUS_CODE_ERROR"},)"
        R"("attributes":[{"key":"service.name","value":{"stringValue":"db"}}]})",
        arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_EQ(pl.level, LogLevel::Error);
    EXPECT_EQ(pl.content, "db_query");
    EXPECT_EQ(pl.component, "db");
    // Zero-duration span (start == end) → the smallest bin, never negative.
    ASSERT_EQ(pl.ordinals.size(), 1U);
    EXPECT_EQ(pl.ordinals[0].value, 0);
}

TEST_F(JsonStrategyTest, OtelSpanRootHasNoParent)
{
    // No parentSpanId → a root span.
    auto result{strategy.parse(
        R"({"traceId":"aa","spanId":"bb","name":"root","startTimeUnixNano":"1705312200000000000",)"
        R"("endTimeUnixNano":"1705312200000010000","status":{"code":"STATUS_CODE_UNSET"},)"
        R"("attributes":[{"key":"service.name","value":{"stringValue":"gw"}}]})",
        arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.trace.present);
    EXPECT_FALSE(pl.trace.has_parent);
    EXPECT_EQ(pl.trace.parent_span_id.value, 0U);
    EXPECT_EQ(pl.ordinals[0].value, 10000);
}

TEST_F(JsonStrategyTest, OtelSeverityNumberBands)
{
    struct Case
    {
        int severity_number;
        LogLevel expected;
    };
    // (n-1)/4 banding across the 6 levels, with clamps at both ends.
    const std::vector<Case> cases{
        {1, LogLevel::Trace}, {4, LogLevel::Trace},  {5, LogLevel::Debug},  {9, LogLevel::Info},
        {13, LogLevel::Warn}, {17, LogLevel::Error}, {21, LogLevel::Fatal}, {24, LogLevel::Fatal},
    };
    for (const auto& cas : cases)
    {
        const std::string line{R"({"severityNumber":)" + std::to_string(cas.severity_number) +
                               R"(,"body":{"stringValue":"m"},"traceId":"t","spanId":"s"})"};
        ArenaAllocator local_arena{1024};
        auto result{strategy.parse(line, local_arena)};
        ASSERT_TRUE(result.has_value()) << "severity_number=" << cas.severity_number;
        EXPECT_EQ(result.value().level, cas.expected)
            << "severity_number=" << cas.severity_number
            << " got=" << to_string(result.value().level.value());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// KVStrategy
// ─────────────────────────────────────────────────────────────────────────────

class KVStrategyTest : public ::testing::Test
{
  protected:
    KVStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(KVStrategyTest, ParsesAllFields)
{
    auto result{strategy.parse(kKVLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    EXPECT_EQ(pl.component, "auth");
    EXPECT_NE(pl.content.find("User logged in"), std::string::npos);
}

TEST_F(KVStrategyTest, ParsesMinimalKV)
{
    auto result{strategy.parse(kKVMinimal, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_EQ(pl.level, LogLevel::Warn);
    EXPECT_FALSE(pl.content.empty());
}

TEST_F(KVStrategyTest, QuotedValueUnquoted)
{
    auto result{strategy.parse(R"(component=db msg="connection timeout after 30s")", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().content.find("connection timeout"), std::string::npos);
}

TEST_F(KVStrategyTest, RejectsLineWithNoPairs)
{
    auto result{strategy.parse("plain log line with no equals signs", arena)};
    EXPECT_FALSE(result.has_value());
}

TEST_F(KVStrategyTest, ConfidenceHighWithManyPairs)
{
    EXPECT_GT(strategy.confidence(kKVLine), 0.5);
}

TEST_F(KVStrategyTest, ConfidenceLowForSyslog)
{
    EXPECT_LT(strategy.confidence(kBSDLine), 0.5);
}

TEST_F(KVStrategyTest, ConfidenceZeroForJSON)
{
    EXPECT_LT(strategy.confidence(kJSONLine), 0.5);
}

// ─────────────────────────────────────────────────────────────────────────────
// CLFStrategy
// ─────────────────────────────────────────────────────────────────────────────

class CLFStrategyTest : public ::testing::Test
{
  protected:
    CLFStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(CLFStrategyTest, ParsesCommonLogFormat)
{
    auto result{strategy.parse(kCLFLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    // DN-43.D8: the client IP is a NODE IDENTITY, so it lands in `host`, and `component` states —
    // positively — that this layout declares no functional source. Both halves are asserted: an
    // arm that only checked `component` would pass on a strategy that simply dropped the address.
    EXPECT_EQ(pl.host, "127.0.0.1");
    EXPECT_TRUE(pl.component.empty()) << "component = \"" << pl.component << "\"";
    EXPECT_NE(pl.content.find("GET"), std::string::npos);
    EXPECT_NE(pl.content.find("200"), std::string::npos);
}

TEST_F(CLFStrategyTest, ParsesCombinedLogFormat)
{
    auto result{strategy.parse(kCombinedLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    // 401 Unauthorized → Warn
    EXPECT_EQ(pl.level, LogLevel::Warn);
    EXPECT_NE(pl.content.find("POST"), std::string::npos);
    EXPECT_NE(pl.content.find("401"), std::string::npos);
}

TEST_F(CLFStrategyTest, Status5xxMapsToError)
{
    auto result{strategy.parse(kCLF5xx, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Error);
}

TEST_F(CLFStrategyTest, RejectsNonCLFLine)
{
    auto result{strategy.parse(kBSDLine, arena)};
    EXPECT_FALSE(result.has_value());
}

TEST_F(CLFStrategyTest, RejectsJSONLine)
{
    auto result{strategy.parse(kJSONLine, arena)};
    EXPECT_FALSE(result.has_value());
}

TEST_F(CLFStrategyTest, ConfidenceHighForCLF)
{
    EXPECT_GT(strategy.confidence(kCLFLine), 0.5);
}

TEST_F(CLFStrategyTest, ContentContainsMethodAndStatus)
{
    auto result{strategy.parse(kCLFLine, arena)};
    ASSERT_TRUE(result.has_value());
    const std::string_view content = result.value().content;
    EXPECT_NE(content.find("GET"), std::string::npos);
    EXPECT_NE(content.find("200"), std::string::npos);
}

TEST_F(CLFStrategyTest, TimestampParsed)
{
    auto result{strategy.parse(kCLFLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().timestamp.has_value());
}

TEST_F(CLFStrategyTest, ConfidenceHighForCombinedLine)
{
    EXPECT_GT(strategy.confidence(kCombinedLine), 0.5);
}

TEST_F(CLFStrategyTest, ConfidenceHighFor5xxLine)
{
    EXPECT_GT(strategy.confidence(kCLF5xx), 0.5);
}

TEST_F(CLFStrategyTest, ConfidenceZeroForKVLine)
{
    EXPECT_EQ(strategy.confidence(kKVLine), 0.0);
}

TEST_F(CLFStrategyTest, Status3xxMapsToInfo)
{
    auto result{strategy.parse(
        R"(127.0.0.1 - - [15/Jan/2024:10:30:00 +0000] "GET /old-path HTTP/1.1" 301 0)", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Info);
}

TEST_F(CLFStrategyTest, DashBodySizeIsParsed)
{
    // "-" is valid for response body size (no body sent, e.g. HEAD or 204).
    auto result{strategy.parse(
        R"(127.0.0.1 - - [15/Jan/2024:10:30:00 +0000] "HEAD /api/ping HTTP/1.1" 204 -)", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Info); // 2xx → Info
}

// ─────────────────────────────────────────────────────────────────────────────
// SyslogStrategy — additional edge cases
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(SyslogStrategyTest, ParsesBSDLineWithSingleDigitDay)
{
    // BSD syslog uses space-padded single-digit days: "Jan  1" (two spaces).
    auto result{strategy.parse("Jan  1 08:03:22 host sshd[1]: service started", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().timestamp.has_value());
    EXPECT_EQ(result.value().component, "sshd");
    EXPECT_EQ(result.value().content, "service started");
}

TEST_F(SyslogStrategyTest, TruncatedSyslogReturnsError)
{
    // A line that only starts with a month abbreviation but has no host/message.
    auto result{strategy.parse("Jan 15", arena)};
    EXPECT_FALSE(result.has_value());
}

TEST_F(SyslogStrategyTest, ConfidenceZeroForKVLine)
{
    EXPECT_EQ(strategy.confidence(kKVLine), 0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// SyslogStrategy — the CLAIM is the header, not the prefix (DN-43.D1/D3)
// ─────────────────────────────────────────────────────────────────────────────

// Clause 1. A level word standing where a hostname belongs is the mechanism that produced the
// published all-INFO window: `(void)sv_take_token` ate `INFO` as a host and the level was never
// read. The assertion is on `confidence()`, not on `parse()` — the gate is what routes, and a
// parse-only guard would drop the line instead of re-routing it.
TEST_F(SyslogStrategyTest, ClaimsNothingWhenTheHostSlotHoldsALevelWord)
{
    for (const std::string_view line :
         {"2026-05-31T08:00:01Z INFO request method=GET path=/api/users/1000 status=200",
          "2026-05-31T09:00:01Z ERROR upstream timeout service=payments after_ms=80",
          "2026-05-31T08:02:03Z DEBUG db query=select_orders duration_ms=15 rows=2",
          "2026-05-31T09:01:02Z WARN slow request path=/api/report latency_ms=103"})
    {
        EXPECT_EQ(strategy.confidence(line), 0.0) << "claimed: " << line;
        EXPECT_FALSE(strategy.parse(line, arena).has_value()) << "parsed: " << line;
    }
}

// The opposite direction, and it is what keeps the arm above from being satisfiable by a bare
// rejection of the whole RFC-3339 shape: a genuine RFC-3339 SYSLOG line still scores.
TEST_F(SyslogStrategyTest, StillClaimsARealRfc3339SyslogLine)
{
    constexpr std::string_view line{"2026-05-31T08:00:01Z web01 nginx[2451]: GET /api/users 200"};
    EXPECT_GT(strategy.confidence(line), 0.5);
    auto result{strategy.parse(line, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().component, "nginx");
    EXPECT_EQ(result.value().content, "GET /api/users 200");
}

// Clause 2. The tag search is bounded to ONE token, so a stray colon deeper in the message can no
// longer terminate it — the defect that made `cache key=session:1021 hit=true` yield the component
// `cache key=session` and the content `1021 hit=true`.
TEST_F(SyslogStrategyTest, TagSearchIsBoundedToOneTokenSoAStrayColonCannotSplitTheMessage)
{
    constexpr std::string_view line{
        "Jan 15 08:03:22 web01 app[1893]: cache key=session:1021 hit=true"};
    auto result{strategy.parse(line, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().component, "app");
    EXPECT_EQ(result.value().content, "cache key=session:1021 hit=true");
}

// A colon INSIDE the `[…]` pair is not a tag colon: `sshd[12:34]` alone does not qualify.
TEST_F(SyslogStrategyTest, ClaimsNothingWhenTheOnlyColonIsInsideTheBracketPair)
{
    constexpr std::string_view line{"Jan 15 08:03:22 web01 sshd[12:34] connection closed"};
    EXPECT_EQ(strategy.confidence(line), 0.0);
}

// DN-43.D5: both branches infer the level FROM THE MESSAGE BODY, in the `inferred` species. The BSD
// arm is asserted here because it is the one the old suite could not see — its only level assertion
// used a body with no level and no cue, so it held before and after.
TEST_F(SyslogStrategyTest, InfersTheLevelFromTheBodyOnBothBranches)
{
    auto bsd{
        strategy.parse("Jan 15 08:03:22 web01 sshd[1]: error: PAM authentication failure", arena)};
    ASSERT_TRUE(bsd.has_value());
    EXPECT_EQ(bsd.value().level, LogLevel::Error);
    EXPECT_FALSE(bsd.value().level.is_declared()) << "a body reading is INFERRED, never declared";

    auto rfc{strategy.parse("2026-05-31T08:00:01Z web01 app[9]: WARN disk almost full", arena)};
    ASSERT_TRUE(rfc.has_value());
    EXPECT_EQ(rfc.value().level, LogLevel::Warn);
    EXPECT_FALSE(rfc.value().level.is_declared());
}

// ─────────────────────────────────────────────────────────────────────────────
// Rfc3339TextStrategy — the LAYOUT split (DN-43.D4)
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kRfc3339AppLine{
    "2026-05-31T08:00:01Z INFO request method=GET path=/api/users/1000 status=200"};

class Rfc3339TextStrategyTest : public ::testing::Test
{
  protected:
    Rfc3339TextStrategy strategy;
    ArenaAllocator arena{4096};
};

// The whole slot in one arm, and the name is exact only after DN-43.D12: the stamp is READ (the
// event time survives — that is what killed bare rejection to raw text) and its BYTES ARE KEPT, so
// `content` is the whole line. Removing them would be the content-side workaround for an absent
// declaration ADR-23.D5 forbids. The level is still inferred from the POST-STAMP remainder, which
// is the assertion that separates this disposition from the one that scans `content` from byte 0
// and loses the level word to the stamp's share of the leading head.
TEST_F(Rfc3339TextStrategyTest, KeepsTheStampAndProjectsTheWholeRemainder)
{
    const std::string_view line{kRfc3339AppLine};
    EXPECT_GT(strategy.confidence(line), 0.0);
    auto result{strategy.parse(line, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value()) << "the event time is what MetaLog windows on";
    EXPECT_EQ(pl.content, line) << "the stamp's bytes must stay: content = \"" << pl.content
                                << "\"";
    EXPECT_TRUE(pl.content.starts_with("2026-05-31T08:00:01Z"))
        << "content = \"" << pl.content << "\"";
    EXPECT_EQ(pl.level, LogLevel::Info)
        << "inferred from the post-stamp remainder, not from byte 0";
    EXPECT_FALSE(pl.level.is_declared());
    EXPECT_TRUE(pl.component.empty()) << "component = \"" << pl.component << "\"";
}

// The level assertion above is satisfiable by a strategy that scans `content` from byte 0, because
// `INFO` still lands inside the leading head on THAT line — so on its own it is a can't-FAIL arm
// for the mechanism DN-43.D12 calls load-bearing. This line is the discriminating one, and it took
// a mutation run to make it so: the stamp plus a bracketed worker tag put the level word at byte
// 45, past infer_leading_log_level's 40-byte LEADING head, so a byte-0 scan cannot reach it in
// stage 1. The level must be NON-ALERTING for the arm to discriminate at all — stage 2's cue scan
// carries a 128-byte head and the words ERROR/FATAL are themselves failure cues, so an alerting
// level is recovered from byte 0 anyway and the arm goes green under the very mutation it exists to
// catch (measured: the first draft of this test passed under it). Bound the scan, never the claim
// (ADR-20). Both mutations red here: removing the stamp's bytes reds the content assert, scanning
// `content` instead of the post-stamp remainder reds the level assert.
TEST_F(Rfc3339TextStrategyTest, InfersTheLevelPastAStampThatSpendsTheLeadingHead)
{
    static constexpr std::string_view kHeadSpendingLine{
        "2026-05-31T08:00:01.123456Z [worker-7-of-16] INFO batch 4 of 9 dispatched"};
    EXPECT_GT(strategy.confidence(kHeadSpendingLine), 0.0);
    auto result{strategy.parse(kHeadSpendingLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_EQ(pl.content, kHeadSpendingLine) << "content = \"" << pl.content << "\"";
    EXPECT_EQ(pl.level, LogLevel::Info)
        << "the level word starts at byte 45 of the WHOLE line, past the leading head, so a byte-0 "
           "scan reads inferred(Unknown); level = "
        << to_string(pl.level.value());
    EXPECT_FALSE(pl.level.is_declared());
}

// DISJOINTNESS, from this side. Without it the sticky latch starves one of the two strategies for
// the rest of the file.
TEST_F(Rfc3339TextStrategyTest, ClaimsNothingWhenTheSyslogHeaderIsPresent)
{
    constexpr std::string_view line{"2026-05-31T08:00:01Z web01 nginx[2451]: GET /api/users 200"};
    EXPECT_EQ(strategy.confidence(line), 0.0);
    EXPECT_FALSE(strategy.parse(line, arena).has_value());
}

TEST_F(Rfc3339TextStrategyTest, ClaimsNothingWithoutAnRfc3339Prefix)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
    EXPECT_EQ(strategy.confidence(kJSONLine), 0.0);
    EXPECT_EQ(strategy.confidence("plain build output, no stamp"), 0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonStrategy — additional edge cases
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(JsonStrategyTest, MalformedJSONReturnsError)
{
    // Truncated object — simdjson rejects the unterminated document.
    auto result{strategy.parse(R"({"level":"INFO","message":"not closed)", arena)};
    EXPECT_FALSE(result.has_value());
}

TEST_F(JsonStrategyTest, EmptyObjectFallbackDump)
{
    // Valid JSON, but no known keys — falls back to j.dump() which is "{}".
    auto result{strategy.parse("{}", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().content.empty());
}

TEST_F(JsonStrategyTest, UTF8ContentPreserved)
{
    // UTF-8 text including a multibyte sequence and an emoji should pass through
    // simdjson and end up verbatim in the content field.
    auto result{strategy.parse(R"({"msg":"Connexion \u00e9tablie \ud83d\ude80"})", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().content.empty());
}

TEST_F(JsonStrategyTest, ConfidenceZeroForCLFLine)
{
    // CLF lines don't start with '{'.
    EXPECT_EQ(strategy.confidence(kCLFLine), 0.0);
}

TEST_F(JsonStrategyTest, ConfidenceOneForMinimalJSON)
{
    // Any line starting with '{' gets confidence 1.0 — format() handles the
    // parse failure if the content turns out to be invalid JSON.
    EXPECT_EQ(strategy.confidence(R"({"x":1})"), 1.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// KVStrategy — additional confidence and value edge cases
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(KVStrategyTest, ConfidenceTwoPairsIsModerate)
{
    // 2 KV pairs → 0.70 per implementation (> 0.4, < 0.9).
    double c{strategy.confidence("level=WARN msg=timeout")};
    EXPECT_GT(c, 0.4);
    EXPECT_LT(c, 0.9);
}

TEST_F(KVStrategyTest, ConfidenceOnePairIsLow)
{
    // 1 KV pair → 0.30 per implementation (> 0.0, < 0.5).
    double c{strategy.confidence("key=value")};
    EXPECT_GT(c, 0.0);
    EXPECT_LT(c, 0.5);
}

// Free text with a lone TRAILING pair is not logfmt: KV must decline it (0
// confidence) so the raw-text fallback keeps the whole message and infers the
// leading level. Otherwise KV keeps only "order=42" / "host=db" as content,
// dropping the human-readable text and fragmenting the template per value —
// which buried the vanished-success signal in Sift's silent-regression demo.
TEST_F(KVStrategyTest, ConfidenceZeroForFreeTextWithTrailingPair)
{
    EXPECT_EQ(strategy.confidence("INFO checkout completed order=42"), 0.0);
    EXPECT_EQ(strategy.confidence("ERROR connection refused host=db"), 0.0);
    EXPECT_EQ(strategy.confidence("retrying payment gateway attempt=3"), 0.0);
}

// The flip side: a genuine logfmt line OPENS with a pair (here `ts=`) and must
// still be claimed by KV at high confidence — the gate keys on the lead token,
// not on the mere presence of free-text-looking values.
TEST_F(KVStrategyTest, ConfidenceHighWhenLeadingPairPresent)
{
    EXPECT_GT(strategy.confidence(kKVLine), 0.5);
    EXPECT_GT(strategy.confidence("level=INFO checkout completed order=42"), 0.0);
}

TEST_F(KVStrategyTest, QuotedValuePreservesInternalSpaces)
{
    auto result{strategy.parse(R"(level=ERROR msg="failed to connect to host port 443")", arena)};
    ASSERT_TRUE(result.has_value());
    // The message content should include the full quoted sentence (minus quotes).
    EXPECT_NE(result.value().content.find("failed to connect"), std::string::npos);
}

TEST_F(KVStrategyTest, UTF8ValuePreserved)
{
    // Accented characters in a quoted KV value must pass through unchanged.
    auto result{strategy.parse(R"(level=INFO msg="connexion \u00e9tablie" component=auth)", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result.value().content.empty());
    EXPECT_EQ(result.value().component, "auth");
}

TEST_F(KVStrategyTest, MultiLineInputHandled)
{
    // A string containing a literal newline: RE2 FindAndConsume stops the
    // unquoted value at the newline (\s) and resumes on the second "line".
    // Both pairs must be extracted → parse succeeds.
    auto result{strategy.parse("key1=val1\nkey2=val2", arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_FALSE(result.value().content.empty());
}

TEST_F(KVStrategyTest, EmptyValueReturnsError)
{
    // "key=" has no value chars — the pattern [^\s,;]+ requires ≥1 character.
    // 0 pairs → parse must return an error, not crash.
    auto result{strategy.parse("key=", arena)};
    EXPECT_FALSE(result.has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// SyslogStrategy — out-of-range day
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(SyslogStrategyTest, InvalidDayLineDoesNotCrash)
{
    // "Feb 30" does not exist. The BSD regex still matches (it only checks the
    // shape of the field, not calendar validity); utc_mktime normalises the
    // date. Parse must succeed without crashing.
    auto result{strategy.parse("Feb 30 12:00:00 myhost proc[1]: msg after invalid day", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().content, "msg after invalid day");
    // Timestamp should be populated (normalised date).
    EXPECT_TRUE(result.value().timestamp.has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// JsonStrategy — nested objects and top-level arrays
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(JsonStrategyTest, NestedObjectMessageExtracted)
{
    // Deep nesting in other keys must not prevent "msg" from being found.
    auto result{strategy.parse(R"({"outer":{"inner":123},"msg":"nested test"})", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().content, "nested test");
}

TEST_F(JsonStrategyTest, TopLevelArrayReturnsError)
{
    // A JSON array is valid JSON but is not an object.
    // json.cpp line 43: !j.is_object() → explicit error.
    // Confidence is also 0 (first char is '[').
    EXPECT_EQ(strategy.confidence(R"([{"msg":"a"}])"), 0.0);
    auto result{strategy.parse(R"([{"msg":"a"},{"msg":"b"}])", arena)};
    EXPECT_FALSE(result.has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// CLFStrategy — non-standard HTTP verbs
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(CLFStrategyTest, NonStandardVerbPatchConfidence)
{
    // kStrongCheck uses [A-Z]+ which matches any uppercase verb — PATCH, PUT,
    // DELETE, OPTIONS, etc.
    constexpr std::string_view line{
        R"(127.0.0.1 - alice [15/Jan/2024:10:30:00 +0000] "PATCH /api/resource HTTP/1.1" 200 512)"};
    EXPECT_GT(strategy.confidence(line), 0.9);
}

TEST_F(CLFStrategyTest, NonStandardVerbPatchParsed)
{
    constexpr std::string_view line{
        R"(127.0.0.1 - alice [15/Jan/2024:10:30:00 +0000] "PATCH /api/resource HTTP/1.1" 200 512)"};
    auto result{strategy.parse(line, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Info); // 200 → Info
    EXPECT_NE(result.value().content.find("PATCH"), std::string::npos);
    EXPECT_NE(result.value().content.find("/api/resource"), std::string::npos);
}

// ─────────────────────────────────────────────────────────────────────────────
// Log4jStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kLog4jHadoopLine =
    "2015-10-18 18:01:47,978 INFO [main] "
    "org.apache.hadoop.mapreduce.v2.app.MRAppMaster: "
    "Created MRAppMaster for application appattempt_1445144423722_0020_000001";

static constexpr std::string_view kLog4jZookeeperLine =
    "2015-07-29 17:41:44,747 - INFO  "
    "[QuorumPeer[myid=1]/0:0:0:0:0:0:0:0:2181:FastLeaderElection@774] - "
    "Notification time out: 3200";

static constexpr std::string_view kLog4jOpenStackLine =
    "nova-api.log.1.2017-05-16_13:53:08 2017-05-16 00:00:00.008 25746 INFO "
    "nova.osapi_compute.wsgi.server "
    "[req-38101a0b-2096-447d-96ea-a692162415ae "
    "113d3a99c3da401fbd62cc2caa5b96d2 "
    "54fadb412c4e40cdbaed9335e4c35a9e - - -] "
    R"(10.11.10.1 "GET /v2/54fadb412c4e40cdbaed9335e4c35a9e/servers/detail HTTP/1.1" )"
    "status: 200 len: 1893 time: 0.2477829";

class Log4jStrategyTest : public ::testing::Test
{
  protected:
    Log4jStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(Log4jStrategyTest, ParsesHadoopLine)
{
    auto result{strategy.parse(kLog4jHadoopLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    EXPECT_EQ(pl.component, "org.apache.hadoop.mapreduce.v2.app.MRAppMaster");
    EXPECT_NE(pl.content.find("MRAppMaster"), std::string::npos);
}

TEST_F(Log4jStrategyTest, ParsesZookeeperLine)
{
    auto result{strategy.parse(kLog4jZookeeperLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    EXPECT_NE(pl.content.find("Notification time out"), std::string::npos);
}

TEST_F(Log4jStrategyTest, ParsesOpenStackLine)
{
    auto result{strategy.parse(kLog4jOpenStackLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    EXPECT_NE(pl.component.find("nova"), std::string::npos);
}

// PROJECTION TOTALITY on the branch the ProjectionIsTotal family structurally cannot see: that
// family feeds each strategy its CANONICAL line, and a canonical line always carries the delimiter.
// The defect is found by DEGRADING the input, never by mutating the code — a colon-less line drove
// sv_take_until's no-delimiter branch, which returns the WHOLE remainder, so `component` became the
// message body (high-card free text on the cube's WHERE axis, published unmasked) and `content`
// became EMPTY (the SHA-256 prefix of the empty string, the universal collision bucket). DN-43
// repaired this shape at SyslogStrategy by bounding its tag search and called the no-delimiter
// branch "unreachable"; it was unreachable only from that caller.
TEST_F(Log4jStrategyTest, ColonlessStandardLineNamesNoComponentAndKeepsEveryByte)
{
    static constexpr std::string_view kNoColon{
        "2015-10-18 18:01:47,978 INFO [main] startup complete after 12 s"};
    auto result{strategy.parse(kNoColon, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_EQ(pl.content, "startup complete after 12 s")
        << "content = \"" << pl.content << "\" component = \"" << pl.component << "\"";
    EXPECT_TRUE(pl.component.empty()) << "the colon TERMINATES the component; absent it the line "
                                         "names none, it does not name the "
                                         "whole message; component = \""
                                      << pl.component << "\"";
    EXPECT_EQ(pl.level, LogLevel::Info) << "the header's declared level is still recovered";
    EXPECT_TRUE(pl.timestamp.has_value()) << "and so is the event time";
}

TEST_F(Log4jStrategyTest, RejectsNonLog4jLine)
{
    EXPECT_FALSE(strategy.parse(kBSDLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

TEST_F(Log4jStrategyTest, ConfidenceHighForLog4j)
{
    EXPECT_GT(strategy.confidence(kLog4jHadoopLine), 0.5);
    EXPECT_GT(strategy.confidence(kLog4jZookeeperLine), 0.5);
    EXPECT_GT(strategy.confidence(kLog4jOpenStackLine), 0.5);
}

TEST_F(Log4jStrategyTest, ConfidenceZeroForJSON)
{
    EXPECT_EQ(strategy.confidence(kJSONLine), 0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// SparkHDFSStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kSparkLine =
    "17/06/09 20:10:40 INFO executor.CoarseGrainedExecutorBackend: "
    "Registered signal handlers for [TERM, HUP, INT]";

static constexpr std::string_view kHDFSLine =
    "081109 203615 148 INFO dfs.DataNode$PacketResponder: "
    "PacketResponder 1 for block blk_38865049064139660 terminating";

class SparkHDFSStrategyTest : public ::testing::Test
{
  protected:
    SparkHDFSStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(SparkHDFSStrategyTest, ParsesSparkLine)
{
    auto result{strategy.parse(kSparkLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    EXPECT_EQ(pl.component, "executor.CoarseGrainedExecutorBackend");
    EXPECT_NE(pl.content.find("Registered signal handlers"), std::string::npos);
}

TEST_F(SparkHDFSStrategyTest, ParsesHDFSLine)
{
    auto result{strategy.parse(kHDFSLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    EXPECT_EQ(pl.component, "dfs.DataNode$PacketResponder");
    EXPECT_NE(pl.content.find("PacketResponder"), std::string::npos);
}

// Both arms, same degraded input, same defect as the Log4j case above — SparkHDFS is the only
// strategy carrying the shape twice, so both are driven rather than one and an inference.
TEST_F(SparkHDFSStrategyTest, ColonlessLinesNameNoComponentAndKeepEveryByte)
{
    static constexpr std::string_view kSparkNoColon{"17/06/09 20:10:40 INFO shutting down cleanly"};
    static constexpr std::string_view kHdfsNoColon{
        "081109 203615 148 INFO waiting for block report"};

    auto spark{strategy.parse(kSparkNoColon, arena)};
    ASSERT_TRUE(spark.has_value()) << spark.error();
    EXPECT_EQ(spark.value().content, "shutting down cleanly")
        << "content = \"" << spark.value().content << "\" component = \"" << spark.value().component
        << "\"";
    EXPECT_TRUE(spark.value().component.empty())
        << "component = \"" << spark.value().component << "\"";
    EXPECT_EQ(spark.value().level, LogLevel::Info);

    auto hdfs{strategy.parse(kHdfsNoColon, arena)};
    ASSERT_TRUE(hdfs.has_value()) << hdfs.error();
    EXPECT_EQ(hdfs.value().content, "waiting for block report")
        << "content = \"" << hdfs.value().content << "\" component = \"" << hdfs.value().component
        << "\"";
    EXPECT_TRUE(hdfs.value().component.empty())
        << "component = \"" << hdfs.value().component << "\"";
    EXPECT_EQ(hdfs.value().level, LogLevel::Info);
}

TEST_F(SparkHDFSStrategyTest, RejectsNonMatchingLines)
{
    EXPECT_FALSE(strategy.parse(kBSDLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

TEST_F(SparkHDFSStrategyTest, ConfidenceHighForSpark)
{
    EXPECT_GT(strategy.confidence(kSparkLine), 0.5);
}

TEST_F(SparkHDFSStrategyTest, ConfidenceHighForHDFS)
{
    EXPECT_GT(strategy.confidence(kHDFSLine), 0.5);
}

// ─────────────────────────────────────────────────────────────────────────────
// BGLStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kBGLLine =
    "- 1117838570 2005.06.03 R02-M1-N0-C:J12-U11 2005-06-03-15.42.50.675872 "
    "R02-M1-N0-C:J12-U11 RAS KERNEL INFO instruction cache parity error "
    "corrected";

static constexpr std::string_view kThunderbirdLine =
    "- 1131566461 2005.11.09 dn228 Nov 9 12:01:01 dn228/dn228 "
    "crond(pam_unix)[2915]: session closed for user root";

class BGLStrategyTest : public ::testing::Test
{
  protected:
    BGLStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(BGLStrategyTest, ParsesBGLLine)
{
    auto result{strategy.parse(kBGLLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    // F3b: component = the low-card subsystem (the cube dim), host = the node identity.
    EXPECT_EQ(pl.component, "KERNEL");
    EXPECT_EQ(pl.host, "R02-M1-N0-C:J12-U11");
    EXPECT_NE(pl.content.find("instruction cache parity error"), std::string::npos);
}

TEST_F(BGLStrategyTest, ParsesThunderbirdLine)
{
    auto result{strategy.parse(kThunderbirdLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    // F3b: component = the daemon ([pid] stripped), host = the node; the message is content.
    EXPECT_EQ(pl.component, "crond(pam_unix)");
    EXPECT_EQ(pl.host, "dn228");
    EXPECT_NE(pl.content.find("session closed for user root"), std::string::npos);
}

// F3b: BGL's FACILITY field is RAS or NULL — a NULL-facility line (DISCOVERY etc.) must still
// yield the subsystem as component (not fall through to the syslog branch and grab a message
// fragment). The digit-leading secondary timestamp routes it to the BGL branch.
TEST_F(BGLStrategyTest, ParsesNullFacilityDiscoveryLine)
{
    static constexpr std::string_view kNullDiscoveryLine =
        "- 1118246007 2005.06.08 R33-M1-N8 2005-06-08-08.53.27.435197 R33-M1-N8 "
        "NULL DISCOVERY WARNING Node card VPD check: U01 node";
    auto result{strategy.parse(kNullDiscoveryLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_EQ(pl.component, "DISCOVERY");
    EXPECT_EQ(pl.host, "R33-M1-N8");
    EXPECT_EQ(pl.level, LogLevel::Warn);
    EXPECT_NE(pl.content.find("Node card VPD check"), std::string::npos);
}

TEST_F(BGLStrategyTest, RejectsNonMatchingLines)
{
    EXPECT_FALSE(strategy.parse(kBSDLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

TEST_F(BGLStrategyTest, ConfidenceHighForBGL)
{
    EXPECT_GT(strategy.confidence(kBGLLine), 0.5);
    EXPECT_GT(strategy.confidence(kThunderbirdLine), 0.5);
}

// ── The ALERT-LABEL column (DN-43.D14) ───────────────────────────────────────────────────────
// LogHub's curators prepended an alert-class column to records BGL's RAS system wrote without one.
// It is the corpus's ANSWER KEY, so the grammar validates it and no projection field carries it:
// a labelled line must project identically to its `-` twin, or one event class splits by curation.

// `kBGLLine` with the label column set to an alert class and the declared level raised to FATAL —
// the exact byte shape of 348 398 of the pinned corpus's 348 460 labelled lines.
static constexpr std::string_view kBGLAlertLine =
    "KERNDTLB 1117838570 2005.06.03 R02-M1-N0-C:J12-U11 2005-06-03-15.42.50.675872 "
    "R02-M1-N0-C:J12-U11 RAS KERNEL FATAL data TLB error interrupt";

TEST_F(BGLStrategyTest, ClaimsAnAlertLabelledLineAndReadsItsDeclaredLevel)
{
    auto result{strategy.parse(kBGLAlertLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Fatal) << "level = " << to_string(pl.level.value());
    EXPECT_TRUE(pl.level.is_declared()) << "BGL writes its severity in a fixed column";
    EXPECT_EQ(pl.component, "KERNEL");
    EXPECT_EQ(pl.host, "R02-M1-N0-C:J12-U11");
    EXPECT_EQ(pl.content, "data TLB error interrupt");
}

// The label reaches NO field. Asserted per field rather than on the projection as a whole, so a
// failure names the field that leaked the oracle.
TEST_F(BGLStrategyTest, TheAlertLabelReachesNoProjectionField)
{
    auto result{strategy.parse(kBGLAlertLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_EQ(pl.content.find("KERNDTLB"), std::string_view::npos) << "content = " << pl.content;
    EXPECT_EQ(pl.component.find("KERNDTLB"), std::string_view::npos)
        << "component = " << pl.component;
    EXPECT_EQ(pl.host.find("KERNDTLB"), std::string_view::npos) << "host = " << pl.host;
}

// The second BGL header shape: `<node>` is `-` and the node is NOT repeated, so `<FACILITY>`
// arrives one token early. 306 lines of the pinned corpus, today mis-parsed to `level` Unknown
// with `component` = `FATAL` — the field-shift the predicate now refuses to publish.
TEST_F(BGLStrategyTest, ParsesTheSecondHeaderShapeWithNoRepeatedNode)
{
    static constexpr std::string_view kNoRepeatedNodeLine =
        "- 1133447860 2005.12.01 - 2005-12-01-06.37.40.117709 RAS KERNEL FATAL "
        "data storage interrupt";
    auto result{strategy.parse(kNoRepeatedNodeLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_EQ(pl.component, "KERNEL");
    EXPECT_EQ(pl.host, "-");
    EXPECT_EQ(pl.level, LogLevel::Fatal) << "level = " << to_string(pl.level.value());
    EXPECT_EQ(pl.content, "data storage interrupt");
}

// `<node2>` is itself the literal `NULL` on 89 296 pinned-corpus lines. Probing the second header
// shape first would read that node as the facility and decline every one of them, so the canonical
// shape is probed first and this line is the detector for that order.
TEST_F(BGLStrategyTest, ParsesARecordWhoseRepeatedNodeIsTheLiteralNull)
{
    static constexpr std::string_view kNullNodeLine =
        "- 1117867321 2005.06.03 NULL 2005-06-03-23.42.01.596122 NULL RAS MMCS INFO "
        "ciodb has been restarted.";
    auto result{strategy.parse(kNullNodeLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_EQ(pl.component, "MMCS");
    EXPECT_EQ(pl.host, "NULL");
    EXPECT_EQ(pl.level, LogLevel::Info) << "level = " << to_string(pl.level.value());
    EXPECT_EQ(pl.content, "ciodb has been restarted.");
}

// A record whose `<node2>` holds a spliced message fragment instead of a node: the facility is at
// neither position, so the parse would be off by several fields. It DECLINES — 10 lines of the
// pinned corpus — rather than publishing a message fragment on the cube's WHERE axis.
TEST_F(BGLStrategyTest, DeclinesARecordWhoseFacilityIsAtNeitherPosition)
{
    static constexpr std::string_view kSplicedLine =
        "- 1133447861 2005.12.01 - 2005-12-01-06.37.41.417709 time for a single instance of a "
        "correctable ddr. RAS KERNEL INFO 0 microseconds spent in the rbs signal handler";
    EXPECT_DOUBLE_EQ(strategy.confidence(kSplicedLine), 0.0);
    EXPECT_FALSE(strategy.parse(kSplicedLine, arena).has_value());
}

// ── `<date>` IS validated, and NOT where the consumption site suggests (DN-43.D15) ───────────
// DN-43.D15's rule is general and it is not a BGL exception: a grammar field the parse CONSUMES
// but does not PUBLISH is validated if and only if its value is what proves the record's FIELD
// ALIGNMENT — never as byte hygiene. Three of BGL's fields are consumed and unpublished, and they
// do NOT share a verdict:
//   * `<FACILITY>` is validated — `starts_with_facility` selects between the two RAS header shapes.
//   * `<node2>` is NOT — the alignment proof sits on the token BEHIND it, so a record whose
//     `<node2>` holds a binary blob parses, and DN-43.D15 rules it must keep parsing.
//   * `<date>` IS validated, by a predicate that is nowhere near where it is consumed. The
//     consumption site is `(void)sv_take_token(rest)` in `scan_bgl_record` and applies nothing;
//     the predicate ran already, in the FORMAT GATE `is_bgl_labelled_prefix`, which keys the
//     BGL/Thunderbird grammar on three opening fields — the label, the digit `<epoch>`, and
//     `<date>`'s exact dotted `YYYY.MM.DD` shape. That is a discriminator, so it is on the
//     alignment side of DN-43.D15's criterion, not the hygiene side.
// These two arms are that contrast, and they exist because a reader who checks the verdict at the
// consumption site reads `<date>` and `<node2>` as the same case and gets one of them backwards.
TEST_F(BGLStrategyTest, TheDateFieldIsValidatedByTheFormatGateNotAtItsConsumptionSite)
{
    // The canonical line with the third field replaced. Everything else is byte-identical, so a
    // decline can only be `<date>`'s.
    const std::array<std::pair<std::string_view, std::string_view>, 2> kRejected{{
        {"printable non-date", "not-a-date"},
        {"the right date, the wrong separator", "2005-06-03"},
    }};
    for (const auto& [name, date] : kRejected)
    {
        const std::string line{"- 1117838570 " + std::string{date} +
                               " R02-M1-N0-C:J12-U11 2005-06-03-15.42.50.675872 "
                               "R02-M1-N0-C:J12-U11 RAS KERNEL INFO instruction cache parity "
                               "error corrected"};
        EXPECT_DOUBLE_EQ(strategy.confidence(line), 0.0)
            << "variant '" << name
            << "': the gate IS the grammar (DN-43.D2), so a field the gate refuses must score zero";
        EXPECT_FALSE(strategy.parse(line, arena).has_value())
            << "variant '" << name
            << "' PARSED. `<date>` carries a real predicate — the exact "
               "dotted YYYY.MM.DD shape, checked in is_bgl_labelled_prefix before a single token "
               "is consumed. If this parses, that predicate is gone and the BGL/Thunderbird "
               "grammar has lost one of the three fields it discriminates on\n  line: "
            << line;
    }
    // The same position, a well-formed value: the control, without which the rows above would pass
    // a strategy that had simply stopped parsing BGL.
    static constexpr std::string_view kValidDateLine =
        "- 1117838570 2005.06.03 R02-M1-N0-C:J12-U11 2005-06-03-15.42.50.675872 "
        "R02-M1-N0-C:J12-U11 RAS KERNEL INFO instruction cache parity error corrected";
    auto control{strategy.parse(kValidDateLine, arena)};
    ASSERT_TRUE(control.has_value()) << control.error();
    EXPECT_EQ(control.value().component, "KERNEL");
    EXPECT_EQ(control.value().level, LogLevel::Info)
        << "level = " << to_string(control.value().level.value());
}

// The other side of DN-43.D15's partition, on the SAME line and one field over: `<node2>` has no
// predicate anywhere, so a binary blob in it parses and every published field stays true. Eight
// lines of the pinned BGL corpus are exactly this, and the ruling turns on their still parsing —
// declining them would move the blob out of a DROPPED field into `RawTextStrategy`'s whole-line
// `content`, where it becomes a stable template NAME, and would throw away a DECLARED level.
// The corpus-scale count is `LogHubProjectionPinGate`'s, which needs the corpus mounted; this arm
// pins the BEHAVIOUR with no corpus at all, so the ruling is not resting on a gate that skips.
TEST_F(BGLStrategyTest, ARecordWhoseUnvalidatedNode2HoldsABinaryBlobStillParsesAndPublishesTruth)
{
    static const std::string kBlobNodeLine{
        "- 1117838570 2005.06.03 R02-M1-N0-C:J12-U11 2005-06-03-15.42.50.675872 "
        "\x01\x02\x03 RAS KERNEL INFO instruction cache parity error corrected"};
    EXPECT_GT(strategy.confidence(kBlobNodeLine), 0.5);
    auto result{strategy.parse(kBlobNodeLine, arena)};
    ASSERT_TRUE(result.has_value())
        << "DECLINED: " << result.error()
        << "\n  a decline here means <node2> acquired a byte-hygiene predicate, which DN-43.D15 "
           "refuses: the alignment proof sits on the token BEHIND <node2> and succeeds truthfully";
    const auto& pl{result.value()};
    EXPECT_EQ(pl.component, "KERNEL");
    EXPECT_EQ(pl.host, "R02-M1-N0-C:J12-U11") << "host comes from <node>, never from <node2>";
    EXPECT_EQ(pl.level, LogLevel::Info)
        << "level = " << to_string(pl.level.value()) << " — the DECLARED level must survive";
    EXPECT_EQ(pl.content, "instruction cache parity error corrected");
    EXPECT_EQ(pl.content.find('\x01'), std::string_view::npos)
        << "the blob must reach NO published field; content = " << pl.content;
}

// A complete header with no message. Legitimate (DN-43.D6 member (a)): the empty template is the
// correct identity of a content-less line, and the header fields are still published. 34 470 lines
// of the pinned corpus.
TEST_F(BGLStrategyTest, AnEmptyBodyProjectsToEmptyContentWithItsHeaderFieldsSet)
{
    static constexpr std::string_view kEmptyBodyLine =
        "- 1117838570 2005.06.03 R02-M1-N0 2005-06-03-15.42.50.675872 R02-M1-N0 "
        "RAS KERNEL FATAL";
    auto result{strategy.parse(kEmptyBodyLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.content.empty()) << "content = " << pl.content;
    EXPECT_EQ(pl.component, "KERNEL");
    EXPECT_EQ(pl.host, "R02-M1-N0");
    EXPECT_EQ(pl.level, LogLevel::Fatal) << "level = " << to_string(pl.level.value());
}

// The Thunderbird branch with NO delimited tag. Nothing is removed: the remainder stays content
// and `component` is empty. 1 309 pinned-corpus lines used to lose their whole body to
// `component` here, leaving a template that is the SHA-256 of nothing.
TEST_F(BGLStrategyTest, AThunderbirdLineWithNoDelimitedTagKeepsItsWholeRemainder)
{
    static constexpr std::string_view kNoTagLine =
        "- 1131566461 2005.11.09 dn228 Nov 9 12:01:01 dn228/dn228 exiting on signal 15";
    auto result{strategy.parse(kNoTagLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.component.empty()) << "component = " << pl.component;
    EXPECT_EQ(pl.content, "exiting on signal 15");
    EXPECT_EQ(pl.host, "dn228");
}

// The one-token bound also refuses a multi-token pseudo-tag: `Server Administrator:` is two
// tokens, so its colon is not a tag delimiter and the whole remainder stays content.
TEST_F(BGLStrategyTest, AThunderbirdMultiTokenPseudoTagIsNotATag)
{
    static constexpr std::string_view kPseudoTagLine =
        "VAPI 1131566461 2005.11.09 dn228 Nov 9 12:01:01 dn228/dn228 "
        "Server Administrator: Instrumentation Service EventID: 1000";
    auto result{strategy.parse(kPseudoTagLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.component.empty()) << "component = " << pl.component;
    EXPECT_EQ(pl.content, "Server Administrator: Instrumentation Service EventID: 1000");
}

// ─────────────────────────────────────────────────────────────────────────────
// AndroidLogcatStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kAndroidLine =
    "03-17 16:13:38.859  2227  2227 D TextView: visible is "
    "system.time.showampm";

class AndroidLogcatStrategyTest : public ::testing::Test
{
  protected:
    AndroidLogcatStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(AndroidLogcatStrategyTest, ParsesLogcatLine)
{
    auto result{strategy.parse(kAndroidLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_EQ(pl.level, LogLevel::Debug);
    EXPECT_EQ(pl.component, "TextView");
    EXPECT_NE(pl.content.find("visible is system.time"), std::string::npos);
}

TEST_F(AndroidLogcatStrategyTest, ParsesInfoLevel)
{
    auto result{strategy.parse(
        "03-17 16:13:38.811  1702  2395 I ActivityManager: Starting activity", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Info);
}

TEST_F(AndroidLogcatStrategyTest, ParsesErrorLevel)
{
    auto result{
        strategy.parse("03-17 16:13:38.811  1702  2395 E System: Uncaught exception", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Error);
}

TEST_F(AndroidLogcatStrategyTest, RejectsNonMatchingLines)
{
    EXPECT_FALSE(strategy.parse(kBSDLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

TEST_F(AndroidLogcatStrategyTest, ConfidenceHighForLogcat)
{
    EXPECT_GT(strategy.confidence(kAndroidLine), 0.5);
}

// ─────────────────────────────────────────────────────────────────────────────
// ApacheErrorLogStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kApacheErrorLine =
    "[Sun Dec 04 04:47:44 2005] [notice] workerEnv.init() ok "
    "/etc/httpd/conf/workers2.properties";

static constexpr std::string_view kApacheErrorLine2 =
    "[Sun Dec 04 04:47:44 2005] [error] mod_jk child workerEnv in error state "
    "6";

class ApacheErrorLogStrategyTest : public ::testing::Test
{
  protected:
    ApacheErrorLogStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(ApacheErrorLogStrategyTest, ParsesNoticeLevel)
{
    auto result{strategy.parse(kApacheErrorLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_NE(pl.content.find("workerEnv.init()"), std::string::npos);
}

TEST_F(ApacheErrorLogStrategyTest, ParsesErrorLevel)
{
    auto result{strategy.parse(kApacheErrorLine2, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result.value().level, LogLevel::Error);
}

TEST_F(ApacheErrorLogStrategyTest, RejectsNonMatchingLines)
{
    EXPECT_FALSE(strategy.parse(kBSDLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

TEST_F(ApacheErrorLogStrategyTest, ConfidenceHighForApache)
{
    EXPECT_GT(strategy.confidence(kApacheErrorLine), 0.5);
}

// ─────────────────────────────────────────────────────────────────────────────
// WindowsCBSStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kWindowsCBSLine =
    "2016-09-28 04:30:30, Info                  CBS    Loaded Servicing Stack "
    "v6.1.7601.23505 with Core: "
    R"(C:\Windows\winsxs\amd64_microsoft-windows-servicingstack.dll)";

class WindowsCBSStrategyTest : public ::testing::Test
{
  protected:
    WindowsCBSStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(WindowsCBSStrategyTest, ParsesWindowsLine)
{
    auto result{strategy.parse(kWindowsCBSLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    EXPECT_EQ(pl.component, "CBS");
    EXPECT_NE(pl.content.find("Loaded Servicing Stack"), std::string::npos);
}

TEST_F(WindowsCBSStrategyTest, RejectsNonMatchingLines)
{
    EXPECT_FALSE(strategy.parse(kBSDLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

TEST_F(WindowsCBSStrategyTest, ConfidenceHighForWindows)
{
    EXPECT_GT(strategy.confidence(kWindowsCBSLine), 0.5);
}

// ─────────────────────────────────────────────────────────────────────────────
// HealthAppStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kHealthAppLine{
    "20171223-22:15:29:606|Step_LSC|30002312|onStandStepChanged 3579"};

class HealthAppStrategyTest : public ::testing::Test
{
  protected:
    HealthAppStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(HealthAppStrategyTest, ParsesHealthAppLine)
{
    auto result{strategy.parse(kHealthAppLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.component, "Step_LSC");
    EXPECT_NE(pl.content.find("onStandStepChanged"), std::string::npos);
}

TEST_F(HealthAppStrategyTest, RejectsNonMatchingLines)
{
    EXPECT_FALSE(strategy.parse(kBSDLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

TEST_F(HealthAppStrategyTest, ConfidenceHighForHealthApp)
{
    EXPECT_GT(strategy.confidence(kHealthAppLine), 0.5);
}

// ─────────────────────────────────────────────────────────────────────────────
// ProxifierStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kProxifierLine =
    "[10.30 16:49:06] chrome.exe - proxy.cse.cuhk.edu.hk:5070 open through "
    "proxy "
    "proxy.cse.cuhk.edu.hk:5070 HTTPS";

class ProxifierStrategyTest : public ::testing::Test
{
  protected:
    ProxifierStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(ProxifierStrategyTest, ParsesProxifierLine)
{
    auto result{strategy.parse(kProxifierLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_EQ(pl.component, "chrome.exe");
    EXPECT_NE(pl.content.find("proxy.cse.cuhk.edu.hk"), std::string::npos);
}

TEST_F(ProxifierStrategyTest, ParsesCloseMessage)
{
    auto result{strategy.parse("[10.30 16:49:07] chrome.exe *64 close, 0 bytes "
                               "sent, 0 bytes received, lifetime 00:01",
                               arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_NE(result.value().content.find("close"), std::string::npos);
}

TEST_F(ProxifierStrategyTest, RejectsNonMatchingLines)
{
    EXPECT_FALSE(strategy.parse(kBSDLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

TEST_F(ProxifierStrategyTest, ConfidenceHighForProxifier)
{
    EXPECT_GT(strategy.confidence(kProxifierLine), 0.5);
}

// ─────────────────────────────────────────────────────────────────────────────
// HPCStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kHPCLine =
    "134681 node-246 unix.hw state_change.unavailable 1077804742 1 "
    R"(Component State Change: Component \042alt0\042 is in the unavailable state (HWID=1973))";

class HPCStrategyTest : public ::testing::Test
{
  protected:
    HPCStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(HPCStrategyTest, ParsesHPCLine)
{
    auto result{strategy.parse(kHPCLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_NE(pl.component.find("unix.hw"), std::string::npos);
    EXPECT_NE(pl.content.find("Component State Change"), std::string::npos);
}

TEST_F(HPCStrategyTest, RejectsNonMatchingLines)
{
    EXPECT_FALSE(strategy.parse(kBSDLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

TEST_F(HPCStrategyTest, ConfidenceHighForHPC)
{
    EXPECT_GT(strategy.confidence(kHPCLine), 0.5);
}

TEST_F(HPCStrategyTest, ParsesNonDottedFacility)
{
    // HPC lines where facility has no dot (e.g. "action", "node", "gige")
    auto result{
        strategy.parse("437261 node-10 action start 1096995263 1 boot  (command 3169)", arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.component, "action.start");
    EXPECT_NE(pl.content.find("boot"), std::string::npos);
}

TEST_F(HPCStrategyTest, ParsesNonStandardNodeName)
{
    // Node names without dash pattern (e.g. "gige7", "full")
    auto result{strategy.parse("44937 gige7 gige temperature 1105776193 1 warning", arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result.value().component, "gige.temperature");
}

TEST_F(HPCStrategyTest, RejectsTooFewFields)
{
    EXPECT_FALSE(strategy.parse("134681 node-246 unix.hw", arena).has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// HealthApp — single-digit hour/second timestamps
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(HealthAppStrategyTest, ParsesSingleDigitHour)
{
    auto result{strategy.parse("20171224-0:32:28:806|HiH_|30002312|"
                               "initUserPrivacy the userPrivacy is true",
                               arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.component, "HiH_");
    EXPECT_NE(pl.content.find("initUserPrivacy"), std::string::npos);
}

TEST_F(HealthAppStrategyTest, ParsesSingleDigitHourAndSecond)
{
    auto result{strategy.parse("20171224-0:51:1:747|Step_LSC|30002312|processHandleBroadcastAction",
                               arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_TRUE(result.value().timestamp.has_value());
}

TEST_F(HealthAppStrategyTest, ConfidenceHighForSingleDigitHour)
{
    EXPECT_GT(strategy.confidence("20171224-0:32:28:806|HiH_|30002312|msg"), 0.5);
}

// ─────────────────────────────────────────────────────────────────────────────
// AndroidLogcat — Silent level
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(AndroidLogcatStrategyTest, ParsesSilentLevel)
{
    auto result{
        strategy.parse("03-17 16:13:38.811  1702  2395 S SilentTag: silent message", arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    EXPECT_EQ(result.value().level, LogLevel::Trace);
}

// ─────────────────────────────────────────────────────────────────────────────
// NginxErrorStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kNginxErrorLine =
    "2024/03/27 10:15:23 [error] 12345#0: *99 connect() failed "
    "(111: Connection refused) while connecting to upstream";

static constexpr std::string_view kNginxWarnLine{
    "2024/01/01 00:00:01 [warn] 1#0: conflicting server name \"example.com\""};

static constexpr std::string_view kNginxCritLine{
    "2023/12/31 23:59:59 [crit] 999#0: *1 SSL_do_handshake() failed"};

class NginxErrorStrategyTest : public ::testing::Test
{
  protected:
    NginxErrorStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(NginxErrorStrategyTest, ParsesErrorLine)
{
    auto result{strategy.parse(kNginxErrorLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Error);
    EXPECT_EQ(pl.component, "nginx");
    EXPECT_NE(pl.content.find("connect()"), std::string::npos);
}

TEST_F(NginxErrorStrategyTest, ParsesWarnLine)
{
    auto result{strategy.parse(kNginxWarnLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Warn);
}

TEST_F(NginxErrorStrategyTest, ParsesCritLine)
{
    auto result{strategy.parse(kNginxCritLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().timestamp.has_value());
    EXPECT_NE(result.value().content.find("SSL"), std::string::npos);
}

TEST_F(NginxErrorStrategyTest, ConfidenceHighForNginx)
{
    EXPECT_GT(strategy.confidence(kNginxErrorLine), 0.8);
}

TEST_F(NginxErrorStrategyTest, ConfidenceZeroForCLF)
{
    EXPECT_EQ(strategy.confidence(kCLFLine), 0.0);
}

TEST_F(NginxErrorStrategyTest, RejectsJSONLine)
{
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// RFC5424Strategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kRFC5424Line{
    "<134>1 2024-01-15T10:30:00.003Z server sshd 1234 ID47 - Accepted password for alice"};

static constexpr std::string_view kRFC5424WarnLine{
    "<132>1 2024-01-15T10:30:00Z router bgpd 555 - - BGP peer 10.0.0.1 down"};

static constexpr std::string_view kRFC5424NilFields =
    "<165>1 2024-06-01T12:00:00Z myhost myapp - - [exampleSDID@32473 iut=\"3\"] An application "
    "event";

class RFC5424StrategyTest : public ::testing::Test
{
  protected:
    RFC5424Strategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(RFC5424StrategyTest, ParsesStandardLine)
{
    auto result{strategy.parse(kRFC5424Line, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.component, "sshd");
    EXPECT_NE(pl.content.find("Accepted"), std::string::npos);
}

TEST_F(RFC5424StrategyTest, SeverityMappedFromPRI)
{
    // PRI=134 → facility=16, severity=6 → Info
    auto r1{strategy.parse(kRFC5424Line, arena)};
    ASSERT_TRUE(r1.has_value());
    EXPECT_EQ(r1.value().level, LogLevel::Info);

    // PRI=132 → severity=4 → Warn
    auto r2{strategy.parse(kRFC5424WarnLine, arena)};
    ASSERT_TRUE(r2.has_value());
    EXPECT_EQ(r2.value().level, LogLevel::Warn);
}

TEST_F(RFC5424StrategyTest, ParsesStructuredData)
{
    auto result{strategy.parse(kRFC5424NilFields, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_NE(result.value().content.find("application event"), std::string::npos);
}

TEST_F(RFC5424StrategyTest, ConfidenceHighForRFC5424)
{
    EXPECT_GT(strategy.confidence(kRFC5424Line), 0.9);
}

// ─────────────────────────────────────────────────────────────────────────────
// IISW3CStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kIISW3CLine =
    "2024-01-15 10:30:00 W3SVC1 SERVER01 GET /index.html - 80 - 10.0.0.1 "
    "Mozilla/5.0+(compatible) 200 0 0 15";

static constexpr std::string_view kIISW3CShortLine{
    "2024-01-15 10:30:00 GET /api/health - 200 0 0 5"};

static constexpr std::string_view kIISW3CPostLine =
    "2024-01-15 10:30:01 W3SVC2 WEBSVR POST /api/login - 443 admin 192.168.1.1 "
    "Mozilla/5.0 401 0 0 120";

static constexpr std::string_view kIISComment{
    "#Fields: date time s-sitename s-computername cs-method"};

class IISW3CStrategyTest : public ::testing::Test
{
  protected:
    IISW3CStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(IISW3CStrategyTest, ParsesFullLine)
{
    auto result{strategy.parse(kIISW3CLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.component, "SERVER01");
    EXPECT_NE(pl.content.find("GET"), std::string::npos);
    EXPECT_NE(pl.content.find("/index.html"), std::string::npos);
}

TEST_F(IISW3CStrategyTest, ParsesShortLine)
{
    auto result{strategy.parse(kIISW3CShortLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().component, "IIS");
}

TEST_F(IISW3CStrategyTest, Detects401AsWarn)
{
    auto result{strategy.parse(kIISW3CPostLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Warn);
}

TEST_F(IISW3CStrategyTest, RejectsCommentLines)
{
    EXPECT_FALSE(strategy.parse(kIISComment, arena).has_value());
}

TEST_F(IISW3CStrategyTest, ConfidenceHighForIIS)
{
    EXPECT_GT(strategy.confidence(kIISW3CLine), 0.8);
}

TEST_F(IISW3CStrategyTest, ConfidenceZeroForComment)
{
    EXPECT_EQ(strategy.confidence(kIISComment), 0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// CloudWatchStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kCloudWatchLine{
    R"({"timestamp":1705312200000,"message":"User login successful","logGroup":"/aws/lambda/myFunc","logStream":"2024/01/15/[$LATEST]abc123"})"};

static constexpr std::string_view kCloudWatchMinimal{
    R"({"message":"timeout","logGroup":"/aws/ecs/myService"})"};

class CloudWatchStrategyTest : public ::testing::Test
{
  protected:
    CloudWatchStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(CloudWatchStrategyTest, ParsesFullLine)
{
    auto result{strategy.parse(kCloudWatchLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.component, "/aws/lambda/myFunc");
    EXPECT_NE(pl.content.find("User login"), std::string::npos);
}

TEST_F(CloudWatchStrategyTest, ParsesMinimalLine)
{
    auto result{strategy.parse(kCloudWatchMinimal, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().content, "timeout");
    EXPECT_EQ(result.value().component, "/aws/ecs/myService");
}

TEST_F(CloudWatchStrategyTest, RejectsGenericJSON)
{
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

TEST_F(CloudWatchStrategyTest, ConfidenceHigherThanGenericJSON)
{
    EXPECT_GT(strategy.confidence(kCloudWatchLine), 1.0);
}

TEST_F(CloudWatchStrategyTest, ConfidenceZeroForPlainJSON)
{
    EXPECT_EQ(strategy.confidence(kJSONLine), 0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// SystemdJournalStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kSystemdJournalLine{
    R"({"__REALTIME_TIMESTAMP":"1705312200000000","PRIORITY":"6","_COMM":"nginx","MESSAGE":"Worker process started","_PID":"1234","_SYSTEMD_UNIT":"nginx.service"})"};

static constexpr std::string_view kSystemdJournalWarn{
    R"({"__REALTIME_TIMESTAMP":"1705312200000000","PRIORITY":"4","_COMM":"sshd","MESSAGE":"Failed password for invalid user"})"};

static constexpr std::string_view kSystemdJournalUnit{
    R"({"_SYSTEMD_UNIT":"docker.service","PRIORITY":"3","MESSAGE":"Container crashed","SYSLOG_IDENTIFIER":"dockerd"})"};

class SystemdJournalStrategyTest : public ::testing::Test
{
  protected:
    SystemdJournalStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(SystemdJournalStrategyTest, ParsesFullLine)
{
    auto result{strategy.parse(kSystemdJournalLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    EXPECT_EQ(pl.component, "nginx");
    EXPECT_NE(pl.content.find("Worker process"), std::string::npos);
}

TEST_F(SystemdJournalStrategyTest, MapsWarningPriority)
{
    auto result{strategy.parse(kSystemdJournalWarn, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Warn);
    EXPECT_EQ(result.value().component, "sshd");
}

TEST_F(SystemdJournalStrategyTest, FallsBackToSyslogIdentifier)
{
    auto result{strategy.parse(kSystemdJournalUnit, arena)};
    ASSERT_TRUE(result.has_value());
    // No _COMM, but has SYSLOG_IDENTIFIER
    EXPECT_EQ(result.value().component, "dockerd");
    EXPECT_EQ(result.value().level, LogLevel::Error);
}

TEST_F(SystemdJournalStrategyTest, RejectsGenericJSON)
{
    EXPECT_FALSE(strategy.parse(kJSONLine, arena).has_value());
}

TEST_F(SystemdJournalStrategyTest, ConfidenceHigherThanCloudWatch)
{
    EXPECT_GT(strategy.confidence(kSystemdJournalLine), 1.05);
}

TEST_F(SystemdJournalStrategyTest, ConfidenceZeroForPlainJSON)
{
    EXPECT_EQ(strategy.confidence(kJSONLine), 0.0);
}

// ─────────────────────────────────────────────────────────────────────────────
// RawTextStrategy — the last-resort catch-all for unstructured application stdout.
// Re-homes the canon raw-path invariants of insight-playground 09 T2
// (RawAppStdout.RawPathTemplatesAndInfersLevels): a bare leading level word is lifted
// (the dominant-level signal that lets Sift/Eidos rank a raw stream), while the prose
// body is kept WHOLE — no KV-steal, no fabricated WHERE — so it does not fragment the
// way the structured KV strategy would. These are single-component canon properties;
// the raw EMISSION format is a LogCraft formatter concern (proven in logcraft), and the
// end-to-end seam is contract fixture 09 — neither needs the full replay to prove this.
// ─────────────────────────────────────────────────────────────────────────────

class RawTextStrategyTest : public ::testing::Test
{
  protected:
    RawTextStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(RawTextStrategyTest, InfersLeadingErrorLevel)
{
    // The error class surfaces with the level inferred from the bare leading word —
    // only possible because canon took the raw path (no structured level field).
    auto result{strategy.parse("ERROR connection refused to db pool", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Error);
}

TEST_F(RawTextStrategyTest, InfersLeadingInfoLevel)
{
    auto result{strategy.parse("INFO checkout completed for order=ORD-4821", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().level, LogLevel::Info);
}

TEST_F(RawTextStrategyTest, KeepsProseWholeNoKvStealNoWhere)
{
    // The checkout prose carries a glued, ever-varying order id (order=ORD-<seq>). The
    // raw strategy must NOT steal it as a KV field or fabricate a component (the
    // WHERE-RAW boundary): the whole message stays the content (the masker templates it
    // downstream), so it does not fragment the way the structured KV strategy would.
    static constexpr std::string_view kProse{"checkout completed for order=ORD-4821 in 12 ms"};
    auto result{strategy.parse(kProse, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_EQ(pl.content, kProse) << "prose must survive whole — no KV-steal fragmentation";
    EXPECT_TRUE(pl.component.empty()) << "raw text carries no structured component (WHERE-RAW)";
    EXPECT_FALSE(pl.timestamp.has_value()) << "raw stdout carries no parsed timestamp";
}

TEST_F(RawTextStrategyTest, LeadingWhitespaceTrimmedForContinuationGrouping)
{
    // Indented continuation lines (e.g. traceback frames) left-trim so they group with
    // their peers — pure pointer arithmetic, no level word ⇒ the default level.
    auto result{strategy.parse("    at frame in module", arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().content, "at frame in module");
}

// ─────────────────────────────────────────────────────────────────────────────
// Table-driven families — the four cross-strategy claim families, folded from
// per-strategy copy-pasted TEST_F bodies into value-parameterized suites over one
// descriptor table. Each row records which families covered its strategy BEFORE
// the fold; the per-family row sets mirror that historical coverage exactly —
// the fold does not blanket-extend a family to strategies it never covered.
//
//   FormatReturns           — format() reports the row's LogFormat tag (all rows)
//   RawLinePreserved        — parse(canonical line) echoes raw_line verbatim
//   ProjectionIsTotal       — parse(canonical line) keeps message bytes in `content` (all rows)
//   RejectsCLFAndKV         — parse() fails on both the CLF and the KV sample
//   ConfidenceZeroForSyslog — confidence(BSD syslog sample) == 0.0 (RFC5424's
//                             identical assert was formerly ConfidenceZeroForBSD)
//
// `canonical_line` is now populated on EVERY row, which deliberately extends RawLinePreserved to
// the six strategies that had no row in it. The rule above bars an ACCIDENTAL blanket-extension
// during a mechanical fold; this one is chosen, and it is what makes ProjectionIsTotal — the
// DN-43.D6 design-time half of the invariant — total over every registered strategy rather than
// over the two branches DN-43 repairs. Each added line is one an existing per-strategy TEST_F
// already proves that strategy parses.
// ─────────────────────────────────────────────────────────────────────────────

struct StrategyCase
{
    // Instantiation label: failure output names the strategy ("…/Syslog").
    const char* name;
    std::unique_ptr<IFormatStrategy> (*make_strategy)();
    LogFormat format;
    // The canonical happy-path line this strategy CLAIMS — the input for RawLinePreserved and for
    // ProjectionIsTotal. Populated on every row.
    std::string_view canonical_line{};
    bool rejects_clf_and_kv{false};
    bool confidence_zero_for_syslog{false};
};

// gtest param printer: name only — the printout lands in ctest display names via test
// discovery, so it must stay clean. The failing input LINE is printed by each family's
// assertion message instead (verbose-on-failure without polluting the test listing).
void PrintTo(const StrategyCase& strategy_case, std::ostream* output_stream)
{
    *output_stream << strategy_case.name;
}

template <std::derived_from<IFormatStrategy> StrategyType>
[[nodiscard]] std::unique_ptr<IFormatStrategy> make_strategy_instance()
{
    return std::make_unique<StrategyType>();
}

static constexpr std::array kStrategyTable{
    StrategyCase{.name = "Syslog",
                 .make_strategy = make_strategy_instance<SyslogStrategy>,
                 .format = LogFormat::Syslog,
                 .canonical_line = kBSDLine},
    StrategyCase{.name = "Json",
                 .make_strategy = make_strategy_instance<JsonStrategy>,
                 .format = LogFormat::JSON,
                 .canonical_line = kJSONLine,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "KV",
                 .make_strategy = make_strategy_instance<KVStrategy>,
                 .format = LogFormat::KeyValue,
                 .canonical_line = kKVLine},
    StrategyCase{.name = "CLF",
                 .make_strategy = make_strategy_instance<CLFStrategy>,
                 .format = LogFormat::CLF,
                 .canonical_line = kCLFLine,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "Log4j",
                 .make_strategy = make_strategy_instance<Log4jStrategy>,
                 .format = LogFormat::Log4j,
                 .canonical_line = kLog4jHadoopLine,
                 .rejects_clf_and_kv = true,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "SparkHDFS",
                 .make_strategy = make_strategy_instance<SparkHDFSStrategy>,
                 .format = LogFormat::SparkHDFS,
                 .canonical_line = kSparkLine,
                 .rejects_clf_and_kv = true,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "BGL",
                 .make_strategy = make_strategy_instance<BGLStrategy>,
                 .format = LogFormat::BGL,
                 .canonical_line = kBGLLine,
                 .rejects_clf_and_kv = true,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "AndroidLogcat",
                 .make_strategy = make_strategy_instance<AndroidLogcatStrategy>,
                 .format = LogFormat::AndroidLogcat,
                 .canonical_line = kAndroidLine,
                 .rejects_clf_and_kv = true,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "ApacheError",
                 .make_strategy = make_strategy_instance<ApacheErrorLogStrategy>,
                 .format = LogFormat::ApacheError,
                 .canonical_line = kApacheErrorLine,
                 .rejects_clf_and_kv = true,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "WindowsCBS",
                 .make_strategy = make_strategy_instance<WindowsCBSStrategy>,
                 .format = LogFormat::WindowsCBS,
                 .canonical_line = kWindowsCBSLine,
                 .rejects_clf_and_kv = true,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "HealthApp",
                 .make_strategy = make_strategy_instance<HealthAppStrategy>,
                 .format = LogFormat::HealthApp,
                 .canonical_line = kHealthAppLine,
                 .rejects_clf_and_kv = true,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "Proxifier",
                 .make_strategy = make_strategy_instance<ProxifierStrategy>,
                 .format = LogFormat::Proxifier,
                 .canonical_line = kProxifierLine,
                 .rejects_clf_and_kv = true,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "HPC",
                 .make_strategy = make_strategy_instance<HPCStrategy>,
                 .format = LogFormat::HPC,
                 .canonical_line = kHPCLine,
                 .rejects_clf_and_kv = true,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "NginxError",
                 .make_strategy = make_strategy_instance<NginxErrorStrategy>,
                 .format = LogFormat::NginxError,
                 .canonical_line = kNginxErrorLine,
                 .rejects_clf_and_kv = true},
    StrategyCase{.name = "RFC5424",
                 .make_strategy = make_strategy_instance<RFC5424Strategy>,
                 .format = LogFormat::RFC5424,
                 .canonical_line = kRFC5424Line,
                 .rejects_clf_and_kv = true,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "IISW3C",
                 .make_strategy = make_strategy_instance<IISW3CStrategy>,
                 .format = LogFormat::IISW3C,
                 .canonical_line = kIISW3CLine,
                 .rejects_clf_and_kv = true},
    StrategyCase{.name = "CloudWatch",
                 .make_strategy = make_strategy_instance<CloudWatchStrategy>,
                 .format = LogFormat::CloudWatch,
                 .canonical_line = kCloudWatchLine,
                 .rejects_clf_and_kv = true},
    StrategyCase{.name = "SystemdJournal",
                 .make_strategy = make_strategy_instance<SystemdJournalStrategy>,
                 .format = LogFormat::SystemdJournal,
                 .canonical_line = kSystemdJournalLine,
                 .rejects_clf_and_kv = true},
    StrategyCase{.name = "Rfc3339Text",
                 .make_strategy = make_strategy_instance<Rfc3339TextStrategy>,
                 .format = LogFormat::Rfc3339Text,
                 .canonical_line = kRfc3339AppLine,
                 .rejects_clf_and_kv = true,
                 .confidence_zero_for_syslog = true},
    StrategyCase{.name = "RawText",
                 .make_strategy = make_strategy_instance<RawTextStrategy>,
                 .format = LogFormat::RawText,
                 .canonical_line = "ERROR build failed after 3 retries"},
};

// The adaptors are spelled as function calls, not `operator|`. Reaching the pipe operator through
// `import insight.canon.test` trips a gcc-15 BMI defect: the CRTP `_Derived` of the first closure
// type in the TU leaks into every later `_Partial`, so the second and third helpers fail to deduce
// (`use of operator| ... before deduction of auto`). clang-21/libc++ accepts the pipe form, so this
// only ever breaks on the ship toolchain.
[[nodiscard]] std::vector<StrategyCase> rows_with_canonical_line()
{
    return std::ranges::to<std::vector>(
        std::views::filter(kStrategyTable, [](const StrategyCase& table_row)
                           { return !table_row.canonical_line.empty(); }));
}

[[nodiscard]] std::vector<StrategyCase> rows_rejecting_clf_and_kv()
{
    return std::ranges::to<std::vector>(
        std::views::filter(kStrategyTable, [](const StrategyCase& table_row)
                           { return table_row.rejects_clf_and_kv; }));
}

[[nodiscard]] std::vector<StrategyCase> rows_with_zero_syslog_confidence()
{
    return std::ranges::to<std::vector>(
        std::views::filter(kStrategyTable, [](const StrategyCase& table_row)
                           { return table_row.confidence_zero_for_syslog; }));
}

[[nodiscard]] std::string
strategy_case_name(const ::testing::TestParamInfo<StrategyCase>& param_info)
{
    return param_info.param.name;
}

// Shared param fixture: builds the row's strategy through the table factory. Families
// that parse allocate a local arena in the body — the format/confidence families need
// none, so the fixture does not carry one.
class StrategyTableTest : public ::testing::TestWithParam<StrategyCase>
{
  protected:
    std::unique_ptr<IFormatStrategy> strategy{GetParam().make_strategy()};
};

class FormatReturns : public StrategyTableTest
{
};
class RawLinePreserved : public StrategyTableTest
{
};
class ProjectionIsTotal : public StrategyTableTest
{
};
class RejectsCLFAndKV : public StrategyTableTest
{
};
class ConfidenceZeroForSyslog : public StrategyTableTest
{
};

TEST_P(FormatReturns, DeclaredFormatMatchesRow)
{
    EXPECT_EQ(strategy->format(), GetParam().format) << "strategy=" << GetParam().name;
}

TEST_P(RawLinePreserved, CanonicalLineEchoedVerbatim)
{
    ArenaAllocator arena{4096};
    const std::string_view canonical_line{GetParam().canonical_line};
    auto result{strategy->parse(canonical_line, arena)};
    ASSERT_TRUE(result.has_value())
        << "strategy=" << GetParam().name << " failed to parse canonical line: " << canonical_line
        << " error: " << result.error();
    EXPECT_EQ(result.value().raw_line, canonical_line) << "strategy=" << GetParam().name;
}

// DN-43.D6, the design-time half. `ParsedLine::content` is a TOTAL projection: `content.empty()`
// implies the line has no message bytes beyond the header the strategy parsed. Checked here for
// EVERY registered strategy — the invariant is the SPI's, not Syslog's, and the failure it guards
// against is invisible downstream (an emptied content templates to the SHA-256 prefix of the empty
// string, a universal collision bucket published as an ordinary identity).
TEST_P(ProjectionIsTotal, ClaimedLineKeepsItsMessageBytes)
{
    ArenaAllocator arena{4096};
    const std::string_view canonical_line{GetParam().canonical_line};
    auto result{strategy->parse(canonical_line, arena)};
    ASSERT_TRUE(result.has_value()) << "strategy=" << GetParam().name
                                    << " failed to parse its own canonical line: " << canonical_line
                                    << " error: " << result.error();
    EXPECT_FALSE(result.value().content.empty())
        << "strategy=" << GetParam().name
        << " emptied content on a line that has message bytes; input: " << canonical_line
        << " component=\"" << result.value().component << "\"";
}

TEST_P(RejectsCLFAndKV, ParseFailsOnBothForeignSamples)
{
    ArenaAllocator arena{4096};
    EXPECT_FALSE(strategy->parse(kCLFLine, arena).has_value())
        << "strategy=" << GetParam().name << " unexpectedly parsed the CLF sample: " << kCLFLine;
    EXPECT_FALSE(strategy->parse(kKVLine, arena).has_value())
        << "strategy=" << GetParam().name << " unexpectedly parsed the KV sample: " << kKVLine;
}

TEST_P(ConfidenceZeroForSyslog, BSDSampleScoresZero)
{
    EXPECT_EQ(strategy->confidence(kBSDLine), 0.0)
        << "strategy=" << GetParam().name << " input: " << kBSDLine;
}

INSTANTIATE_TEST_SUITE_P(Strategies, FormatReturns, ::testing::ValuesIn(kStrategyTable),
                         strategy_case_name);
INSTANTIATE_TEST_SUITE_P(Strategies, RawLinePreserved,
                         ::testing::ValuesIn(rows_with_canonical_line()), strategy_case_name);
INSTANTIATE_TEST_SUITE_P(Strategies, ProjectionIsTotal, ::testing::ValuesIn(kStrategyTable),
                         strategy_case_name);
INSTANTIATE_TEST_SUITE_P(Strategies, RejectsCLFAndKV,
                         ::testing::ValuesIn(rows_rejecting_clf_and_kv()), strategy_case_name);
INSTANTIATE_TEST_SUITE_P(Strategies, ConfidenceZeroForSyslog,
                         ::testing::ValuesIn(rows_with_zero_syslog_confidence()),
                         strategy_case_name);
