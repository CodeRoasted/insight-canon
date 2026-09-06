
// invariant: stage 1's budget is a TOKEN count and never a raw-byte head, because a budget that
// decides whether the producer's own kind word is read at all is part of the CLAIM.
// invariant: a byte head made that verdict a function of the PREFIX'S LENGTH.
// invariant: the rows pin, in order: the two shapes the value was pre-registered on, the property
// the change buys, the budget's two edges, and the declared residual in both halves.
// invariant: EVERY diagnostic row carries a failure cue in its BODY on purpose — without one,
// stage 1 and stage 2 would agree and the verdict could not name the stage that produced it.
// invariant: with a failure word in the body the two stages DISAGREE, so the verdict names the
// stage; the real shape is a compiler inlining diagnostic.
// invariant: under the old byte head these rows were RED, which is what makes them a falsifier and
// not a description.
// refs: ADR-16.D7, ADR-16.D8
#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogLevel;
using insight::utils::for_each_token;
using insight::utils::infer_leading_log_level;
using insight::utils::parse_log_level;

namespace
{
// invariant: the budget is quoted here rather than linked, so a value edit at its definition reds
// here — the coupling is the point.
constexpr std::size_t kBudget{8};
// invariant: stage 2's cue head is quoted the same way and for the same reason: the last row needs
// it to ASSERT its fixture satisfies stage 2's condition rather than assuming it.
constexpr std::size_t kCueHead{128};

// post: where the first level word sits, under the SHARED canon tokenization stage 1 scans with, so
// an index asserted here is the instrument's index.
// invariant: whole-line scan — this LOCATES the word wherever it is; whether stage 1 READS it is
// what the rows assert.
struct LevelWord
{
    bool found{false};
    std::size_t index{0};
    std::size_t byte{0};
    std::size_t size{0};
    LogLevel level{LogLevel::Unknown};
};

[[nodiscard]] LevelWord locate_level_word(std::string_view line)
{
    LevelWord word;
    std::size_t index{0};
    (void)for_each_token(line, 0U,
                         [&](std::string_view token) noexcept
                         {
                             const LogLevel level{parse_log_level(token)};
                             if (level == LogLevel::Unknown)
                             {
                                 ++index;
                                 return false;
                             }
                             word = {.found = true,
                                     .index = index,
                                     .byte = static_cast<std::size_t>(token.data() - line.data()),
                                     .size = token.size(),
                                     .level = level};
                             return true;
                         });
    return word;
}

// invariant: a producer-declared WARNING whose body carries a failure word, so stage 2 on its own
// would answer Error.
constexpr std::string_view kInlineBody{
    "inlining failed in call to 'always_inline' 'int parse(int)': function body not available "
    "[-Winline]"};

[[nodiscard]] std::string diagnostic(std::string_view path)
{
    return std::string{path} + ":210:21: warning: " + std::string{kInlineBody};
}

// post: a bracket-enclosed prefix of the requested token count in front of a CAPS level word, whose
// anchor is the pure token test and needs no kind-slot walk to be authoritative.
[[nodiscard]] std::string caps_after_brackets(std::size_t count)
{
    constexpr std::array<std::string_view, 10> kTags{"runner-7",  "job-42", "step-3",  "attempt-1",
                                                     "linux-x64", "gcc-16", "release", "shard-2",
                                                     "cache-hit", "retry-0"};
    std::string line;
    for (std::size_t i{0}; i < count; ++i)
        line += "[" + std::string{kTags[i % kTags.size()]} + "] ";
    return line + "WARNING " + std::string{kInlineBody};
}

// post: a NESTED RECORD — an outer CI frame, then a wrapped record with its own bare frame, then
// that inner record's kind word spliced in.
// invariant: the outer frame is BRACKETED on purpose, because bracketed tokens ARE declared prefix
// material and do NOT displace the kind slot.
// invariant: the first token that displaces it is the INNER record's own bare frame — without
// that the row would be re-pinning a different declared limitation and would prove nothing.
// refs: SRC-D-OUT-4c
[[nodiscard]] std::string nested_record(std::string_view kind)
{
    return "[runner-7] [job-42] [step-3] [attempt-1] May 29 10:00:00 api-1 kernel: " +
           std::string{kind} + ": worker died";
}
} // namespace

