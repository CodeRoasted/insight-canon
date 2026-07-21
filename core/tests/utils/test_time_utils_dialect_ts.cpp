// NOLINTBEGIN
// Unit tests: allow short identifiers and test-specific patterns
// core/tests/utils/test_time_utils_dialect_ts.cpp
//
// Unit tests for the DIALECT timestamp parsers in insight::utils — the eight formats each
// owned by one strategy (epoch, OTLP unix-nano, HDFS compact, Spark short-year-slash,
// Apache error, HealthApp, Log4j, Nginx error). The three cross-dialect formats
// (ISO 8601 / BSD syslog / CLF) and the log-level inference live in test_time_utils.cpp.
//
// HOMING (Kleio): pure functions over a string_view. Unit tests in canon, not strategy
// tests — a strategy test exercises ONE well-formed line and cannot reach the malformed
// and boundary branches, which is exactly where a timestamp parser silently produces a
// wrong instant instead of nullopt. Every one of these eight was reachable only through
// its strategy's happy path before this file.
//
// ORACLE (anti-vacuity): expected instants are built from std::chrono::sys_days — the
// standard library's civil calendar — NOT from the parsers' own utc_mktime helper.
// Re-using utc_mktime would make SUT == ORACLE and the whole file tautological: a broken
// leap-year rule would agree with itself. sys_days is an independent implementation.
//
// The parsers deliberately gate on a MINIMUM length and read a fixed prefix: they are
// called on the head of a log line, so trailing content must be ignored, not refused.
// That looseness is pinned below (…IgnoresTrailingLineContent) because tightening it to
// an exact-length check would silently break every strategy that relies on it.

#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::utils;

