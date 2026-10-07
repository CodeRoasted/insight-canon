// invariant: the two halves of a yearless stamp's reading that need no stream — the stamp parsers
// and the nearest-candidate rule — each asserted on literal values, no clock, no RNG.
// refs: ADR-16.D16
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::utils;

namespace
{

// post: the instant of a UTC calendar reading, built through std::chrono so the oracle shares no
// code with the parser under test.
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
    return std::format("{:%FT%TZ}", std::chrono::floor<std::chrono::milliseconds>(*when));
}

[[nodiscard]] YearlessStamp bsd(std::string_view text)
{
    const auto stamp{parse_bsd_syslog_ts(text)};
    EXPECT_TRUE(stamp.has_value()) << "the fixture stamp \"" << text << "\" did not parse";
    return stamp.value_or(YearlessStamp{});
}

} // namespace

TEST(YearlessStampParse, LogcatKeepsItsMilliseconds)
{
    const auto stamp{parse_logcat_stamp("03-17 16:13:38.811  1702  2395 D WindowManager: x")};
    ASSERT_TRUE(stamp.has_value());
    const YearlessStamp expected{
        .month = 3, .day = 17, .millisecond_of_day = (((16 * 3600) + (13 * 60) + 38) * 1000) + 811};
    EXPECT_EQ(*stamp, expected) << "month=" << int{stamp->month} << " day=" << int{stamp->day}
                                << " ms=" << stamp->millisecond_of_day << ", expected ms "
                                << expected.millisecond_of_day;
}

TEST(YearlessStampParse, ProxifierReadsItsBracketedStamp)
{
    const auto stamp{parse_proxifier_stamp("[10.30 16:49:06] chrome.exe - open")};
    ASSERT_TRUE(stamp.has_value());
    const YearlessStamp expected{
        .month = 10, .day = 30, .millisecond_of_day = ((16 * 3600) + (49 * 60) + 6) * 1000};
    EXPECT_EQ(*stamp, expected) << "month=" << int{stamp->month} << " day=" << int{stamp->day}
                                << " ms=" << stamp->millisecond_of_day;
}

TEST(YearlessStampParse, ADayNoYearHasIsRefusedInEveryFormat)
{
    EXPECT_FALSE(parse_logcat_stamp("02-30 00:00:00.000").has_value());
    EXPECT_FALSE(parse_logcat_stamp("13-01 00:00:00.000").has_value());
    EXPECT_FALSE(parse_proxifier_stamp("[04.31 00:00:00]").has_value());
    EXPECT_FALSE(parse_proxifier_stamp("[06.15 24:00:00]").has_value());
    EXPECT_TRUE(parse_logcat_stamp("02-29 00:00:00.000").has_value());
    EXPECT_TRUE(parse_proxifier_stamp("[02.29 00:00:00]").has_value());
}

TEST(ResolveYearless, TheYearNearestTheReferenceWins)
{
    const auto resolved{resolve_yearless(bsd("May  8 09:00:00"), utc(2026, 5, 7, 21, 25, 30))};
    EXPECT_EQ(resolved, std::optional<Timestamp>{utc(2026, 5, 8, 9, 0, 0)})
        << "got " << render(resolved);
}

// invariant: a chain across New Year needs no new anchor — the next year is a candidate.
TEST(ResolveYearless, JanuaryAfterDecemberTakesTheNextYear)
{
    const auto resolved{resolve_yearless(bsd("Jan  1 00:00:01"), utc(2025, 12, 31, 23, 59, 59))};
    EXPECT_EQ(resolved, std::optional<Timestamp>{utc(2026, 1, 1, 0, 0, 1)})
        << "got " << render(resolved);
}

// invariant: the anchor `2025-12-31T23:30-05:00` is 2026-01-01T04:30Z; copying its year would put
// the next local `Dec 31 23:31:00` eleven months ahead, and nearest puts it five hours before.
TEST(ResolveYearless, AnAnchorPastMidnightUtcResolvesTheLocalDecemberStampToTheEarlierYear)
{
    const auto anchor{parse_iso8601("2025-12-31T23:30:00-05:00")};
    ASSERT_TRUE(anchor.has_value());
    ASSERT_EQ(*anchor, utc(2026, 1, 1, 4, 30, 0));
    const auto resolved{resolve_yearless(bsd("Dec 31 23:31:00"), *anchor)};
    EXPECT_EQ(resolved, std::optional<Timestamp>{utc(2025, 12, 31, 23, 31, 0)})
        << "got " << render(resolved);
}

TEST(ResolveYearless, FebTwentyNineResolvesOnlyInALeapCandidate)
{
    const auto leap{resolve_yearless(bsd("Feb 29 12:00:00"), utc(2025, 3, 1, 0, 0, 0))};
    EXPECT_EQ(leap, std::optional<Timestamp>{utc(2024, 2, 29, 12, 0, 0)})
        << "got " << render(leap) << "; 2024 is the only leap year among 2024, 2025 and 2026";
    const auto none{resolve_yearless(bsd("Feb 29 12:00:00"), utc(2022, 6, 1, 0, 0, 0))};
    EXPECT_FALSE(none.has_value())
        << "2021, 2022 and 2023 have no Feb 29, yet it resolved to " << render(none);
}

// invariant: 2025-07-02T12:00Z is exactly 182.5 days from both 2025-01-01 and 2026-01-01.
TEST(ResolveYearless, AnExactTieGoesToTheLaterYear)
{
    const auto resolved{resolve_yearless(bsd("Jan  1 00:00:00"), utc(2025, 7, 2, 12, 0, 0))};
    EXPECT_EQ(resolved, std::optional<Timestamp>{utc(2026, 1, 1, 0, 0, 0)})
        << "got " << render(resolved);
}
