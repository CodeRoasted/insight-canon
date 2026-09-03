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
    // Degenerate (zero-package) composition: the detector registers its 18 core
    // REPRESENTATION-format strategies (the GitHub-Actions DIALECT strategy is no longer
    // a builtin; it arrives via the composition, so GHA detection is now a github-package property,
    // tested in that suite).
    FormatDetector detector{insight::test_support::degenerate_composition()};
};

// ─────────────────────────────────────────────────────────────────────────────
// Strategy registration
// ─────────────────────────────────────────────────────────────────────────────

TEST_F(FormatDetectorTest, HasNineteenBuiltInRepresentationStrategies)
{
    // 19 core representation strategies (dialect strategies register into custom_strategies_, not
    // here). 18 until DN-43.D4 split the leading-RFC-3339 LAYOUT out of Syslog.
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

// DN-43.D3 clause 1: the routing, not just the parse. A level word where a hostname belongs is what
// made SyslogStrategy claim application lines, and a test that only checked the parse could not
// tell the gate from the grammar.
TEST_F(FormatDetectorTest, RoutesRfc3339AppLineToRfc3339TextNotSyslog)
{
    auto* s{detector.detect("2026-05-31T08:00:01Z INFO request method=GET path=/api/users/1000")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::Rfc3339Text)
        << "routed to " << to_string(s->format())
        << "; a level word must never be consumed as a syslog hostname";
}

// The mirror arm: the predicates are DISJOINT, so the same prefix still reaches Syslog when the
// header is genuinely there. A test that only had the arm above would pass on a bare rejection.
TEST_F(FormatDetectorTest, RoutesRfc3339SyslogLineToSyslogNotRfc3339Text)
{
    auto* s{detector.detect("2026-05-31T08:00:01Z web01 nginx[2451]: GET /api/users 200")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::Syslog) << "routed to " << to_string(s->format());
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

// The alert-label column routes exactly as `-` does (DN-43.D14). Before it did not: the candidate
// list was gated on a leading `-`, so 348 460 lines of the pinned BGL corpus were never offered a
// BGL probe at all and fell to the raw-text fallback with their declared FATAL unread.
TEST_F(FormatDetectorTest, DetectsAnAlertLabelledBGLLine)
{
    auto* s{detector.detect("KERNDTLB 1117838570 2005.06.03 R02-M1-N0 2005-06-03-15.42.50 "
                            "R02-M1-N0 RAS KERNEL FATAL data TLB error interrupt")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::BGL);
}

// The uppercase arm of the candidate gate must not start claiming BSD syslog: the label alphabet
// has no lowercase, so a month name fails it at the second byte.
TEST_F(FormatDetectorTest, AnUppercaseLeadingSyslogLineStillRoutesToSyslog)
{
    auto* s{detector.detect("Jun 14 15:16:01 combo sshd[19939]: authentication failure")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::Syslog);
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

// DN-43.D16 — a HealthApp head with too few separators is DEMOTED, and demotion keeps the bytes.
//
// The two arms below assert `content`, not merely the routed format, because an arm that checks
// only the format is satisfied by the defective code too: before this rule both lines routed to
// LogFormat::HealthApp and both published an EMPTY `content`. At one separator the second
// sv_take_until found no delimiter and returned the whole remainder, so the message body was
// published as `component` — high-cardinality free text on an identity field. At two separators
// the process-id skip consumed the body outright and it reached NO projection field at all.
//
// The seat matters and it is the ruling: the fix moves the separator count into
// is_health_app_prefix rather than adding a guard to parse(). A parse()-side decline is a
// DELETION — LogParser::parse_line increments failed_count_ and returns std::unexpected, so the
// line yields no event of any kind. A confidence() of 0.0 is a DEMOTION — FormatDetector::detect
// falls back to RawTextStrategy whenever best_score is 0.0, and RawTextStrategy::parse puts the
// whole line in `content`. Both arms therefore end on the same assertion: every byte survives.
TEST_F(FormatDetectorTest, HealthAppHeadWithTooFewSeparatorsDemotesToRawTextKeepingEveryByte)
{
    ArenaAllocator arena{4096};

    struct Case
    {
        std::string_view line;
        std::string_view why;
    };
    const std::vector<Case> cases{
        Case{.line = "20171223-22:15:29:606|onStandStepChanged 3579",
             .why = "one separator: parse() would publish the message body as `component`"},
        Case{.line = "20171223-22:15:29:606|Step_LSC|onStandStepChanged 3579",
             .why = "two separators: the process-id skip would swallow the message body"}};

    for (const auto& tc : cases)
    {
        auto* strategy{detector.detect(tc.line)};
        ASSERT_NE(strategy, nullptr) << tc.why;
        EXPECT_EQ(strategy->format(), LogFormat::RawText)
            << "routed to " << to_string(strategy->format()) << ", expected RawText — " << tc.why
            << "\n  line: " << tc.line;

        auto parsed{strategy->parse(tc.line, arena)};
        ASSERT_TRUE(parsed.has_value()) << parsed.error() << " — " << tc.why;
        EXPECT_EQ(parsed.value().content, tc.line)
            << "the demotion lost bytes.\n  expected content: " << tc.line
            << "\n  actual content:   " << parsed.value().content << "\n  " << tc.why;
    }
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
