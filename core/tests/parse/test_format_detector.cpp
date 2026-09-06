
// invariant: unit coverage for the format detector — strategy registration, single-line detection
// and batch-weighted detection.
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

class FormatDetectorTest : public ::testing::Test
{
  protected:
    // invariant: the DEGENERATE zero-package composition, so the detector registers only its core
    // REPRESENTATION-format strategies.
    // invariant: the dialect strategy is no longer a builtin — it arrives through the
    // composition, so its detection is that package's property and is tested in that suite.
    FormatDetector detector{insight::test_support::degenerate_composition()};
};

TEST_F(FormatDetectorTest, HasNineteenBuiltInRepresentationStrategies)
{
    // invariant: dialect strategies register into the CUSTOM set and not here.
    // invariant: the count was one lower until the leading-timestamp LAYOUT was split out of the
    // syslog strategy.
    // refs: DN-43.D4
    EXPECT_EQ(detector.strategies().size(), 19u);
}

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

// invariant: the ROUTING, not just the parse — a level word where a hostname belongs is what made
// the syslog strategy claim application lines.
// invariant: a test that only checked the PARSE could not tell the gate from the grammar.
// refs: DN-43.D3
TEST_F(FormatDetectorTest, RoutesRfc3339AppLineToRfc3339TextNotSyslog)
{
    auto* s{detector.detect("2026-05-31T08:00:01Z INFO request method=GET path=/api/users/1000")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::Rfc3339Text)
        << "routed to " << to_string(s->format())
        << "; a level word must never be consumed as a syslog hostname";
}

// invariant: the MIRROR arm — the predicates are DISJOINT, so the same prefix still reaches the
// syslog strategy when the header is genuinely there.
// invariant: a test that only had the arm above would pass on a BARE REJECTION.
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
    // invariant: non-empty unstructured text is templated as RAW and never dropped.
    auto* s{detector.detect("???")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::RawText);
}

TEST_F(FormatDetectorTest, ReturnsNullForEmptyLine)
{
    // invariant: empty and whitespace-only lines stay DROPPED — the raw fallback skips them.
    EXPECT_EQ(detector.detect(""), nullptr);
    EXPECT_EQ(detector.detect("   "), nullptr);
}

TEST_F(FormatDetectorTest, BatchDetectsJSON)
{
    // invariant: the MAJORITY format wins a batch.
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
    const std::vector<std::string_view> batch = {
        R"({"msg":"json line 1"})",
        "Jan 15 08:03:22 host sshd[1]: syslog line",
        R"({"msg":"json line 2"})",
    };
    auto* s{detector.detect_from_batch(batch)};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::JSON);
}

TEST_F(FormatDetectorTest, NearTieKVBeatsWeakCLF)
{
    // invariant: the arm's NAME describes a near-tie that NO LONGER OCCURS.
    // invariant: the competing candidate gate needs four distinct bytes and this line carries only
    // one of them, so that strategy is never scored at all.
    // invariant: the assertion still holds, by candidate EXCLUSION rather than by a score
    // comparison.
    // invariant: that strategy's confidence is now BINARY, and its own note records that the
    // strong-versus-weak distinction was a tie-break.
    constexpr std::string_view line{
        "level=INFO msg=test component=api [15/Jan/2024:10:30:00 flag=on"};
    auto* s{detector.detect(line)};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::KeyValue);
}

TEST_F(FormatDetectorTest, JSONAlwaysBeatsSyslogOnObjectLine)
{
    // invariant: a JSON object opening with a brace scores the maximum, which the syslog strategy
    // cannot reach, so JSON must ALWAYS win for any valid JSON line.
    auto* s{detector.detect(R"({"ts":"2024-01-15T10:30:00Z","level":"INFO","msg":"hi"})")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::JSON);
}

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

// invariant: the alert-label column routes EXACTLY as the unlabelled form does.
// invariant: before it did not — the candidate list was gated on a leading dash, so 348 460 lines
// of the pinned corpus were never offered a probe at all.
// invariant: they fell to the raw-text fallback with their DECLARED level unread.
// refs: DN-43.D14
TEST_F(FormatDetectorTest, DetectsAnAlertLabelledBGLLine)
{
    auto* s{detector.detect("KERNDTLB 1117838570 2005.06.03 R02-M1-N0 2005-06-03-15.42.50 "
                            "R02-M1-N0 RAS KERNEL FATAL data TLB error interrupt")};
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->format(), LogFormat::BGL);
}

// invariant: the uppercase arm of the candidate gate must NOT start claiming syslog — the label
// alphabet has no lowercase, so a month name fails it at the SECOND byte.
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

// invariant: a head with too few separators is DEMOTED, and demotion keeps the BYTES.
// invariant: the two arms assert CONTENT and not merely the routed format, because an arm checking
// only the format is satisfied by the defective code too.
// invariant: before this rule both lines routed to the same format and both published an EMPTY
// content.
// invariant: at one separator the second take found no delimiter and returned the whole remainder,
// so the message body was published as the component.
// invariant: high-cardinality free text on an identity field.
// invariant: at two separators the process-id skip consumed the body outright and it reached NO
// projection field at all.
// invariant: THE SEAT MATTERS AND IT IS THE RULING — the fix moves the separator count into the
// PREFIX PREDICATE rather than adding a guard to the parse.
// invariant: a parse-side decline is a DELETION: the parser increments its failure count and
// returns an error, so the line yields no event of any kind.
// invariant: a zero confidence is a DEMOTION: the detector falls back to the raw-text strategy,
// which puts the whole line in the content.
// invariant: both arms therefore end on the SAME assertion — every byte survives.
// refs: DN-43.D16
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
