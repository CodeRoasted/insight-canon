// invariant: a fail glyph that is the first outcome-bearing token in a line's 128-byte head reads
// invariant: Error, inferred, with no failure word on the line — a test runner's result line.
// invariant: LEADS is the first OUTCOME-BEARING token, never the first byte: a stamp or an indent
// invariant: before the glyph carries no outcome, so the glyph still leads behind it.
// invariant: the glyphs are U+2715 to U+2718 and U+274C, the set that already anchors a failure
// invariant: word; the multiplication sign U+00D7 and the ASCII `x` are not glyphs and stay unread.
// invariant: every guard is built to fail a rule that is NEARLY right — a glyph read anywhere in
// invariant: the line, read past a pass glyph or a failure word met first, read past the head,
// invariant: read through a counted summary's Warn cap, or a glyph set widened to look-alikes.
// invariant: RED FIRST at insight-canon 13a429a on the glyph rows: a glyph only anchored a failure
// invariant: word, so a line without one read Unknown; every guard reads the same before and after.
// invariant: the lines are synthetic, written in the producers' public shapes (a test runner's
// invariant: list reporter, on Linux and on Windows); no corpus byte is in this file.
// refs: ADR-20.D5, DN-154.D4, F-SRC-insight-canon:failure_lexicon.cpp:leading_outcome_is_fail
#include <gtest/gtest.h>

import insight.canon.test;

using insight::utils::infer_leading_log_level;

namespace
{
// post: the level's name, so a failure prints `Unknown` against `Error`, never two byte dumps.
[[nodiscard]] std::string_view level_of(std::string_view line)
{
    return insight::to_string(infer_leading_log_level(line).value());
}

constexpr std::string_view kError{"Error"};

struct GlyphRow
{
    std::string_view label;
    std::string_view line;
};

// invariant: each row holds NO failure-lexicon word, so only the leading glyph can make it Error.
constexpr std::array<GlyphRow, 8U> kGlyphRows{{
    {.label = "a test runner's result line, U+2718",
     .line = "  \xe2\x9c\x98  3 [chromium] \xe2\x80\xba tests/login.spec.ts:4:4 \xe2\x80\xba "
             "signs the user in (18.5s)"},
    {.label = "U+2715 leading", .line = "\xe2\x9c\x95 renders the checkout summary (42 ms)"},
    {.label = "U+2716 leading", .line = "\xe2\x9c\x96 renders the checkout summary (42 ms)"},
    {.label = "U+2717 leading", .line = "\xe2\x9c\x97 renders the checkout summary (42 ms)"},
    {.label = "U+274C leading", .line = "\xe2\x9d\x8c renders the checkout summary (42 ms)"},
    {.label = "a stamp before the glyph carries no outcome, so the glyph still leads",
     .line = "2026-11-12T10:00:00.000Z \xe2\x9c\x98 renders the checkout summary (42 ms)"},
    {.label = "the glyph in colour",
     .line =
         "\x1b[31m  \xe2\x9c\x98  3 [chromium] \xe2\x80\xba tests/login.spec.ts:4:4 \xe2\x80\xba "
         "signs the user in (18.5s)\x1b[39m"},
    {.label = "a glyph whose token starts at byte 127, the last one inside the 128-byte head",
     .line = "0123456789012345678901234567890123456789012345678901234567890123456789"
             "01234567890123456789012345678901234567890123456789012345 "
             "\xe2\x9c\x98 renders the checkout summary"},
}};

// invariant: a guard holds the SAME verdict before and after the rule lands.
struct GuardRow
{
    std::string_view label;
    std::string_view line;
    std::string_view expected;
};

constexpr std::array<GuardRow, 9U> kGuardRows{{
    {.label = "a passing result line",
     .line = "  \xe2\x9c\x93  1 [chromium] \xe2\x80\xba tests/home.spec.ts:3:3 \xe2\x80\xba "
             "renders the home page (1.1s)",
     .expected = "Unknown"},
    {.label = "a fail glyph that does not lead: a pass glyph is the first outcome token",
     .line = "\xe2\x9c\x94 renders the \xe2\x9c\x98 icon beside a rejected upload",
     .expected = "Unknown"},
    {.label = "a fail glyph that does not lead: a failure word is the first outcome token",
     .line = "error handling \xe2\x9c\x98 disabled in this build",
     .expected = "Unknown"},
    {.label = "a fail glyph whose token starts at byte 128, past the head",
     .line = "0123456789012345678901234567890123456789012345678901234567890123456789"
             "012345678901234567890123456789012345678901234567890123456 "
             "\xe2\x9c\x98 renders the checkout summary",
     .expected = "Unknown"},
    {.label = "the Windows result line, led by an ASCII `x` (a declared limit, DN-154.D4)",
     .line = "  x  3 [chromium] \xe2\x80\xba tests\\login.spec.ts:4:4 \xe2\x80\xba "
             "signs the user in (18.5s)",
     .expected = "Unknown"},
    {.label = "the multiplication sign U+00D7 is not a fail glyph",
     .line = "\xc3\x97 3 [chromium] \xe2\x80\xba tests/login.spec.ts:4:4 \xe2\x80\xba "
             "signs the user in (18.5s)",
     .expected = "Unknown"},
    {.label = "a glyph-led counted summary keeps its Warn cap",
     .line = "\xe2\x9c\x98 3 failed, 12 passed (31.2s)",
     .expected = "Warn"},
    {.label = "a glyph-led line with a leading level word keeps the word's level",
     .line = "\xe2\x9c\x98 INFO: retrying the upload",
     .expected = "Info"},
    {.label = "a line with no glyph and no failure word",
     .line = "  3 [chromium] \xe2\x80\xba tests/login.spec.ts:4:4 \xe2\x80\xba "
             "signs the user in (18.5s)",
     .expected = "Unknown"},
}};
} // namespace

// invariant: THE PROPERTY — a leading fail glyph is a failure verdict on its own.
TEST(LeadingFailGlyph, AFailGlyphLeadingTheHeadReadsError)
{
    for (const GlyphRow& row : kGlyphRows)
        EXPECT_EQ(level_of(row.line), kError)
            << "row '" << row.label
            << "': a fail glyph as the first outcome-bearing token must read Error\n  line: "
            << row.line << "\n  bytes: " << row.line.size();
}

// invariant: the guards are what a rule NEARLY right would break; each must hold before and after.
TEST(LeadingFailGlyph, AGlyphThatDoesNotLeadOrIsNotAFailGlyphReadsNoError)
{
    for (const GuardRow& row : kGuardRows)
        EXPECT_EQ(level_of(row.line), row.expected)
            << "guard '" << row.label << "'\n  line: " << row.line
            << "\n  bytes: " << row.line.size();
}
