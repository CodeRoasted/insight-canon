module;

module insight.canon.api;
import insight.canon.internal;

namespace insight::utils
{
namespace
{
    [[nodiscard]] constexpr bool is_alpha(char chr) noexcept
    {
        return ((static_cast<unsigned>(static_cast<unsigned char>(chr)) | detail::kAsciiCaseBit) -
                'a') < detail::kAlphabetLen;
    }
    [[nodiscard]] constexpr char lower(char chr) noexcept
    {
        return chr >= 'A' && chr <= 'Z' ? static_cast<char>(chr - 'A' + 'a') : chr;
    }
    [[nodiscard]] bool iequals(std::string_view lhs, std::string_view rhs) noexcept
    {
        if (lhs.size() != rhs.size())
            return false;
        for (std::size_t idx{0}; idx < lhs.size(); ++idx)
            if (lower(lhs[idx]) != lower(rhs[idx]))
                return false;
        return true;
    }

    // Negation segments that INVERT an "…Error"/"…Exception" suffix: the CamelCase
    // word right before the suffix. "…NotError"/"…NoError" is not an error type — the
    // textbook false match is a test named "…IsNotError", whose ctest/gtest result
    // line would otherwise read as a HIGH "New error", i.e. a false regression on a
    // PASSING test (the cardinal sin). This is a property of the token itself,
    // independent of the surrounding line.
    constexpr std::array<std::string_view, 3U> kNegationSegments{"no", "not", "non"};

    // The CamelCase word ending just before `suffix_start`: the maximal lowercase run
    // ending at suffix_start-1, plus its leading uppercase letter if present. For
    // "EvidenceNotError" with the "Error" suffix at index 12, this returns "Not".
    // substr(begin, suffix_start - begin): begin <= suffix_start, and the sole caller
    // (is_camel_error_type) guards token.size() > suffix.size(), so
    // suffix_start = token.size() - suffix.size() < token.size() — pos in-bounds, cannot throw.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] std::string_view preceding_camel_word(std::string_view token,
                                                        std::size_t suffix_start) noexcept
    {
        std::size_t begin{suffix_start};
        while (begin > 0U && token[begin - 1U] >= 'a' && token[begin - 1U] <= 'z')
            --begin;
        if (begin > 0U && token[begin - 1U] >= 'A' && token[begin - 1U] <= 'Z')
            --begin;
        return token.substr(begin, suffix_start - begin);
    }

    // A CamelCase error-type name: ends in "Error"/"Exception" (exact case) preceded
    // by an ASCII letter (OperationalError, ValueError, IOError, RuntimeException).
    // This recovers compound type names a standalone-word match would miss, while
    // staying immune to a lowercase "error" buried in a path/identifier (which is
    // never capitalised, so it can never form this suffix). A negated type name
    // ("…NotError"/"…NoError"/"…NonError") is EXCLUDED — see kNegationSegments.
    [[nodiscard]] bool is_camel_error_type(std::string_view token) noexcept
    {
        constexpr std::array<std::string_view, 2U> kSuffixes{"Error", "Exception"};
        return std::ranges::any_of(
            kSuffixes,
            [&token](std::string_view suffix)
            {
                if (token.size() <= suffix.size() || !token.ends_with(suffix))
                    return false;
                const std::size_t suffix_start{token.size() - suffix.size()};
                if (!is_alpha(token[suffix_start - 1U]))
                    return false;
                const std::string_view preceding{preceding_camel_word(token, suffix_start)};
                return std::ranges::none_of(kNegationSegments,
                                            [preceding](std::string_view neg) noexcept
                                            { return iequals(preceding, neg); });
            });
    }

    // The unified failure lexicon — token forms (base + the inflections that occur in
    // real CI / app logs). Plural "errors" is omitted ON PURPOSE so a negation like
    // "no errors found" / "0 errors" is not read as an error (count-dependent, a
    // numeric-rule concern, not the lexicon's); CamelCase `…Error`/`…Exception` types
    // are caught structurally by is_camel_error_type, not enumerated here.
    constexpr std::array<std::string_view, 18U> kFailureWords{
        "error",   "exception", "fatal", "panic",   "panicked", "refused",
        "timeout", "traceback", "fail",  "failed",  "failure",  "segfault",
        "denied",  "unhandled", "abort", "aborted", "crash",    "crashed"};
    constexpr std::array<std::string_view, 2U> kWarningWords{"warn", "warning"};

