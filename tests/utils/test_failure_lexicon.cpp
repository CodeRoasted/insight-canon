// NOLINTBEGIN
// Unit tests: allow short identifiers and test-specific patterns
// tests/unit/test_failure_lexicon.cpp
//
// Token-aware failure / warning lexicon matching (insight::utils). The contract:
// a cue counts ONLY as a standalone whitespace-delimited token (surrounding
// punctuation trimmed, ASCII case-insensitive) equal to a lexicon word, or as a
// CamelCase `…Error`/`…Exception` type name — never as a substring buried inside
// a larger token. The buried-substring over-match was the bug (spurious HIGH
// "New error" diff promotions + inflated MetaLog severity).

#include <gtest/gtest.h>

import insight.canon.test;

using insight::utils::contains_failure_cue;
using insight::utils::contains_warning_cue;

// ── The bug: a failure word inside a larger token must NOT match ───────────────
TEST(FailureLexicon, BuriedSubstringIsNotACue)
{
    EXPECT_FALSE(contains_failure_cue("Writing tsc-error-report.json"))
        << "'error' inside a filename token";
    EXPECT_FALSE(contains_failure_cue("Compiled error_handler.ts successfully"))
        << "'error' inside an identifier token";
    EXPECT_FALSE(contains_failure_cue("no errors found")) << "plural 'errors' is not 'error'";
    EXPECT_FALSE(contains_failure_cue("page fault handler registered"))
        << "'fault' is generic; only 'segfault' is a cue";
    EXPECT_FALSE(contains_failure_cue("retrying without backoff")) << "no cue present";
}

// ── Standalone words match (case-insensitive, surrounding punctuation trimmed) ─
TEST(FailureLexicon, StandaloneWordIsACue)
{
    EXPECT_TRUE(contains_failure_cue("ERROR: db connection failed")) << "leading 'ERROR:'";
    EXPECT_TRUE(contains_failure_cue("Traceback (most recent call last):")) << "'Traceback'";
    EXPECT_TRUE(contains_failure_cue("connection refused to db host")) << "'refused'";
    EXPECT_TRUE(contains_failure_cue("FAILED tests/orders.spec.ts")) << "'FAILED'";
    EXPECT_TRUE(contains_failure_cue("deploy aborted by operator")) << "'aborted'";
    EXPECT_TRUE(contains_failure_cue("kernel: segfault at 0x0 ip ...")) << "'segfault'";
}

// ── CamelCase exception types match even with no other error word ──────────────
TEST(FailureLexicon, CamelCaseTypeIsACue)
{
    EXPECT_TRUE(contains_failure_cue("raise ValueError")) << "lowercase-before suffix";
    EXPECT_TRUE(contains_failure_cue("E   sqlalchemy.exc.OperationalError: bad")) << "dotted type";
    EXPECT_TRUE(contains_failure_cue("IOError: disk full")) << "uppercase-before suffix";
    EXPECT_TRUE(contains_failure_cue("threw a RuntimeException")) << "Exception suffix";
    EXPECT_FALSE(contains_failure_cue("see error-report.json for the ErrorBudget"))
        << "'Error' as a PREFIX (ErrorBudget) is not a cue — only the type suffix is";
}

// ── A negated type name (…NotError) is NOT an error type ───────────────────────
// Guard 1 (token-local, context-independent). The dogfood reproducer: a test named
// "…IsNotError" whose ctest result line was promoted to a HIGH "New error" — a false
// regression on a PASSING test (the cardinal sin). "…NotError"/"…NoError" semantically
// negate the suffix; a real type name never does.
TEST(FailureLexicon, NegatedErrorTypeIsNotACue)
{
    EXPECT_FALSE(
        contains_failure_cue("InsightCanon.BenignNoveltyEntryIsCappedEvidenceNotError completed"))
        << "the exact dogfood false match — '…NotError' is a negation, not an error type";
    EXPECT_FALSE(contains_failure_cue("status NoError after retry")) << "'NoError' negates";
    EXPECT_FALSE(contains_failure_cue("assert HandlesNonError path")) << "'NonError' negates";
    EXPECT_TRUE(contains_failure_cue("raises a ValueError here"))
        << "control: a real type ('Value' before 'Error') is still a cue";
}

