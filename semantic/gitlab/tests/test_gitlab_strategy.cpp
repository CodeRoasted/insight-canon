// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_gitlab_strategy.cpp — the GitLab dialect CODE TIER (ADR 0024 §2.3): the line-selective
// format strategy. What it guards: the strategy claims EXACTLY the dialect-marked shapes (a line
// carrying the 32-byte runner transport prefix, a bare `section_start:` marker, the terminal verdict
// line), PEELS the prefix and parses its timestamp as the event time, and stays silent on every
// other line AND on the RFC3339-prefixed look-alikes (a GHA line, a Syslog line) — the anti-phantom
// guard the strict fixed-width shape buys. Determinism: byte-only parse, no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;           // ArenaAllocator / LogFormat / compose
import insight.semantic.gitlab; // make_strategy + (via export import spi) IFormatStrategy

using insight::LogFormat;
using insight::tokenization::ArenaAllocator;

namespace
{
// Verbatim from marker_corpus_v1 (code.videolan.org/esphynox__vlc job_2728336), with the ANSI
// escapes canon's SRC-D-TID-11 ingest strip has already removed — which is what a strategy actually
// sees.
constexpr std::string_view kStamped{"2026-07-21T18:06:18.101984Z 00O section_start:1784657178:"
                                    "prepare_executor\r"};
constexpr std::string_view kStampedContinuation{
    "2026-07-21T18:06:19.366429Z 00O+section_start:1784657179:prepare_script\r"};
constexpr std::string_view kStampedStderr{"2026-07-21T18:06:20.001000Z 00E warning: deprecated"};
constexpr std::string_view kStampedPlain{"2026-07-21T18:15:01.294229Z 00O Job succeeded"};
constexpr std::string_view kBareSection{"section_start:1784657178:prepare_executor\r"};
constexpr std::string_view kBareVerdict{"ERROR: Job failed: exit code 1"};

// NOT GitLab. The first two both open with an RFC3339 token, which is exactly why the shape check
// has to be strict.
constexpr std::string_view kGhaLine{"2026-05-27T15:26:41.7842152Z Run yarn lint"};
constexpr std::string_view kSyslogish{"2026-07-21T18:06:18.101984Z host app: started"};
constexpr std::string_view kJenkinsLine{"[2025-06-25T14:31:12.339Z] [Pipeline] sh"};
constexpr std::string_view kPlain{"+ meson compile -C build"};
constexpr std::string_view kWrongFraction{"2026-07-21T18:06:18.101Z 00O section_start:1:build"};
constexpr std::string_view kWrongTag{"2026-07-21T18:06:18.101984Z 00X section_start:1:build"};
} // namespace

TEST(GitLabStrategy, FormatReturnsGitLab)
{
    const auto strategy{insight::semantic::gitlab::make_strategy()};
    EXPECT_EQ(strategy->format(), LogFormat::GitLab);
}

TEST(GitLabStrategy, ClaimsExactlyTheDialectMarkedShapes)
{
    const auto strategy{insight::semantic::gitlab::make_strategy()};
    EXPECT_GT(strategy->confidence(kStamped), 0.0);
    EXPECT_GT(strategy->confidence(kStampedContinuation), 0.0) << "the '+' continuation flag";
    EXPECT_GT(strategy->confidence(kStampedStderr), 0.0) << "the 'E' stream tag";
    EXPECT_GT(strategy->confidence(kStampedPlain), 0.0)
        << "a stamped line is claimed regardless of content — the PEEL is the dialect knowledge, "
           "and it is what exposes every marker underneath it";
    EXPECT_GT(strategy->confidence(kBareSection), 0.0) << "the un-stamped runner generation";
    EXPECT_GT(strategy->confidence(kBareVerdict), 0.0);

    EXPECT_EQ(strategy->confidence(kGhaLine), 0.0)
        << "a GHA line opens with RFC3339 too — the fixed fraction width and the stream tag are "
           "what tell them apart";
    EXPECT_EQ(strategy->confidence(kSyslogish), 0.0);
    EXPECT_EQ(strategy->confidence(kJenkinsLine), 0.0);
    EXPECT_EQ(strategy->confidence(kPlain), 0.0) << "plain output falls through (RawText)";
    EXPECT_EQ(strategy->confidence(kWrongFraction), 0.0)
        << "the prefix is FIXED-WIDTH by measurement (3 446 260 stamped lines, one width); another "
           "fraction width is DECLINED, which is a fail-closed miss rather than a wrong peel";
    EXPECT_EQ(strategy->confidence(kWrongTag), 0.0) << "the stream tag is O or E, nothing else";
}

TEST(GitLabStrategy, PeelsThePrefixAndParsesItsTimestamp)
{
    const auto strategy{insight::semantic::gitlab::make_strategy()};
    ArenaAllocator arena{4096};
    const auto parsed{strategy->parse(kStamped, arena)};
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(parsed->content, "section_start:1784657178:prepare_executor\r")
        << "the 32-byte transport prefix is peeled; the marker's own CR terminator is the row "
           "grammar's business, not the strategy's";
    ASSERT_TRUE(parsed->timestamp.has_value())
        << "the prefix's RFC3339 head is the event observation time";
    EXPECT_TRUE(parsed->component.empty()) << "GitLab trace lines carry no component / tag";
}

TEST(GitLabStrategy, TheContinuationFlagPeelsToTheSameWidth)
{
    const auto strategy{insight::semantic::gitlab::make_strategy()};
    ArenaAllocator arena{4096};
    const auto parsed{strategy->parse(kStampedContinuation, arena)};
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(parsed->content, "section_start:1784657179:prepare_script\r")
        << "'+' occupies the same column the separator space otherwise holds — which is why both "
           "forms land on 32 bytes";
}

TEST(GitLabStrategy, ABareUnstampedLineIsClaimedWithoutATimestamp)
{
    const auto strategy{insight::semantic::gitlab::make_strategy()};
    ArenaAllocator arena{4096};
    const auto parsed{strategy->parse(kBareSection, arena)};
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(parsed->content, kBareSection) << "nothing to peel on the un-stamped generation";
    EXPECT_FALSE(parsed->timestamp.has_value());
}

TEST(GitLabStrategy, APrefixOnlyLineIsDeclinedAsBlank)
{
    const auto strategy{insight::semantic::gitlab::make_strategy()};
    ArenaAllocator arena{4096};
    // A stamped blank line. Declining it keeps an empty "" template out of the tokenizer — the
    // GHA/Jenkins strategy discipline.
    const auto parsed{strategy->parse("2026-07-21T18:06:18.101984Z 00O ", arena)};
    EXPECT_FALSE(parsed.has_value());
}

TEST(GitLabStrategy, ANonGitLabLineIsDeclinedWithADiagnosticRatherThanMisParsed)
{
    const auto strategy{insight::semantic::gitlab::make_strategy()};
    ArenaAllocator arena{4096};
    const auto parsed{strategy->parse(kGhaLine, arena)};
    ASSERT_FALSE(parsed.has_value());
    EXPECT_FALSE(parsed.error().empty()) << "a decline must say why";
}
// NOLINTEND
