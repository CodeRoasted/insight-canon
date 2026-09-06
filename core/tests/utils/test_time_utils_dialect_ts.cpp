
// invariant: the eight DIALECT timestamp parsers, each owned by one strategy; the three
// cross-dialect formats and the level inference live in the sibling file.
// invariant: HOMED as unit tests rather than strategy tests, because a strategy test exercises ONE
// well-formed line and cannot reach the malformed and boundary branches.
// invariant: that is exactly where a timestamp parser silently produces a WRONG instant instead of
// an absence, and every one of the eight was reachable only through its happy path before.
// invariant: the ORACLE is the standard library's civil calendar and NOT the parsers' own helper
// — re-using it would make subject equal oracle and a broken leap rule would agree with itself.
// invariant: the parsers gate on a MINIMUM length and read a fixed prefix, because they are called
// on the head of a line, so trailing content must be IGNORED rather than refused.
// invariant: that looseness is pinned below, because tightening it to an exact-length check would
// silently break every strategy that relies on it.
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::utils;

namespace
{

[[nodiscard]] std::int64_t utc_epoch(int year_value, unsigned month_value, unsigned day_value,
                                     int hour_value, int minute_value, int second_value)
{
    using namespace std::chrono;
    const sys_days civil_day{std::chrono::year{year_value} / month_value / day_value};
    return (civil_day + hours{hour_value} + minutes{minute_value} + seconds{second_value})
        .time_since_epoch()
        .count();
}

[[nodiscard]] std::int64_t epoch_of(Timestamp timestamp)
{
    return static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(timestamp));
}

// invariant: both sides are printed, because a timestamp test that fails with false-is-not-true
// costs a debugger session.
#define EXPECT_PARSES_TO(expr, expected_epoch)                                                     \
    do                                                                                             \
    {                                                                                              \
        const auto parsed_{(expr)};                                                                \
        ASSERT_TRUE(parsed_.has_value())                                                           \
            << #expr << " returned nullopt, expected epoch " << (expected_epoch);                  \
        EXPECT_EQ(epoch_of(*parsed_), (expected_epoch))                                            \
            << #expr << "\n  actual epoch   : " << epoch_of(*parsed_)                              \
            << "\n  expected epoch : " << (expected_epoch)                                         \
            << "\n  delta (seconds): " << (epoch_of(*parsed_) - (expected_epoch));                 \
    } while (false)

} // namespace

TEST(ParseEpochTimestamp, ValidSecondsParsed)
{
    EXPECT_PARSES_TO(parse_epoch_timestamp("1117838570"), 1117838570);
    EXPECT_PARSES_TO(parse_epoch_timestamp("0"), 0);
}

TEST(ParseEpochTimestamp, MillisecondEpochIsRefusedNotSilentlyReadAsSeconds)
{
    // invariant: thirteen digits is the MILLISECOND epoch, the commonest way a timestamp column
    // arrives wrong; read as seconds it would place the record millennia away.
    // invariant: the twelve-digit cap must refuse it, and twelve digits reaches far enough that no
    // real log second is lost.
    EXPECT_FALSE(parse_epoch_timestamp("1705312200000").has_value())
        << "a 13-digit millisecond epoch was accepted as SECONDS";
    EXPECT_TRUE(parse_epoch_timestamp("170531220000").has_value())
        << "12 digits is the documented maximum and must still parse";
}

TEST(ParseEpochTimestamp, MalformedInputRefused)
{
    EXPECT_FALSE(parse_epoch_timestamp("").has_value());
    EXPECT_FALSE(parse_epoch_timestamp("-1").has_value()) << "a negative epoch is not a log time";
    EXPECT_FALSE(parse_epoch_timestamp("abc").has_value());
    EXPECT_FALSE(parse_epoch_timestamp("+1117838570").has_value());
    EXPECT_FALSE(parse_epoch_timestamp(" 1117838570").has_value());
    // invariant: unlike the fixed-layout parsers, this one consumes the WHOLE view, so a partial
    // number is a REFUSAL rather than a prefix parse.
    EXPECT_FALSE(parse_epoch_timestamp("1117838570.5").has_value());
    EXPECT_FALSE(parse_epoch_timestamp("1117838570x").has_value());
}

