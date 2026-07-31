// NOLINTBEGIN : Unit tests may intentionally violate some style rules for clarity or simplicity.
// tests/scan/test_ansi_normalization.cpp
//
// Unit coverage for STAGE 1 — `normalize()`, canon's universal ANSI ingest normalization
// (SRC-D-TID-11), now the FACTORY that mints `NormalizedLine` (ADR-21.D2/D3: the out-parameter
// strip form is REMOVED; the return type carries the proof that stage 1 ran).
//
// WHY THIS FILE EXISTS AT ALL. Stage 1's only coverage used to be three assertions inside
// `tests/mask/test_stateless_template.cpp`, filed under `mask/` because the template-identity path
// was its only consumer. That is no longer true: the function is public in `insight.canon.api` and
// carries a NORMATIVE precondition on `recognize()`/`classify()` — every consumer of those two must
// run it. A newly-public API whose tests live under an unrelated domain is coverage nobody finds,
// so the three assertions MOVED here and grew. (The corresponding move is why
// `test_stateless_template.cpp` no longer tests escapes at all.)
//
// WHAT A FIXTURE IS GOOD FOR, AND WHAT IT IS NOT. The population-bearing property — *do the two
// content derivations agree over a real corpus* — cannot be a fixture: a hand-authored set can only
// encode the shapes its author enumerated, and the shape that caused the ingest-normalization
// defect was by definition one nobody had. That property is a differential corpus gate elsewhere.
// What belongs HERE is the complement: NAMED hazards on a single function, which is the one thing a
// fixture does well. Each test below is a named hazard with a stated consequence.

#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight::tokenization;

// SRC-D-TID-11's core claim: colour is presentation, never content. Two coloured variants of one
// line fold to the same colour-free bytes, so they cannot mint two templates.
TEST(AnsiNormalization, EscapesInterleavedWithTokensAreRemoved)
{
    std::string scratch;
    EXPECT_EQ(normalize("\x1b[31mERROR\x1b[0m: pool down", scratch).bytes(), "ERROR: pool down");
    // An escape INSIDE a word is the reason a per-token mask cannot reach these and they must die
    // at ingest: `for_each_token` treats an escape run as a DELIMITER, so un-normalized this is two
    // tokens and the level word is never seen.
    EXPECT_EQ(normalize("\x1b[31mER\x1b[0mROR: pool down", scratch).bytes(), "ERROR: pool down");
}

// The fast path now lives INSIDE the factory: a line with no escape byte is a FIXED POINT of the
// strip, so `normalize()` borrows the caller's line with NO copy and the scratch is untouched.
// Both halves are asserted — the bytes AND the zero-copy borrow (data pointer identity), because
// the borrow is what makes the per-line cost of the typed seam a memchr rather than a memcpy.
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

// A leading CSI run displaces an anchored prefix off offset 0, and `recognize()`/`classify()` are
// anchored longest-prefix walks — so the row simply does not fire, with no error and no counter.
// That is the whole mechanism behind the 1 077 lost GitLab section markers.
//
// BOTH forms are asserted because the CLASS is "a leading CSI run", not the byte string `ESC[0K`:
// the same corpus carries it as `ESC[0;m`, and a stage 1 that handled only the first would be a
// mirror of one producer with an expiry date nobody would notice.
TEST(AnsiNormalization, ALeadingCsiRunIsRemovedSoAnAnchoredPrefixReachesOffsetZero)
{
    std::string scratch;
    EXPECT_EQ(normalize("\x1b[0Ksection_start:1746093602:prepare_executor", scratch).bytes(),
              "section_start:1746093602:prepare_executor");
    EXPECT_EQ(normalize("\x1b[0;msection_start:1746093602:prepare_executor", scratch).bytes(),
              "section_start:1746093602:prepare_executor");
    // Several escapes in one run — the same rule, no special case for "exactly one".
    EXPECT_EQ(normalize("\x1b[0K\x1b[36;1msection_start:1:x", scratch).bytes(),
              "section_start:1:x");
}

