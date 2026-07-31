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

#include <gtest/gtest.h>

import insight.canon.test;

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

// The level commonly sits right after a leading timestamp — the dominant raw
// app-log shape ("<ts> LEVEL message"). Stage 1 must skip the timestamp (incl.
// the ISO "T"/"Z" date artifacts) and recover the level, not stop at the first
// alpha run. Regression: ts-led INFO/WARN/DEBUG lines were silently Unknown, so
// the severity-salience axis never saw an INFO→ERROR deployment flip.
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
    // "<date> <time>,<millis> LEVEL …" (e.g. Python logging default).
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
    // Guard: the fix must not treat the ISO "T" as a skippable char and shear the
    // leading "TRACE"/"T…" tokens. The level vocabulary is matched whole.
    EXPECT_EQ(infer_leading_log_level("TRACE startup complete in 12ms"), LogLevel::Trace)
        << "leading TRACE must not be mangled";
}
TEST(InferLeadingLogLevel, TimestampThenNoLevelIsUnknown)
{
    // A ts-led line with no level word stays Unknown — the timestamp skip must
    // not manufacture a level, and no error/warn keyword is present for stage 2.
    EXPECT_EQ(infer_leading_log_level("2026-05-29T10:00:00 request GET /api/orders 200 14ms"),
              LogLevel::Unknown)
        << "ts-led, no level token";
}
// Regression: a failure word buried INSIDE a token (a filename / identifier /
// plural negation) must NOT be read as Error. The old stage-2 raw-substring scan
// fired on the embedded "error", which downstream promoted a benign NEW template
// to HIGH "New error" in the eidos diff. Stage 2 is now token-aware.
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
// Recall guard: a CamelCase exception type in verdict register is the failure cue even with
// no other error word on the line. D-MSK-4 (2026-07-21): the type anchors ONLY in verdict
// register (colon/caps/bracket/✗-led); a bare `raise ValueError(...)` source echo no longer
// promotes to Error — but the thrown `ValueError: …` verdict line still does.
TEST(InferLeadingLogLevel, CamelCaseErrorTypeIsError)
{
    EXPECT_EQ(infer_leading_log_level("  ValueError: bad input"), LogLevel::Error)
        << "ValueError type name in verdict register (colon)";
    EXPECT_EQ(infer_leading_log_level("IOError: disk full"), LogLevel::Error)
        << "IOError type name, colon";
}
// A bare OS/shell crash carries no level keyword, so the failure lexicon is the
// only signal — "Segmentation fault" must be recovered as Error (as the adjacent
// pair; a lone "segmentation"/"fault" is benign and stays Unknown).
TEST(InferLeadingLogLevel, BareSegmentationFaultIsError)
{
    EXPECT_EQ(infer_leading_log_level("Segmentation fault (core dumped)"), LogLevel::Error)
        << "OS crash line, no level prefix";
    EXPECT_EQ(infer_leading_log_level("running image segmentation on shard 3"), LogLevel::Unknown)
        << "bare 'segmentation' is benign";
}
// Real CI logs wrap the level/result word in ANSI SGR colour (`ESC[31mFAILED`).
// The escape's `31m` tail must not glue to the word — the cue is still recovered.
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

