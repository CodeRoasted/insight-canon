#pragma once

#include <cstddef>
#include <string_view>

namespace insight::utils
{

// Token-aware failure / warning lexicon matching — the single source of truth
// shared by canon raw-text level inference (the RawTextStrategy fallback in
// infer_leading_log_level) and the MetaLog F7 severity signal.
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
// benign new templates to HIGH "New error" in the diff and inflated F7 severity.
//
// `scan_limit` bounds the head: a token must START within the first `scan_limit`
// chars (it may extend past them — the full word is captured). 0 = scan all of
// `text`. Single pass, alloc-free, noexcept (hot-path safe).
[[nodiscard]] bool contains_failure_cue(std::string_view text, std::size_t scan_limit = 0) noexcept;
[[nodiscard]] bool contains_warning_cue(std::string_view text, std::size_t scan_limit = 0) noexcept;

} // namespace insight::utils
