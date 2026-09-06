
// invariant: a cue counts ONLY as a standalone whitespace-delimited token — punctuation trimmed,
// ASCII case-insensitive — or as a CamelCase error-type name.
// invariant: never as a substring buried inside a larger token; that over-match promoted benign new
// templates to a high-severity new-error verdict and inflated severity downstream.
// refs: ADR-20.D5
#include <gtest/gtest.h>

import insight.canon.test;

using insight::utils::contains_failure_cue;
using insight::utils::contains_warning_cue;

// invariant: the buried-substring case IS the original defect, so this row is the falsifier and not
// a description.
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

TEST(FailureLexicon, StandaloneWordIsACue)
{
    EXPECT_TRUE(contains_failure_cue("ERROR: db connection failed")) << "leading 'ERROR:'";
    EXPECT_TRUE(contains_failure_cue("Traceback (most recent call last):")) << "'Traceback'";
    EXPECT_TRUE(contains_failure_cue("connection refused to db host")) << "'refused'";
    EXPECT_TRUE(contains_failure_cue("FAILED tests/orders.spec.ts")) << "'FAILED'";
    EXPECT_TRUE(contains_failure_cue("deploy aborted by operator")) << "'aborted'";
    EXPECT_TRUE(contains_failure_cue("kernel: segfault at 0x0 ip ...")) << "'segfault'";
}

// invariant: an error-TYPE name fires ONLY in verdict register — a thrown line carries the colon
// and still fires, so the lost recall is on non-verdict echoes only.
// invariant: the colon half anchors only in the line's KIND SLOT, and this consumer moved with the
// kernel by construction rather than as a side effect.
// invariant: the gutter row below is the measured cost, and it is the SAME ruling that excludes a
// code-frame gutter marker — carving one out would be the per-shape allowlist the rule avoids.
// refs: SRC-D-OUT-4b, SRC-D-OUT-4c
TEST(FailureLexicon, CamelCaseTypeIsACueOnlyInVerdictRegister)
{
    EXPECT_FALSE(contains_failure_cue("E   sqlalchemy.exc.OperationalError: bad"))
        << "SRC-D-OUT-4c: the pytest report gutter 'E' is neither colon-terminated nor "
           "bracket-enclosed, so the type is not in the kind slot — the declared recall edge";
    EXPECT_TRUE(contains_failure_cue("sqlalchemy.exc.OperationalError: bad"))
        << "the same dotted type at index 0 IS the kind slot — the discriminator is POSITION, not "
           "the token (without this row the row above would pass a blanket suppressor)";
    EXPECT_TRUE(contains_failure_cue("IOError: disk full")) << "uppercase-before suffix, colon";
    EXPECT_FALSE(contains_failure_cue("raise ValueError"))
        << "a BARE type with no verdict register (source echo) no longer fires — the actual "
           "`ValueError: …` verdict line still does";
    EXPECT_FALSE(contains_failure_cue("threw a RuntimeException"))
        << "a bare …Exception in prose, no register — not a verdict";
    EXPECT_FALSE(contains_failure_cue("see error-report.json for the ErrorBudget"))
        << "'Error' as a PREFIX (ErrorBudget) is not a cue — only the type suffix is";
}

