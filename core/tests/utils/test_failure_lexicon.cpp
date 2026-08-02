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

// ── CamelCase exception types are a cue ONLY in verdict register (SRC-D-MSK-4 ruling) ──
// SRC-D-OUT-4b re-baselined 2026-07-21: error_type_anchors == is_verdict_anchored. A …Error/
// …Exception type fires when it carries verdict register (a `:` verdict colon, caps, a
// [bracket], or a ✗-led line); a BARE type in prose / a source-echo (`raise ValueError`,
// no register) does NOT — the actual thrown line `ValueError: …` carries the colon and still
// fires, so the lost recall is on non-verdict echoes only (precision-first).
//
// SRC-D-OUT-4c re-baselined the colon half 2026-07-29: the colon anchors only in the line's KIND
// SLOT. error_type_anchors IS is_verdict_anchored, so this consumer moved with the kernel — by
// construction, not as a side effect. The pytest gutter row below is the measured cost, and it is
// the SAME ruling that excludes the jest/rust code-frame gutter (`> 10 |   err: &str`): a gutter
// marker is not prefix material, and carving one out would be the per-shape allowlist the rule
// exists to avoid.
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

// ── SRC-D-OUT-4b: a CamelCase error-TYPE in DESCRIPTIVE register is a reference ─────
// The discriminator is REGISTER/POSITION, not the token. A …Error/…Exception name
// inside a node:test ▶-suite NAME line NAMES a type, it does not throw one — the
// 1.6.5 dogfood FP "▶ … when frameworkError calls …" (fastify) classified Error on a
// PASSING run. Demote it there; a real thrown verdict (`:`-bound / a ✗-led fail line)
// still fires — no recall loss. (Heph: verifies the slice; Kleio owns the full RED —
// it()/describe() source register + the separate ❌-glyph recognition gap.)
TEST(FailureLexicon, CamelCaseErrorTypeDemotedInDescriptiveRegister)
{
    // The exact dogfood FP — ▶-led subtest NAME, 'frameworkError' mid-clause.
    EXPECT_FALSE(contains_failure_cue("\xE2\x96\xB6 Send 200 when frameworkError calls "
                                      "reply.callNotFound"))
        << "a ▶-led node:test subtest name references an error type, it does not throw one";
    EXPECT_FALSE(contains_failure_cue("\xE2\x96\xB6 should map a FrameworkError to a 500 status"))
        << "a ▶-led describe/subtest name with a …Error type is descriptive, not a verdict";

    // Recall preserved — the SAME token fires in verdict register (register, not token):
    EXPECT_TRUE(contains_failure_cue("FrameworkError: connection reset by peer"))
        << "a `:`-bound thrown type is a verdict — still fires (no ▶ lead)";
    EXPECT_FALSE(contains_failure_cue("raise FrameworkError"))
        << "SRC-D-MSK-4: a bare non-▶ echo with no verdict register no longer fires — the "
           "discriminator "
           "is register (is_verdict_anchored), and the actual `FrameworkError: …` line still fires";
    EXPECT_TRUE(contains_failure_cue("\xE2\x9C\x97 teardown threw FrameworkError"))
        << "a ✗-led FAIL line confirms the verdict even with a …Error type name";
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
    EXPECT_TRUE(contains_failure_cue("ValueError: bad value"))
        << "control: a real type ('Value' before 'Error') in verdict register (a kind-slot colon) "
           "is a cue — the negation guard is not a blanket suppressor";
    EXPECT_FALSE(contains_failure_cue("raises ValueError: bad value"))
        << "SRC-D-OUT-4c: 'raises' is prose, so the type is not in the kind slot — consistent with "
           "`raise ValueError` (no register) already demoting; the thrown line still fires above";
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
           "GLYPH does (SRC-D-OUT-1) — here the line is led by a failure word, not a glyph";
}

// ── A leading PASS GLYPH demotes even an explicit failure WORD (SRC-D-OUT-1) ───────
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

