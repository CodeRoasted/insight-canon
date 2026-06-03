#pragma once

#include <cstddef>
#include <string_view>

namespace insight::utils
{

// Token-aware failure / warning lexicon matching — the single source of truth
// shared by canon raw-text level inference (the RawTextStrategy fallback in
// infer_leading_log_level) and the MetaLog severity signal.
//
// A cue matches ONLY as a standalone, whitespace-delimited token — surrounding
// punctuation trimmed, ASCII case-insensitive — that EQUALS a lexicon word, or
// that is a CamelCase `…Error` / `…Exception` type name (OperationalError,
// ValueError, IOError, RuntimeException), or that completes a fixed two-token
// phrase ("segmentation fault" — precision-safe only as an adjacent pair).
//
// A lexicon word buried INSIDE a larger token does NOT match: a filename
// `tsc-error-report.json`, an identifier `error_handler`, or a negation
// `no errors found`. That raw-substring over-match was the bug — it promoted
// benign new templates to HIGH "New error" in the diff and inflated severity.
//
// Two precision guards keep a PASSING test from ever reading as a failure (the
// cardinal false-positive — a HIGH "regression" on green torches trust):
//   • a negated type name (`…NotError`/`…NoError`/`…NonError`) is NOT an error type
//     (a test named `…IsNotError` is the textbook false match); and
//   • an error-TYPE NAME alone (`…RaisesValueError`) is demoted to a non-failure when
//     the text also DECLARES a pass verdict ("Passed" / gtest "[ OK ]" / "PASSED").
// Both guards override only the weak, name-based signal — an explicit failure WORD
// ("error"/"failed"/"segfault"/…) always wins, so a real failure still matches.
//
// `scan_limit` bounds the head: a token must START within the first `scan_limit`
// chars (it may extend past them — the full word is captured). 0 = scan all of
// `text`. Alloc-free, noexcept (hot-path safe); a single head-bounded pass, except
// the rare error-type-without-failure-word line, which costs one extra full scan for
// the demoting verdict.
[[nodiscard]] bool contains_failure_cue(std::string_view text, std::size_t scan_limit = 0) noexcept;
[[nodiscard]] bool contains_warning_cue(std::string_view text, std::size_t scan_limit = 0) noexcept;

} // namespace insight::utils
