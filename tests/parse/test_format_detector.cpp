// NOLINTBEGIN
// Unit tests: allow short identifiers and test-specific patterns
// tests/1_tokenization/test_format_detector.cpp
//
// Unit tests for FormatDetector: strategy registration, single-line detection,
// and batch-weighted detection.

#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

// ─────────────────────────────────────────────────────────────────────────────
// Fixture
// ─────────────────────────────────────────────────────────────────────────────

class FormatDetectorTest : public ::testing::Test
{
  protected:
    FormatDetector detector; // constructor auto-registers JSON, Syslog, CLF, KV
};

// ─────────────────────────────────────────────────────────────────────────────
// Strategy registration
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FormatDetectorTest, HasNineteenBuiltInStrategies)
{
    EXPECT_EQ(detector.strategies().size(), 19u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Single-line detection
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FormatDetectorTest, DetectsJSON)
{
    auto* s{detector.detect(R"({"level":"INFO","message":"hello"})")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::JSON);
}

TEST_F(FormatDetectorTest, DetectsBSDSyslog)
{
    auto* s{detector.detect("Jan 15 08:03:22 myhost sshd[1234]: Accepted password")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::Syslog);
}

TEST_F(FormatDetectorTest, DetectsRFC3339Syslog)
{
    auto* s{detector.detect("2024-01-15T10:30:00Z myhost app[42]: User logged in")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::Syslog);
}

TEST_F(FormatDetectorTest, DetectsGitHubActions)
{
    // GHA stamps an RFC3339 + 7-digit-fractional 'Z' prefix on every line. It
    // must win over Syslog (which shares the bare RFC3339 prefix) on this shape.
    auto* s{detector.detect(
        "2026-05-27T15:26:41.7842152Z   CODEROAST_IPC_REPO: CodeRoasted/coderoast-ipc")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::GitHubActions);
}

TEST_F(FormatDetectorTest, GitHubActionsDoesNotStealRFC3339Syslog)
{
    // Whole-second RFC3339 (no 7-digit fraction) is real syslog, not GHA.
    auto* s{detector.detect("2024-01-15T10:30:00Z myhost app[42]: User logged in")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::Syslog);
}

TEST_F(FormatDetectorTest, DetectsCLF)
{
    auto* s{detector.detect(
        R"(127.0.0.1 - frank [10/Oct/2000:13:55:36 -0700] "GET /index.html HTTP/1.0" 200 2326)")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::CLF);
}

TEST_F(FormatDetectorTest, DetectsKV)
{
    auto* s{detector.detect("ts=2024-01-15T10:30:00Z level=INFO component=auth msg=login")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::KeyValue);
}

TEST_F(FormatDetectorTest, ReturnsRawTextForGarbageLine)
{
    // Non-empty unstructured text is templated as raw, never dropped.
    auto* s{detector.detect("???")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::RawText);
}

TEST_F(FormatDetectorTest, ReturnsNullForEmptyLine)
{
    // Empty / whitespace-only lines stay dropped (the raw fallback skips them).
    EXPECT_EQ(detector.detect(""), nullptr);
    EXPECT_EQ(detector.detect("   "), nullptr);
}

// ─────────────────────────────────────────────────────────────────────────────
// Batch detection
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FormatDetectorTest, BatchDetectsJSON)
{
    const std::vector<std::string_view> batch = {
        R"({"msg":"one"})",
        R"({"msg":"two"})",
        R"({"msg":"three"})",
    };
    auto* s{detector.detect_from_batch(batch)};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::JSON);
}

TEST_F(FormatDetectorTest, BatchDetectsSyslog)
{
    const std::vector<std::string_view> batch = {
        "Jan 15 08:03:22 host sshd[1]: line one",
        "Jan 15 08:03:23 host sshd[1]: line two",
        "Jan 15 08:03:24 host sshd[1]: line three",
    };
    auto* s{detector.detect_from_batch(batch)};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::Syslog);
}