    // Multi-word cues that are only precision-safe as an ADJACENT token pair. A bare
    // "segmentation"/"fault" collides with benign uses (image/network segmentation,
    // page fault), so the crash signal needs the two-token adjacency — and the OS/
    // shell form "Segmentation fault (core dumped)" carries no level keyword, so the
    // lexicon is the only thing that flags it.
    using Phrase = std::array<std::string_view, 2U>;
    constexpr std::array<Phrase, 1U> kFailurePhrases{{{"segmentation", "fault"}}};

    // Explicit positive test/CI verdicts (success WORDS). A line that DECLARES a pass
    // ("… Passed", gtest "[ OK ]", pytest "PASSED") must not be promoted to a failure on
    // the strength of a CamelCase error-TYPE name alone — a test named
    // "…RaisesValueError" that PASSED is not a regression. A success WORD overrides ONLY
    // that weak, name-based signal; it NEVER demotes an explicit failure WORD
    // ("error"/"failed"/…), because a leading pass WORD would false-demote a genuine
    // failure summary ("25 passed, 5 failed"). The strong failure WORD is demoted only by
    // an unambiguous leading pass GLYPH (D-OUT-1, leading_outcome_is_pass), never a word.
    constexpr std::array<std::string_view, 4U> kSuccessVerdicts{"passed", "ok", "success",
                                                                "succeeded"};

    // Only throw path is for_each_token's substr(begin, ...) with begin <= limit <= text.size()
    // (see token_scan.hpp); the noexcept body cannot throw.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] bool any_standalone_word(std::string_view text,
                                           std::span<const std::string_view> words,
                                           std::size_t scan_limit) noexcept
    {
        return for_each_token(text, scan_limit,
                              [words](std::string_view token) noexcept
                              {
                                  return std::ranges::any_of(words,
                                                             [token](std::string_view word) noexcept
                                                             { return iequals(token, word); });
                              });
    }

    // ── Outcome-aware demotion: a leading PASS GLYPH (D-OUT-1) ───────────────────────
    // Per-test PASS glyphs (3-byte UTF-8, lead byte 0xE2) are unambiguous verdicts: they
    // appear in CI output as a result marker and NOWHERE else, unlike the words
    // "passed"/"ok" (which double as summary counts, prose, and test names). They are
    // also INVISIBLE to for_each_token — a standalone glyph is all-non-alnum, so the
    // tokenizer trims it to an empty token and the verdict is destroyed before any
    // lexicon sees it — so they are byte-matched directly at a token start.
    //   ✓ U+2713 · ✔ U+2714 · ✅ U+2705 · √ U+221A (the mocha-on-Windows pass mark)
    using Glyph = std::array<unsigned char, 3U>;
    constexpr std::array<Glyph, 4U> kPassGlyphs{{
        {0xE2U, 0x9CU, 0x93U}, // ✓ U+2713 CHECK MARK
        {0xE2U, 0x9CU, 0x94U}, // ✔ U+2714 HEAVY CHECK MARK
        {0xE2U, 0x9CU, 0x85U}, // ✅ U+2705 WHITE HEAVY CHECK MARK
        {0xE2U, 0x88U, 0x9AU}, // √ U+221A SQUARE ROOT — mocha's Windows pass mark
    }};

    [[nodiscard]] bool starts_with_pass_glyph(std::string_view text, std::size_t pos) noexcept
    {
        constexpr std::size_t kGlyphLen{3U};
        if (pos + kGlyphLen > text.size())
            return false;
        const Glyph head{static_cast<unsigned char>(text[pos]),
                         static_cast<unsigned char>(text[pos + 1U]),
                         static_cast<unsigned char>(text[pos + 2U])};
        return std::ranges::find(kPassGlyphs, head) != kPassGlyphs.end();
    }

    // Advance past ANSI escapes + token delimiters; returns the next token-start index
    // (or text.size() if the rest of the line is delimiters). Mirrors for_each_token's
    // inter-token skip so the two agree on token boundaries.
    [[nodiscard]] std::size_t skip_to_token_start(std::string_view text, std::size_t pos) noexcept
    {
        for (;;)
        {
            if (const std::size_t esc{detail::ansi_escape_len(text, pos)}; esc != 0U)
                pos += esc;
            else if (pos < text.size() && detail::is_token_delimiter(text[pos]))
                ++pos;
            else
                return pos;
        }
    }

    // The trimmed token starting at `pos` (caller guarantees a non-delimiter, non-ANSI
    // start); advances `pos` past it. Mirrors for_each_token's token extraction + the
    // surrounding-non-alnum trim. substr begin <= pos <= size → noexcept body cannot throw.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] std::string_view take_trimmed_token(std::string_view text,
                                                      std::size_t& pos) noexcept
    {
        const std::size_t begin{pos};
        while (pos < text.size() && !detail::is_token_delimiter(text[pos]) &&
               detail::ansi_escape_len(text, pos) == 0U)
            ++pos;
        std::string_view token{text.substr(begin, pos - begin)};
        while (!token.empty() && !detail::is_token_alnum(token.front()))
            token.remove_prefix(1U);
        while (!token.empty() && !detail::is_token_alnum(token.back()))
            token.remove_suffix(1U);
        return token;
    }

    // Does the line's FIRST outcome-bearing token declare a pass? Walk the head
    // left-to-right (ANSI- and scope-prefix-tolerant, tokenising exactly as
    // for_each_token): the first token that is a pass GLYPH ⇒ true (pass leads); the
    // first token that is a failure WORD ⇒ false (failure leads — stop, so a pathological
    // "ERROR … ✓" still fires and the "ERROR teardown failed though setup was ok" guard
    // holds); any other token (scope segment "@cline/core", a name, a number) is skipped.
    // End-of-head ⇒ false. Glyph-gated, NOT word-gated: a leading pass WORD
    // ("25 passed, 5 failed") is a summary count, not a verdict, and must not demote a
    // real failure summary. Pure byte-compare + ASCII case-fold ⇒ cross-stdlib and MSVC
    // bit-identical (F5).
    [[nodiscard]] bool leading_outcome_is_pass(std::string_view text) noexcept
    {
        static constexpr std::size_t kOutcomeHead{128U}; // generous monorepo scope-prefix bound
        const std::size_t limit{text.size() < kOutcomeHead ? text.size() : kOutcomeHead};
        std::size_t pos{0};
        while ((pos = skip_to_token_start(text, pos)) < limit)
        {
            if (starts_with_pass_glyph(text, pos)) // first outcome token is a pass glyph
                return true;
            const std::string_view token{take_trimmed_token(text, pos)};
            for (const std::string_view word : kFailureWords)
                if (iequals(token, word)) // first outcome token is a failure word
                    return false;
            // otherwise a non-outcome token (scope segment / name / number) — continue
        }
        return false; // no leading outcome token within the head
    }

} // namespace

