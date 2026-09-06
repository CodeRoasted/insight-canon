
// invariant: unit coverage for STAGE 1, the ingest normalization, which is now the FACTORY that
// mints the normalized-line type.
// invariant: the out-parameter strip form is REMOVED, so the return type carries the proof that
// stage 1 ran.
// invariant: WHY THIS FILE EXISTS AT ALL — stage 1's only coverage used to be three assertions
// inside the masker suite, filed there because template identity was its only consumer.
// invariant: that is no longer true: the function is PUBLIC and carries a NORMATIVE precondition on
// the two recognition walkers, so every consumer of those must run it.
// invariant: a newly-public API whose tests live under an unrelated domain is COVERAGE NOBODY
// FINDS, so the three assertions moved here and grew.
// invariant: WHAT A FIXTURE IS GOOD FOR, AND WHAT IT IS NOT — the population-bearing property,
// whether the two content derivations agree over a real corpus, CANNOT be a fixture.
// invariant: a hand-authored set can only encode the shapes its author enumerated, and the shape
// that caused the defect was BY DEFINITION one nobody had.
// invariant: that property is a differential corpus gate elsewhere; what belongs HERE is the
// complement — NAMED hazards on a single function, which is the one thing a fixture does well.
// invariant: each test below is a NAMED hazard with a STATED consequence.
// refs: SRC-D-TID-11
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight::tokenization;

// invariant: two coloured variants of one line fold to the same colour-free bytes, so they cannot
// mint two templates.
// refs: SRC-D-TID-11
TEST(AnsiNormalization, EscapesInterleavedWithTokensAreRemoved)
{
    std::string scratch;
    EXPECT_EQ(normalize("\x1b[31mERROR\x1b[0m: pool down", scratch).bytes(), "ERROR: pool down");
    // invariant: an escape INSIDE a word is why a per-token mask cannot reach these and they must
    // die at INGEST.
    // invariant: the token walk treats an escape run as a DELIMITER, so un-normalized this is two
    // tokens and the level word is never seen.
    EXPECT_EQ(normalize("\x1b[31mER\x1b[0mROR: pool down", scratch).bytes(), "ERROR: pool down");
}

// invariant: the fast path lives INSIDE the factory — a line with no escape byte is a FIXED POINT
// of the strip, so the factory borrows the caller's line with NO copy and the scratch is untouched.
// invariant: BOTH halves are asserted, the bytes AND the zero-copy borrow through pointer identity,
// because the borrow is what makes the typed seam's per-line cost a scan and not a copy.
TEST(AnsiNormalization, AnEscapeFreeLineIsAZeroCopyFixedPoint)
{
    std::string scratch{"sentinel"};
    constexpr std::string_view plain{"plain text, no escapes"};
    const auto normalized{normalize(plain, scratch)};
    EXPECT_EQ(normalized.bytes(), plain);
    EXPECT_EQ(static_cast<const void*>(normalized.bytes().data()),
              static_cast<const void*>(plain.data()))
        << "an escape-free line must be BORROWED, not copied";
    EXPECT_EQ(scratch, "sentinel") << "the fast path must not touch the scratch";
    EXPECT_EQ(normalize("", scratch).bytes(), "");
}

// invariant: a LEADING control run displaces an anchored prefix off offset 0, and the two
// recognition walkers are anchored longest-prefix walks.
// invariant: so the row simply does not fire, with NO error and NO counter — that is the whole
// mechanism behind 1 077 lost section markers.
// invariant: BOTH forms are asserted because the CLASS is a leading control run and not one byte
// string — the same corpus carries a second spelling.
// invariant: a stage 1 handling only the first would be a MIRROR of one producer, with an expiry
// date nobody would notice.
TEST(AnsiNormalization, ALeadingCsiRunIsRemovedSoAnAnchoredPrefixReachesOffsetZero)
{
    std::string scratch;
    EXPECT_EQ(normalize("\x1b[0Ksection_start:1746093602:prepare_executor", scratch).bytes(),
              "section_start:1746093602:prepare_executor");
    EXPECT_EQ(normalize("\x1b[0;msection_start:1746093602:prepare_executor", scratch).bytes(),
              "section_start:1746093602:prepare_executor");
    // invariant: several escapes in one run take the same rule, with no special case for exactly
    // one.
    EXPECT_EQ(normalize("\x1b[0K\x1b[36;1msection_start:1:x", scratch).bytes(),
              "section_start:1:x");
}