// ── A declared PASS verdict demotes a bare error-TYPE NAME (Guard 2) ───────────
// A test NAMED after an error type ("…RaisesValueError") that PASSED is not a
// regression. The pass verdict overrides ONLY the weak name-based cue — an explicit
// failure WORD ("Failed") still fires, so the failing counterpart is caught. The
// verdict is scanned full-text because it trails a long test name past the keyword head.
TEST(FailureLexicon, PassVerdictDemotesBareErrorTypeName)
{
    EXPECT_FALSE(contains_failure_cue(
        "1/3 Test #1: InsightCanon.SalienceRaisesValueError ..........   Passed    0.00 sec"))
        << "PASSING ctest line whose name embeds a real type — never a failure";
    EXPECT_FALSE(contains_failure_cue("[       OK ] InsightCanon.ThrowsRuntimeException (0 ms)"))
        << "gtest '[ OK ]' pass verdict demotes the embedded type name";
    EXPECT_TRUE(contains_failure_cue(
        "3/3 Test #3: InsightCanon.SalienceRaisesValueError ......***Failed    0.42 sec"))
        << "the FAILING counterpart: the explicit 'Failed' word still fires";
    EXPECT_TRUE(contains_failure_cue("[  FAILED  ] InsightCanon.ThrowsRuntimeException (0 ms)"))
        << "gtest '[ FAILED ]' — the failure word wins";
    EXPECT_TRUE(contains_failure_cue("ERROR teardown failed though setup was ok"))
        << "a pass WORD never overrides a failure word; only an unambiguous leading pass "
           "GLYPH does (D-OUT-1) — here the line is led by a failure word, not a glyph";
}

// ── A leading PASS GLYPH demotes even an explicit failure WORD (D-OUT-1) ───────
// CI test logs are full of PASSING tests whose NAMES carry failure vocabulary
// ("✓ marks runs failed when the runtime throws", "✔ write_bash failure returns …").
// The per-test pass glyph (✓/✔/✅/√) is an unambiguous verdict — it appears as a result
// marker and nowhere else — so a line LED by one is a pass, regardless of the failure
// words in the test name. The glyph is byte-matched directly: for_each_token trims a
// standalone glyph to an empty token, so a lexicon alone never sees it. This is the
// strict half of the precision gradient — a failure WORD is demoted ONLY by a leading
// pass GLYPH, never by a pass WORD (a pass WORD would false-demote a real failure
// summary, asserted negative below).
TEST(FailureLexicon, LeadingPassGlyphDemotesFailureWord)
{
    EXPECT_FALSE(contains_failure_cue("@cline/core test: ✓ marks runs failed when it throws"))
        << "P1: ✓ leads after a monorepo scope prefix — passing test, 'failed' is in the name";
    EXPECT_FALSE(contains_failure_cue("✔ write_bash failure returns a non-empty error message"))
        << "P3: ✔ leads — passing test, 'failure'/'error' are in the name";
    EXPECT_FALSE(contains_failure_cue("✅ should reject when the upstream call errored"))
        << "✅ leads — 'errored' is part of a passing test's name";
    EXPECT_FALSE(contains_failure_cue("√ returns an error on a bad path"))
        << "√ (mocha-on-Windows pass mark) leads — 'error' is in the name";
    EXPECT_FALSE(
        contains_failure_cue("tests/db.spec \x1b[32m✓\x1b[0m asserts the query failed cleanly"))
        << "ANSI-wrapped ✓ leads — the colour SGR is skipped, the glyph still demotes";
}