// invariant: a CamelCase error-TYPE inside a suite NAME line NAMES a type, it does not throw one,
// so it is demoted there while a real thrown verdict still fires — no recall loss.
// invariant: the discriminator is REGISTER and POSITION, never the token.
// refs: SRC-D-OUT-4b
TEST(FailureLexicon, CamelCaseErrorTypeDemotedInDescriptiveRegister)
{
    EXPECT_FALSE(contains_failure_cue("\xE2\x96\xB6 Send 200 when frameworkError calls "
                                      "reply.callNotFound"))
        << "a ▶-led node:test subtest name references an error type, it does not throw one";
    EXPECT_FALSE(contains_failure_cue("\xE2\x96\xB6 should map a FrameworkError to a 500 status"))
        << "a ▶-led describe/subtest name with a …Error type is descriptive, not a verdict";

    EXPECT_TRUE(contains_failure_cue("FrameworkError: connection reset by peer"))
        << "a `:`-bound thrown type is a verdict — still fires (no ▶ lead)";
    EXPECT_FALSE(contains_failure_cue("raise FrameworkError"))
        << "SRC-D-MSK-4: a bare non-▶ echo with no verdict register no longer fires — the "
           "discriminator "
           "is register (is_verdict_anchored), and the actual `FrameworkError: …` line still fires";
    EXPECT_TRUE(contains_failure_cue("\xE2\x9C\x97 teardown threw FrameworkError"))
        << "a ✗-led FAIL line confirms the verdict even with a …Error type name";
}

// invariant: a NEGATED type name semantically negates the suffix and a real type name never does,
// so it is not an error type — token-local and context-independent.
// invariant: the reproducer was a test NAMED for the negation whose result line was promoted to a
// high-severity new error — a false regression on a PASSING test.
TEST(FailureLexicon, NegatedErrorTypeIsNotACue)
{
    EXPECT_FALSE(
        contains_failure_cue("InsightCanon.BenignNoveltyEntryIsCappedEvidenceNotError completed"))
        << "the exact dogfood false match — '…NotError' is a negation, not an error type";
    EXPECT_FALSE(contains_failure_cue("status NoError after retry")) << "'NoError' negates";
    EXPECT_FALSE(contains_failure_cue("assert HandlesNonError path")) << "'NonError' negates";
    EXPECT_TRUE(contains_failure_cue("ValueError: bad value"))
        << "control: a real type ('Value' before 'Error') in verdict register (a kind-slot colon) "
           "is a cue — the negation guard is not a blanket suppressor";
    EXPECT_FALSE(contains_failure_cue("raises ValueError: bad value"))
        << "SRC-D-OUT-4c: 'raises' is prose, so the type is not in the kind slot — consistent with "
           "`raise ValueError` (no register) already demoting; the thrown line still fires above";
}

// invariant: a declared PASS verdict demotes a bare error-TYPE NAME only — an explicit failure
// WORD still fires, so the failing counterpart is still caught.
// invariant: the verdict is scanned full-text because it trails a long test name past the keyword
// head.
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
           "GLYPH does (SRC-D-OUT-1) — here the line is led by a failure word, not a glyph";
}

// invariant: a leading pass GLYPH demotes even an explicit failure WORD, because a glyph is an
// unambiguous per-test verdict that appears as a result marker and nowhere else.
// invariant: the glyph is byte-matched directly — the tokenizer trims a standalone glyph to an
// empty token, so a lexicon alone never sees it.
// invariant: this is the STRICT half of the precision gradient: a failure word is demoted by a
// leading pass GLYPH and never by a pass WORD, which would false-demote a real summary.
// refs: SRC-D-OUT-1
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

// invariant: the demotion is GLYPH-gated and not word-gated — the discriminator is that the first
// outcome token is a pass glyph, and nothing weaker.
// refs: SRC-D-OUT-1
TEST(FailureLexicon, PassWordOrTrailingGlyphDoesNotDemote)
{
    EXPECT_FALSE(contains_failure_cue("======== 25 passed, 5 failed ========"))
        << "pytest summary: '5 failed' is a count summary, not a per-item verdict (SRC-D-CNT-1)";
    EXPECT_TRUE(contains_failure_cue("build failed after a retry"))
        << "'failed' preceded by a WORD (not a bare-integer count) is a genuine verdict — fires";
    EXPECT_TRUE(contains_failure_cue("ERROR build broke ✓ cache restored"))
        << "first outcome token is the failure word ERROR; a trailing ✓ must not demote it";
    EXPECT_TRUE(contains_failure_cue("✗ marks the run failed when the worker dies"))
        << "✗ is a FAIL glyph (not a pass glyph), skipped — the failure word 'failed' stands";
}