// ── The pass demotion is GLYPH-gated, NOT word-gated (SRC-D-OUT-1 disconfirming) ───
// A leading pass WORD or a non-leading glyph must NOT demote a real failure — the
// discriminator is "first outcome token is a pass glyph", nothing weaker.
TEST(FailureLexicon, PassWordOrTrailingGlyphDoesNotDemote)
{
    // SRC-D-CNT-1: "5 failed" is a COUNT-register summary (a bare integer immediately precedes the
    // failure word), so contains_failure_cue — a per-item VERDICT predicate — does NOT fire. The
    // summary still surfaces at Warn via the level path; it just never outranks the per-item
    // verdicts it summarizes (the symmetric dual of the "25 passed" pass-count).
    EXPECT_FALSE(contains_failure_cue("======== 25 passed, 5 failed ========"))
        << "pytest summary: '5 failed' is a count summary, not a per-item verdict (SRC-D-CNT-1)";
    EXPECT_TRUE(contains_failure_cue("build failed after a retry"))
        << "'failed' preceded by a WORD (not a bare-integer count) is a genuine verdict — fires";
    EXPECT_TRUE(contains_failure_cue("ERROR build broke ✓ cache restored"))
        << "first outcome token is the failure word ERROR; a trailing ✓ must not demote it";
    EXPECT_TRUE(contains_failure_cue("✗ marks the run failed when the worker dies"))
        << "✗ is a FAIL glyph (not a pass glyph), skipped — the failure word 'failed' stands";
}

// ── SRC-D-OUT-4 — verdict-register awareness: a failure WORD is a cue ───────────────
// ONLY in verdict register. A bare base-form failure NOUN in prose ("crash", "fail",
// "error", "timeout" with no decoration) is NOT a verdict and must not fire — the
// re-run hard-floor false positives: P3 rank 1 `Storing crash reports into
// '<path>'` (an informational startup line, the NOUN "crash") and P3 rank 2
// `- deleting watched path emits watcher fail event` (a mocha test DESCRIPTION,
// "fail" modifying "event"). This is one level past SRC-D-OUT-1/SRC-D-OUT-1b: SRC-D-OUT-1 demotes a
// pass-MARKED verdict (a ✓ leads); here there is NO marker to demote against — the
// line is simply not a verdict. The discriminator is the same decoration CI/test
// tooling uses to MARK an outcome — caps / `:` / `[ ]` / `( )` / CamelCase type /
// the "segmentation fault" phrase / a self-anchoring outcome VERB. A term-noun
// carries none → it does not classify.
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

// ── SRC-D-OUT-4 recall guard: a verdict-ANCHORED failure token still fires ──────────
// The partition is by grammatical ROLE, not a blanket family suppression — proved by
// the minimal pairs against the demotes above: the NOUN "crash"/"fail"/"error"
// demotes, but the SAME stem in verdict register survives. Five anchors CONFIRM an
// existing failure token (they never create one): caps, `:`-bound, `[ ]`/`( )`-
// enclosed, CamelCase `…Error`/`…Exception` type, the "segmentation fault" phrase;
// plus the self-anchoring outcome VERB (inflected morphology) which fires bare-
// lowercase in prose. This is is_camel_error_type's existing rule generalised — a
// caps `ValueError` is a verdict, a lowercase "error" in a path is not.
TEST(FailureLexicon, VerdictAnchoredFailureSurvives)
{
    // caps register
    EXPECT_TRUE(contains_failure_cue("BUILD FAILED in 3.2s")) << "caps 'FAILED'";
    EXPECT_TRUE(contains_failure_cue("ERROR db connection reset by peer")) << "caps 'ERROR'";
    // delimiter-bound — colon / bracket / paren
    EXPECT_TRUE(contains_failure_cue("fatal: expected 'packfile' but got EOF")) << "'fatal:' colon";
    EXPECT_TRUE(contains_failure_cue("testSitesStats (FAILED)")) << "paren+caps '(FAILED)'";
    EXPECT_TRUE(contains_failure_cue("##[error]Process completed with exit code 1"))
        << "bracket-bound '[error]' (GitHub Actions marker)";
    // CamelCase error TYPE in verdict register (a `:` verdict colon) — SRC-D-MSK-4: the type is a
    // cue only in register; the `raise ValueError("bad input")` source echo (shape `Type(`, no
    // colon) is NOT verdict register and no longer fires, but the thrown `ValueError:` line does.
    EXPECT_TRUE(contains_failure_cue("ValueError: bad input")) << "CamelCase error TYPE, colon";
    // the crash phrase (kFailurePhrases, existing)
    EXPECT_TRUE(contains_failure_cue("Segmentation fault (core dumped)")) << "the crash phrase";
    // self-anchoring outcome VERBS — fire bare lowercase (the minimal-pair survivors:
    // "crashed"≠"crash", "failed"≠"fail" — inflection self-anchors the verdict)
    EXPECT_TRUE(contains_failure_cue("build failed after 4 retries")) << "outcome verb 'failed'";
    EXPECT_TRUE(contains_failure_cue("connection refused to db host 10.0.0.7")) << "'refused'";
    EXPECT_TRUE(contains_failure_cue("worker crashed during graceful shutdown")) << "'crashed'";
    EXPECT_TRUE(contains_failure_cue("deploy aborted by operator")) << "'aborted'";
}