// ── SRC-D-OUT-1b — outcome-awareness at the LEVEL altitude (§4A.6) ────────────────
// infer_leading_log_level is what the diff consumes (→ dominant_level → salience →
// eidos NewErrorPattern). It has TWO severity feeders and D-OUT-1 guarded only one:
//   • Stage 2 (contains_failure_cue) — D-OUT-1-guarded, caps at Error.
//   • Stage 1 (explicit level token via parse_log_level) — runs FIRST, authoritative,
//     UNGUARDED: the bare words `failure`/`fatal`/`critical` → Fatal, `error` → Error.
// So a passing test whose NAME embeds an explicit level word ("✔ … failure …") is
// classified Fatal by Stage 1, bypassing the glyph guard, and the diff fires
// NewErrorPattern [CRITICAL] on a GREEN test (the cardinal FP — the §6.7 reassess P3
// storm). The pass GLYPH leading the line says it PASSED → an alerting tier (Warn/
// Error/Fatal) must demote to Unknown.
//
// THE TEST HOLE THIS CLOSES: the D-OUT-1 RED asserted contains_failure_cue (the cue
// boolean) and went green while the LEVEL of the same line was Fatal — outcome-
// awareness asserted at the wrong altitude. These cases assert the LEVEL the diff
// actually consumes. [[sift-failure-lexicon-must-be-outcome-aware]]
TEST(InferLeadingLogLevel, LeadingPassGlyphDemotesAlertingStage1Level)
{
    // "failure" is a parse_log_level word → Stage 1 Fatal (unguarded). ✔ leads ⇒ demote.
    EXPECT_EQ(infer_leading_log_level("✔ start debugging failure (134ms)"), LogLevel::Unknown)
        << "leading ✔ + Stage-1 'failure' (Fatal) must demote to Unknown";
    EXPECT_EQ(infer_leading_log_level("✔ write_bash failure returns a non-empty error message"),
              LogLevel::Unknown)
        << "leading ✔ + Stage-1 'failure' (Fatal) must demote to Unknown";
    // "ERROR" is a parse_log_level word → Stage 1 Error (unguarded). ✔ leads ⇒ demote.
    EXPECT_EQ(infer_leading_log_level("✔ maps status codes: ERROR → ERROR, missing → UNSET"),
              LogLevel::Unknown)
        << "leading ✔ + Stage-1 'ERROR' (Error) must demote to Unknown";
}
// Real CI wraps the verdict glyph in ANSI SGR colour. The escape must be skipped so the
// glyph is still the leading outcome token (mirrors AnsiColourWrappedLevelRecovered).
TEST(InferLeadingLogLevel, AnsiWrappedLeadingPassGlyphDemotesStage1Level)
{
    EXPECT_EQ(
        infer_leading_log_level("\x1b[32m✔\x1b[0m write_bash failure returns a non-empty error"),
        LogLevel::Unknown)
        << "ANSI-wrapped leading ✔ + Stage-1 'failure' must demote to Unknown";
}
// Recall guard (no demotion without a LEADING pass glyph). leading_outcome_is_pass is
// true ONLY when the first outcome-bearing token is a pass glyph: a genuine leading
// failure WORD leads → stop → false, and a summary with no leading glyph stays a
// failure. These must NOT regress when the guard lands — they are the rejected
// "true-leading only" recall loss made into a standing assertion (§4A.6 SRC-D-OUT-1b).
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
    // Symmetric disconfirm: the SAME line as the first demote case but led by a FAIL
    // glyph (✗, not in the pass-glyph set) — leading_outcome_is_pass is false, so the
    // Stage-1 'failure' (Fatal) survives. Proves the guard demotes PASS glyphs only,
    // never any glyph (no over-demotion of a genuine glyph-led failure).
    EXPECT_EQ(infer_leading_log_level("✗ start debugging failure (134ms)"), LogLevel::Fatal)
        << "leading FAIL glyph ✗ must NOT demote — Stage-1 'failure' stays Fatal";
}

// ── SRC-D-OUT-4 — verdict-register awareness at the LEVEL altitude (§4A.6) ─────────
// The level (infer_leading_log_level) is what the diff consumes (→ dominant_level →
// salience → eidos NewErrorPattern). One level past SRC-D-OUT-1b: not "does a pass glyph
// lead?" but "is the line a failure VERDICT at all?" A non-verdict line carrying a
// failure NOUN ("crash"/"fail", no register) must infer Unknown, not Error — the §6.7
// re-run P3 FPs (`Storing crash reports into '<path>'`, `… watcher fail event`). These
// route through Stage 2 (contains_failure_cue): neither "crash" nor "fail" is a
// parse_log_level vocabulary word, so Stage 1 is silent — the cue fix alone demotes
// them, no Stage-1 change required. [[sift-failure-lexicon-must-be-outcome-aware]]
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
// Recall guard: a verdict-ANCHORED failure still infers its alerting level (no
// regression). Caps / colon / scope-prefixed caps / outcome verb / phrase survive.
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
// ── SRC-D-OUT-4 Stage-1 completion — the LATENT TWIN (§4A.6), GATED ────────────────
// SEPARABLE from the blocker (the design marks Stage 1 a deferred-able twin): KEEP
// this block IFF Heph lands the Stage-1 completion (gate the leading bare LEVEL word
// on the same register kernel); DELETE it if he ships only the Stage-2 cue fix — these
// stay Error/Fatal without it, and a RED that the shipped engine cannot satisfy is
// tech debt. A LEADING bare level word with no register ("error handling enabled",
// "failure modes documented") is classified authoritative-Error/Fatal by Stage 1
// (parse_log_level), unguarded — the non-pass-glyph form of the SRC-D-OUT-1b feeder. The
// rule: a leading level WORD is authoritative only when register-anchored (caps / `:` /
// bracket) or it is the terminal/sole significant token.
TEST(InferLeadingLogLevel, LeadingBareLevelWordDemotedWhenUnanchored)
{
    EXPECT_EQ(infer_leading_log_level("error handling enabled for the worker pool"),
              LogLevel::Unknown)
        << "leading bare 'error' (lowercase, no register), more tokens follow — not a verdict";
    EXPECT_EQ(infer_leading_log_level("failure modes documented in the runbook"), LogLevel::Unknown)
        << "leading bare 'failure' (Stage-1 Fatal, no register) — not a verdict";
    // Recall guard (universal under any reasonable completion): caps + colon is
    // unambiguously authoritative and must survive.
    EXPECT_EQ(infer_leading_log_level("ERROR: db connection failed"), LogLevel::Error)
        << "caps + colon — authoritative, preserved";
}

