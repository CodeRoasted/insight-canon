// NOLINTBEGIN
// Unit tests: allow short identifiers and test-specific patterns
// core/tests/utils/test_time_utils.cpp
//
// Unit tests for insight::utils time-parsing functions.
//
// Tests cover:
//   - ISO 8601 valid/invalid inputs including leap-year Feb 29
//   - Timezone offset normalisation
//   - BSD syslog timestamp with an out-of-range day (normalisation vs crash)
//   - CLF timestamp with negative timezone
//   - parse_log_level: all canonical aliases, case-insensitivity, unknowns

#include <chrono>
#include <gtest/gtest.h>

#include "insight/core/types.hpp"
#include "insight/utils/time_utils.hpp"

using namespace insight;
using namespace insight::utils;

// Convenience: convert Timestamp to seconds-since-epoch for comparison.
static std::time_t to_tt(Timestamp ts)
{
    return std::chrono::system_clock::to_time_t(ts);
}

// ─────────────────────────────────────────────────────────────────────────────
// parse_iso8601
// ─────────────────────────────────────────────────────────────────────────────

TEST(ParseISO8601, ValidUtcReturnsValue)
{
    EXPECT_TRUE(parse_iso8601("2024-01-15T10:30:00Z").has_value());
}

TEST(ParseISO8601, TooShortReturnsNullopt)
{
    EXPECT_FALSE(parse_iso8601("2024").has_value());
    EXPECT_FALSE(parse_iso8601("2024-01").has_value());
}

TEST(ParseISO8601, SpaceSeparatorAccepted)
{
    // "2024-01-15 10:30:00" (space instead of T) must parse.
    EXPECT_TRUE(parse_iso8601("2024-01-15 10:30:00").has_value());
}

TEST(ParseISO8601, FractionalSecondsIgnored)
{
    auto t1{parse_iso8601("2024-01-15T10:30:00Z")};
    auto t2{parse_iso8601("2024-01-15T10:30:00.999Z")};
    ASSERT_TRUE(t1.has_value() && t2.has_value());
    // Fractional seconds are stripped; whole-second timestamps must be equal.
    EXPECT_EQ(to_tt(*t1), to_tt(*t2));
}

TEST(ParseISO8601, PositiveTimezoneOffsetNormalisedToUTC)
{
    // 14:00:00 +05:30  →  UTC 08:30:00
    auto plus0530{parse_iso8601("2026-03-23T14:00:00+05:30")};
    auto utc{parse_iso8601("2026-03-23T08:30:00Z")};
    ASSERT_TRUE(plus0530.has_value() && utc.has_value());
    EXPECT_EQ(to_tt(*plus0530), to_tt(*utc));
}

TEST(ParseISO8601, NegativeTimezoneOffsetNormalisedToUTC)
{
    // 20:00:00 -0700   →  UTC 03:00:00 next day
    auto minus07{parse_iso8601("2024-01-15T20:00:00-07:00")};
    auto utc{parse_iso8601("2024-01-16T03:00:00Z")};
    ASSERT_TRUE(minus07.has_value() && utc.has_value());
    EXPECT_EQ(to_tt(*minus07), to_tt(*utc));
}

TEST(ParseISO8601, LeapDayFeb29ValidYear)
{
    // 2024 is a leap year: Feb 29 must parse successfully.
    auto t{parse_iso8601("2024-02-29T12:00:00Z")};
    EXPECT_TRUE(t.has_value());
}

TEST(ParseISO8601, LeapDayFeb29NonLeapYearNormalises)
{
    // 2023 is NOT a leap year. utc_mktime normalises Feb 29 → March 1.
    // The function must not crash or return nullopt.
    auto t{parse_iso8601("2023-02-29T00:00:00Z")};
    EXPECT_TRUE(t.has_value());
    // Normalised date should equal 2023-03-01T00:00:00Z.
    auto expected{parse_iso8601("2023-03-01T00:00:00Z")};
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(to_tt(*t), to_tt(*expected));
}