// ── SRC-D-OUT-4 role partition (Daidalos Q1 ruling): the criterion is BENIGN-COLLISION-
// PRONENESS, not grammatical role. A SelfAnchoring noun essentially never appears
// benignly (segfault / traceback / unhandled) → it fires BARE, no register; gating it
// only suppressed recall ("unhandled exception" / "unhandled promise rejection" are
// real, lowercase failures). These pin the role assignment — a future re-tag of any
// of these to RegisterAnchored breaks this test.
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
// The RegisterAnchored half of the same partition: a collision-prone noun has a real
// benign sense ("panic button", "fatal flaw") → it demotes in prose and fires ONLY in
// verdict register. The minimal pairs against the SelfAnchoring set above are the point.
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
// ── SRC-D-OUT-4a — a leading FAIL glyph (✗/✕/✖/✘) ANCHORS a RegisterAnchored word but ──
// never CREATES a cue. So a failing test "✗ should not crash …" surfaces (✗ anchors
// "crash"), while a glyph-only line with no failure word stays silent — provably safe
// against the deferred D-OUT-3 ×-risk: × U+00D7 (the "1920×1080" dimension separator)
// is excluded from the fail-glyph set on purpose.
TEST(FailureLexicon, LeadingFailGlyphAnchorsButNeverCreates)
{
    EXPECT_TRUE(contains_failure_cue("✗ should not crash on empty input"))
        << "✗ leads → anchors the RegisterAnchored 'crash' (a failing-test verdict)";
    EXPECT_FALSE(contains_failure_cue("✗ 1920×1080"))
        << "✗ leads but NO failure word present — the glyph anchors nothing, stays silent";
    EXPECT_FALSE(contains_failure_cue("rendering at 1920×1080 then 800×600"))
        << "× U+00D7 is a dimension separator, NOT a fail glyph — no false cue";

    // ❌ U+274C CROSS MARK — the emoji jest/vitest/mocha emit on a failing test. The 1.6.5
    // dogfood found ❌-led fail lines anchoring nothing where ✗-led ones did (a recall gap of
    // opposite polarity to the ▶ demotion). Added to the fail-glyph catalog: it anchors a
    // failure word exactly like ✗, and — never doubling as a separator — creates no cue alone.
    EXPECT_TRUE(contains_failure_cue("\xE2\x9D\x8C should not crash on empty input"))
        << "❌ leads → anchors the RegisterAnchored 'crash', like ✗";
    EXPECT_FALSE(contains_failure_cue("\xE2\x9D\x8C 1920x1080"))
        << "❌ leads but NO failure word — anchors nothing, stays silent (never CREATES a cue)";
}

// ── Tokenization boundary the P3 re-run finding identified (documenting pin) ───
// `_` is NOT a token delimiter and the ends are alnum, so "ERR_FAILED" is ONE atom —
// it matches no kFailureLexicon word and is no CamelCase `…Error` type, so it fires no
// cue (and infer_leading_log_level → Unknown). This is BY DESIGN, not a gap SRC-D-OUT-4
// addresses: the real vscode P3 GT crash (`ERR_FAILED (-2) loading 'about:blank'`, 20×)
// is surfaced by RECURRENCE/NOVELTY, not the failure level — orthogonal to verdict-
// register classification. The caps anchor only CONFIRMS an already-matched failure
// word; with no match it is never consulted. Pinned so this boundary can't shift
// silently.
TEST(FailureLexicon, UnderscoreCompoundIsOneAtomNotADecomposedCue)
{
    EXPECT_FALSE(contains_failure_cue("ERR_FAILED (-2) loading 'about:blank'"))
        << "'ERR_FAILED' is ONE atom (underscore is not a delimiter) — matches no lexicon "
           "word; the 20x crash is caught by recurrence, not the failure level";
}

