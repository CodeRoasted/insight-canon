// NOLINTBEGIN — unit test: short identifiers and string literals are fine.
// test_jenkins_strategy.cpp — the Jenkins dialect CODE TIER (ADR 0024 §2.3): the line-selective
// format strategy. What it guards: the strategy claims EXACTLY the three dialect-marked shapes
// (timestamper-prefixed / `[Pipeline] ` / `Finished: <RESULT>`), strips + parses the timestamper
// prefix (the template-collapse restorer), and stays silent on every other console line (freestyle
// output keeps its RawText behavior) AND on look-alike bracket prefixes (Proxifier / ApacheError /
// bare `[HH:MM:SS]` — the anti-phantom guard the strict RFC3339 shape buys). Determinism: byte-only
// parse, no RNG/clock/float.
#include <gtest/gtest.h>

import std;
import insight.canon;            // ArenaAllocator / LogFormat / Tokenizer / compose
import insight.semantic.jenkins; // make_strategy + (via export import spi) IFormatStrategy / ParsedLine

using insight::LogFormat;
using insight::tokenization::ArenaAllocator;

namespace
{
constexpr std::string_view kTimestamped{
    "[2025-06-25T14:31:12.339Z] [Pipeline] { (Build / linux-jdk17)"};
constexpr std::string_view kTimestampedPlain{"[2025-06-25T14:31:12.339Z] + mvn -B verify"};
constexpr std::string_view kTimestampedOffset{"[2025-06-25T14:31:12+02:00] hello from CET"};
constexpr std::string_view kBarePipeline{"[Pipeline] sh"};
constexpr std::string_view kFinished{"Finished: UNSTABLE"};
// NOT Jenkins: plain console output, a GHA line, and the bracket look-alikes.
constexpr std::string_view kPlain{"+ mvn -B verify"};
constexpr std::string_view kGHALine{"2026-05-27T15:26:41.7842152Z Run yarn lint"};
constexpr std::string_view kProxifierish{"[10.20.30.40]:443 open through proxy"};
constexpr std::string_view kApacheish{"[Mon Oct 03 12:00:01 2011] [error] boom"};
constexpr std::string_view kBareClock{"[12:34:56] not a timestamper line"};
constexpr std::string_view kDecoratedFinished{"Finished: SUCCESS (took 3s)"};
} // namespace

TEST(JenkinsStrategy, FormatReturnsJenkins)
{
    const auto strategy{insight::semantic::jenkins::make_strategy()};
    EXPECT_EQ(strategy->format(), LogFormat::Jenkins);
}

TEST(JenkinsStrategy, ClaimsExactlyTheDialectMarkedShapes)
{
    const auto strategy{insight::semantic::jenkins::make_strategy()};
    EXPECT_GT(strategy->confidence(kTimestamped), 0.0);
    EXPECT_GT(strategy->confidence(kTimestampedPlain), 0.0)
        << "a timestamper-prefixed line is claimed regardless of its content (the strip IS the "
           "win)";
    EXPECT_GT(strategy->confidence(kTimestampedOffset), 0.0) << "±HH:MM zone form";
    EXPECT_GT(strategy->confidence(kBarePipeline), 0.0);
    EXPECT_GT(strategy->confidence(kFinished), 0.0);

    EXPECT_EQ(strategy->confidence(kPlain), 0.0) << "plain console output falls through (RawText)";
    EXPECT_EQ(strategy->confidence(kGHALine), 0.0) << "a GHA line is never claimed";
    EXPECT_EQ(strategy->confidence(kProxifierish), 0.0) << "bracket look-alike: not RFC3339";
    EXPECT_EQ(strategy->confidence(kApacheish), 0.0) << "bracket look-alike: alpha date";
    EXPECT_EQ(strategy->confidence(kBareClock), 0.0) << "bracket look-alike: time-only";
    EXPECT_EQ(strategy->confidence(kDecoratedFinished), 0.0)
        << "a decorated epilogue is not the terminal-verdict line (studies/006 ^Finished: (\\w+)$)";
}

TEST(JenkinsStrategy, StripsAndParsesTheTimestamperPrefix)
{
    const auto strategy{insight::semantic::jenkins::make_strategy()};
    ArenaAllocator arena{64U * 1024U};
    const auto parsed{strategy->parse(kTimestamped, arena)};
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    EXPECT_EQ(parsed->content, "[Pipeline] { (Build / linux-jdk17)")
        << "the timestamper prefix must be stripped from the templated content";
    ASSERT_TRUE(parsed->timestamp.has_value()) << "the bracket interior is the event timestamp";
    // 2025-06-25T14:31:12(.339)Z — second-precision UTC check (parse_iso8601 truncates sub-second).
    const auto secs{
        std::chrono::duration_cast<std::chrono::seconds>(parsed->timestamp->time_since_epoch())
            .count()};
    EXPECT_EQ(secs, 1750861872) << "expected 2025-06-25T14:31:12Z as epoch seconds";
}

