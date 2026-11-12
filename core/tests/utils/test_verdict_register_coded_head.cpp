// invariant: a level word qualified by a bracketed diagnostic code is the colon anchor with the
// invariant: code read through — `error[E0425]:` and `Error [ERR_X]:` are the verdict of `error:`.
// invariant: the code is ONE bracket pair of letters, digits, `_` or `-`, after at most one space,
// invariant: followed at once by the colon; the word keeps the colon anchor's kind slot.
// invariant: the coded rows are METAMORPHIC over their own uncoded twin — the twin's verdict is
// invariant: asserted first as a premise, so a red on a coded row is the code, never the twin.
// invariant: every guard is built to fail a rule that is NEARLY right — a bracket read as a code
// invariant: whatever it holds, a code read without its colon or its kind slot, or a rule keyed
// invariant: on the bracket shape that forgets which word carries it.
// invariant: the lines are synthetic, written in the producers' public shapes (rustc's coded
// invariant: diagnostic, Node's coded error); no corpus byte is in this file.
// refs: DN-154.D2, F-SRC-insight-canon:canon.api.cppm:is_verdict_anchored
#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogLevel;
using insight::utils::infer_leading_log_level;

namespace
{
// post: the level's name, so a failure prints `Unknown` against `Error`, never two byte dumps.
[[nodiscard]] std::string_view level_of(std::string_view line)
{
    return insight::to_string(infer_leading_log_level(line).value());
}

constexpr std::string_view kError{"Error"};

// invariant: `coded` and `uncoded` differ ONLY by the code (and the one space before it, where the
// invariant: producer writes one), so the pair varies the code and holds every other byte fixed.
struct CodedRow
{
    std::string_view label;
    std::string_view coded;
    std::string_view uncoded;
};

constexpr std::array<CodedRow, 4U> kCodedRows{{
    {.label = "a compiler diagnostic, code glued to the word",
     .coded = "error[E0425]: cannot find value `total` in this scope",
     .uncoded = "error: cannot find value `total` in this scope"},
    {.label = "a runtime error, one space before the code",
     .coded = "Error [ERR_INVALID_STATE]: the stream was already closed",
     .uncoded = "Error: the stream was already closed"},
    {.label = "a compiler diagnostic in the kind slot behind a bracket-enclosed prefix",
     .coded = "[build] error[E0599]: no method named `size` found for struct `Grid`",
     .uncoded = "[build] error: no method named `size` found for struct `Grid`"},
    {.label = "a code of letters, digits, '_' and '-'",
     .coded = "error[lint-unused_9]: value assigned is never read",
     .uncoded = "error: value assigned is never read"},
}};

// invariant: the compiler's colour form — bold red over the word and its code, a reset, bold again
// invariant: before the colon — so an escape run sits between the code's `]` and the colon.
// invariant: the uncoded twin reads Unknown at insight-canon 00f5aea (the colon anchor is byte
// invariant: adjacent), so only the twin relation is pinned here, never a level.
constexpr std::string_view kColourCoded{
    "\x1b[0m\x1b[1m\x1b[38;5;9merror[E0425]\x1b[0m\x1b[0m\x1b[1m: cannot find value `total` in "
    "this scope\x1b[0m"};
constexpr std::string_view kColourUncoded{
    "\x1b[0m\x1b[1m\x1b[38;5;9merror\x1b[0m\x1b[0m\x1b[1m: cannot find value `total` in this "
    "scope\x1b[0m"};

// invariant: a guard holds the SAME verdict before and after the anchor lands.
struct GuardRow
{
    std::string_view label;
    std::string_view line;
    std::string_view expected;
};

// invariant: four families — another level's code keeps its level; a word that is not an error
// invariant: word asserts nothing; the kind slot still binds; a bracket that is not a code, or a
// invariant: code outside its exact frame (one space at most before, the colon at once after), is
// invariant: not read through.
constexpr std::array<GuardRow, 12U> kGuardRows{{
    {.label = "a coded warning stays Warn",
     .line = "warning[W0001]: unused variable `total`",
     .expected = "Warn"},
    {.label = "a coded note: not a level word",
     .line = "note[E0425]: the value is declared here",
     .expected = "Unknown"},
    {.label = "a coded help: not a level word",
     .line = "help[E0425]: a local variable with a similar name exists",
     .expected = "Unknown"},
    {.label = "'errors' is not lexicon, whatever it carries",
     .line = "errors[0]: the first entry of the list",
     .expected = "Unknown"},
    {.label = "a coded head after a non-prefix token",
     .line = "see error[E0001]: the explanation of this code",
     .expected = "Unknown"},
    {.label = "a coded head displaced behind a bracket-enclosed prefix",
     .line = "[build] see error[E0599]: no method named `size` found for struct `Grid`",
     .expected = "Unknown"},
    {.label = "a bracket holding words, not a code",
     .line = "error [some text]: a message after free text",
     .expected = "Unknown"},
    {.label = "a bracketed timestamp, not a code",
     .line = "error [2026-11-12T10:00:00Z]: the handler was registered",
     .expected = "Unknown"},
    {.label = "an empty bracket, not a code",
     .line = "error[]: the handler was registered",
     .expected = "Unknown"},
    {.label = "two spaces before the code",
     .line = "error  [E0425]: cannot find value `total` in this scope",
     .expected = "Unknown"},
    {.label = "a code with no colon after it",
     .line = "error[E0425] cannot find value `total` in this scope",
     .expected = "Unknown"},
    {.label = "a space between the code and the colon",
     .line = "error[E0425] : cannot find value `total` in this scope",
     .expected = "Unknown"},
}};
} // namespace

// invariant: the premise — every uncoded twin already reads Error, so the coded rows below test
// invariant: the code and nothing else.
TEST(VerdictRegisterCodedHead, EveryUncodedTwinReadsError)
{
    for (const CodedRow& row : kCodedRows)
        EXPECT_EQ(level_of(row.uncoded), kError)
            << "premise '" << row.label
            << "': the uncoded twin must already read Error\n  line: " << row.uncoded;
}

// invariant: THE PROPERTY — the code is read through, so the coded head reads what its twin reads.
TEST(VerdictRegisterCodedHead, ACodedErrorHeadReadsError)
{
    for (const CodedRow& row : kCodedRows)
        EXPECT_EQ(level_of(row.coded), kError)
            << "row '" << row.label
            << "': a bracketed code between the level word and its colon must not break the "
               "colon anchor\n  coded:   "
            << row.coded << "\n  uncoded: " << row.uncoded;
}

// invariant: the code is read through and NOTHING else is — in colour the coded head reads what
// invariant: its uncoded colour twin reads, so the anchor never reaches past an escape run alone.
TEST(VerdictRegisterCodedHead, ACodedHeadInColourReadsAsItsUncodedTwin)
{
    EXPECT_EQ(level_of(kColourCoded), level_of(kColourUncoded))
        << "the compiler's colour form: the code must change nothing its uncoded twin does not";
}

// invariant: the guards are what a rule NEARLY right would break; each must hold before and after.
TEST(VerdictRegisterCodedHead, ABracketThatIsNotACodeOrAWordThatIsNotErrorReadsNoError)
{
    for (const GuardRow& row : kGuardRows)
        EXPECT_EQ(level_of(row.line), row.expected)
            << "guard '" << row.label << "'\n  line: " << row.line;
}
