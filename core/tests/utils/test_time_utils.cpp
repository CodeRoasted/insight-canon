
// invariant: the CROSS-DIALECT time parsers and the level inference; the eight per-dialect parsers
// live in the sibling file.
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::utils;

static std::time_t to_tt(Timestamp ts)
{
    return std::chrono::system_clock::to_time_t(ts);
}

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
    // invariant: a space separator instead of the date-time letter must parse — both spellings
    // reach the same instant.
    EXPECT_TRUE(parse_iso8601("2024-01-15 10:30:00").has_value());
}

TEST(ParseISO8601, FractionalSecondsIgnored)
{
    auto t1{parse_iso8601("2024-01-15T10:30:00Z")};
    auto t2{parse_iso8601("2024-01-15T10:30:00.999Z")};
    ASSERT_TRUE(t1.has_value() && t2.has_value());
    // invariant: fractional seconds are STRIPPED, so two whole-second timestamps that differ only
    // below the second compare equal.
    EXPECT_EQ(to_tt(*t1), to_tt(*t2));
}

TEST(ParseISO8601, PositiveTimezoneOffsetNormalisedToUTC)
{
    auto plus0530{parse_iso8601("2026-03-23T14:00:00+05:30")};
    auto utc{parse_iso8601("2026-03-23T08:30:00Z")};
    ASSERT_TRUE(plus0530.has_value() && utc.has_value());
    EXPECT_EQ(to_tt(*plus0530), to_tt(*utc));
}

TEST(ParseISO8601, NegativeTimezoneOffsetNormalisedToUTC)
{
    auto minus07{parse_iso8601("2024-01-15T20:00:00-07:00")};
    auto utc{parse_iso8601("2024-01-16T03:00:00Z")};
    ASSERT_TRUE(minus07.has_value() && utc.has_value());
    EXPECT_EQ(to_tt(*minus07), to_tt(*utc));
}

TEST(ParseISO8601, LeapDayFeb29ValidYear)
{
    // invariant: a leap day in a leap year must parse; the calendar rule is not assumed.
    auto t{parse_iso8601("2024-02-29T12:00:00Z")};
    EXPECT_TRUE(t.has_value());
}

TEST(ParseISO8601, LeapDayFeb29NonLeapYearNormalises)
{
    // invariant: a leap day in a NON-leap year is NORMALIZED rather than refused, and the function
    // must neither crash nor return an absence.
    auto t{parse_iso8601("2023-02-29T00:00:00Z")};
    EXPECT_TRUE(t.has_value());
    auto expected{parse_iso8601("2023-03-01T00:00:00Z")};
    ASSERT_TRUE(expected.has_value());
    EXPECT_EQ(to_tt(*t), to_tt(*expected));
}

TEST(ParseISO8601, CompactTimezoneWithoutColon)
{
    // invariant: a zone offset written without its colon must parse IDENTICALLY to the colon form.
    auto colon{parse_iso8601("2026-03-23T14:00:00+05:30")};
    auto nocolon{parse_iso8601("2026-03-23T14:00:00+0530")};
    ASSERT_TRUE(colon.has_value() && nocolon.has_value());
    EXPECT_EQ(to_tt(*colon), to_tt(*nocolon));
}

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
    // invariant: an unrecognised month abbreviation is refused rather than guessed.
    EXPECT_FALSE(parse_bsd_syslog_ts("Xxx 15 08:03:22").has_value());
}

TEST(ParseBSDSyslog, OutOfRangeDayNormalises)
{
    // invariant: an impossible day is normalized rather than refused, so the parser must not crash
    // and must return a value.
    EXPECT_TRUE(parse_bsd_syslog_ts("Feb 30 12:00:00").has_value());
}

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
    auto clf{parse_clf_timestamp("15/Jan/2024:20:00:00 -0700")};
    auto iso{parse_iso8601("2024-01-16T03:00:00Z")};
    ASSERT_TRUE(clf.has_value() && iso.has_value());
    EXPECT_EQ(to_tt(*clf), to_tt(*iso));
}