// THE PRECISION LEG, and it can genuinely fail. A shell xtrace echoes the SOURCE of a command, so
// the corpus carries lines whose `\e` is a LITERAL BACKSLASH followed by 'e' — two ordinary
// characters, not the 0x1b byte. That is prose DESCRIBING an escape and must survive as prose: a
// stage 1 that also interpreted textual escape notation would mint markers out of scripts that
// merely mention one. (The recognition half of this property is the GitLab package's, asserted at
// semantic/gitlab/tests/test_gitlab_markers.cpp.)
TEST(AnsiNormalization, LiteralEscapeNotationSurvivesAsProse)
{
    std::string scratch;
    EXPECT_EQ(normalize(R"(++ echo -e '\e[0Ksection_start:1746093602:prepare_executor')", scratch)
                  .bytes(),
              R"(++ echo -e '\e[0Ksection_start:1746093602:prepare_executor')");
}

// OSC is the arm the SIBLING grammar in this codebase does not have: `detail::ansi_escape_len`
// (canon.api.cppm, feeding `for_each_token`) returns 1 for an `ESC ]` and then scans the OSC BODY
// as content. Measured at 0 occurrences in the scanned head across all four corpora — latent, not
// live — which is exactly why the arm that DOES handle it needs a test that keeps it honest.
TEST(AnsiNormalization, OscSequencesAreDroppedWholeAtEitherTerminator)
{
    std::string scratch;
    // BEL-terminated. (`\a` = BEL 0x07; a `\x07b` hex-escape would greedily absorb the trailing
    // 'b'.)
    EXPECT_EQ(normalize("a\x1b]0;title\ab", scratch).bytes(), "ab");
    // ST-terminated (ESC \) — the other ECMA-48 terminator, and the one a BEL-only scanner would
    // run past, swallowing the rest of the line as an OSC body.
    EXPECT_EQ(normalize("a\x1b]0;title\x1b\\b", scratch).bytes(), "ab");
}

// A two-byte ESC sequence that is neither CSI nor OSC (charset select, reset, …) is consumed whole.
// The hazard is off-by-one in either direction: consuming one byte leaves the final byte as content
// ('B' from `ESC ( B` becoming a token), consuming three eats a byte of real content.
TEST(AnsiNormalization, ATwoByteEscapeSequenceIsConsumedWhole)
{
    std::string scratch;
    // ESC ( is the two-byte sequence; 'B' is the charset designator, kept.
    EXPECT_EQ(normalize("a\x1b(Bb", scratch).bytes(), "aBb");
    // ESC 7 (save cursor) — nothing follows the designator.
    EXPECT_EQ(normalize("a\x1b"
                        "7b",
                        scratch)
                  .bytes(),
              "ab");
    // A lone trailing ESC has no sequence to complete and is dropped rather than emitted.
    EXPECT_EQ(normalize("tail\x1b", scratch).bytes(), "tail");
}

// `LogParser::parse_line` branches on an EMPTY strip result — a line that was nothing but escape
// bytes yields no event rather than an empty one. That branch is only reachable if the strip can
// actually return empty for a non-empty input, so the property is load-bearing rather than
// cosmetic.
TEST(AnsiNormalization, ALineOfNothingButEscapeBytesStripsToEmpty)
{
    std::string scratch;
    const auto normalized{normalize("\x1b[0K\x1b[39m\x1b[0m", scratch)};
    EXPECT_TRUE(normalized.bytes().empty())
        << "expected empty, got: " << std::string{normalized.bytes()};
}

// The strip is idempotent, and the caller's scratch is REUSED across lines (LogParser holds one
// `escape_scratch_` for the whole stream; eidos's seam holds one per line loop). A strip that
// appended instead of clearing would leak the previous line into this one — silently, and only on
// the second and later ESC-bearing lines of a stream.
TEST(AnsiNormalization, TheScratchIsClearedSoReuseCannotLeakThePreviousLine)
{
    std::string scratch;
    EXPECT_EQ(normalize("\x1b[31mfirst line\x1b[0m", scratch).bytes(), "first line");
    EXPECT_EQ(normalize("\x1b[32msecond\x1b[0m", scratch).bytes(), "second");
    // Idempotence: normalizing already-normalized bytes changes nothing (and, being escape-free,
    // takes the zero-copy fast path).
    std::string again;
    const auto once{normalize("\x1b[31mfirst line\x1b[0m", scratch)};
    EXPECT_EQ(normalize(once.bytes(), again).bytes(), once.bytes());
}
// NOLINTEND