TEST(ParseUnixNanoTimestamp, EpochNanosParsedAsEventTime)
{
    // invariant: without the nanosecond channel there is no event-time axis and the ingest yields
    // ZERO windows.
    EXPECT_PARSES_TO(parse_unix_nano_timestamp("1705314600000000000"),
                     utc_epoch(2024, 1, 15, 10, 30, 0));
    EXPECT_PARSES_TO(parse_unix_nano_timestamp("0"), 0);
}

TEST(ParseUnixNanoTimestamp, SubSecondNanosTruncateTowardTheSecond)
{
    // invariant: the integer duration cast truncates, and the producer emits millisecond-granular
    // nanoseconds, so window membership stays bit-identical across standard libraries.
    EXPECT_PARSES_TO(parse_unix_nano_timestamp("1705314600999000000"),
                     utc_epoch(2024, 1, 15, 10, 30, 0));
}

TEST(ParseUnixNanoTimestamp, OverflowingAndMalformedInputRefused)
{
    // invariant: twenty digits exceeds the signed range, and it is refused at the LENGTH gate
    // before the conversion can overflow.
    EXPECT_FALSE(parse_unix_nano_timestamp("12345678901234567890").has_value())
        << "a 20-digit nano value was accepted — the int64 overflow guard is not holding";
    EXPECT_FALSE(parse_unix_nano_timestamp("").has_value());
    EXPECT_FALSE(parse_unix_nano_timestamp("-1705312200000000000").has_value());
    EXPECT_FALSE(parse_unix_nano_timestamp("1705312200000000000 ").has_value());
    EXPECT_FALSE(parse_unix_nano_timestamp("1.7053122e18").has_value());
}

TEST(ParseCompactDateTime, ValidPairParsed)
{
    EXPECT_PARSES_TO(parse_compact_date_time("240115", "103000"),
                     utc_epoch(2024, 1, 15, 10, 30, 0));
}

TEST(ParseCompactDateTime, TwoDigitYearPivotsAt70)
{
    // invariant: the two-digit-year pivot decides a CENTURY — an off-by-one moves a record a
    // hundred years and is invisible in any happy-path strategy test, so both sides are pinned.
    EXPECT_PARSES_TO(parse_compact_date_time("691231", "235959"),
                     utc_epoch(2069, 12, 31, 23, 59, 59));
    EXPECT_PARSES_TO(parse_compact_date_time("700101", "000000"), utc_epoch(1970, 1, 1, 0, 0, 0));
}

TEST(ParseCompactDateTime, WidthIsExactOnBothFields)
{
    // invariant: this parser receives two ALREADY-SPLIT tokens, so a wrong width means a wrong
    // split upstream and it must refuse rather than guess.
    EXPECT_FALSE(parse_compact_date_time("24015", "103000").has_value());
    EXPECT_FALSE(parse_compact_date_time("2401150", "103000").has_value());
    EXPECT_FALSE(parse_compact_date_time("240115", "10300").has_value());
    EXPECT_FALSE(parse_compact_date_time("240115", "1030000").has_value());
    EXPECT_FALSE(parse_compact_date_time("", "").has_value());
    EXPECT_FALSE(parse_compact_date_time("24011a", "103000").has_value());
    EXPECT_FALSE(parse_compact_date_time("240115", "10:000").has_value());
}

TEST(ParseShortYearSlash, ValidTimestampParsed)
{
    EXPECT_PARSES_TO(parse_short_year_slash("24/01/15 10:30:00"),
                     utc_epoch(2024, 1, 15, 10, 30, 0));
}

TEST(ParseShortYearSlash, TwoDigitYearPivotsAt70)
{
    EXPECT_PARSES_TO(parse_short_year_slash("69/12/31 23:59:59"),
                     utc_epoch(2069, 12, 31, 23, 59, 59));
    EXPECT_PARSES_TO(parse_short_year_slash("70/01/01 00:00:00"), utc_epoch(1970, 1, 1, 0, 0, 0));
}

TEST(ParseShortYearSlash, IgnoresTrailingLineContent)
{
    // invariant: called on the HEAD of a line, so the rest of the line must not refuse it.
    EXPECT_PARSES_TO(parse_short_year_slash("24/01/15 10:30:00 INFO Executor: running task"),
                     utc_epoch(2024, 1, 15, 10, 30, 0));
}

