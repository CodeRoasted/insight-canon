// invariant: a textual header and not a module export, because every JSON egress wrapper that calls
// it is itself a textual header included before any `import`.
// note: pure and header-only; including it adds no library to a consumer's link line.
// refs: DN-43.D20, ADR-26.D12
#ifndef INSIGHT_CANON_UTF8_WELL_FORMED_HPP
#define INSIGHT_CANON_UTF8_WELL_FORMED_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace insight::utf8
{

// invariant: U+FFFD REPLACEMENT CHARACTER, the only bytes the replacement ever writes.
inline constexpr std::string_view kReplacementCharacter{"\xEF\xBF\xBD"};

namespace detail
{

    // invariant: one row of the Unicode Standard's Table 3-7, Well-Formed UTF-8 Byte Sequences:
    // the lead range, how many continuation bytes follow, and the range the FIRST one must fall in.
    struct LeadRow
    {
        std::uint8_t first_lead;
        std::uint8_t last_lead;
        std::uint8_t continuations;
        std::uint8_t second_low;
        std::uint8_t second_high;
    };

    inline constexpr std::uint8_t kContinuationLow{0x80};
    inline constexpr std::uint8_t kContinuationHigh{0xBF};

    // note: a lead byte outside every row (80..C1, F5..FF) is never the start of a character.
    inline constexpr std::array<LeadRow, 8> kWellFormedLeads{{
        {.first_lead = 0xC2,
         .last_lead = 0xDF,
         .continuations = 1,
         .second_low = 0x80,
         .second_high = 0xBF},
        {.first_lead = 0xE0,
         .last_lead = 0xE0,
         .continuations = 2,
         .second_low = 0xA0,
         .second_high = 0xBF},
        {.first_lead = 0xE1,
         .last_lead = 0xEC,
         .continuations = 2,
         .second_low = 0x80,
         .second_high = 0xBF},
        {.first_lead = 0xED,
         .last_lead = 0xED,
         .continuations = 2,
         .second_low = 0x80,
         .second_high = 0x9F},
        {.first_lead = 0xEE,
         .last_lead = 0xEF,
         .continuations = 2,
         .second_low = 0x80,
         .second_high = 0xBF},
        {.first_lead = 0xF0,
         .last_lead = 0xF0,
         .continuations = 3,
         .second_low = 0x90,
         .second_high = 0xBF},
        {.first_lead = 0xF1,
         .last_lead = 0xF3,
         .continuations = 3,
         .second_low = 0x80,
         .second_high = 0xBF},
        {.first_lead = 0xF4,
         .last_lead = 0xF4,
         .continuations = 3,
         .second_low = 0x80,
         .second_high = 0x8F},
    }};

    struct Step
    {
        std::size_t length{0};
        bool well_formed{false};
    };

    // pre: `at` < `text.size()`.
    // post: the length of the well-formed character at `at`, or of the maximal subpart of an
    // ill-formed subsequence starting there; either length is at least 1.
    // invariant: a byte below 0x80 is never consumed as part of a longer step, because no ASCII
    // byte is a continuation byte in any row.
    [[nodiscard]] constexpr Step step_at(std::string_view text, std::size_t at) noexcept
    {
        const auto lead{static_cast<std::uint8_t>(text[at])};
        if (lead < kContinuationLow)
            return {.length = 1, .well_formed = true};
        for (const LeadRow& row : kWellFormedLeads)
        {
            if (lead < row.first_lead || lead > row.last_lead)
                continue;
            std::uint8_t low{row.second_low};
            std::uint8_t high{row.second_high};
            for (std::size_t offset{1}; offset <= row.continuations; ++offset)
            {
                if (at + offset >= text.size())
                    return {.length = offset, .well_formed = false};
                const auto next{static_cast<std::uint8_t>(text[at + offset])};
                if (next < low || next > high)
                    return {.length = offset, .well_formed = false};
                low = kContinuationLow;
                high = kContinuationHigh;
            }
            return {.length = std::size_t{row.continuations} + 1U, .well_formed = true};
        }
        return {.length = 1, .well_formed = false};
    }

} // namespace detail

// refs: DN-43.D20
// post: `text` with each maximal subpart of an ill-formed UTF-8 subsequence replaced by one U+FFFD,
// the Unicode Standard's recommended practice, which the WHATWG decoder implements too.
// post: well-formed input is returned byte-identical and moved, so that path allocates nothing.
// invariant: every byte below 0x80 is kept in place and in order, so a JSON quote, backslash or
// escape inside the buffer is never touched and the pass is safe over a whole written document.
// invariant: idempotent — the output is well-formed, and the only bytes it adds are U+FFFD's.
[[nodiscard]] inline std::string replace_ill_formed(std::string text)
{
    std::size_t at{0};
    while (at < text.size())
    {
        const detail::Step step{detail::step_at(text, at)};
        if (!step.well_formed)
            break;
        at += step.length;
    }
    if (at == text.size())
        return text;

    std::string out;
    out.reserve(text.size() + kReplacementCharacter.size());
    out.append(text, 0, at);
    while (at < text.size())
    {
        const detail::Step step{detail::step_at(text, at)};
        if (step.well_formed)
            out.append(text, at, step.length);
        else
            out += kReplacementCharacter;
        at += step.length;
    }
    return out;
}

} // namespace insight::utf8

#endif