// invariant: a bare base-form failure NOUN in prose is NOT a verdict and must not fire — the
// measured false positives were an informational startup line and a test DESCRIPTION.
// invariant: this is one level past the glyph rule: there the verdict was pass-MARKED, here there
// is no marker to demote against because the line is simply not a verdict.
// invariant: the discriminator is the decoration CI and test tooling use to MARK an outcome, and a
// term-noun carries none.
// refs: SRC-D-OUT-1, SRC-D-OUT-1b, SRC-D-OUT-4
TEST(FailureLexicon, InformationalFailureWordIsNotAVerdictCue)
{
    EXPECT_FALSE(
        contains_failure_cue("Storing crash reports into 'D:\\a\\_work\\vscode\\.build\\crashes'"))
        << "P3 rank-1 FP: the NOUN 'crash' in an informational startup config line, no register";
    EXPECT_FALSE(contains_failure_cue("- deleting watched path emits watcher fail event"))
        << "P3 rank-2 FP: a mocha test DESCRIPTION — 'fail' modifies 'event', not a verdict";
    EXPECT_FALSE(contains_failure_cue("the error path is documented in the runbook"))
        << "the base-form noun 'error' in prose, no register — not a failure verdict";
    EXPECT_FALSE(contains_failure_cue("timeout budget set to 30s for the slow shards"))
        << "the noun 'timeout' as configuration prose, no register";
}

// invariant: the partition is by ROLE and not a blanket family suppression, and the minimal pairs
// against the demotions above are what prove it.
// invariant: the anchors CONFIRM an existing failure token and never create one.
// refs: SRC-D-OUT-4
TEST(FailureLexicon, VerdictAnchoredFailureSurvives)
{
    EXPECT_TRUE(contains_failure_cue("BUILD FAILED in 3.2s")) << "caps 'FAILED'";
    EXPECT_TRUE(contains_failure_cue("ERROR db connection reset by peer")) << "caps 'ERROR'";
    EXPECT_TRUE(contains_failure_cue("fatal: expected 'packfile' but got EOF")) << "'fatal:' colon";
    EXPECT_TRUE(contains_failure_cue("testSitesStats (FAILED)")) << "paren+caps '(FAILED)'";
    EXPECT_TRUE(contains_failure_cue("##[error]Process completed with exit code 1"))
        << "bracket-bound '[error]' (GitHub Actions marker)";
    EXPECT_TRUE(contains_failure_cue("ValueError: bad input")) << "CamelCase error TYPE, colon";
    EXPECT_TRUE(contains_failure_cue("Segmentation fault (core dumped)")) << "the crash phrase";
    EXPECT_TRUE(contains_failure_cue("build failed after 4 retries")) << "outcome verb 'failed'";
    EXPECT_TRUE(contains_failure_cue("connection refused to db host 10.0.0.7")) << "'refused'";
    EXPECT_TRUE(contains_failure_cue("worker crashed during graceful shutdown")) << "'crashed'";
    EXPECT_TRUE(contains_failure_cue("deploy aborted by operator")) << "'aborted'";
}

