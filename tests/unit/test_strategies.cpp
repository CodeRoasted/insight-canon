// NOLINTBEGIN : Unit tests may intentionally violate some style rules for clarity or simplicity.
// Unit tests: allow short identifiers and test-specific patterns
// tests/1_tokenization/test_strategies.cpp
//
// Unit tests for the eighteen IFormatStrategy implementations:
//   SyslogStrategy, JsonStrategy, KVStrategy, CLFStrategy,
//   Log4jStrategy, SparkHDFSStrategy, BGLStrategy, AndroidLogcatStrategy,
//   ApacheErrorLogStrategy, WindowsCBSStrategy, HealthAppStrategy,
//   ProxifierStrategy, HPCStrategy, NginxErrorStrategy, RFC5424Strategy,
//   IISW3CStrategy, CloudWatchStrategy, SystemdJournalStrategy.

#include <gtest/gtest.h>
#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/strategies/android_logcat.hpp"
#include "insight/tokenization/strategies/apache_error.hpp"
#include "insight/tokenization/strategies/bgl.hpp"
#include "insight/tokenization/strategies/clf.hpp"
#include "insight/tokenization/strategies/cloudwatch.hpp"
#include "insight/tokenization/strategies/github_actions.hpp"
#include "insight/tokenization/strategies/health_app.hpp"
#include "insight/tokenization/strategies/hpc.hpp"
#include "insight/tokenization/strategies/iis_w3c.hpp"
#include "insight/tokenization/strategies/json.hpp"
#include "insight/tokenization/strategies/kv.hpp"
#include "insight/tokenization/strategies/log4j.hpp"
#include "insight/tokenization/strategies/nginx_error.hpp"
#include "insight/tokenization/strategies/proxifier.hpp"
#include "insight/tokenization/strategies/rfc5424.hpp"
#include "insight/tokenization/strategies/spark_hdfs.hpp"
#include "insight/tokenization/strategies/syslog.hpp"
#include "insight/tokenization/strategies/systemd_journal.hpp"
#include "insight/tokenization/strategies/windows_cbs.hpp"

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

TEST_F(SyslogStrategyTest, FormatReturnsSyslog)
{
    EXPECT_EQ(strategy.format(), LogFormat::Syslog);
}

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

TEST_F(SyslogStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kBSDLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kBSDLine);
}

// ─────────────────────────────────────────────────────────────────────────────
// GitHubActionsStrategy
// ─────────────────────────────────────────────────────────────────────────────

static constexpr std::string_view kGHALine{
    "2026-05-27T15:26:41.7842152Z   CODEROAST_IPC_REPO: CodeRoasted/coderoast-ipc"};

static constexpr std::string_view kGHAError{
    "2026-05-27T15:26:41.7842152Z ##[error]connection refused to db host 10.0.0.5"};

// Bare, UNMARKED bodies (an INFO line carries no ##[…] marker). The strategy must
// infer the level from the body's leading-level / failure cue — the level-escaping
// crash path. Only the 2-token "segmentation fault" adjacency is a cue; the two
// distractors carry "segmentation"/"fault" as NON-adjacent words and stay benign.
static constexpr std::string_view kGHASegfault{
    "2026-05-27T15:26:41.7842152Z Segmentation fault (core dumped)"};
static constexpr std::string_view kGHASegPipe{
    "2026-05-27T15:26:41.7842152Z image segmentation pipeline complete"};
static constexpr std::string_view kGHAPageFault{
    "2026-05-27T15:26:41.7842152Z page fault handler registered"};

// GHA stamps a timestamp on blank output lines too — with and without a
// trailing separator space. Both are blank lines and must be declined.
static constexpr std::string_view kGHABlankWithSpace{"2026-05-27T15:26:41.7842152Z "};
static constexpr std::string_view kGHABlankNoSpace{"2026-05-27T15:26:41.7842152Z"};

class GitHubActionsStrategyTest : public ::testing::Test
{
  protected:
    GitHubActionsStrategy strategy;
    ArenaAllocator arena{4096};
};

TEST_F(GitHubActionsStrategyTest, FormatReturnsGitHubActions)
{
    EXPECT_EQ(strategy.format(), LogFormat::GitHubActions);
}

