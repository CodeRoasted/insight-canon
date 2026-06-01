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

    // A CamelCase error-type name: ends in "Error"/"Exception" (exact case) preceded
    // by an ASCII letter (OperationalError, ValueError, IOError, RuntimeException).
    // This recovers compound type names a standalone-word match would miss, while
    // staying immune to a lowercase "error" buried in a path/identifier (which is
    // never capitalised, so it can never form this suffix).
    [[nodiscard]] bool is_camel_error_type(std::string_view token) noexcept
    {
        constexpr std::array<std::string_view, 2U> kSuffixes{"Error", "Exception"};
        return std::ranges::any_of(kSuffixes,
                                   [&token](std::string_view suffix)
                                   {
                                       return token.size() > suffix.size() &&
                                              token.ends_with(suffix) &&
                                              is_alpha(token[token.size() - suffix.size() - 1U]);
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

    [[nodiscard]] bool any_token_matches(std::string_view text,
                                         std::span<const std::string_view> words,
                                         std::span<const Phrase> phrases, bool camel_error_types,
                                         std::size_t scan_limit) noexcept
    {
        std::string_view prev{};
        return for_each_token(text, scan_limit,
                              [&](std::string_view token) noexcept
                              {
                                  for (const Phrase& phrase : phrases)
                                      if (iequals(prev, phrase[0]) && iequals(token, phrase[1]))
                                          return true;
                                  for (const std::string_view word : words)
                                      if (iequals(token, word))
                                          return true;
                                  if (camel_error_types && is_camel_error_type(token))
                                      return true;
                                  prev = token; // remember for the next adjacency check
                                  return false;
                              });
    }

} // namespace

bool contains_failure_cue(std::string_view text, std::size_t scan_limit) noexcept
{
    return any_token_matches(text, kFailureWords, kFailurePhrases,
                             /*camel_error_types=*/true, scan_limit);
}

bool contains_warning_cue(std::string_view text, std::size_t scan_limit) noexcept
{
    return any_token_matches(text, kWarningWords, std::span<const Phrase>{},
                             /*camel_error_types=*/false, scan_limit);
}

} // namespace insight::utils