// The §6.5 NAMED HOLDER of the post-stamp whitespace boundary (jenkins_retrofit_gates.md §6.5:
// "the strategy's own unit arms hold the boundary bytes"). adr/0046's bundled-behavior #3 is a
// GREEDY `[ \t]+` strip after the stamp — it consumes the payload's own leading indentation (a
// Java stack frame's tab), and it stops at the first non-whitespace byte, never inside the
// content. The §6.5 P2b corpus leg asserts conformance to this exact spelling on real bytes;
// these arms pin the boundary synthetically so a strip regression is red HERE first, without a
// corpus. Whether eating content indentation is WISE is deliberately not asserted anywhere — that
// semantic question is the flaws.md parked entry (Eqya·9), measurement-gated.
TEST(JenkinsStrategy, PostStampWhitespaceStripIsGreedyOverSpaceAndTabAndStopsAtContent)
{
    const auto strategy{insight::semantic::jenkins::make_strategy()};
    ArenaAllocator arena{64U * 1024U};
    // Greedy over the mixed run: space + tabs are ALL consumed, content starts at `at`.
    const auto indented{strategy->parse(
        "[2025-06-25T14:31:12.339Z] \t\tat org.example.Foo.bar(Foo.java:12)", arena)};
    ASSERT_TRUE(indented.has_value()) << indented.error();
    EXPECT_EQ(indented->content, "at org.example.Foo.bar(Foo.java:12)")
        << "bundled #3: the strip is greedy over [ \\t]+ — the payload's leading tabs are "
           "consumed with the separator";
    // Stops at the first non-whitespace byte: INTERIOR whitespace is content and survives.
    const auto interior{strategy->parse("[2025-06-25T14:31:12.339Z] x\ty z", arena)};
    ASSERT_TRUE(interior.has_value()) << interior.error();
    EXPECT_EQ(interior->content, "x\ty z")
        << "the strip must stop at the first non-whitespace byte — interior tabs are content";
    // A whitespace-only payload is the blank-line decline (bundled #4), tabs included.
    EXPECT_FALSE(strategy->parse("[2025-06-25T14:31:12.339Z] \t \t", arena).has_value())
        << "a stamp followed by only [ \\t]+ is a blank line — declined";
}

TEST(JenkinsStrategy, BareShapesCarryNoTimestamp)
{
    const auto strategy{insight::semantic::jenkins::make_strategy()};
    ArenaAllocator arena{64U * 1024U};
    const auto pipeline{strategy->parse(kBarePipeline, arena)};
    ASSERT_TRUE(pipeline.has_value()) << pipeline.error();
    EXPECT_FALSE(pipeline->timestamp.has_value());
    EXPECT_EQ(pipeline->content, kBarePipeline);
    const auto finished{strategy->parse(kFinished, arena)};
    ASSERT_TRUE(finished.has_value()) << finished.error();
    EXPECT_EQ(finished->content, kFinished);
}

TEST(JenkinsStrategy, TimestamperOnlyLineIsDeclinedAsBlank)
{
    const auto strategy{insight::semantic::jenkins::make_strategy()};
    ArenaAllocator arena{64U * 1024U};
    EXPECT_FALSE(strategy->parse("[2025-06-25T14:31:12.339Z] ", arena).has_value())
        << "a timestamp-only line is a blank line — declined, never an empty template";
    EXPECT_FALSE(strategy->parse(kPlain, arena).has_value())
        << "parse() must refuse a line confidence() would not claim";
}

// The composed end-to-end: a Jenkins-marked line ROUTES to the Jenkins strategy through the
// FormatDetector (custom-strategy probe), a plain line falls through to RawText.
TEST(JenkinsStrategy, RoutesThroughTheComposedDetector)
{
    const std::array manifests{insight::semantic::jenkins::kManifest};
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose(manifests)};
    ArenaAllocator arena{64U * 1024U};
    insight::tokenization::Tokenizer tokenizer{arena, insight::tokenization::MaskConfig{},
                                               composed};

    const auto pipeline{tokenizer.process_line(std::string{kBarePipeline})};
    ASSERT_TRUE(pipeline.has_value()) << pipeline.error();
    EXPECT_EQ(pipeline->format, LogFormat::Jenkins);

    const auto plain{tokenizer.process_line(std::string{kPlain})};
    ASSERT_TRUE(plain.has_value()) << plain.error();
    EXPECT_EQ(plain->format, LogFormat::RawText)
        << "unmarked console output keeps its RawText routing (the freestyle floor is unchanged)";
}
// NOLINTEND