namespace
{

/// Seconds-since-epoch of a UTC civil instant, computed by the standard library's civil
/// calendar. This is the independent oracle — see the ORACLE note above.
[[nodiscard]] std::int64_t utc_epoch(int year_value, unsigned month_value, unsigned day_value,
                                     int hour_value, int minute_value, int second_value)
{
    using namespace std::chrono;
    const sys_days civil_day{std::chrono::year{year_value} / month_value / day_value};
    return (civil_day + hours{hour_value} + minutes{minute_value} + seconds{second_value})
        .time_since_epoch()
        .count();
}

/// Seconds-since-epoch of a parsed Timestamp.
[[nodiscard]] std::int64_t epoch_of(Timestamp timestamp)
{
    return static_cast<std::int64_t>(std::chrono::system_clock::to_time_t(timestamp));
}

/// Assert a parser produced exactly the given UTC civil instant, printing both sides —
/// a timestamp test that fails with "false is not true" costs a debugger session.
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

// ─────────────────────────────────────────────────────────────────────────────
// parse_epoch_timestamp — Unix epoch seconds
// ─────────────────────────────────────────────────────────────────────────────

TEST(ParseEpochTimestamp, ValidSecondsParsed)
{
    EXPECT_PARSES_TO(parse_epoch_timestamp("1117838570"), 1117838570);
    EXPECT_PARSES_TO(parse_epoch_timestamp("0"), 0);
}

TEST(ParseEpochTimestamp, MillisecondEpochIsRefusedNotSilentlyReadAsSeconds)
{
    // 13 digits is the JavaScript/Java millisecond epoch — the single most common way a
    // timestamp column arrives wrong. Read as seconds it would place the record in the
    // year 55000 and silently poison every window boundary derived from it. The 12-digit
    // cap must refuse it. (12 digits ≈ year 33658, so no real log second is lost.)
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
    // Trailing content: unlike the fixed-layout parsers below, this one consumes the WHOLE
    // view (from_chars ptr == end), so a partial number is a refusal, not a prefix parse.
    EXPECT_FALSE(parse_epoch_timestamp("1117838570.5").has_value());
    EXPECT_FALSE(parse_epoch_timestamp("1117838570x").has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// parse_unix_nano_timestamp — OTLP timeUnixNano
// ─────────────────────────────────────────────────────────────────────────────

TEST(ParseUnixNanoTimestamp, EpochNanosParsedAsEventTime)
{
    // OTLP carries event time in nanoseconds; without it there is no event-time axis and
    // the ingest yields zero windows (see the OTEL ingestion contract).
    EXPECT_PARSES_TO(parse_unix_nano_timestamp("1705314600000000000"),
                     utc_epoch(2024, 1, 15, 10, 30, 0));
    EXPECT_PARSES_TO(parse_unix_nano_timestamp("0"), 0);
}

TEST(ParseUnixNanoTimestamp, SubSecondNanosTruncateTowardTheSecond)
{
    // The integer duration_cast truncates; the OTLP producer emits millisecond-granular
    // nanos, so window membership stays bit-identical across stdlibs (D-OTEL-3).
    EXPECT_PARSES_TO(parse_unix_nano_timestamp("1705314600999000000"),
                     utc_epoch(2024, 1, 15, 10, 30, 0));
}

TEST(ParseUnixNanoTimestamp, OverflowingAndMalformedInputRefused)
{
    // 20 digits exceeds int64 nanoseconds (max ≈ 9.2e18) — refused at the LENGTH gate,
    // before from_chars can overflow.
    EXPECT_FALSE(parse_unix_nano_timestamp("12345678901234567890").has_value())
        << "a 20-digit nano value was accepted — the int64 overflow guard is not holding";
    EXPECT_FALSE(parse_unix_nano_timestamp("").has_value());
    EXPECT_FALSE(parse_unix_nano_timestamp("-1705312200000000000").has_value());
    EXPECT_FALSE(parse_unix_nano_timestamp("1705312200000000000 ").has_value());
    EXPECT_FALSE(parse_unix_nano_timestamp("1.7053122e18").has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// parse_compact_date_time — HDFS "YYMMDD" + "HHMMSS"
// ─────────────────────────────────────────────────────────────────────────────

TEST(ParseCompactDateTime, ValidPairParsed)
{
    EXPECT_PARSES_TO(parse_compact_date_time("240115", "103000"),
                     utc_epoch(2024, 1, 15, 10, 30, 0));
}

TEST(ParseCompactDateTime, TwoDigitYearPivotsAt70)
{
    // The pivot decides a CENTURY. An off-by-one here moves a record 100 years and is
    // invisible in any happy-path strategy test — both sides of the boundary are pinned.
    EXPECT_PARSES_TO(parse_compact_date_time("691231", "235959"),
                     utc_epoch(2069, 12, 31, 23, 59, 59));
    EXPECT_PARSES_TO(parse_compact_date_time("700101", "000000"), utc_epoch(1970, 1, 1, 0, 0, 0));
}

TEST(ParseCompactDateTime, WidthIsExactOnBothFields)
{
    // Unlike the line-prefix parsers, this one receives two ALREADY-SPLIT tokens, so a
    // wrong width means a wrong split upstream and must refuse rather than guess.
    EXPECT_FALSE(parse_compact_date_time("24015", "103000").has_value());
    EXPECT_FALSE(parse_compact_date_time("2401150", "103000").has_value());
    EXPECT_FALSE(parse_compact_date_time("240115", "10300").has_value());
    EXPECT_FALSE(parse_compact_date_time("240115", "1030000").has_value());
    EXPECT_FALSE(parse_compact_date_time("", "").has_value());
    EXPECT_FALSE(parse_compact_date_time("24011a", "103000").has_value());
    EXPECT_FALSE(parse_compact_date_time("240115", "10:000").has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// parse_short_year_slash — Spark "YY/MM/DD HH:MM:SS"
// ─────────────────────────────────────────────────────────────────────────────

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
    // Called on the head of a Spark log line — the rest of the line must not refuse it.
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

// ─────────────────────────────────────────────────────────────────────────────
// parse_apache_error_ts — "Sun Dec 04 04:47:44 2005"
// ─────────────────────────────────────────────────────────────────────────────

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
    // DECLARED behaviour, pinned so it is a decision rather than an accident: the weekday
    // is redundant with the date, so it is skipped rather than cross-checked. A log line
    // with a wrong weekday still yields the right instant.
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

// ─────────────────────────────────────────────────────────────────────────────
// parse_health_app_ts — "YYYYMMDD-HH:MM:SS:mmm" (variable-width hour/second)
// ─────────────────────────────────────────────────────────────────────────────

TEST(ParseHealthAppTs, ValidTimestampParsed)
{
    EXPECT_PARSES_TO(parse_health_app_ts("20171223-22:15:29:606"),
                     utc_epoch(2017, 12, 23, 22, 15, 29));
}

TEST(ParseHealthAppTs, SingleDigitHourAndSecondParsed)
{
    // The format is genuinely variable-width; a fixed-offset reader would mis-slice these.
    EXPECT_PARSES_TO(parse_health_app_ts("20171223-9:15:29:606"),
                     utc_epoch(2017, 12, 23, 9, 15, 29));
    EXPECT_PARSES_TO(parse_health_app_ts("20171223-22:15:9:606"),
                     utc_epoch(2017, 12, 23, 22, 15, 9));
    EXPECT_PARSES_TO(parse_health_app_ts("20171223-9:15:9:606"), utc_epoch(2017, 12, 23, 9, 15, 9));
}

TEST(ParseHealthAppTs, MillisecondsAreConsumedButNotRetained)
{
    // The trailing ":mmm" is required as a TERMINATOR for the variable-width second, yet
    // its value is discarded — Timestamp resolution here is whole seconds. Two lines one
    // millisecond apart are the same instant, which is what window membership sees.
    const auto early{parse_health_app_ts("20171223-22:15:29:001")};
    const auto late{parse_health_app_ts("20171223-22:15:29:999")};
    ASSERT_TRUE(early.has_value() && late.has_value());
    EXPECT_EQ(epoch_of(*early), epoch_of(*late))
        << "sub-second digits changed the parsed instant — this parser resolves to seconds";
}

TEST(ParseHealthAppTs, MissingMillisecondTerminatorRefused)
{
    // Without the terminating ':' the second field has no end, so the parser refuses
    // rather than guessing where it stops.
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

// ─────────────────────────────────────────────────────────────────────────────
// parse_log4j_timestamp — "2024-01-15 10:30:00,123" / ".123"
// ─────────────────────────────────────────────────────────────────────────────

TEST(ParseLog4jTimestamp, CommaAndDotSubSecondSeparatorsAreEquivalent)
{
    // Log4j emits ',' with the default layout and '.' under a locale-driven one. Both are
    // the same instant; treating one as malformed would drop half a dialect's lines.
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
    // The separator at index 19 is the discriminator against plain ISO-with-space, which
    // parse_iso8601 owns. Accepting a bare "2024-01-15 10:30:00" here would let two
    // parsers claim the same line and make dialect detection order-dependent.
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

// ─────────────────────────────────────────────────────────────────────────────
// parse_nginx_error_ts — "YYYY/MM/DD HH:MM:SS"
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// Cross-parser: leap-day and year-boundary arithmetic
// ─────────────────────────────────────────────────────────────────────────────

TEST(DialectTimestampCalendar, LeapDayAndYearBoundariesAgreeWithTheCivilCalendar)
{
    // utc_mktime is hand-rolled (no timegm, for cross-stdlib determinism). These pin its
    // leap-year rule against std::chrono for the three cases the naive rule gets wrong:
    // 2024 (÷4, leap), 2000 (÷400, leap), 2100 (÷100 not ÷400, NOT leap).
    EXPECT_PARSES_TO(parse_nginx_error_ts("2024/02/29 12:00:00"), utc_epoch(2024, 2, 29, 12, 0, 0));
    EXPECT_PARSES_TO(parse_nginx_error_ts("2000/02/29 12:00:00"), utc_epoch(2000, 2, 29, 12, 0, 0));
    EXPECT_PARSES_TO(parse_nginx_error_ts("2100/03/01 00:00:00"), utc_epoch(2100, 3, 1, 0, 0, 0));
    EXPECT_PARSES_TO(parse_nginx_error_ts("1999/12/31 23:59:59"),
                     utc_epoch(1999, 12, 31, 23, 59, 59));
    EXPECT_PARSES_TO(parse_nginx_error_ts("2000/01/01 00:00:00"), utc_epoch(2000, 1, 1, 0, 0, 0));
}

// NOLINTEND