TEST(ParseCLF, PositiveTimezoneNormalisedToUTC)
{
    auto clf{parse_clf_timestamp("23/Mar/2026:14:00:00 +0530")};
    auto iso{parse_iso8601("2026-03-23T08:30:00Z")};
    ASSERT_TRUE(clf.has_value() && iso.has_value());
    EXPECT_EQ(to_tt(*clf), to_tt(*iso));
}

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
    // invariant: a failure word sitting MID-LINE on an informational line must not move the
    // verdict.
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
    // invariant: a runner that leads failure detail with a single letter puts the error word
    // mid-line, which must not classify.
    EXPECT_EQ(
        infer_leading_log_level("E   sqlalchemy.exc.OperationalError: connection to server failed"),
        LogLevel::Error);
}
TEST(InferLeadingLogLevel, BareErrorBodyWithoutLevelPrefix)
{
    EXPECT_EQ(infer_leading_log_level("Traceback (most recent call last):"), LogLevel::Error);
    EXPECT_EQ(infer_leading_log_level("connection refused to db host 10.0.0.7"), LogLevel::Error);
}

// invariant: the level commonly sits right after a leading timestamp — the dominant raw
// application shape — so stage 1 must skip the stamp and recover the level.
// invariant: it must not stop at the first alphabetic run; the regression was that stamp-led lines
// were silently Unknown, so the severity axis never saw a deployment flip.
TEST(InferLeadingLogLevel, IsoTimestampThenLevelTokenRecovered)
{
    EXPECT_EQ(infer_leading_log_level(
                  "2026-05-29T10:00:00 INFO request id=1 path=/api/users status=200 latency_ms=12"),
              LogLevel::Info)
        << "ISO ts + INFO";
    EXPECT_EQ(infer_leading_log_level(
                  "2026-05-29T11:00:01 ERROR request id=2 status=500 error=\"db timeout\""),
              LogLevel::Error)
        << "ISO ts + ERROR";
    EXPECT_EQ(infer_leading_log_level("2026-05-29T10:00:00 WARN db.pool exhausted"), LogLevel::Warn)
        << "ISO ts + WARN";
    EXPECT_EQ(infer_leading_log_level("2026-05-29T10:00:00.123456Z DEBUG cache miss key=user:42"),
              LogLevel::Debug)
        << "fractional ISO ts + DEBUG";
}
TEST(InferLeadingLogLevel, SpaceSeparatedDateTimeThenLevel)
{
    EXPECT_EQ(infer_leading_log_level("2026-05-29 10:00:00,123 WARN slow query 4200ms"),
              LogLevel::Warn)
        << "space-separated date+time prefix";
}
TEST(InferLeadingLogLevel, BracketedTimestampThenLevel)
{
    EXPECT_EQ(infer_leading_log_level("[2026-05-29T10:00:00Z] DEBUG connecting to db"),
              LogLevel::Debug)
        << "bracketed ISO timestamp prefix";
}
TEST(InferLeadingLogLevel, LeadingTraceTokenSurvivesTimestampSkip)
{
    // invariant: the fix must not treat the stamp's date-time letter as a skippable character and
    // shear a leading level word — the vocabulary is matched WHOLE.
    EXPECT_EQ(infer_leading_log_level("TRACE startup complete in 12ms"), LogLevel::Trace)
        << "leading TRACE must not be mangled";
}
TEST(InferLeadingLogLevel, TimestampThenNoLevelIsUnknown)
{
    // invariant: a stamp-led line with no level word stays Unknown — the skip must not
    // MANUFACTURE a level.
    EXPECT_EQ(infer_leading_log_level("2026-05-29T10:00:00 request GET /api/orders 200 14ms"),
              LogLevel::Unknown)
        << "ts-led, no level token";
}
// invariant: a failure word buried INSIDE a token must not be read as an error; the old
// raw-substring scan fired on it and promoted a benign new template to a high-severity verdict.
TEST(InferLeadingLogLevel, EmbeddedFailureSubstringIsNotError)
{
    EXPECT_EQ(infer_leading_log_level("Writing tsc-error-report.json"), LogLevel::Unknown)
        << "filename containing 'error' is not an error line";
    EXPECT_EQ(infer_leading_log_level("Compiled error_handler.ts successfully"), LogLevel::Unknown)
        << "identifier containing 'error' is not an error line";
    EXPECT_EQ(infer_leading_log_level("no errors found"), LogLevel::Unknown)
        << "negated plural 'errors' is not an error line";
    EXPECT_EQ(infer_leading_log_level("page fault handler registered"), LogLevel::Unknown)
        << "'fault' is not a standalone failure cue (only 'segfault' is)";
}
// invariant: a CamelCase error type in verdict register IS the cue even with no other error word on
// the line; a bare source echo no longer promotes, and the thrown verdict line still does.
// refs: SRC-D-MSK-4, SRC-D-OUT-4b
TEST(InferLeadingLogLevel, CamelCaseErrorTypeIsError)
{
    EXPECT_EQ(infer_leading_log_level("  ValueError: bad input"), LogLevel::Error)
        << "ValueError type name in verdict register (colon)";
    EXPECT_EQ(infer_leading_log_level("IOError: disk full"), LogLevel::Error)
        << "IOError type name, colon";
}
// invariant: the bare crash phrase carries no level keyword, so the lexicon is the only signal —
// and it is recovered as the ADJACENT PAIR, where either word alone stays benign.
TEST(InferLeadingLogLevel, BareSegmentationFaultIsError)
{
    EXPECT_EQ(infer_leading_log_level("Segmentation fault (core dumped)"), LogLevel::Error)
        << "OS crash line, no level prefix";
    EXPECT_EQ(infer_leading_log_level("running image segmentation on shard 3"), LogLevel::Unknown)
        << "bare 'segmentation' is benign";
}
// invariant: real logs wrap the level word in colour, and the escape's tail must not glue to the
// word.
TEST(InferLeadingLogLevel, AnsiColourWrappedLevelRecovered)
{
    EXPECT_EQ(infer_leading_log_level("tests/test_db.py::tc_0 \x1b[31mFAILED\x1b[0m [ 99%]"),
              LogLevel::Error)
        << "ANSI-wrapped FAILED";
    EXPECT_EQ(infer_leading_log_level("\x1b[31mERROR\x1b[0m: db connection refused"),
              LogLevel::Error)
        << "ANSI-wrapped leading ERROR";
    EXPECT_EQ(infer_leading_log_level("tests/test_api.py::tc_0 \x1b[32mPASSED\x1b[0m [ 0%]"),
              LogLevel::Unknown)
        << "ANSI-wrapped PASSED is not a level";
}