TEST(ParseShortYearSlash, MalformedSeparatorsAndShortInputRefused)
{
    EXPECT_FALSE(parse_short_year_slash("24/01/15 10:30:0").has_value()) << "16 chars is short";
    EXPECT_FALSE(parse_short_year_slash("").has_value());
    EXPECT_FALSE(parse_short_year_slash("24-01-15 10:30:00").has_value()) << "dashes, not slashes";
    EXPECT_FALSE(parse_short_year_slash("24/01/15T10:30:00").has_value()) << "T, not space";
    EXPECT_FALSE(parse_short_year_slash("24/01/15 10.30.00").has_value()) << "dots, not colons";
    EXPECT_FALSE(parse_short_year_slash("2024/01/15 10:30:00").has_value())
        << "a four-digit year shifts every field — this is the nginx format, not Spark's";
}

TEST(ParseApacheErrorTs, ValidTimestampParsed)
{
    EXPECT_PARSES_TO(parse_apache_error_ts("Sun Dec 04 04:47:44 2005"),
                     utc_epoch(2005, 12, 4, 4, 47, 44));
}

TEST(ParseApacheErrorTs, MonthNameIsParsedNotPositional)
{
    EXPECT_PARSES_TO(parse_apache_error_ts("Mon Jan 01 00:00:00 2024"),
                     utc_epoch(2024, 1, 1, 0, 0, 0));
    EXPECT_PARSES_TO(parse_apache_error_ts("Tue Jul 31 23:59:59 2029"),
                     utc_epoch(2029, 7, 31, 23, 59, 59));
    EXPECT_FALSE(parse_apache_error_ts("Sun Xyz 04 04:47:44 2005").has_value())
        << "an unknown month name must refuse, not default to January";
}

TEST(ParseApacheErrorTs, WeekdayIsSkippedWithoutValidation)
{
    // invariant: DECLARED rather than accidental — the weekday is redundant with the date, so it
    // is skipped rather than cross-checked and a wrong one still yields the right instant.
    EXPECT_PARSES_TO(parse_apache_error_ts("Xxx Dec 04 04:47:44 2005"),
                     utc_epoch(2005, 12, 4, 4, 47, 44));
}

TEST(ParseApacheErrorTs, MalformedInputRefused)
{
    EXPECT_FALSE(parse_apache_error_ts("Sun Dec 04 04:47:44 200").has_value()) << "23 chars";
    EXPECT_FALSE(parse_apache_error_ts("").has_value());
    EXPECT_FALSE(parse_apache_error_ts("SunXDec 04 04:47:44 2005").has_value());
    EXPECT_FALSE(parse_apache_error_ts("Sun Dec 04 04-47-44 2005").has_value());
    EXPECT_FALSE(parse_apache_error_ts("Sun Dec 04 04:47:44_2005").has_value());
}

TEST(ParseHealthAppTs, ValidTimestampParsed)
{
    EXPECT_PARSES_TO(parse_health_app_ts("20171223-22:15:29:606"),
                     utc_epoch(2017, 12, 23, 22, 15, 29));
}

TEST(ParseHealthAppTs, SingleDigitHourAndSecondParsed)
{
    // invariant: the format is genuinely variable-width, and a fixed-offset reader would mis-slice
    // these.
    EXPECT_PARSES_TO(parse_health_app_ts("20171223-9:15:29:606"),
                     utc_epoch(2017, 12, 23, 9, 15, 29));
    EXPECT_PARSES_TO(parse_health_app_ts("20171223-22:15:9:606"),
                     utc_epoch(2017, 12, 23, 22, 15, 9));
    EXPECT_PARSES_TO(parse_health_app_ts("20171223-9:15:9:606"), utc_epoch(2017, 12, 23, 9, 15, 9));
}

TEST(ParseHealthAppTs, SingleDigitMinuteParsed)
{
    // invariant: the MINUTE was read as exactly two digits, which made widening the claim predicate
    // a half-fix — 187 of the 247 rejected corpus lines carry a one-digit minute.
    // invariant: they would have routed to the strategy and still carried NO event time.
    // invariant: the old failure was SAFE — an absence, never a wrong instant — but silent, and
    // an inconsistency rather than a decision: the fields beside it were already variable-width.
    // refs: DN-43.O5
    EXPECT_PARSES_TO(parse_health_app_ts("20171223-22:5:29:606"),
                     utc_epoch(2017, 12, 23, 22, 5, 29));
    EXPECT_PARSES_TO(parse_health_app_ts("20171224-0:0:0:232"), utc_epoch(2017, 12, 24, 0, 0, 0));
    EXPECT_PARSES_TO(parse_health_app_ts("20171223-9:7:6:1"), utc_epoch(2017, 12, 23, 9, 7, 6));
}