TEST(ParseISO8601, CompactTimezoneWithoutColon)
{
    // "+0530" (no colon) must parse identically to "+05:30".
    auto colon{parse_iso8601("2026-03-23T14:00:00+05:30")};
    auto nocolon{parse_iso8601("2026-03-23T14:00:00+0530")};
    ASSERT_TRUE(colon.has_value() && nocolon.has_value());
    EXPECT_EQ(to_tt(*colon), to_tt(*nocolon));
}

// ─────────────────────────────────────────────────────────────────────────────
// parse_bsd_syslog_ts
// ─────────────────────────────────────────────────────────────────────────────

TEST(ParseBSDSyslog, ValidTimestampParsed)
{
    EXPECT_TRUE(parse_bsd_syslog_ts("Jan 15 08:03:22").has_value());
}

TEST(ParseBSDSyslog, SingleDigitDayParsed)
{
    EXPECT_TRUE(parse_bsd_syslog_ts("Jan  1 08:03:22").has_value());
}

TEST(ParseBSDSyslog, TooShortReturnsNullopt)
{
    EXPECT_FALSE(parse_bsd_syslog_ts("Jan 1").has_value());
}

TEST(ParseBSDSyslog, InvalidMonthReturnsNullopt)
{
    // "Xxx" is not a recognised month abbreviation.
    EXPECT_FALSE(parse_bsd_syslog_ts("Xxx 15 08:03:22").has_value());
}