TEST_F(FormatDetectorTest, BatchReturnsNullForEmpty)
{
    const std::vector<std::string_view> batch;
    EXPECT_EQ(detector.detect_from_batch(batch), nullptr);
}

TEST_F(FormatDetectorTest, BatchHandlesMixedFormats)
{
    // Majority JSON (2/3) should win.
    const std::vector<std::string_view> batch = {
        R"({"msg":"json line 1"})",
        "Jan 15 08:03:22 host sshd[1]: syslog line",
        R"({"msg":"json line 2"})",
    };
    auto* s{detector.detect_from_batch(batch)};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::JSON);
}

// ─────────────────────────────────────────────────────────────────────────────
// Confidence resolution — near-tie / ambiguous lines
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FormatDetectorTest, NearTieKVBeatsWeakCLF)
{
    // This line triggers CLF's weak confidence (0.60 — bracket timestamp
    // present) AND KV's high confidence (0.90 — ≥ 3 pairs).
    // detect() must return KV because 0.90 > 0.60.
    constexpr std::string_view line{
        "level=INFO msg=test component=api [15/Jan/2024:10:30:00 flag=on"};
    auto* s{detector.detect(line)};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::KeyValue);
}

TEST_F(FormatDetectorTest, JSONAlwaysBeatsSyslogOnObjectLine)
{
    // A JSON object starting with '{' gives JSON confidence 1.0; BSD syslog
    // confidence is at most 0.85. JSON must always win for any valid JSON line.
    auto* s{detector.detect(R"({"ts":"2024-01-15T10:30:00Z","level":"INFO","msg":"hi"})")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::JSON);
}

// ─────────────────────────────────────────────────────────────────────────────
// New strategy detection
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FormatDetectorTest, DetectsLog4j)
{
    auto* s{detector.detect("2015-10-18 18:01:47,978 INFO [main] "
                            "org.apache.hadoop.mapreduce: Created MRAppMaster")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::Log4j);
}

TEST_F(FormatDetectorTest, DetectsSparkHDFS)
{
    auto* s{detector.detect("17/06/09 20:10:40 INFO executor.CoarseGrainedExecutorBackend: "
                            "Registered signal handlers")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::SparkHDFS);
}

TEST_F(FormatDetectorTest, DetectsBGL)
{
    auto* s{detector.detect("- 1117838570 2005.06.03 R02-M1-N0 2005-06-03-15.42.50 R02-M1-N0 "
                            "RAS KERNEL INFO instruction cache parity error corrected")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::BGL);
}

TEST_F(FormatDetectorTest, DetectsAndroidLogcat)
{
    auto* s{detector.detect("03-17 16:13:38.859  2227  2227 D TextView: visible is system.time")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::AndroidLogcat);
}

TEST_F(FormatDetectorTest, DetectsApacheError)
{
    auto* s{detector.detect("[Sun Dec 04 04:47:44 2005] [notice] workerEnv.init() ok")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::ApacheError);
}

TEST_F(FormatDetectorTest, DetectsWindowsCBS)
{
    auto* s{detector.detect("2016-09-28 04:30:30, Info                  CBS    "
                            "Loaded Servicing Stack")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::WindowsCBS);
}

TEST_F(FormatDetectorTest, DetectsHealthApp)
{
    auto* s{detector.detect("20171223-22:15:29:606|Step_LSC|30002312|onStandStepChanged 3579")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::HealthApp);
}

TEST_F(FormatDetectorTest, DetectsProxifier)
{
    auto* s{detector.detect("[10.30 16:49:06] chrome.exe - "
                            "proxy.cse.cuhk.edu.hk:5070 open through proxy")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::Proxifier);
}

TEST_F(FormatDetectorTest, DetectsHPC)
{
    auto* s{detector.detect("134681 node-246 unix.hw state_change.unavailable "
                            "1077804742 1 Component State Change")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::HPC);
}

// NOLINTEND
