#include "insight/utils/failure_lexicon.hpp"

#include "insight/utils/token_scan.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace insight::utils
{
namespace
{
    inline constexpr unsigned kAsciiCaseBit{0x20U}; // OR-mask that folds uppercase to lowercase
    inline constexpr unsigned kAlphabetLen{26U};

    [[nodiscard]] constexpr bool is_alpha(char chr) noexcept
    {
        return ((static_cast<unsigned>(static_cast<unsigned char>(chr)) | kAsciiCaseBit) - 'a') <
               kAlphabetLen;
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

    // Explicit positive test/CI verdicts. A line that DECLARES a pass ("… Passed",
    // gtest "[ OK ]", pytest "PASSED") must not be promoted to a failure on the
    // strength of a CamelCase error-TYPE name alone — a test named "…RaisesValueError"
    // that PASSED is not a regression. The verdict overrides ONLY that weak,
    // name-based signal, never an explicit failure WORD ("error"/"failed"/…), so a
    // genuinely failing line ("***Failed") is still flagged.
    constexpr std::array<std::string_view, 4U> kSuccessVerdicts{"passed", "ok", "success",
                                                                "succeeded"};

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

} // namespace

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
    if (saw_failure_word)
        return true;
    if (!saw_error_type)
        return false;
    // Only a weak error-TYPE name was seen: a declared pass verdict demotes it to a
    // non-failure. Scanned across the WHOLE text — a ctest verdict trails a long test
    // name well past the keyword head. This extra pass runs ONLY on the rare
    // error-type-without-failure-word line, so the hot path is unaffected.
    return !any_standalone_word(text, kSuccessVerdicts, /*scan_limit=*/0U);
}

bool contains_warning_cue(std::string_view text, std::size_t scan_limit) noexcept
{
    return any_standalone_word(text, kWarningWords, scan_limit);
}

} // namespace insight::utils