// ── SRC-D-CNT-1 — count register at the LEVEL altitude (§3.2) ──────────────────────
// infer_leading_log_level is what the diff consumes. A count-register failure word
// ("1 failure", "5 failed") is a SUMMARY: it must NOT confer an alerting verdict tier
// (Error/Fatal), but it still surfaces — capped at Warn (demote, never suppress). This is
// the P5 root: `There was 1 failure:` was Fatal (colon anchor) and outranked the named
// `testSitesStats (FAILED)` it summarized. `25 passed, 5 failed` → Warn is the dual,
// already pinned by GenuineLeadingFailureSurvivesWithoutPassGlyph.
// [[sift-failure-lexicon-must-be-outcome-aware]]
TEST(InferLeadingLogLevel, CountRegisterSummaryCapsAtWarn)
{
    EXPECT_EQ(infer_leading_log_level("There was 1 failure:"), LogLevel::Warn)
        << "P5 root: '1 failure:' is a count summary → Warn, NOT the Fatal the colon would confer";
    EXPECT_EQ(infer_leading_log_level("Tests: 5 failed"), LogLevel::Warn)
        << "'5 failed' count summary → surfaced at Warn, below per-item verdicts";
    EXPECT_EQ(infer_leading_log_level("HTTP 500 error"), LogLevel::Warn)
        << "accepted precision boundary (§3.2): '500 error' is count-preceded → Warn summary, not "
           "an Error verdict (corroborated by 5xx-rate signals in-window, not shouted)";
    // The minimal-pair survivor: a genuine per-item verdict (predecessor is a WORD) stays Error.
    EXPECT_EQ(infer_leading_log_level("1 test failed"), LogLevel::Error)
        << "'failed' preceded by 'test' (not the count '1') — a real per-item failure, stays Error";
}

// ── SRC-D-OUT-2 — leading pass WORD demotes the level (§3.3) ───────────────────────
// A passing TAP/node-runner assertion ("ok 1 - … failed …") whose description embeds
// failure vocab must not earn an alerting level. The pass WORD demotes ONLY as the first
// significant token (the count register is the independent backstop for "25 passed, …").
TEST(InferLeadingLogLevel, LeadingPassWordDemotesLevel)
{
    EXPECT_EQ(infer_leading_log_level("ok 1 - request failed and retried"), LogLevel::Unknown)
        << "TAP pass: 'ok' leads → the self-anchoring 'failed' is demoted to Unknown";
    EXPECT_EQ(infer_leading_log_level("success - worker crashed cleanly under SIGTERM"),
              LogLevel::Unknown)
        << "'success' leads → 'crashed' demoted";
    // Disconfirm: no leading pass word → the genuine failure level survives (recall guard).
    EXPECT_EQ(infer_leading_log_level("request failed and retried"), LogLevel::Error)
        << "no leading pass word — 'failed' stays an Error verdict";
    EXPECT_EQ(infer_leading_log_level("worker crashed but all 4 checks passed"), LogLevel::Error)
        << "'crashed' is the first significant token; a TRAILING 'passed' must not demote it";
}

// NOLINTEND