// ── SRC-D-CNT-1 — the count register: a counted-noun failure word is a SUMMARY ─────────
// The symmetric dual of the "25 passed, 5 failed" pass-count: a failure word whose
// IMMEDIATELY-preceding token is a bare integer ("1 failure", "5 failed") asserts an
// aggregate, not a per-item verdict — so it does NOT fire as a cue, even when it carries
// a verdict anchor (a counted noun is a summary even with a trailing colon). The P5 root:
// `There was 1 failure:` read as a Fatal verdict outranked the named `testSitesStats
// (FAILED)` it summarized. Count register is checked BEFORE the verdict anchors.
TEST(FailureLexicon, CountRegisterFailureWordIsSummaryNotVerdict)
{
    EXPECT_FALSE(contains_failure_cue("There was 1 failure:"))
        << "P5 root: '1 failure' is count register — count wins over the trailing-colon anchor";
    EXPECT_FALSE(contains_failure_cue("Tests: 5 failed"))
        << "'5 failed' is a count summary — the self-anchoring verb is demoted by the count";
    EXPECT_FALSE(contains_failure_cue("Suite finished with 3 failed"))
        << "'3 failed' count summary — not a per-item verdict";
    // The minimal pairs that MUST stay verdicts — the predecessor is a WORD, not a number.
    EXPECT_TRUE(contains_failure_cue("1 test failed"))
        << "'failed' is preceded by 'test' (not the count '1') — a genuine per-item verdict";
    EXPECT_TRUE(contains_failure_cue("testSitesStats (FAILED)"))
        << "paren+caps '(FAILED)', no count predecessor — the specific verdict the summary counts";
    EXPECT_TRUE(contains_failure_cue("Build failed with 1 error"))
        << "'failed' preceded by 'Build' (not a count) — the verdict survives elsewhere on the "
           "line";
}

