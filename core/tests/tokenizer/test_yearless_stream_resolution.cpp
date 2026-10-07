// invariant: a yearless stamp takes its year from the stream's previous event time and is absent
// before one — asserted at the TOKENIZER grain, the one object that holds a stream's state.
// invariant: determinism — literal lines, one tokenizer per case, no wall clock, no RNG.
// refs: ADR-16.D16
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

namespace
{

[[nodiscard]] Timestamp utc(int year, unsigned month, unsigned day, int hour, int minute,
                            int second)
{
    const std::chrono::sys_days date{std::chrono::year{year} / std::chrono::month{month} /
                                     std::chrono::day{day}};
    return Timestamp{date} + std::chrono::hours{hour} + std::chrono::minutes{minute} +
           std::chrono::seconds{second};
}

[[nodiscard]] std::string render(const std::optional<Timestamp>& when)
{
    if (!when)
        return "absent";
    return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::seconds>(*when));
}

// invariant: the composition must OUTLIVE the const-ref the tokenizer holds.
struct StreamFixture
{
    static constexpr std::size_t kArenaSize{1U << 20U};
    ArenaAllocator arena{kArenaSize};
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed, StreamContext{}};

    // post: the event time of one line; a line that produces no event fails the test.
    [[nodiscard]] std::optional<Timestamp> time_of(std::string_view line)
    {
        const auto event{tokenizer.process_line(line)};
        EXPECT_TRUE(event.has_value()) << "no event for: " << line;
        return event ? event->timestamp : std::nullopt;
    }
};

constexpr std::string_view kAnchor{"2026-05-07T21:25:30Z runnervm systemd[1]: Started job."};
constexpr std::string_view kYearless{"May 07 21:25:31 runnervm systemd[1]: Stopped job."};

} // namespace

TEST(YearlessStream, AYearlessLineBeforeAnyAnchorIsAbsent)
{
    StreamFixture fx;
    const auto first{fx.time_of(kYearless)};
    EXPECT_FALSE(first.has_value())
        << "no line before it carried a year, yet it resolved to " << render(first);
    const auto second{fx.time_of(kYearless)};
    EXPECT_FALSE(second.has_value()) << "an absent time became a reference: " << render(second);
}

TEST(YearlessStream, AYearlessLineAfterAnAnchorTakesTheAnchorsYear)
{
    StreamFixture fx;
    ASSERT_EQ(fx.time_of(kAnchor), std::optional<Timestamp>{utc(2026, 5, 7, 21, 25, 30)});
    const auto resolved{fx.time_of(kYearless)};
    EXPECT_EQ(resolved, std::optional<Timestamp>{utc(2026, 5, 7, 21, 25, 31)})
        << "got " << render(resolved);
}

TEST(YearlessStream, TheChainCrossesNewYearWithNoNewAnchor)
{
    StreamFixture fx;
    ASSERT_TRUE(fx.time_of("2025-12-31T23:59:00Z host cron[1]: tick").has_value());
    const auto december{fx.time_of("Dec 31 23:59:59 host cron[1]: tick")};
    const auto january{fx.time_of("Jan  1 00:00:01 host cron[1]: tick")};
    const auto later{fx.time_of("Jan  1 00:05:00 host cron[1]: tick")};
    EXPECT_EQ(december, std::optional<Timestamp>{utc(2025, 12, 31, 23, 59, 59)})
        << "got " << render(december);
    EXPECT_EQ(january, std::optional<Timestamp>{utc(2026, 1, 1, 0, 0, 1)})
        << "got " << render(january);
    EXPECT_EQ(later, std::optional<Timestamp>{utc(2026, 1, 1, 0, 5, 0)}) << "got " << render(later);
}

// invariant: Sift declares a context before each side it reads, so the baseline's last instant
// must never resolve a line of the changed log.
TEST(YearlessStream, DeclaringAContextResetsTheChain)
{
    StreamFixture fx;
    ASSERT_TRUE(fx.time_of(kAnchor).has_value());
    fx.tokenizer.declare_context(StreamContext{});
    const auto after{fx.time_of(kYearless)};
    EXPECT_FALSE(after.has_value()) << "the previous context's anchor resolved a line of the next "
                                       "one, to "
                                    << render(after);
}

TEST(YearlessStream, ADateInTheBodyIsNotAnAnchor)
{
    StreamFixture fx;
    const auto body_date{
        fx.time_of("May 07 21:25:31 host trustd[1]: cert creationDate=2017-02-18 12:00:00 +0000")};
    EXPECT_FALSE(body_date.has_value())
        << "a date inside the message became the event time " << render(body_date);
    const auto next{fx.time_of(kYearless)};
    EXPECT_FALSE(next.has_value())
        << "a date inside a message body anchored the next line to " << render(next);
}

TEST(YearlessStream, ABsdStampInAJsonFieldResolvesFromTheStream)
{
    StreamFixture fx;
    ASSERT_TRUE(
        fx.time_of(R"({"timestamp":"2026-05-07T21:25:30Z","message":"started"})").has_value());
    const auto resolved{fx.time_of(R"({"timestamp":"May 07 21:25:31","message":"stopped"})")};
    EXPECT_EQ(resolved, std::optional<Timestamp>{utc(2026, 5, 7, 21, 25, 31)})
        << "got " << render(resolved);
}

TEST(YearlessStream, ABsdStampInAKvFieldResolvesFromTheStream)
{
    StreamFixture fx;
    ASSERT_TRUE(fx.time_of("ts=2026-05-07T21:25:30Z level=info msg=started").has_value());
    const auto resolved{fx.time_of(R"(ts="May 07 21:25:31" level=info msg=stopped)")};
    EXPECT_EQ(resolved, std::optional<Timestamp>{utc(2026, 5, 7, 21, 25, 31)})
        << "got " << render(resolved);
}

// invariant: a two-digit year read under the existing pivot is an instant, so it anchors.
TEST(YearlessStream, ATwoDigitYearLineIsAnAnchor)
{
    StreamFixture fx;
    ASSERT_EQ(fx.time_of("17/06/09 20:10:40 INFO executor.Backend: Registered"),
              std::optional<Timestamp>{utc(2017, 6, 9, 20, 10, 40)});
    const auto resolved{fx.time_of("Jun  9 20:10:41 host app[1]: next")};
    EXPECT_EQ(resolved, std::optional<Timestamp>{utc(2017, 6, 9, 20, 10, 41)})
        << "got " << render(resolved);
}

// invariant: canon infers the year, so a resolved time is PARSED and never outranks a transport
// stamp the way a declared one does.
TEST(YearlessStream, AResolvedTimeIsNeverDeclared)
{
    StreamFixture fx;
    ASSERT_TRUE(fx.time_of(kAnchor).has_value());
    const auto event{fx.tokenizer.process_line(kYearless)};
    ASSERT_TRUE(event.has_value() && event->timestamp.has_value());
    EXPECT_FALSE(event->declared_timestamp);
}