// invariant: the level is what the diff consumes, and it has TWO severity feeders where the glyph
// rule guarded only one.
// invariant: the explicit-level arm runs FIRST and is authoritative, so a passing test whose NAME
// embeds a level word was classified at an alerting tier, bypassing the guard.
// invariant: the pass GLYPH leading the line says it PASSED, so an alerting tier must demote.
// invariant: THE TEST HOLE THIS CLOSES — the earlier assertion was on the cue BOOLEAN and went
// green while the LEVEL of the same line was alerting: outcome-awareness at the wrong altitude.
// refs: SRC-D-OUT-1, SRC-D-OUT-1b
TEST(InferLeadingLogLevel, LeadingPassGlyphDemotesAlertingStage1Level)
{
    EXPECT_EQ(infer_leading_log_level("✔ start debugging failure (134ms)"), LogLevel::Unknown)
        << "leading ✔ + Stage-1 'failure' (Fatal) must demote to Unknown";
    EXPECT_EQ(infer_leading_log_level("✔ write_bash failure returns a non-empty error message"),
              LogLevel::Unknown)
        << "leading ✔ + Stage-1 'failure' (Fatal) must demote to Unknown";
    EXPECT_EQ(infer_leading_log_level("✔ maps status codes: ERROR → ERROR, missing → UNSET"),
              LogLevel::Unknown)
        << "leading ✔ + Stage-1 'ERROR' (Error) must demote to Unknown";
}
// invariant: real runners wrap the verdict glyph in colour too, so the escape must be skipped for
// the glyph to still be the leading outcome token.
TEST(InferLeadingLogLevel, AnsiWrappedLeadingPassGlyphDemotesStage1Level)
{
    EXPECT_EQ(
        infer_leading_log_level("\x1b[32m✔\x1b[0m write_bash failure returns a non-empty error"),
        LogLevel::Unknown)
        << "ANSI-wrapped leading ✔ + Stage-1 'failure' must demote to Unknown";
}
// invariant: the recall guard — a demotion requires a LEADING pass glyph, so a genuine leading
// failure word stops the walk and a summary with no leading glyph stays a failure.
// invariant: these are the rejected true-leading-only recall loss made into a standing assertion.
// refs: SRC-D-OUT-1b
TEST(InferLeadingLogLevel, GenuineLeadingFailureSurvivesWithoutPassGlyph)
{
    EXPECT_EQ(infer_leading_log_level("ERROR: db connection failed"), LogLevel::Error)
        << "leading failure WORD, no pass glyph — preserved";
    EXPECT_EQ(infer_leading_log_level("FATAL: kernel panic"), LogLevel::Fatal)
        << "leading FATAL, no pass glyph — preserved";
    EXPECT_EQ(infer_leading_log_level("[worker-3] ERROR connection refused"), LogLevel::Error)
        << "scope-prefixed ERROR (token 1, not 0) — preserved (the recall guard)";
    EXPECT_EQ(infer_leading_log_level("======== 25 passed, 5 failed ========"), LogLevel::Warn)
        << "SRC-D-CNT-1: '5 failed' is a count summary → demoted to Warn (surfaced, below per-item "
           "verdicts), not Error";
    // invariant: the symmetric disconfirm — the SAME line led by a FAIL glyph keeps its level,
    // which proves the guard demotes PASS glyphs only and never any glyph.
    EXPECT_EQ(infer_leading_log_level("✗ start debugging failure (134ms)"), LogLevel::Fatal)
        << "leading FAIL glyph ✗ must NOT demote — Stage-1 'failure' stays Fatal";
}