// invariant: the criterion is BENIGN-COLLISION-PRONENESS, not grammatical role — a word that
// essentially never appears benignly fires BARE, and gating it only suppressed recall.
// invariant: these rows pin the role assignment, so a future re-tag of any of them breaks this
// test.
// refs: SRC-D-OUT-4
TEST(FailureLexicon, SelfAnchoringNounsFireBare)
{
    EXPECT_TRUE(contains_failure_cue("segfault in worker 3 during teardown"))
        << "'segfault' is a zero-collision failure noun — fires bare";
    EXPECT_TRUE(contains_failure_cue("Traceback follows below for the failing request"))
        << "'Traceback' fires bare (no colon / bracket register needed)";
    EXPECT_TRUE(contains_failure_cue("unhandled exception in the request handler"))
        << "'unhandled' fires bare — register-gating it only suppressed recall";
    EXPECT_TRUE(contains_failure_cue("unhandled promise rejection during boot"))
        << "'unhandled' bare again — the recall case the ruling protects";
}
// invariant: the other half of the same partition — a collision-prone noun has a real benign
// sense, so it demotes in prose and fires ONLY when anchored.
// invariant: the minimal pairs against the self-anchoring set are the point.
TEST(FailureLexicon, RegisterAnchoredNounDemotesInProseFiresAnchored)
{
    EXPECT_FALSE(contains_failure_cue("hit the panic button to roll back the deploy"))
        << "'panic' has a benign sense (panic button), no register — demote";
    EXPECT_FALSE(contains_failure_cue("a fatal flaw in the original design"))
        << "'fatal' has a benign sense (fatal flaw), no register — demote";
    EXPECT_TRUE(contains_failure_cue("FATAL out of memory, killing worker"))
        << "caps 'FATAL' fires";
    EXPECT_TRUE(contains_failure_cue("panic: runtime stack overflow")) << "'panic:' colon fires";
}
// invariant: a leading FAIL glyph ANCHORS a collision-prone word but never CREATES a cue, so a
// glyph-only line with no failure word stays silent.
// invariant: the multiplication sign is excluded from the glyph set on purpose — it doubles as a
// dimension separator, which is the precision risk that deferred an earlier rule.
// refs: SRC-D-OUT-4a
TEST(FailureLexicon, LeadingFailGlyphAnchorsButNeverCreates)
{
    EXPECT_TRUE(contains_failure_cue("✗ should not crash on empty input"))
        << "✗ leads → anchors the RegisterAnchored 'crash' (a failing-test verdict)";
    EXPECT_FALSE(contains_failure_cue("✗ 1920×1080"))
        << "✗ leads but NO failure word present — the glyph anchors nothing, stays silent";
    EXPECT_FALSE(contains_failure_cue("rendering at 1920×1080 then 800×600"))
        << "× U+00D7 is a dimension separator, NOT a fail glyph — no false cue";

    EXPECT_TRUE(contains_failure_cue("\xE2\x9D\x8C should not crash on empty input"))
        << "❌ leads → anchors the RegisterAnchored 'crash', like ✗";
    EXPECT_FALSE(contains_failure_cue("\xE2\x9D\x8C 1920x1080"))
        << "❌ leads but NO failure word — anchors nothing, stays silent (never CREATES a cue)";
}

// invariant: an underscore is not a token delimiter and the ends are alphanumeric, so an underscore
// compound is ONE atom that matches no lexicon word and is no error type.
// invariant: that is BY DESIGN and not a gap the register addresses — the real recurrence is
// surfaced by novelty rather than by the failure level, which is an orthogonal axis.
// invariant: the caps anchor only CONFIRMS an already-matched word, so with no match it is never
// consulted; pinned so the boundary cannot shift silently.
TEST(FailureLexicon, UnderscoreCompoundIsOneAtomNotADecomposedCue)
{
    EXPECT_FALSE(contains_failure_cue("ERR_FAILED (-2) loading 'about:blank'"))
        << "'ERR_FAILED' is ONE atom (underscore is not a delimiter) — matches no lexicon "
           "word; the 20x crash is caught by recurrence, not the failure level";
}