// invariant: the pre-registered stamp shape puts the level word at index five; the structural
// self-termination was REFUSED on exactly this row because one stamp token is not colon-terminated.
// invariant: which is why stage 1 needs a COUNT, and this row pins that the count reaches it.
TEST(LeadingScanTokenBudget, IsoCommaMillisStampPutsTheLevelWordAtIndexFiveAndItIsRead)
{
    constexpr std::string_view kLine{"2026-05-29 10:00:00,123 WARN pool exhausted"};
    const LevelWord word{locate_level_word(kLine)};
    ASSERT_TRUE(word.found) << "no level word located on: " << kLine;
    EXPECT_EQ(word.index, 5U) << "pre-registered index 5 (byte 24); got index " << word.index
                              << " at byte " << word.byte;
    EXPECT_EQ(word.byte, 24U);
    EXPECT_EQ(infer_leading_log_level(kLine), LogLevel::Warn)
        << "Stage 1 must read the stamped WARN (caps, authoritative); Stage 2 has no cue on this "
           "line and would answer Unknown\n  line: "
        << kLine;
}

// invariant: the witness the ruling was argued on — a 34-byte build path alone puts the kind word
// past the old head with no escape byte anywhere.
// invariant: the same diagnostic then classified by HOW LONG ITS PATH WAS; the token index is three
// whatever the path's length.
TEST(LeadingScanTokenBudget, PathLineColumnDiagnosticPutsTheKindWordAtIndexThreeAndItIsRead)
{
    const std::string line{diagnostic("/builds/acme/widget/src/parser.cpp")};
    const LevelWord word{locate_level_word(line)};
    ASSERT_TRUE(word.found) << "no level word located on: " << line;
    EXPECT_EQ(word.index, 3U) << "pre-registered index 3; got " << word.index;
    EXPECT_EQ(word.byte, 43U) << "pre-registered byte 43 (past the replaced 40-byte head); got "
                              << word.byte;
    EXPECT_EQ(infer_leading_log_level(line), LogLevel::Warn)
        << "the producer's own kind word must win: Stage 1 reads `warning:` in the kind slot; an "
           "Error here is Stage 2 answering from the `failed` in the message body, i.e. the kind "
           "word was never reached\n  line: "
        << line;
}

// invariant: METAMORPHIC over one shape — the path grows from 1 to 200 bytes, the token index
// stays three, and the verdict must not move.
// invariant: measured on the old byte head the same diagnostic read Warn, then Error, then Unknown
// as the path grew; no byte constant fixes that, only removing bytes from the claim.
TEST(LeadingScanTokenBudget, VerdictIsInvariantUnderPathLengthAtAFixedTokenIndex)
{
    constexpr std::array<std::size_t, 10> kPathLengths{1, 8, 16, 24, 32, 34, 40, 64, 100, 200};
    for (const std::size_t length : kPathLengths)
    {
        const std::string line{diagnostic("/" + std::string(length, 'p') + "/parser.cpp")};
        const LevelWord word{locate_level_word(line)};
        ASSERT_TRUE(word.found) << "no level word located on: " << line;
        EXPECT_EQ(word.index, 3U) << "the path is ONE token whatever its length; got index "
                                  << word.index << " at path length " << length;
        EXPECT_EQ(infer_leading_log_level(line), LogLevel::Warn)
            << "the verdict moved at path length " << length << " (the kind word starts at byte "
            << word.byte << ", token index " << word.index
            << ") — Stage 1's budget must be a token count, never a byte count\n  line: " << line;
    }
}