// invariant: one level past the glyph rule — not whether a pass glyph leads, but whether the line
// is a failure VERDICT at all.
// invariant: a non-verdict line carrying a failure NOUN must infer Unknown, and these route through
// the cue stage, so the cue fix alone demotes them with no explicit-level change.
// refs: SRC-D-OUT-4
TEST(InferLeadingLogLevel, InformationalFailureWordLineIsNotAlerting)
{
    EXPECT_EQ(infer_leading_log_level(
                  "Storing crash reports into 'D:\\a\\_work\\vscode\\.build\\crashes'"),
              LogLevel::Unknown)
        << "P3 rank-1 FP: informational startup line, NOUN 'crash' — not an error line";
    EXPECT_EQ(infer_leading_log_level("- deleting watched path emits watcher fail event"),
              LogLevel::Unknown)
        << "P3 rank-2 FP: mocha test description, 'fail' modifies 'event' — not an error line";
}
// invariant: the recall guard — a verdict-ANCHORED failure still infers its alerting level, so
// every anchor class survives.
TEST(InferLeadingLogLevel, VerdictAnchoredFailureLevelSurvives)
{
    EXPECT_EQ(infer_leading_log_level("fatal: expected 'packfile' but got EOF"), LogLevel::Fatal)
        << "'fatal:' colon-anchored — Stage-1 Fatal preserved";
    EXPECT_EQ(infer_leading_log_level("[worker-3] ERROR connection refused"), LogLevel::Error)
        << "scope-prefixed caps ERROR (token 1, not 0) — preserved";
    EXPECT_EQ(infer_leading_log_level("build failed after 4 retries"), LogLevel::Error)
        << "outcome verb 'failed' (Stage-2 cue) — still Error";
    EXPECT_EQ(infer_leading_log_level("Segmentation fault (core dumped)"), LogLevel::Error)
        << "the crash phrase — still Error";
}
// invariant: a LEADING bare level word with no register is classified authoritative by the
// explicit-level stage, which is the non-glyph form of the same unguarded feeder.
// invariant: the rule is that a leading level WORD is authoritative only when register-anchored or
// when it is the terminal or sole significant token.
// refs: SRC-D-OUT-4
TEST(InferLeadingLogLevel, LeadingBareLevelWordDemotedWhenUnanchored)
{
    EXPECT_EQ(infer_leading_log_level("error handling enabled for the worker pool"),
              LogLevel::Unknown)
        << "leading bare 'error' (lowercase, no register), more tokens follow — not a verdict";
    EXPECT_EQ(infer_leading_log_level("failure modes documented in the runbook"), LogLevel::Unknown)
        << "leading bare 'failure' (Stage-1 Fatal, no register) — not a verdict";
    EXPECT_EQ(infer_leading_log_level("ERROR: db connection failed"), LogLevel::Error)
        << "caps + colon — authoritative, preserved";
}