// Only throw path is for_each_token / any_standalone_word, whose substr has begin <= text.size()
// (see token_scan.hpp); the noexcept body cannot throw.
// NOLINTNEXTLINE(bugprone-exception-escape)
bool contains_failure_cue(std::string_view text, std::size_t scan_limit) noexcept
{
    // One head-bounded pass. An explicit failure WORD, or the "segmentation fault"
    // phrase, is a strong, unconditional cue (short-circuit true). A CamelCase
    // error-TYPE name is a WEAKER signal — recorded, but the scan continues.
    bool saw_error_type{false};
    std::string_view prev{};
    const bool saw_failure_word{
        for_each_token(text, scan_limit,
                       [&](std::string_view token) noexcept
                       {
                           for (const Phrase& phrase : kFailurePhrases)
                               if (iequals(prev, phrase[0]) && iequals(token, phrase[1]))
                                   return true;
                           for (const std::string_view word : kFailureWords)
                               if (iequals(token, word))
                                   return true;
                           if (is_camel_error_type(token))
                               saw_error_type = true;
                           prev = token; // remember for the next adjacency check
                           return false;
                       })};
    // A strong failure WORD fires unconditionally; a weak error-TYPE name (no failure
    // word) is demoted by any success WORD anywhere — scanned across the WHOLE text, as a
    // ctest verdict trails a long test name well past the keyword head. The success-word
    // pass runs ONLY on the rare error-type-without-failure-word line (`||` short-circuit),
    // so the hot path is unaffected.
    const bool result{saw_failure_word ||
                      (saw_error_type && !any_standalone_word(text, kSuccessVerdicts,
                                                              /*scan_limit=*/0U))};
    // Outcome guard (D-OUT-1): a line that carries a failure cue but is LED by an
    // unambiguous pass GLYPH (✓/✔/✅/√) is a passing test whose NAME embeds failure
    // vocabulary ("✓ marks runs failed …"), not a regression. Only a leading pass GLYPH
    // demotes a failure WORD; a leading pass WORD ("25 passed, 5 failed") must not, so
    // words do not. Paid only on the would-be-positive minority (short-circuit), and the
    // walk stops at the first outcome token.
    if (result && leading_outcome_is_pass(text))
        return false;
    return result;
}

bool contains_warning_cue(std::string_view text, std::size_t scan_limit) noexcept
{
    return any_standalone_word(text, kWarningWords, scan_limit);
}

} // namespace insight::utils