TEST(ParseHealthAppTs, ASignedFieldIsRefusedRatherThanNormalized)
{
    // invariant: the standard conversion accepts a leading sign for a signed type, so reading a
    // clock field bare would take a negative minute, normalize it and publish a WRONG instant.
    // invariant: precision-first: refuse.
    EXPECT_FALSE(parse_health_app_ts("20171223--5:15:29:606").has_value()) << "negative hour";
    EXPECT_FALSE(parse_health_app_ts("20171223-22:-5:29:606").has_value()) << "negative minute";
    EXPECT_FALSE(parse_health_app_ts("20171223-22:15:-9:606").has_value()) << "negative second";
}

TEST(ParseHealthAppTs, AClockFieldWiderThanTwoDigitsIsRefused)
{
    // invariant: the accepted language stays EQUAL to what the claim predicate proves, so canon's
    // only production caller can hand this function nothing new.
    EXPECT_FALSE(parse_health_app_ts("20171223-221:15:29:606").has_value()) << "three-digit hour";
    EXPECT_FALSE(parse_health_app_ts("20171223-22:151:29:606").has_value()) << "three-digit minute";
    EXPECT_FALSE(parse_health_app_ts("20171223-22:15:299:606").has_value()) << "three-digit second";
}

TEST(ParseHealthAppTs, MillisecondsAreConsumedButNotRetained)
{
    // invariant: the trailing millisecond field is required as a TERMINATOR for the variable-width
    // second, yet its VALUE is discarded — resolution here is whole seconds.
    // invariant: two lines one millisecond apart are the same instant, which is what window
    // membership sees.
    const auto early{parse_health_app_ts("20171223-22:15:29:001")};
    const auto late{parse_health_app_ts("20171223-22:15:29:999")};
    ASSERT_TRUE(early.has_value() && late.has_value());
    EXPECT_EQ(epoch_of(*early), epoch_of(*late))
        << "sub-second digits changed the parsed instant — this parser resolves to seconds";
}

TEST(ParseHealthAppTs, MissingMillisecondTerminatorRefused)
{
    // invariant: without the terminating separator the second field has no end, so the parser
    // refuses rather than guessing where it stops.
    EXPECT_FALSE(parse_health_app_ts("20171223-22:15:29").has_value());
    EXPECT_FALSE(parse_health_app_ts("20171223-22:15:29.606").has_value()) << "dot, not colon";
}

TEST(ParseHealthAppTs, MalformedInputRefused)
{
    EXPECT_FALSE(parse_health_app_ts("").has_value());
    EXPECT_FALSE(parse_health_app_ts("20171223").has_value());
    EXPECT_FALSE(parse_health_app_ts("20171223 22:15:29:606").has_value()) << "space, not dash";
    EXPECT_FALSE(parse_health_app_ts("2017122x-22:15:29:606").has_value());
    EXPECT_FALSE(parse_health_app_ts("20171223-xx:15:29:606").has_value());
    EXPECT_FALSE(parse_health_app_ts("20171223-22:1x:29:606").has_value());
}

TEST(ParseLog4jTimestamp, CommaAndDotSubSecondSeparatorsAreEquivalent)
{
    // invariant: this dialect emits one sub-second separator under its default layout and the other
    // under a locale-driven one; both are the same instant, and refusing one drops half a dialect.
    const auto expected{utc_epoch(2024, 1, 15, 10, 30, 0)};
    EXPECT_PARSES_TO(parse_log4j_timestamp("2024-01-15 10:30:00,123"), expected);
    EXPECT_PARSES_TO(parse_log4j_timestamp("2024-01-15 10:30:00.123"), expected);
}

TEST(ParseLog4jTimestamp, SubSecondDigitsDoNotChangeTheInstant)
{
    const auto early{parse_log4j_timestamp("2024-01-15 10:30:00,001")};
    const auto late{parse_log4j_timestamp("2024-01-15 10:30:00,999")};
    ASSERT_TRUE(early.has_value() && late.has_value());
    EXPECT_EQ(epoch_of(*early), epoch_of(*late));
}