// ── SRC-D-OUT-2 — a leading pass WORD demotes, but ONLY as the first significant token ──
// SRC-D-OUT-1 is glyph-gated (a leading pass GLYPH demotes; a pass WORD does not) to protect
// the "25 passed, 5 failed" summary. SRC-D-OUT-2 closes the TAP/node-runner recall gap: a
// kSuccessVerdicts WORD (passed/ok/success/succeeded) leading the line (first significant
// token, mirroring the glyph rule) demotes a failure word — `ok 1 - … failed` is a PASSING
// TAP assertion whose description carries failure vocab. The count register (above) is the
// independent backstop for count-summaries, so the pass-WORD rule is now low-risk.
TEST(FailureLexicon, LeadingPassWordDemotesAsFirstSignificantToken)
{
    EXPECT_FALSE(contains_failure_cue("ok 1 - request failed and retried"))
        << "TAP pass: 'ok' is the first significant token — demotes the self-anchoring 'failed'";
    EXPECT_FALSE(contains_failure_cue("passed: should reject when the upstream call refused"))
        << "'passed' leads — 'refused' is in a passing assertion's description";
    EXPECT_FALSE(contains_failure_cue("success - worker crashed cleanly under SIGTERM"))
        << "'success' leads — 'crashed' is demoted as the line is verdict-led pass";
    // Disconfirming — the rule is FIRST-significant-token only, never anywhere.
    EXPECT_TRUE(contains_failure_cue("request failed and retried"))
        << "no leading pass word — 'failed' fires (the recall the rule must not cost)";
    EXPECT_TRUE(contains_failure_cue("worker crashed but all 4 checks passed"))
        << "'crashed' is the first significant token; a TRAILING 'passed' must not demote it";
    EXPECT_FALSE(contains_failure_cue("======== 25 passed, 5 failed ========"))
        << "a NUMBER is the first significant token (not 'passed') — SRC-D-OUT-2 does not fire; "
           "the "
           "count register independently demotes '5 failed' (so still no cue)";
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

// ── The NOTE register (SRC-D-NOTE-1) — a compiler note asserts no verdict ──────────
// A gcc/clang failure is ONE `error:` line plus N `note:` lines, and the notes carry the
// outcome vocabulary: `note: template argument deduction/substitution failed:`. `failed`
// is SelfAnchoring — it fires bare, by design — so every such note was classified as a
// failure cue and emitted under an ERROR-class label, naming the wrong file. Measured on
// 3 real green→red cascade pairs: 28 of 29 ranked `note:` findings (96.6 %) carried an
// ERROR-class label; rank inversions were 0/3, so this is a CLASSIFICATION defect, not a
// ranking one.
//
// The demotion is anchored at the STRUCTURAL diagnostic-kind position
// `<path>:<line>:<col>: note: ` and nowhere else. That constraint is the whole design: a
// bare-word demoter would suppress legitimate `failed` findings — turning a labelling
// defect into a detection defect, which is strictly worse — so the two-sidedness arms
// below are not decoration, they are what keeps the fix from going too far.
//
// The lexicon is NOT touched: `{"failed", SelfAnchoring}` is unchanged. The defect is
// CONTEXT, not vocabulary.
TEST(FailureLexicon, CompilerNoteDiagnosticCarriesNoFailureVerdict)
{
    // The origin line, verbatim from the v1.8.1 release red that surfaced the defect.
    EXPECT_FALSE(contains_failure_cue("/opt/gcc-15.3/include/c++/15.3.0/bits/random.tcc:910:5: "
                                      "note: template argument deduction/substitution failed:"))
        << "the note's own 'failed' must not confer a verdict — this is the line Sift blamed";
    EXPECT_FALSE(contains_failure_cue(
        "/opt/gcc-15.3/include/c++/15.3.0/bits/fs_path.h:542:14: note: there are 102 candidates"))
        << "a note carrying no failure word stays silent (the trivial control)";
    // The CamelCase error-TYPE path, isolated: the type is colon-anchored (SRC-D-OUT-4b would fire
    // it) and the line carries no lexicon word, so this row is red without the register and green
    // with it — it is not a restatement of the row above.
    EXPECT_FALSE(contains_failure_cue("/src/parser.cpp:88:12: note: candidate function not viable: "
                                      "ValueError: no known conversion from 'int'"))
        << "a verdict-anchored CamelCase error TYPE inside a note message is the note's word too";

    // ── Two-sidedness. Without these the demoter could be a blanket suppressor and this
    // test would still be green — green-BLIND.
    EXPECT_TRUE(contains_failure_cue("/opt/gcc-15.3/include/c++/15.3.0/bits/fs_path.h:542:14: "
                                     "error: no match for ‘operator<<’"))
        << "the REAL error line in the same cascade must still fire — that is the whole point";
    EXPECT_TRUE(contains_failure_cue("Note: the deploy failed"))
        << "bare prose 'Note' is not the diagnostic-kind position and must NOT demote";
    EXPECT_TRUE(contains_failure_cue("see note: the upload failed"))
        << "a lexical 'note:' with no <line>:<col> is not the diagnostic-kind position";
}

// ── The note register is REGISTER-scoped, never LINE-scoped (SRC-D-NOTE-1) ─────────
// It removes the NOTE's authority over its own message; a verdict anchored EARLIER on the
// same line is a different claim by a different author and survives untouched. This is
// what makes it a fourth register beside verdict / count / echoed-source rather than a
// suppression path.
TEST(FailureLexicon, NoteRegisterDoesNotReachAVerdictAnchoredEarlierOnTheLine)
{
    EXPECT_TRUE(contains_failure_cue(
        "##[error]/src/foo.cpp:12:3: note: template argument deduction/substitution failed:"))
        << "the runner's own ##[error] wrapper precedes the note marker and keeps its verdict";
    EXPECT_TRUE(contains_failure_cue(
        "build failed -- /src/foo.cpp:12:3: note: candidate template ignored"))
        << "a verdict word before the diagnostic-kind position is not the note's word";
}

// ── scan_limit bounds where a token may START (it may extend past the limit) ────
TEST(FailureLexicon, ScanLimitBoundsTheHead)
{
    // The late cue is CAPS 'ERROR' (verdict-anchored under SRC-D-OUT-4) so it genuinely
    // fires — the head bound is then the SOLE reason the limit=20 scan stays silent
    // (a bare lowercase 'error' would now demote on its own, masking the bound).
    const std::string_view line{"INFO request ok ............................ then ERROR happened"};
    EXPECT_TRUE(contains_failure_cue(line, 0)) << "whole-line scan sees the late caps 'ERROR'";
    EXPECT_FALSE(contains_failure_cue(line, 20))
        << "the late 'ERROR' STARTS past a 20-char head — out of head, not unanchored";
    // A token that STARTS within the head but extends past it is fully captured. Uses a
    // verdict-register cue (colon) — SRC-D-MSK-4: a bare `OperationalError` echo is no longer a cue.
    EXPECT_TRUE(contains_failure_cue("OperationalError: happened later", 5))
        << "the cue token starts at offset 0, within the head";
}

// NOLINTEND
