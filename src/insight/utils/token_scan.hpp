#pragma once

#include <cstddef>
#include <string_view>
#include <utility>

namespace insight::utils
{
namespace detail
{

// Token delimiters: whitespace + STRUCTURAL punctuation (brackets, parens,
// quotes, and the `: = , ; |` value/field separators). Identifier- and
// path-join chars (`- _ . /` …) are deliberately NOT delimiters, so a compound
// like `tsc-error-report.json` stays a single atom while a bracketed marker
// `##[error]` or a structured value `level=error` exposes its inner word.
[[nodiscard]] constexpr bool is_token_delimiter(char chr) noexcept
{
    switch (chr)
    {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\f':
    case '\v':
    case '[':
    case ']':
    case '(':
    case ')':
    case '{':
    case '}':
    case '<':
    case '>':
    case '"':
    case '\'':
    case '`':
    case ',':
    case ':':
    case ';':
    case '|':
    case '=':
        return true;
    default:
        return false;
    }
}

[[nodiscard]] constexpr bool is_token_alnum(char chr) noexcept
{
    const unsigned chu{static_cast<unsigned>(static_cast<unsigned char>(chr))};
    return ((chu | 0x20U) - 'a') < 26U || (chu - '0') < 10U;
}

// Length of an ANSI escape sequence starting at text[pos], or 0 if none. Handles
// the CSI form `ESC [ <params> <final>` (covers the SGR colour codes CI logs wrap
// level words in, e.g. `ESC[31mFAILED`) and a bare ESC. ANSI codes are formatting
// noise, never token content, so a sequence is consumed as a delimiter — a level
// or cue glued to one is still extracted as a clean word.
[[nodiscard]] constexpr std::size_t ansi_escape_len(std::string_view text, std::size_t pos) noexcept
{
    if (pos >= text.size() || text[pos] != '\x1b')
        return 0U;
    // A CSI sequence terminates at its first "final byte" (ECMA-48 §5.4).
    constexpr unsigned kCsiFinalByteMin{0x40U};
    constexpr unsigned kCsiFinalByteMax{0x7EU};
    std::size_t end{pos + 1U};
    if (end < text.size() && text[end] == '[')
    {
        ++end; // CSI params + intermediates, up to the final byte
        while (end < text.size() && (static_cast<unsigned char>(text[end]) < kCsiFinalByteMin ||
                                     static_cast<unsigned char>(text[end]) > kCsiFinalByteMax))
            ++end;
        if (end < text.size())
            ++end; // include the final byte
    }
    return end - pos;
}

} // namespace detail

// Iterate the tokens of `text` under the shared canon tokenisation (see
// is_token_delimiter): split on whitespace + structural punctuation, keep
// identifier/path-join chars inside a token, then trim each token's surrounding
// non-alphanumerics. `visit(token)` is invoked for every non-empty token whose
// START lies within the first `scan_limit` chars (0 = all of `text`); a token
// may extend past the limit (the whole word is captured). Iteration stops early
// when `visit` returns true, and for_each_token then returns true. Alloc-free,
// single pass — used by both leading-level inference and the failure lexicon so
// the two never disagree on what counts as a standalone word.
template <class Visit>
[[nodiscard]] bool for_each_token(std::string_view text, std::size_t scan_limit, Visit&& visit)
{
    const std::size_t limit{scan_limit == 0U || scan_limit > text.size() ? text.size() : scan_limit};
    std::size_t pos{0};
    while (pos < limit)
    {
        for (;;) // skip delimiters and ANSI escape sequences
        {
            if (const std::size_t esc{detail::ansi_escape_len(text, pos)}; esc != 0U)
                pos += esc;
            else if (pos < text.size() && detail::is_token_delimiter(text[pos]))
                ++pos;
            else
                break;
        }
        if (pos >= limit)
            break;
        const std::size_t begin{pos};
        while (pos < text.size() && !detail::is_token_delimiter(text[pos]) &&
               detail::ansi_escape_len(text, pos) == 0U)
            ++pos;
        std::string_view token{text.substr(begin, pos - begin)};
        while (!token.empty() && !detail::is_token_alnum(token.front()))
            token.remove_prefix(1);
        while (!token.empty() && !detail::is_token_alnum(token.back()))
            token.remove_suffix(1);
        if (!token.empty() && std::forward<Visit>(visit)(token))
            return true;
    }
    return false;
}

} // namespace insight::utils
