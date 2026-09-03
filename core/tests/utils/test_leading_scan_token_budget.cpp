// Unit tests: allow short identifiers and test-specific patterns
// tests/utils/test_leading_scan_token_budget.cpp
//
// ADR-16.D7 — Stage 1's budget is a TOKEN count (kLeadingScanTokens{8}, time_utils.cpp), never a
// raw-byte head. A budget that decides whether the producer's own kind word is read at all is part
// of the CLAIM, and a byte head made that verdict a function of the prefix's LENGTH. These rows
// pin, in order: the two shapes the budget's value was pre-registered on (the G-L11 instrument,
// tools/leading_level_token_index_measure.cpp, self-tests the same two rows); the property the unit
// change buys — the verdict follows the level word's token INDEX and not the byte it starts at;
// the budget's two edges; and the declared residual, as a positive boundary assertion — in BOTH
// its halves. Row 5 is Stage 1's half (the budget does not reach a nested record's kind word);
// Row 6 is Stage 2's, ADR-16.D8's R2 class (the register refuses a lowercase `error:` behind that
// same frame). ADR-16.D8 refuses two remedies ON the R2 shape, so the shape is asserted here
// rather than left to shift silently.
//
// EVERY DIAGNOSTIC ROW CARRIES A FAILURE CUE IN ITS BODY, on purpose. A `warning:` line whose
// message says nothing alarming classifies Warn under Stage 2's warning cue as well, so such a row
// cannot tell "Stage 1 read the producer's kind word" from "Stage 2 guessed the same level". With
// `failed` in the body the two stages DISAGREE — Stage 1 says Warn (the producer's own kind word,
// anchored in the kind slot), Stage 2 says Error (canon's own cue) — so the verdict names the stage
// that produced it. gcc's -Winline text is the real shape: `warning: inlining failed in call to …`.
// Under the 40-byte head this file's diagnostic rows were RED (the level word at byte 43 was never
// reached, Stage 2 answered Error), which is what makes them a falsifier and not a description.

#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogLevel;
using insight::utils::for_each_token;
using insight::utils::infer_leading_log_level;
using insight::utils::parse_log_level;

namespace
{
// The budget as landed. Function-local in time_utils.cpp and deliberately NOT linked: this file
// pins the budget's EDGES by behaviour (index 7 read, index 8 not), so a value edit there reds
// here — the coupling is the point, exactly as the kind-slot rows couple to the prefix classes.
constexpr std::size_t kBudget{8};
// Stage 2's cue head, quoted the same way and for the same reason: kKeywordHead{128} raw bytes is
// the byte a token must START before to be read by the cue scan, and Row 6 needs it to assert that
// its fixture satisfies Stage 2's condition (a) rather than assuming it.
constexpr std::size_t kCueHead{128};

// Where the first level word sits, under the shared canon tokenisation Stage 1 scans with — the
// same predicate the instrument's self-test uses, so an index asserted here is the instrument's
// index. Whole line (scan limit 0): this locates the word wherever it is; whether Stage 1 READS it
// is what the rows below assert.
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

// gcc -Winline, verbatim shape: a producer-declared WARNING whose body carries `failed`, so Stage 2
// on its own would answer Error.
constexpr std::string_view kInlineBody{
    "inlining failed in call to 'always_inline' 'int parse(int)': function body not available "
    "[-Winline]"};

[[nodiscard]] std::string diagnostic(std::string_view path)
{
    return std::string{path} + ":210:21: warning: " + std::string{kInlineBody};
}

// A bracket-enclosed prefix of `count` tokens (kind-slot prefix material, one token each — no
// colon inside, so the token count IS the bracket count) in front of a CAPS level word, whose
// anchor is the pure token test (anchor #1) and needs no kind-slot walk to be authoritative.
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

// A NESTED RECORD, in the shape ADR-16.D8 names: an outer CI frame — runner, job, step, attempt —
// then a wrapped syslog record with its own bare month/day/time/host/tag frame, then that inner
// record's kind word. `kind` is spliced in so the two arms of Row 6 differ in the CASE OF ONE
// WORD and in nothing else.
//
// THE OUTER FRAME IS BRACKETED ON PURPOSE. `[runner-7]` and its siblings ARE declared prefix
// material (SRC-D-OUT-4c class 2), so they do NOT displace the kind slot; the first token that
// does is `May`, the INNER record's own bare frame. Without that the row would be re-pinning
// `VerdictRegisterKindSlot.ALeadingTimestampIsNotPrefixMaterialDeclaredLimitation` — a different
// declared limitation, whose displacing token is a leading timestamp at index 0 — and an R2 row
// whose refusal is not attributable to the nested record proves nothing about R2.
[[nodiscard]] std::string nested_record(std::string_view kind)
{
    return "[runner-7] [job-42] [step-3] [attempt-1] May 29 10:00:00 api-1 kernel: " +
           std::string{kind} + ": worker died";
}
} // namespace

// ── Row 1 — pre-registered shape: the ISO comma-millis stamp, level word at index 5 ─────────
// `2026-05-29 10:00:00,123 WARN` tokenises to 2026-05-29 / 10 / 00 / 00 / 123 / WARN: five short
// stamp tokens, byte 24. ADR-16.D7 refused the structural self-termination ("scan while the token
// is prefix material") on exactly this row — the second `00` is not colon-terminated — which is why
// Stage 1 needs a COUNT. The row pins that the count reaches it.
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