TEST_F(GitHubActionsStrategyTest, StripsTimestampAndTemplatesRealContent)
{
    auto result{strategy.parse(kGHALine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    // The whole message survives (no token eaten as a fake hostname), leading
    // GHA indentation stripped.
    EXPECT_EQ(pl.content, "CODEROAST_IPC_REPO: CodeRoasted/coderoast-ipc");
}

TEST_F(GitHubActionsStrategyTest, LiftsErrorLevelFromWorkflowCommand)
{
    auto result{strategy.parse(kGHAError, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_EQ(pl.level, LogLevel::Error);
    // The marker stays in the templated content (it is part of the line shape).
    EXPECT_TRUE(pl.content.starts_with("##[error]"));
}

// A bare, UNMARKED GHA body is effectively raw stdout. The strategy must fall back
// to failure-cue inference so a level-escaping OS/shell crash is still recovered as
// Error — while benign lines that merely contain "segmentation"/"fault" as NON-
// adjacent words stay Unknown (the 2-token "segmentation fault" lexicon adjacency).
TEST_F(GitHubActionsStrategyTest, InfersErrorFromBodyCueWhenUnmarked)
{
    auto crash{strategy.parse(kGHASegfault, arena)};
    ASSERT_TRUE(crash.has_value());
    EXPECT_EQ(crash.value().level, LogLevel::Error)
        << "bare 'Segmentation fault (core dumped)' (no ##[error] marker) escalates via the "
           "lexicon";
    EXPECT_EQ(crash.value().content, "Segmentation fault (core dumped)")
        << "body templated bare (no marker), timestamp stripped";

    auto seg_pipe{strategy.parse(kGHASegPipe, arena)};
    ASSERT_TRUE(seg_pipe.has_value());
    EXPECT_EQ(seg_pipe.value().level, LogLevel::Unknown)
        << "'image segmentation pipeline complete' — 'segmentation' is not adjacent to 'fault'";

    auto page_fault{strategy.parse(kGHAPageFault, arena)};
    ASSERT_TRUE(page_fault.has_value());
    EXPECT_EQ(page_fault.value().level, LogLevel::Unknown)
        << "'page fault handler registered' — bare 'fault' / 'page fault' is not the cue phrase";
}

TEST_F(GitHubActionsStrategyTest, DeclinesTimestampOnlyBlankLines)
{
    // A timestamp with no message is a blank line: decline so it is dropped,
    // never collapsing into an empty "" template (the bug this strategy fixes).
    EXPECT_FALSE(strategy.parse(kGHABlankWithSpace, arena).has_value());
    EXPECT_FALSE(strategy.parse(kGHABlankNoSpace, arena).has_value());
}

TEST_F(GitHubActionsStrategyTest, ConfidenceHighForGHAShape)
{
    EXPECT_GT(strategy.confidence(kGHALine), 0.85);
}

TEST_F(GitHubActionsStrategyTest, ConfidenceZeroForWholeSecondRFC3339)
{
    // Real RFC3339 syslog (no 7-digit fraction) is NOT GHA.
    EXPECT_EQ(strategy.confidence(kRFC3339Line), 0.0);
}

TEST_F(GitHubActionsStrategyTest, ConfidenceZeroForJSON)
{
    EXPECT_EQ(strategy.confidence(kJSONLine), 0.0);
}

TEST_F(GitHubActionsStrategyTest, OutranksSyslogOnGHALines)
{
    // The crux of the fix: GHA must beat Syslog's RFC3339 claim on its own lines.
    const SyslogStrategy syslog;
    EXPECT_GT(strategy.confidence(kGHALine), syslog.confidence(kGHALine));
}

TEST_F(GitHubActionsStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kGHALine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kGHALine);
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

TEST_F(JsonStrategyTest, FormatReturnsJSON)
{
    EXPECT_EQ(strategy.format(), LogFormat::JSON);
}

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

TEST_F(JsonStrategyTest, ConfidenceZeroForSyslog)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
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

TEST_F(JsonStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kJSONLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kJSONLine);
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

TEST_F(KVStrategyTest, FormatReturnsKeyValue)
{
    EXPECT_EQ(strategy.format(), LogFormat::KeyValue);
}

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

TEST_F(KVStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kKVLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kKVLine);
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

TEST_F(CLFStrategyTest, FormatReturnsCLF)
{
    EXPECT_EQ(strategy.format(), LogFormat::CLF);
}

TEST_F(CLFStrategyTest, ParsesCommonLogFormat)
{
    auto result{strategy.parse(kCLFLine, arena)};
    ASSERT_TRUE(result.has_value());
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    EXPECT_EQ(pl.component, "127.0.0.1");
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

TEST_F(CLFStrategyTest, ConfidenceZeroForSyslog)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
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

TEST_F(CLFStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kCLFLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kCLFLine);
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

TEST_F(Log4jStrategyTest, FormatReturnsLog4j)
{
    EXPECT_EQ(strategy.format(), LogFormat::Log4j);
}

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

TEST_F(Log4jStrategyTest, ConfidenceZeroForSyslog)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
}

TEST_F(Log4jStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kLog4jHadoopLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kLog4jHadoopLine);
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

TEST_F(SparkHDFSStrategyTest, FormatReturnsSparkHDFS)
{
    EXPECT_EQ(strategy.format(), LogFormat::SparkHDFS);
}

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

TEST_F(SparkHDFSStrategyTest, ConfidenceZeroForSyslog)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
}

TEST_F(SparkHDFSStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kSparkLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kSparkLine);
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

TEST_F(BGLStrategyTest, FormatReturnsBGL)
{
    EXPECT_EQ(strategy.format(), LogFormat::BGL);
}

TEST_F(BGLStrategyTest, ParsesBGLLine)
{
    auto result{strategy.parse(kBGLLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.level, LogLevel::Info);
    EXPECT_NE(pl.content.find("instruction cache parity error"), std::string::npos);
}

TEST_F(BGLStrategyTest, ParsesThunderbirdLine)
{
    auto result{strategy.parse(kThunderbirdLine, arena)};
    ASSERT_TRUE(result.has_value()) << result.error();
    const auto& pl{result.value()};
    EXPECT_TRUE(pl.timestamp.has_value());
    EXPECT_EQ(pl.component, "dn228");
    EXPECT_NE(pl.content.find("crond"), std::string::npos);
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

TEST_F(BGLStrategyTest, ConfidenceZeroForSyslog)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
}

TEST_F(BGLStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kBGLLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kBGLLine);
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

TEST_F(AndroidLogcatStrategyTest, FormatReturnsAndroidLogcat)
{
    EXPECT_EQ(strategy.format(), LogFormat::AndroidLogcat);
}

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

TEST_F(AndroidLogcatStrategyTest, ConfidenceZeroForSyslog)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
}

TEST_F(AndroidLogcatStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kAndroidLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kAndroidLine);
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

TEST_F(ApacheErrorLogStrategyTest, FormatReturnsApacheError)
{
    EXPECT_EQ(strategy.format(), LogFormat::ApacheError);
}

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

TEST_F(ApacheErrorLogStrategyTest, ConfidenceZeroForSyslog)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
}

TEST_F(ApacheErrorLogStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kApacheErrorLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kApacheErrorLine);
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

TEST_F(WindowsCBSStrategyTest, FormatReturnsWindowsCBS)
{
    EXPECT_EQ(strategy.format(), LogFormat::WindowsCBS);
}

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

TEST_F(WindowsCBSStrategyTest, ConfidenceZeroForSyslog)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
}

TEST_F(WindowsCBSStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kWindowsCBSLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kWindowsCBSLine);
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

TEST_F(HealthAppStrategyTest, FormatReturnsHealthApp)
{
    EXPECT_EQ(strategy.format(), LogFormat::HealthApp);
}

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

TEST_F(HealthAppStrategyTest, ConfidenceZeroForSyslog)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
}