// invariant: a failure word whose IMMEDIATELY-preceding token is a bare integer asserts an
// aggregate, not a per-item verdict, so it does not fire even when it carries an anchor.
// invariant: the root was a counted summary read as a fatal verdict, outranking the named per-item
// failure it summarized.
// invariant: count register is checked BEFORE the verdict anchors.
// refs: SRC-D-CNT-1
TEST(FailureLexicon, CountRegisterFailureWordIsSummaryNotVerdict)
{
    EXPECT_FALSE(contains_failure_cue("There was 1 failure:"))
        << "P5 root: '1 failure' is count register — count wins over the trailing-colon anchor";
    EXPECT_FALSE(contains_failure_cue("Tests: 5 failed"))
        << "'5 failed' is a count summary — the self-anchoring verb is demoted by the count";
    EXPECT_FALSE(contains_failure_cue("Suite finished with 3 failed"))
        << "'3 failed' count summary — not a per-item verdict";
    EXPECT_TRUE(contains_failure_cue("1 test failed"))
        << "'failed' is preceded by 'test' (not the count '1') — a genuine per-item verdict";
    EXPECT_TRUE(contains_failure_cue("testSitesStats (FAILED)"))
        << "paren+caps '(FAILED)', no count predecessor — the specific verdict the summary counts";
    EXPECT_TRUE(contains_failure_cue("Build failed with 1 error"))
        << "'failed' preceded by 'Build' (not a count) — the verdict survives elsewhere on the "
           "line";
}

// invariant: a leading pass WORD demotes, but ONLY as the first significant token — which is what
// protects a counted pass-and-fail summary.
// invariant: it closes the runner recall gap where a PASSING assertion's description carries
// failure vocabulary, and the count register is the independent backstop.
// refs: SRC-D-OUT-1, SRC-D-OUT-2
TEST(FailureLexicon, LeadingPassWordDemotesAsFirstSignificantToken)
{
    EXPECT_FALSE(contains_failure_cue("ok 1 - request failed and retried"))
        << "TAP pass: 'ok' is the first significant token — demotes the self-anchoring 'failed'";
    EXPECT_FALSE(contains_failure_cue("passed: should reject when the upstream call refused"))
        << "'passed' leads — 'refused' is in a passing assertion's description";
    EXPECT_FALSE(contains_failure_cue("success - worker crashed cleanly under SIGTERM"))
        << "'success' leads — 'crashed' is demoted as the line is verdict-led pass";
    EXPECT_TRUE(contains_failure_cue("request failed and retried"))
        << "no leading pass word — 'failed' fires (the recall the rule must not cost)";
    EXPECT_TRUE(contains_failure_cue("worker crashed but all 4 checks passed"))
        << "'crashed' is the first significant token; a TRAILING 'passed' must not demote it";
    EXPECT_FALSE(contains_failure_cue("======== 25 passed, 5 failed ========"))
        << "a NUMBER is the first significant token (not 'passed') — SRC-D-OUT-2 does not fire; "
           "the "
           "count register independently demotes '5 failed' (so still no cue)";
}

TEST(FailureLexicon, WarningCue)
{
    EXPECT_TRUE(contains_warning_cue("WARN db.pool exhausted"));
    EXPECT_TRUE(contains_warning_cue("deprecation warning: x removed"));
    EXPECT_FALSE(contains_warning_cue("forwarding request upstream"))
        << "'forwarding' is not 'warn'";
    EXPECT_FALSE(contains_failure_cue("WARN db.pool exhausted")) << "a warn line is not a failure";
}

// invariant: the crash phrase is a cue only as an ADJACENT PAIR — the shell form carries no level
// keyword so the lexicon is the only signal, and either word alone collides with benign uses.
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

// invariant: ANSI colour is formatting noise and not a token boundary, so a cue wrapped in an SGR
// escape is still extracted as a clean word rather than glued to the escape's tail.
TEST(FailureLexicon, AnsiColourWrappedCueIsExtracted)
{
    EXPECT_TRUE(contains_failure_cue("tests/test_db.py::test_query_0 \x1b[31mFAILED\x1b[0m [ 99%]"))
        << "ANSI-wrapped FAILED";
    EXPECT_TRUE(contains_failure_cue("\x1b[31mERROR\x1b[0m: db connection refused"))
        << "ANSI-wrapped ERROR";
    EXPECT_FALSE(contains_failure_cue("tests/test_api.py::test_case_0 \x1b[32mPASSED\x1b[0m [ 0%]"))
        << "ANSI-wrapped PASSED is not a failure";
}