// invariant: the budget's two EDGES, pinned by behaviour — the last index inside is read and the
// first outside is not, so a budget one smaller reds the first arm and one larger the second.
TEST(LeadingScanTokenBudget, TheLastIndexInsideTheBudgetIsReadAndTheFirstOutsideIsNot)
{
    const std::string inside{caps_after_brackets(kBudget - 1)};
    const LevelWord inside_word{locate_level_word(inside)};
    ASSERT_TRUE(inside_word.found);
    ASSERT_EQ(inside_word.index, kBudget - 1) << "fixture error: expected the level word at index "
                                              << kBudget - 1 << ", got " << inside_word.index;
    EXPECT_EQ(infer_leading_log_level(inside), LogLevel::Warn)
        << "index " << kBudget - 1 << " is the last index inside kLeadingScanTokens{" << kBudget
        << "} and must be read (the word starts at byte " << inside_word.byte
        << ")\n  line: " << inside;

    const std::string outside{caps_after_brackets(kBudget)};
    const LevelWord outside_word{locate_level_word(outside)};
    ASSERT_TRUE(outside_word.found);
    ASSERT_EQ(outside_word.index, kBudget) << "fixture error: expected the level word at index "
                                           << kBudget << ", got " << outside_word.index;
    EXPECT_EQ(infer_leading_log_level(outside), LogLevel::Error)
        << "index " << kBudget << " is the first index OUTSIDE kLeadingScanTokens{" << kBudget
        << "}: Stage 1 must not read it, so Stage 2 decides from the body's `failed` (Error). A "
           "Warn here means the budget reads one token too many\n  line: "
        << outside;
}

// invariant: a NESTED record's kind word is the verdict-shaped part of the residual the budget
// leaves unread, measured at 2.05 % of level-bearing lines on one corpus.
// invariant: the ruling is that the word is the INNER record's level, and reading it as the line's
// verdict would attribute the inner severity to the outer line — a residual, not a miss.
// invariant: the fixture uses a level word that is NOT a cue, so stage 1 reading it would answer
// one level and stage 2 answers Unknown — the verdict names the stage.
// invariant: MEASURED while writing this row: an error-class inner word classifies the same on both
// budgets, because stage 2's cue scan promotes it wherever stage 1 stopped.
// invariant: so an error-class inner word discriminates nothing here, and the residual the ruling
// declares is stage 1's ONLY.
TEST(LeadingScanTokenBudget, ANestedRecordsKindWordAtIndexNineIsTheDeclaredResidualAndIsNotRead)
{
    constexpr std::string_view kLine{
        "[2026-05-29 10:00:00] [runner-7] [job-42] May api-1 kernel: INFO: worker restarted"};
    const LevelWord word{locate_level_word(kLine)};
    ASSERT_TRUE(word.found) << "no level word located on: " << kLine;
    ASSERT_EQ(word.index, 9U) << "fixture error: the negative control is pre-registered at index "
                                 "9; got "
                              << word.index;
    EXPECT_EQ(infer_leading_log_level(kLine), LogLevel::Unknown)
        << "the wrapped record's INFO at index " << word.index << " is outside kLeadingScanTokens{"
        << kBudget
        << "} and is the declared residual: Stage 1 must not read it, and Stage 2 has no cue on "
           "this line. An Info is the budget reaching a nested record's kind word\n  line: "
        << kLine;
}