// invariant: a count-register failure word is a SUMMARY, so it must not confer an alerting tier —
// but it still surfaces, capped at a warning: demote, never suppress.
// invariant: the root was a counted summary read as a fatal verdict, outranking the named per-item
// failure it summarized.
// refs: SRC-D-CNT-1
TEST(InferLeadingLogLevel, CountRegisterSummaryCapsAtWarn)
{
    EXPECT_EQ(infer_leading_log_level("There was 1 failure:"), LogLevel::Warn)
        << "P5 root: '1 failure:' is a count summary → Warn, NOT the Fatal the colon would confer";
    EXPECT_EQ(infer_leading_log_level("Tests: 5 failed"), LogLevel::Warn)
        << "'5 failed' count summary → surfaced at Warn, below per-item verdicts";
    EXPECT_EQ(infer_leading_log_level("HTTP 500 error"), LogLevel::Warn)
        << "accepted precision boundary: '500 error' is count-preceded → Warn summary, not an "
           "Error verdict (an HTTP status is corroborated by 5xx-rate signals in-window, never "
           "shouted per line)";
    EXPECT_EQ(infer_leading_log_level("1 test failed"), LogLevel::Error)
        << "'failed' preceded by 'test' (not the count '1') — a real per-item failure, stays Error";
}

// invariant: a passing runner assertion whose description embeds failure vocabulary must not earn
// an alerting level, and the pass WORD demotes ONLY as the first significant token.
// refs: SRC-D-OUT-2
TEST(InferLeadingLogLevel, LeadingPassWordDemotesLevel)
{
    EXPECT_EQ(infer_leading_log_level("ok 1 - request failed and retried"), LogLevel::Unknown)
        << "TAP pass: 'ok' leads → the self-anchoring 'failed' is demoted to Unknown";
    EXPECT_EQ(infer_leading_log_level("success - worker crashed cleanly under SIGTERM"),
              LogLevel::Unknown)
        << "'success' leads → 'crashed' demoted";
    EXPECT_EQ(infer_leading_log_level("request failed and retried"), LogLevel::Error)
        << "no leading pass word — 'failed' stays an Error verdict";
    EXPECT_EQ(infer_leading_log_level("worker crashed but all 4 checks passed"), LogLevel::Error)
        << "'crashed' is the first significant token; a TRAILING 'passed' must not demote it";
}