// ── The pass demotion is GLYPH-gated, NOT word-gated (D-OUT-1 disconfirming) ───
// A leading pass WORD or a non-leading glyph must NOT demote a real failure — the
// discriminator is "first outcome token is a pass glyph", nothing weaker.
TEST(FailureLexicon, PassWordOrTrailingGlyphDoesNotDemote)
{
    EXPECT_TRUE(contains_failure_cue("======== 25 passed, 5 failed ========"))
        << "pytest summary: 'passed' leads as a COUNT, not a verdict — the failure stands";
    EXPECT_TRUE(contains_failure_cue("ERROR build broke ✓ cache restored"))
        << "first outcome token is the failure word ERROR; a trailing ✓ must not demote it";
    EXPECT_TRUE(contains_failure_cue("✗ marks the run failed when the worker dies"))
        << "✗ is a FAIL glyph (not a pass glyph), skipped — the failure word 'failed' stands";
}

// ── Warning lexicon is separate (no CamelCase types) ───────────────────────────
TEST(FailureLexicon, WarningCue)
{
    EXPECT_TRUE(contains_warning_cue("WARN db.pool exhausted"));
    EXPECT_TRUE(contains_warning_cue("deprecation warning: x removed"));
    EXPECT_FALSE(contains_warning_cue("forwarding request upstream"))
        << "'forwarding' is not 'warn'";
    EXPECT_FALSE(contains_failure_cue("WARN db.pool exhausted")) << "a warn line is not a failure";
}

// ── "segmentation fault" is a cue only as an ADJACENT pair (precision-safe) ────
// The OS/shell crash form carries no level keyword, so the lexicon is the only
// signal; but a bare "segmentation"/"fault" collides with benign uses.
TEST(FailureLexicon, SegmentationFaultPhrase)
{
    EXPECT_TRUE(contains_failure_cue("Segmentation fault (core dumped)")) << "adjacent pair";
    EXPECT_TRUE(contains_failure_cue("[ 12.3] worker: segmentation fault at 0x0"))
        << "mid-line pair";
    EXPECT_FALSE(contains_failure_cue("image segmentation pipeline complete"))
        << "bare segmentation";
    EXPECT_FALSE(contains_failure_cue("network segmentation enabled")) << "bare segmentation";
    EXPECT_FALSE(contains_failure_cue("page fault handler registered")) << "bare fault";
}

// ── ANSI colour codes are formatting noise, not token boundaries ───────────────
// Real CI logs wrap level words in SGR colour (`ESC[31mFAILED ESC[0m`); the cue
// must still be extracted as a clean word, not glued to the escape's `31m` tail.
TEST(FailureLexicon, AnsiColourWrappedCueIsExtracted)
{
    EXPECT_TRUE(contains_failure_cue("tests/test_db.py::test_query_0 \x1b[31mFAILED\x1b[0m [ 99%]"))
        << "ANSI-wrapped FAILED";
    EXPECT_TRUE(contains_failure_cue("\x1b[31mERROR\x1b[0m: db connection refused"))
        << "ANSI-wrapped ERROR";
    EXPECT_FALSE(contains_failure_cue("tests/test_api.py::test_case_0 \x1b[32mPASSED\x1b[0m [ 0%]"))
        << "ANSI-wrapped PASSED is not a failure";
}

// ── scan_limit bounds where a token may START (it may extend past the limit) ────
TEST(FailureLexicon, ScanLimitBoundsTheHead)
{
    const std::string_view line{"INFO request ok ............................ then error happened"};
    EXPECT_TRUE(contains_failure_cue(line, 0)) << "whole-line scan sees the late 'error'";
    EXPECT_FALSE(contains_failure_cue(line, 20)) << "the late 'error' starts past a 20-char head";
    // A token that STARTS within the head but extends past it is fully captured.
    EXPECT_TRUE(contains_failure_cue("OperationalError happened later", 5))
        << "the cue token starts at offset 0, within the head";
}

// NOLINTEND