// ── Row 2 — pre-registered shape: `<path>:<line>:<col>: warning:`, index 3 at byte 43 ───────
// The witness ADR-16.D7 was argued on: a 34-byte build path alone puts `warning` past the old
// 40-byte head with no escape byte anywhere, and the same diagnostic then classified by how long
// its path was. Index 3 — path / line / column / kind word — whatever the path's length.
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

// ── Row 3 — the property the unit change buys: byte-length invariance at a fixed index ─────
// METAMORPHIC over one shape: the path grows from 1 to 200 bytes, the token index stays 3, and the
// verdict must not move. MEASURED on the 40-byte head before this arm's code landed: Warn at path
// lengths 1-16, Error from 24 (the kind word at byte 44, the message body deciding) and Unknown at
// 200 (the body's own cue past the 128-byte cue head as well) — ADR-16.D7's table, walked by one
// diagnostic. No byte constant fixes that; only removing bytes from the claim does.
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

// ── Row 4 — the budget's two edges: index kBudget-1 is read, index kBudget is not ───────────
// The same line grown by one prefix token. At index 7 the CAPS level word is authoritative (Warn);
// at index 8 it is outside the budget and Stage 2 answers from the body (`failed` -> Error). This
// pins the VALUE by behaviour: a budget of 7 reds the first arm, a budget of 9 reds the second.
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

// ── Row 5 — the declared residual, pinned as a positive boundary assertion ──────────────────
// A NESTED record — a CI line wrapping an application/syslog record, its level word at token 8-15
// — is the verdict-shaped part of the residual the value 8 leaves unread (GHA 23 324 lines, 2.05 %
// of level-bearing, on the G-L11 measurement). Eqya's ruling (2026-09-02, the claim boundary's
// owner): that word is the INNER record's level, and reading it as the line's verdict would
// attribute the inner severity to the outer line — so it is a residual, not a recall miss. Index 9
// here, on a level word that is NOT a cue: with INFO, Stage 1 reading it would answer Info and
// Stage 2 answers Unknown, so the verdict names the stage. MEASURED while writing this row: the
// same line with `ERROR:` classifies Error on the 40-byte head AND under the token budget alike,
// because Stage 2's cue scan (kKeywordHead, 128 bytes) promotes a caps error-class word wherever
// Stage 1 stopped — so an error-class inner word discriminates nothing here, and the residual the
// ruling declares is Stage 1's only: a nested error-class word within 128 bytes is still promoted
// through canon's own cue path, as it was before this budget existed.
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

// ── Row 6 — the OTHER half of the same declared residual: the R2 shape ──────────────────────
// ADR-16.D8 partitions the UNREAD nested-record population by which of Stage 2's three conditions
// fails: (a) the word starts inside kKeywordHead{128} raw bytes, (b) it is a kFailureLexicon word,
// (c) it is verdict-anchored. R2 is the class where (a) and (b) HOLD and (c) does not — a
// lowercase `error:` at token index >= kLeadingScanTokens{8}, behind a nested record's bare outer
// frame, whose kind slot those bare tokens break (SRC-D-OUT-4c). Row 5 above pins Stage 1's half
// of the residual; this row pins Stage 2's, and the two are one subject.
//
// WHY IT IS OWED. At ADR-16.D8's coordinate — three CI corpus roots, measured 2026-09-02 — R2 is
// 51 of GitHub Actions' 86 unread error-class inner words and 83 of GitLab's 83, and the ruling
// REFUSES to relax the register on exactly this shape (admitting it would promote 62 GitLab words
// sitting in runs that PASSED, against a precision-first contract). A refusal argued from a
// boundary that can move without notice rests on nothing, so the boundary is asserted here.
//
// TWO-SIDED, AND BOTH SIDES ARE CHANGES THE REFUSAL WAS ARGUED AGAINST:
//   * the lowercase arm reads Unknown — if it starts reading Error, the register admitted the
//     shape and the ruling's refusal is no longer describing the code;
//   * the CAPS arm on the SAME line reads Error — anchor #1 is a pure token test and needs no kind
//     slot. If that stops firing, the line no longer reaches Stage 2 at all (the lexicon lost
//     `error`, the cue head shrank, the tokenizer moved) and the Unknown above has become VACUOUS
//     — still green, pinning nothing. That is the failure mode a one-sided residual row cannot
//     see, and it is why the ADR names the caps control as the row's second side.
//
// The fixture's own preconditions are ASSERTED rather than assumed: (a) and (b) must hold, or the
// line is R3 or R1 and this row would be pinning a different class under R2's name.
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
    // Condition (a) — the word starts inside Stage 2's cue head, so R3 (past the head) is excluded.
    ASSERT_LT(word.byte, kCueHead)
        << "fixture error: the word starts at byte " << word.byte << ", outside kKeywordHead{"
        << kCueHead << "} — that is class R3 (a BUDGET), not R2 (the REGISTER)";
    const std::string_view token{std::string_view{declined}.substr(word.byte, word.size)};
    // Condition (b) — membership asked of the SHIPPED table, never a re-listing here (DN-37.D20):
    // a row that enumerates the eighteen words for itself goes stale the day one is added.
    ASSERT_TRUE(insight::utils::detail::is_failure_lexicon_word(token))
        << "fixture error: `" << token
        << "` is not a kFailureLexicon word — that is class R1 (invisible to every register), not "
           "R2";
    // Condition (c) — the one that fails, and the mechanism named rather than inferred from the
    // verdict: the kind-slot walk stops at `May`, the inner record's first bare token.
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

    // The second side. Same frame, same index, same byte, one word's CASE.
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