TEST(ParseBSDSyslog, OutOfRangeDayNormalises)
{
    // Feb 30 does not exist. utc_mktime normalises it (Feb 30 → March 2 in a
    // non-leap year). Must not crash and must return a value.
    EXPECT_TRUE(parse_bsd_syslog_ts("Feb 30 12:00:00").has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// parse_clf_timestamp
// ─────────────────────────────────────────────────────────────────────────────

TEST(ParseCLF, ValidTimestampParsed)
{
    EXPECT_TRUE(parse_clf_timestamp("10/Oct/2000:13:55:36 -0700").has_value());
}

TEST(ParseCLF, TooShortReturnsNullopt)
{
    EXPECT_FALSE(parse_clf_timestamp("10/Oct/2000").has_value());
}

TEST(ParseCLF, NegativeTimezoneNormalisedToUTC)
{
    // 20:00:00 -0700 → UTC 03:00:00 next day.
    auto clf{parse_clf_timestamp("15/Jan/2024:20:00:00 -0700")};
    auto iso{parse_iso8601("2024-01-16T03:00:00Z")};
    ASSERT_TRUE(clf.has_value() && iso.has_value());
    EXPECT_EQ(to_tt(*clf), to_tt(*iso));
}

TEST(ParseCLF, PositiveTimezoneNormalisedToUTC)
{
    // 14:00:00 +0530 → UTC 08:30:00.
    auto clf{parse_clf_timestamp("23/Mar/2026:14:00:00 +0530")};
    auto iso{parse_iso8601("2026-03-23T08:30:00Z")};
    ASSERT_TRUE(clf.has_value() && iso.has_value());
    EXPECT_EQ(to_tt(*clf), to_tt(*iso));
}

// ─────────────────────────────────────────────────────────────────────────────
// parse_log_level
// ─────────────────────────────────────────────────────────────────────────────

TEST(ParseLogLevel, TraceAlias)
{
    EXPECT_EQ(parse_log_level("trace"), LogLevel::Trace);
}
TEST(ParseLogLevel, DebugAlias)
{
    EXPECT_EQ(parse_log_level("debug"), LogLevel::Debug);
}
TEST(ParseLogLevel, DbgAlias)
{
    EXPECT_EQ(parse_log_level("dbg"), LogLevel::Debug);
}
TEST(ParseLogLevel, InfoAlias)
{
    EXPECT_EQ(parse_log_level("info"), LogLevel::Info);
}
TEST(ParseLogLevel, InformationAlias)
{
    EXPECT_EQ(parse_log_level("information"), LogLevel::Info);
}
TEST(ParseLogLevel, WarnAlias)
{
    EXPECT_EQ(parse_log_level("warn"), LogLevel::Warn);
}
TEST(ParseLogLevel, WarningAlias)
{
    EXPECT_EQ(parse_log_level("warning"), LogLevel::Warn);
}
TEST(ParseLogLevel, ErrorAlias)
{
    EXPECT_EQ(parse_log_level("error"), LogLevel::Error);
}
TEST(ParseLogLevel, ErrAlias)
{
    EXPECT_EQ(parse_log_level("err"), LogLevel::Error);
}
TEST(ParseLogLevel, FatalAlias)
{
    EXPECT_EQ(parse_log_level("fatal"), LogLevel::Fatal);
}
TEST(ParseLogLevel, CriticalAlias)
{
    EXPECT_EQ(parse_log_level("critical"), LogLevel::Fatal);
}
TEST(ParseLogLevel, CritAlias)
{
    EXPECT_EQ(parse_log_level("crit"), LogLevel::Fatal);
}

TEST(ParseLogLevel, CaseInsensitive)
{
    EXPECT_EQ(parse_log_level("INFO"), LogLevel::Info);
    EXPECT_EQ(parse_log_level("Warning"), LogLevel::Warn);
    EXPECT_EQ(parse_log_level("ERROR"), LogLevel::Error);
    EXPECT_EQ(parse_log_level("FATAL"), LogLevel::Fatal);
}

TEST(ParseLogLevel, UnknownReturnsUnknown)
{
    EXPECT_EQ(parse_log_level("verbose"), LogLevel::Unknown);
    EXPECT_EQ(parse_log_level(""), LogLevel::Unknown);
    EXPECT_EQ(parse_log_level("xyz"), LogLevel::Unknown);
    EXPECT_EQ(parse_log_level("42"), LogLevel::Unknown);
}

// ───────────────────────────────────────────────────────────────────────────
// infer_leading_log_level — raw-text fallback level recovery (leading token)
// ───────────────────────────────────────────────────────────────────────────
TEST(InferLeadingLogLevel, LeadingErrorToken)
{
    EXPECT_EQ(infer_leading_log_level("ERROR connection refused to db host 10.0.0.7"),
              LogLevel::Error);
}
TEST(InferLeadingLogLevel, LeadingWarnToken)
{
    EXPECT_EQ(infer_leading_log_level("WARN db.pool exhausted, waiting 5000ms"), LogLevel::Warn);
}
TEST(InferLeadingLogLevel, LeadingInfoToken)
{
    EXPECT_EQ(infer_leading_log_level("INFO request GET /healthz 200 1ms"), LogLevel::Info);
}
TEST(InferLeadingLogLevel, FailedTestResultIsError)
{
    EXPECT_EQ(infer_leading_log_level("FAILED tests/orders.spec.ts"), LogLevel::Error);
}
TEST(InferLeadingLogLevel, BracketedGithubActionsMarkerIsError)
{
    EXPECT_EQ(infer_leading_log_level("##[error]Process completed with exit code 1"),
              LogLevel::Error);
}
TEST(InferLeadingLogLevel, MidLineErrorWordDoesNotMisclassify)
{
    // "error rate" sits mid-line on an INFO line — must stay Info, never Error.
    EXPECT_EQ(infer_leading_log_level("INFO [t=30s] 4200 req/s, error rate 0/10000"),
              LogLevel::Info);
}
TEST(InferLeadingLogLevel, NoLeadingLevelIsUnknown)
{
    EXPECT_EQ(infer_leading_log_level("request GET /api/orders 200 14ms"), LogLevel::Unknown);
    EXPECT_EQ(infer_leading_log_level("PASS tests/auth.spec.ts"), LogLevel::Unknown);
}
TEST(InferLeadingLogLevel, EmptyIsUnknown)
{
    EXPECT_EQ(infer_leading_log_level(""), LogLevel::Unknown);
}
TEST(InferLeadingLogLevel, PytestErrorBodyInferredFromHead)
{
    // pytest leads failure detail with "E   "; the error word is mid-line.
    EXPECT_EQ(
        infer_leading_log_level("E   sqlalchemy.exc.OperationalError: connection to server failed"),
        LogLevel::Error);
}
TEST(InferLeadingLogLevel, BareErrorBodyWithoutLevelPrefix)
{
    EXPECT_EQ(infer_leading_log_level("Traceback (most recent call last):"), LogLevel::Error);
    EXPECT_EQ(infer_leading_log_level("connection refused to db host 10.0.0.7"), LogLevel::Error);
}

// NOLINTEND
