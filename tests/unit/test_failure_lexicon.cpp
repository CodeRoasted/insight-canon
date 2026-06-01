// NOLINTBEGIN
// Unit tests: allow short identifiers and test-specific patterns
// tests/unit/test_failure_lexicon.cpp
//
// Token-aware failure / warning lexicon matching (insight::utils). The contract:
// a cue counts ONLY as a standalone whitespace-delimited token (surrounding
// punctuation trimmed, ASCII case-insensitive) equal to a lexicon word, or as a
// CamelCase `…Error`/`…Exception` type name — never as a substring buried inside
// a larger token. The buried-substring over-match was the bug (spurious HIGH
// "New error" diff promotions + inflated MetaLog F7 severity).

#include <string_view>

#include <gtest/gtest.h>

#include "insight/utils/failure_lexicon.hpp"

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