// invariant: the OTHER half of the same declared residual — the class where stage 2's first two
// conditions hold and the third does not.
// invariant: a lowercase kind word behind a nested record's bare outer frame, whose bare tokens
// break the kind slot.
// invariant: it is OWED because the ruling REFUSES to relax the register on exactly this shape, and
// a refusal argued from a boundary that can move without notice rests on nothing.
// invariant: TWO-SIDED, and both sides are changes the refusal was argued against — the lowercase
// arm must stay Unknown and the CAPS arm on the SAME line must stay Error.
// invariant: if the caps arm stops firing the line no longer reaches stage 2 at all and the Unknown
// above has become VACUOUS — still green, pinning nothing.
// invariant: the fixture's own preconditions are ASSERTED rather than assumed, or the line is a
// different class under this one's name.
// refs: ADR-16.D8, SRC-D-OUT-4c
TEST(LeadingScanTokenBudget, ANestedRecordsLowercaseErrorAtIndexElevenIsTheDeclaredR2Residual)
{
    const std::string declined{nested_record("error")};
    const LevelWord word{locate_level_word(declined)};
    ASSERT_TRUE(word.found) << "no level word located on: " << declined;
    ASSERT_EQ(word.index, 11U)
        << "fixture error: the R2 witness is pre-registered at token index 11; got " << word.index
        << "\n  line: " << declined;
    ASSERT_GE(word.index, kBudget)
        << "fixture error: the word must sit OUTSIDE kLeadingScanTokens{" << kBudget
        << "} — inside it, Stage 1 reads it and the line is not in the residual at all";
    // invariant: condition (a) — the word starts inside stage 2's cue head, which excludes the
    // class that sits past it.
    ASSERT_LT(word.byte, kCueHead)
        << "fixture error: the word starts at byte " << word.byte << ", outside kKeywordHead{"
        << kCueHead << "} — that is class R3 (a BUDGET), not R2 (the REGISTER)";
    const std::string_view token{std::string_view{declined}.substr(word.byte, word.size)};
    // invariant: condition (b) — membership is asked of the SHIPPED table and never re-listed
    // here, because a row that enumerates the vocabulary goes stale the day one word is added.
    // refs: DN-37.D20
    ASSERT_TRUE(insight::utils::detail::is_failure_lexicon_word(token))
        << "fixture error: `" << token
        << "` is not a kFailureLexicon word — that is class R1 (invisible to every register), not "
           "R2";
    // invariant: condition (c) is the one that FAILS, and the mechanism is named rather than
    // inferred: the kind-slot walk stops at the inner record's first bare token.
    EXPECT_FALSE(insight::utils::detail::is_verdict_anchored(declined, token))
        << "R2 IS the register's population: `" << token
        << "` must be REFUSED the anchor. Anchored here means the kind-slot walk now accepts a "
           "nested record's bare outer frame as prefix material\n  line: "
        << declined;

    EXPECT_EQ(infer_leading_log_level(declined), LogLevel::Unknown)
        << "the wrapped record's `" << token << "` at token index " << word.index << " (byte "
        << word.byte << ", inside kKeywordHead{" << kCueHead
        << "}) is ADR-16.D8's R2 residual: Stage 1 is past its budget and Stage 2's register "
           "refuses it, so the line reads Unknown. An Error here means the register ADMITTED the "
           "shape — the ruling refused that on measurement (62 GitLab words in runs that passed) "
           "and the refusal must be re-argued before this row moves\n  line: "
        << declined;

    // invariant: the second side — same frame, same index, same byte, and one word's CASE.
    const std::string anchored{nested_record("ERROR")};
    const LevelWord caps{locate_level_word(anchored)};
    ASSERT_TRUE(caps.found) << "no level word located on: " << anchored;
    ASSERT_EQ(caps.index, word.index)
        << "fixture error: the control must differ from the R2 line in CASE ONLY — token index "
        << caps.index << " against " << word.index;
    ASSERT_EQ(caps.byte, word.byte)
        << "fixture error: the control must differ from the R2 line in CASE ONLY — byte "
        << caps.byte << " against " << word.byte;
    EXPECT_EQ(infer_leading_log_level(anchored), LogLevel::Error)
        << "the CAPS control on the same frame must fire: anchor #1 is a pure token test and needs "
           "no kind slot. An Unknown here means the R2 assertion above is VACUOUS — the line stops "
           "reaching Stage 2 (lexicon, cue head or tokenizer moved) and would keep passing while "
           "pinning nothing\n  line: "
        << anchored;
}