TEST_F(HealthAppStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kHealthAppLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kHealthAppLine);
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

TEST_F(ProxifierStrategyTest, FormatReturnsProxifier)
{
    EXPECT_EQ(strategy.format(), LogFormat::Proxifier);
}

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

TEST_F(ProxifierStrategyTest, ConfidenceZeroForSyslog)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
}

TEST_F(ProxifierStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kProxifierLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kProxifierLine);
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

TEST_F(HPCStrategyTest, FormatReturnsHPC)
{
    EXPECT_EQ(strategy.format(), LogFormat::HPC);
}

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

TEST_F(HPCStrategyTest, ConfidenceZeroForSyslog)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
}

TEST_F(HPCStrategyTest, RawLinePreserved)
{
    auto result{strategy.parse(kHPCLine, arena)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value().raw_line, kHPCLine);
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
// Cross-format rejection tests (new strategies reject all other major formats)
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(Log4jStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
}

TEST_F(Log4jStrategyTest, ConfidenceZeroForJSON)
{
    EXPECT_EQ(strategy.confidence(kJSONLine), 0.0);
}

TEST_F(SparkHDFSStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
}

TEST_F(BGLStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
}

TEST_F(AndroidLogcatStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
}

TEST_F(ApacheErrorLogStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
}

TEST_F(WindowsCBSStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
}

TEST_F(HealthAppStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
}

TEST_F(ProxifierStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
}

TEST_F(HPCStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
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

TEST_F(NginxErrorStrategyTest, FormatReturnsNginxError)
{
    EXPECT_EQ(strategy.format(), LogFormat::NginxError);
}

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

TEST_F(NginxErrorStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
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

TEST_F(RFC5424StrategyTest, FormatReturnsRFC5424)
{
    EXPECT_EQ(strategy.format(), LogFormat::RFC5424);
}

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

TEST_F(RFC5424StrategyTest, ConfidenceZeroForBSD)
{
    EXPECT_EQ(strategy.confidence(kBSDLine), 0.0);
}

TEST_F(RFC5424StrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
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

TEST_F(IISW3CStrategyTest, FormatReturnsIISW3C)
{
    EXPECT_EQ(strategy.format(), LogFormat::IISW3C);
}

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

TEST_F(IISW3CStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
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

TEST_F(CloudWatchStrategyTest, FormatReturnsCloudWatch)
{
    EXPECT_EQ(strategy.format(), LogFormat::CloudWatch);
}

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

TEST_F(CloudWatchStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
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

TEST_F(SystemdJournalStrategyTest, FormatReturnsSystemdJournal)
{
    EXPECT_EQ(strategy.format(), LogFormat::SystemdJournal);
}

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

TEST_F(SystemdJournalStrategyTest, RejectsCLFAndKV)
{
    EXPECT_FALSE(strategy.parse(kCLFLine, arena).has_value());
    EXPECT_FALSE(strategy.parse(kKVLine, arena).has_value());
}

// NOLINTEND : Unit tests may intentionally violate some style rules for clarity or simplicity.