TEST(ParseLog4jTimestamp, IgnoresTrailingLineContent)
{
    EXPECT_PARSES_TO(parse_log4j_timestamp("2024-01-15 10:30:00,123 ERROR [main] boom"),
                     utc_epoch(2024, 1, 15, 10, 30, 0));
}

TEST(ParseLog4jTimestamp, MissingSubSecondSeparatorRefused)
{
    // invariant: the separator at the fixed index is the DISCRIMINATOR against plain
    // ISO-with-space, which another parser owns.
    // invariant: accepting a bare one here would let two parsers claim the same line and make
    // dialect detection order-dependent.
    EXPECT_FALSE(parse_log4j_timestamp("2024-01-15 10:30:00").has_value());
    EXPECT_FALSE(parse_log4j_timestamp("2024-01-15 10:30:00 123").has_value());
    EXPECT_FALSE(parse_log4j_timestamp("2024-01-15T10:30:00,123").has_value()) << "T, not space";
}

TEST(ParseLog4jTimestamp, MalformedInputRefused)
{
    EXPECT_FALSE(parse_log4j_timestamp("").has_value());
    EXPECT_FALSE(parse_log4j_timestamp("2024-01-15 10:30:0,123").has_value()) << "22 chars";
    EXPECT_FALSE(parse_log4j_timestamp("2024/01/15 10:30:00,123").has_value());
    EXPECT_FALSE(parse_log4j_timestamp("2024-01-15 10-30-00,123").has_value());
    EXPECT_FALSE(parse_log4j_timestamp("20x4-01-15 10:30:00,123").has_value());
}

TEST(ParseNginxErrorTs, ValidTimestampParsed)
{
    EXPECT_PARSES_TO(parse_nginx_error_ts("2024/01/15 10:30:00"),
                     utc_epoch(2024, 1, 15, 10, 30, 0));
}

TEST(ParseNginxErrorTs, IgnoresTrailingLineContent)
{
    EXPECT_PARSES_TO(
        parse_nginx_error_ts("2024/01/15 10:30:00 [error] 1234#0: *5 connect() failed"),
        utc_epoch(2024, 1, 15, 10, 30, 0));
}

TEST(ParseNginxErrorTs, MalformedInputRefused)
{
    EXPECT_FALSE(parse_nginx_error_ts("2024/01/15 10:30:0").has_value()) << "18 chars";
    EXPECT_FALSE(parse_nginx_error_ts("").has_value());
    EXPECT_FALSE(parse_nginx_error_ts("2024-01-15 10:30:00").has_value()) << "dashes, not slashes";
    EXPECT_FALSE(parse_nginx_error_ts("24/01/15 10:30:00xx").has_value())
        << "a two-digit year shifts every field — this is the Spark format, not nginx's";
    EXPECT_FALSE(parse_nginx_error_ts("2024/01/15T10:30:00").has_value());
    EXPECT_FALSE(parse_nginx_error_ts("2024/01/15 10:30-00").has_value());
}

TEST(DialectTimestampCalendar, LeapDayAndYearBoundariesAgreeWithTheCivilCalendar)
{
    // invariant: the civil-calendar helper is hand-rolled for cross-stdlib determinism, so its
    // leap-year rule is pinned against the standard library on the three cases naivety gets wrong.
    EXPECT_PARSES_TO(parse_nginx_error_ts("2024/02/29 12:00:00"), utc_epoch(2024, 2, 29, 12, 0, 0));
    EXPECT_PARSES_TO(parse_nginx_error_ts("2000/02/29 12:00:00"), utc_epoch(2000, 2, 29, 12, 0, 0));
    EXPECT_PARSES_TO(parse_nginx_error_ts("2100/03/01 00:00:00"), utc_epoch(2100, 3, 1, 0, 0, 0));
    EXPECT_PARSES_TO(parse_nginx_error_ts("1999/12/31 23:59:59"),
                     utc_epoch(1999, 12, 31, 23, 59, 59));
    EXPECT_PARSES_TO(parse_nginx_error_ts("2000/01/01 00:00:00"), utc_epoch(2000, 1, 1, 0, 0, 0));
}