// invariant: THE PRECISION LEG, and it can genuinely fail — a shell trace echoes the SOURCE of a
// command, so the corpus carries lines whose escape notation is two ORDINARY characters.
// invariant: that is prose DESCRIBING an escape and must survive as prose.
// invariant: a stage 1 that also interpreted textual escape notation would mint markers out of
// scripts that merely MENTION one.
TEST(AnsiNormalization, LiteralEscapeNotationSurvivesAsProse)
{
    std::string scratch;
    EXPECT_EQ(normalize(R"(++ echo -e '\e[0Ksection_start:1746093602:prepare_executor')", scratch)
                  .bytes(),
              R"(++ echo -e '\e[0Ksection_start:1746093602:prepare_executor')");
}

// invariant: the operating-system-command arm is the one the SIBLING grammar in this codebase does
// NOT have.
// invariant: that scanner returns a length of one for such an introducer and then reads the BODY as
// content.
// invariant: measured at ZERO occurrences in the scanned head across all four corpora, so it is
// LATENT rather than live.
// invariant: that is exactly why the arm that DOES handle it needs a test to keep it honest.
TEST(AnsiNormalization, OscSequencesAreDroppedWholeAtEitherTerminator)
{
    std::string scratch;
    // invariant: the bell-terminated form, and the hex escape is written carefully because a bare
    // one would greedily absorb the following letter.
    EXPECT_EQ(normalize("a\x1b]0;title\ab", scratch).bytes(), "ab");
    // invariant: the other standard terminator, and the one a bell-only scanner would run PAST,
    // swallowing the rest of the line as a command body.
    EXPECT_EQ(normalize("a\x1b]0;title\x1b\\b", scratch).bytes(), "ab");
}

// invariant: a two-byte escape sequence that is neither of the two families is consumed WHOLE.
// invariant: the hazard is off-by-one in EITHER direction — consuming one byte leaves the final
// byte as content, and consuming three eats a byte of real content.
TEST(AnsiNormalization, ATwoByteEscapeSequenceIsConsumedWhole)
{
    std::string scratch;
    EXPECT_EQ(normalize("a\x1b(Bb", scratch).bytes(), "aBb");
    EXPECT_EQ(normalize("a\x1b"
                        "7b",
                        scratch)
                  .bytes(),
              "ab");
    // invariant: a LONE TRAILING escape has no sequence to complete and is DROPPED rather than
    // emitted.
    EXPECT_EQ(normalize("tail\x1b", scratch).bytes(), "tail");
}

// invariant: the parser branches on an EMPTY strip result, so a line that was nothing but escape
// bytes yields no event rather than an empty one.
// invariant: that branch is only REACHABLE if the strip can actually return empty for a non-empty
// input, so the property is load-bearing rather than cosmetic.
TEST(AnsiNormalization, ALineOfNothingButEscapeBytesStripsToEmpty)
{
    std::string scratch;
    const auto normalized{normalize("\x1b[0K\x1b[39m\x1b[0m", scratch)};
    EXPECT_TRUE(normalized.bytes().empty())
        << "expected empty, got: " << std::string{normalized.bytes()};
}

// invariant: the strip is IDEMPOTENT, and the caller's scratch is REUSED across lines — the
// parser holds one for the whole stream and the downstream seam holds one per line loop.
// invariant: a strip that APPENDED instead of clearing would leak the previous line into this one,
// SILENTLY, and only on the second and later escape-bearing lines of a stream.
TEST(AnsiNormalization, TheScratchIsClearedSoReuseCannotLeakThePreviousLine)
{
    std::string scratch;
    EXPECT_EQ(normalize("\x1b[31mfirst line\x1b[0m", scratch).bytes(), "first line");
    EXPECT_EQ(normalize("\x1b[32msecond\x1b[0m", scratch).bytes(), "second");
    // invariant: IDEMPOTENCE — normalizing already-normalized bytes changes nothing, and being
    // escape-free it takes the zero-copy fast path.
    std::string again;
    const auto once{normalize("\x1b[31mfirst line\x1b[0m", scratch)};
    EXPECT_EQ(normalize(once.bytes(), again).bytes(), once.bytes());
}