// invariant: a compiler failure is ONE error line plus N note lines, and the notes carry the
// outcome vocabulary, so every such note was classified as a cue and labelled error-class.
// invariant: measured on three real cascade pairs at 28 of 29 ranked note findings, with zero rank
// inversions — a CLASSIFICATION defect and not a ranking one.
// invariant: the demotion is anchored at the STRUCTURAL diagnostic-kind position and nowhere else,
// because a bare-word demoter would turn a labelling defect into a detection defect.
// invariant: the lexicon is NOT touched — the defect is CONTEXT, not vocabulary.
// refs: SRC-D-NOTE-1
TEST(FailureLexicon, CompilerNoteDiagnosticCarriesNoFailureVerdict)
{
    EXPECT_FALSE(contains_failure_cue("/opt/gcc-15.3/include/c++/15.3.0/bits/random.tcc:910:5: "
                                      "note: template argument deduction/substitution failed:"))
        << "the note's own 'failed' must not confer a verdict — this is the line Sift blamed";
    EXPECT_FALSE(contains_failure_cue(
        "/opt/gcc-15.3/include/c++/15.3.0/bits/fs_path.h:542:14: note: there are 102 candidates"))
        << "a note carrying no failure word stays silent (the trivial control)";
    EXPECT_FALSE(contains_failure_cue("/src/parser.cpp:88:12: note: candidate function not viable: "
                                      "ValueError: no known conversion from 'int'"))
        << "a verdict-anchored CamelCase error TYPE inside a note message is the note's word too";

    // invariant: the two-sided rows are what stop the demoter being a blanket suppressor that would
    // leave this test green and BLIND.
    EXPECT_TRUE(contains_failure_cue("/opt/gcc-15.3/include/c++/15.3.0/bits/fs_path.h:542:14: "
                                     "error: no match for ‘operator<<’"))
        << "the REAL error line in the same cascade must still fire — that is the whole point";
    EXPECT_TRUE(contains_failure_cue("Note: the deploy failed"))
        << "bare prose 'Note' is not the diagnostic-kind position and must NOT demote";
    EXPECT_TRUE(contains_failure_cue("see note: the upload failed"))
        << "a lexical 'note:' with no <line>:<col> is not the diagnostic-kind position";
}

// invariant: the note register is REGISTER-scoped and never LINE-scoped — it removes the note's
// authority over its OWN message, and a verdict anchored earlier is a different author's claim.
// invariant: that is what makes it a fourth register beside verdict, count and echoed-source rather
// than a suppression path.
// refs: SRC-D-NOTE-1
TEST(FailureLexicon, NoteRegisterDoesNotReachAVerdictAnchoredEarlierOnTheLine)
{
    EXPECT_TRUE(contains_failure_cue(
        "##[error]/src/foo.cpp:12:3: note: template argument deduction/substitution failed:"))
        << "the runner's own ##[error] wrapper precedes the note marker and keeps its verdict";
    EXPECT_TRUE(
        contains_failure_cue("build failed -- /src/foo.cpp:12:3: note: candidate template ignored"))
        << "a verdict word before the diagnostic-kind position is not the note's word";
}

// invariant: the scan limit bounds where a token may START and a token may extend past it.
// invariant: the late cue is CAPS so it genuinely fires, which is what makes the head bound the
// SOLE reason a limited scan stays silent — a bare lowercase word would demote on its own.
TEST(FailureLexicon, ScanLimitBoundsTheHead)
{
    const std::string_view line{"INFO request ok ............................ then ERROR happened"};
    EXPECT_TRUE(contains_failure_cue(line, 0)) << "whole-line scan sees the late caps 'ERROR'";
    EXPECT_FALSE(contains_failure_cue(line, 20))
        << "the late 'ERROR' STARTS past a 20-char head — out of head, not unanchored";
    EXPECT_TRUE(contains_failure_cue("OperationalError: happened later", 5))
        << "the cue token starts at offset 0, within the head";
}
