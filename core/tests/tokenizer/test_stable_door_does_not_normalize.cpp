// test_stable_door_does_not_normalize.cpp — the stable door's EXEMPTION, stated positively.
//
// THE PROPERTY. `insight::tokenization::Tokenizer` carries two raw-byte producer doors
// (ADR-21.D4). `process_line` performs stage-1 ingest normalization — the ANSI/escape strip of
// `insight::tokenization::normalize` — unconditionally. `process_stable_line` performs NO stage 1
// at all, deliberately, so the bytes that reach the returned `CanonicalEvent` are the caller's
// bytes UNCHANGED and every answer downstream of that door (template, level, role, marker) is a
// function of the caller's PRESENTATION bytes.
//
// WHY THIS FILE HAD TO BE WRITTEN, and it is the whole reason the arms take the shape they do.
// *"We do not do X"* has no failing instance until someone writes one. ADR-21.D4 enumerates the
// four producer doors and their obligations in PROSE, and says so itself: a gate sees the absent
// inbound constructor, the friend census and the declared peel door — it sees no facade door at
// all. Nothing asserted that `process_line` normalizes and `process_stable_line` does not. So the
// day someone "simplifies" the stable door by routing it through `process_line` — the wrong fix
// ADR-21.D4 names by hand precisely because it is plausible — every existing canon test stays
// green: the events still parse, still template, still carry a level. This file is the failing
// instance that day.
//
// ── HOMING (Kleio) ────────────────────────────────────────────────────────────────────────────
// UNIT, in `insight-canon`, `core/tests/tokenizer/` — the shelf that mirrors `src/tokenizer/`,
// where both doors are defined (`tokenizer_engine.cpp`).
//
// The discriminator is what the FIXTURE must control, and here it is exactly two things: the
// input BYTES, and WHICH DOOR they enter by. Both are literals and a function call. Nothing else
// is needed and nothing else may vary — which is why this is not an integration home. A seam test
// asks LogCraft to emit ANSI-bearing lines and InSight to ingest them; the seam would then supply
// the bytes, and the arm would be reading a generator's output instead of controlling it. No fact
// on the far side of any seam enters this property.
//
// It is also NOT homed beside `test_normalized_content_doors.cpp` even though the subject is the
// same ADR slot. That file's SUT is the TYPE `NormalizedContent` — its absent inbound constructor,
// its friend census, the declared peel door — and its scope is the type's PRODUCERS, so the
// tokenizer facade is outside it by construction. This file's SUT is the FACADE's two doors. Same
// ADR, disjoint apparatus; merging them would put a source census and a byte differential in one
// file and blur which of the two claims a red belongs to.
//
// ── WHAT EACH ARM IS FOR ──────────────────────────────────────────────────────────────────────
//   ARM 1  THE DIFFERENTIAL. One ESC-bearing line, both doors. The stable door's event still
//          carries the escape bytes; `process_line`'s does not. This is the arm that reds if the
//          stable door ever starts normalizing.
//   ARM 2  WHAT THE DIFFERENCE IS. A differential alone says only "the two doors disagree". This
//          arm names the disagreement as stage 1 exactly: `process_line(raw)` equals
//          `process_stable_line(normalize(raw))` — the normalized door IS the stable door with
//          stage 1 in front of it, which is the composition ADR-21.D4 describes.
//   ARM 3  THE PRICE, at the door grain. ADR-21.D4 records that a caller handing
//          presentation-bearing bytes to the stable door gets "precisely the silently-coarser
//          answer ADR-21.D1 names", and calls that the door's price rather than a defect. This arm
//          makes the price a measured fact on a real corpus line instead of a sentence: the same
//          GitLab `after_script` warning reads a FAILING level through the stable door and Warn
//          through the normalized one. It asserts the routed format is IDENTICAL first — otherwise
//          a level split would be attributable to strategy routing rather than to stage 1.
//   ARM 4  THE ANTI-VACUITY CONTROL, and without it the three arms above prove less than they
//          look. On an ESC-FREE line the two doors must agree byte-for-byte on every
//          content-derived field. That is what makes ARM 1 a statement about STAGE 1 and not about
//          "the stable door mangles bytes somehow" — a door broken for any unrelated reason
//          (a missing left-trim, a stray copy, a different strategy) reds HERE, on the input where
//          stage 1 is the identity.
//
// ── FALSIFIABILITY — measured 2026-09-01 on clang-21 (`linux-clang21-libcxx-release`) ─────────
// Three mutations, each applied, measured and reverted (`sha256sum -c` confirms both touched
// sources are byte-identical to the state the green run was measured on). The arms PARTITION —
// no two mutations red the same set — which is what says the four are four properties and not
// one property written four times.
//
//   SD-A  `Tokenizer::process_stable_line` routed through `parser.parse_line` instead of
//         `parser.parse_stable` — the plausible wrong fix ADR-21.D4 names by hand.
//         RED 2 of 4: ARM 1 ("THE EXEMPTION IS GONE", stable template printed with no `<ESC>`)
//         and ARM 3 (stable level Warn, so `is_failing` false and the two doors' levels equal).
//         ARM 2 and ARM 4 stay GREEN, correctly: with the stable door normalizing, ARM 2's
//         `stable(normalize(raw))` normalizes an already-normalized line — a fixed point — so
//         the equality still holds, and ARM 4's input has no escape byte for the mutation to eat.
//         **ARM 1 is the only arm that catches this mutation on its own**, which is why it is the
//         one written as a direct byte assertion rather than a comparison.
//   SD-B  `LogParser::parse_line` given `raw_line` where it takes `normalized.bytes()` — stage 1
//         deleted at the door that owes it unconditionally.
//         RED 3 of 4: ARM 1's second half ("process_line let an escape byte through"), ARM 2 on
//         both its assertions (template AND level diverge), ARM 3's `process_line` half (level
//         Error where the producer wrote WARNING). ARM 4 GREEN — the control's input is escape
//         free, so stage 1 is the identity on it under this mutation too.
//   SD-C  an INSTRUMENT mutation, not a plausible product regression, and it is here because a
//         control that has never fired is a control armed at zero: `LogParser::parse_stable`
//         hands the strategy `stable_line.substr(0, size - 1)`, i.e. the stable door diverges by
//         one byte for a reason that has nothing to do with normalization.
//         RED 2 of 4: ARM 2 and ARM 4 — and ARM 1 and ARM 3 stay GREEN. That is the whole reason
//         ARM 4 exists: a stable door broken for a non-stage-1 reason passes the escape test and
//         is caught only by the control.
//
// The line grain versus the classifier grain: `test_ingest_normalization_level_flip.cpp` pins that
// `infer_leading_log_level` reads this same corpus line differently before and after
// `normalize()`. That is a claim about the CLASSIFIER. ARM 3 is a claim about which bytes the DOOR
// puts in front of it, and neither file can see the other's property — the classifier arm stays
// green if both doors start normalizing, and this one stays green if the classifier's verdict on
// either byte string changes, as long as the two verdicts still differ.
//
// Determinism: string literals only, one arena and one Tokenizer per door invocation (a shared
// LogParser latches a sticky strategy, so sharing one would make arm k's routing depend on arm
// k-1's input), no seed, no RNG, no worker pool, no wall clock, no float. Every assertion is on
// bytes, an enum or a boolean.

#include <gtest/gtest.h>

import insight.canon.test;

using insight::LogFormat;
using insight::LogLevel;
using insight::to_string;
using insight::tokenization::ArenaAllocator;
using insight::tokenization::CanonicalEvent;
using insight::tokenization::MaskConfig;
using insight::tokenization::normalize;
using insight::tokenization::Tokenizer;

namespace
{

// The ESC byte the whole subject turns on, spelled once.
constexpr char kEsc{'\x1b'};

// ── The inputs ────────────────────────────────────────────────────────────────────────────────

// A REAL corpus line, byte-exact from marker_corpus_v1 (the GitLab `after_script` warning; the
// same literal `tests/utils/test_ingest_normalization_level_flip.cpp` carries, and it is quoted
// here rather than shared because the two files assert about different subjects and a shared
// constant would couple them). Three escape runs: the erase-to-end-of-line `ESC[0K`, the yellow
// `ESC[0;33m`, and the reset `ESC[0;m`. The bare `\r` is GitLab's own and is CONTENT, not a
// delimiter — stage 1 removes the escape runs and correctly leaves it.
constexpr std::string_view kAnsiWrapped{
    "section_end:1737226867:after_script\r\x1b[0K\x1b[0;33mWARNING: after_script failed, but job "
    "will continue unaffected: exit code 1\x1b[0;m"};

// The control input: the same SHAPE of line with no escape byte anywhere, so stage 1 is the
// identity on it (`normalize` returns the input as a fixed point, zero-copy). Everything the two
// doors do to it must agree.
constexpr std::string_view kEscapeFree{
    "section_end:1737226867:after_script WARNING: after_script failed, but job will continue "
    "unaffected: exit code 1"};

// ── The fixture ───────────────────────────────────────────────────────────────────────────────

// One arena + one composition + one Tokenizer. Declaration order is load-bearing: the Tokenizer
// holds a const ref to `composed`, which must therefore be declared before it and outlive it.
// The composition is the degenerate zero-package one — core tests never link the semantic
// packages, and no dialect row is needed here: the universal RawText strategy and canon's own
// leading-level inference carry every arm below.
struct Door
{
    static constexpr std::size_t kArenaSize{1U << 20U};
    ArenaAllocator arena{kArenaSize};
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed};
};

// ── Verbose-on-failure support ────────────────────────────────────────────────────────────────

// Render bytes with the control characters SPELLED OUT. A failure message that prints a raw ESC
// into a terminal reports the escape sequence to the terminal instead of to the reader, and the
// one byte the whole file is about is then the one byte the diagnostic cannot show.
[[nodiscard]] std::string visible(std::string_view bytes)
{
    std::string out;
    out.reserve(bytes.size() + bytes.size() / 4U);
    for (const char byte : bytes)
    {
        switch (byte)
        {
        case kEsc:
            out += "<ESC>";
            break;
        case '\r':
            out += "<CR>";
            break;
        case '\n':
            out += "<LF>";
            break;
        case '\t':
            out += "<TAB>";
            break;
        default:
            out.push_back(byte);
            break;
        }
    }
    return out;
}

[[nodiscard]] bool carries_escape(std::string_view bytes)
{
    return bytes.find(kEsc) != std::string_view::npos;
}

// A failing verdict, in canon's own terms: the two levels that make a line an alerting one.
[[nodiscard]] bool is_failing(LogLevel level)
{
    return level == LogLevel::Error || level == LogLevel::Fatal;
}

// One line of actual-vs-expected context every arm can append.
[[nodiscard]] std::string describe(std::string_view door, const CanonicalEvent& event)
{
    return std::string{door} + ": format=" + std::string{to_string(event.format)} +
           " level=" + std::string{to_string(event.level)} +
           " declared_level=" + (event.declared_level ? "true" : "false") + " template=\"" +
           visible(event.template_str) + '"';
}

} // namespace

// ── ARM 1 — THE DIFFERENTIAL ──────────────────────────────────────────────────────────────────
// The one arm that reds the day the stable door starts normalizing.

TEST(StableDoorDoesNotNormalize, TheStableDoorKeepsTheEscapeBytesAndProcessLineDoesNot)
{
    Door stable_door;
    Door normalizing_door;

    const auto stable{stable_door.tokenizer.process_stable_line(kAnsiWrapped)};
    const auto normalized{normalizing_door.tokenizer.process_line(kAnsiWrapped)};

    ASSERT_TRUE(stable.has_value()) << "process_stable_line declined the line: " << stable.error()
                                    << " — input: \"" << visible(kAnsiWrapped) << '"';
    ASSERT_TRUE(normalized.has_value()) << "process_line declined the line: " << normalized.error()
                                        << " — input: \"" << visible(kAnsiWrapped) << '"';

    // The positive half: the caller's bytes reached the event UNCHANGED. `template_str` is the
    // masker's product over the strategy's `content`, and the masker keeps every byte it does not
    // mask, so an escape run present in the caller's line is present here iff no stage 1 ran.
    EXPECT_TRUE(carries_escape(stable->template_str))
        << "THE EXEMPTION IS GONE: the stable door's event carries NO escape byte, so stage 1 ran "
           "on a path ADR-21.D4 specifies over the bytes AS SUPPLIED. If this door was routed "
           "through process_line, the echoed-source register is now silently dead on the one path "
           "that holds a single view — that is the wrong fix ADR-21.D4 names.\n  input : \""
        << visible(kAnsiWrapped) << "\"\n  " << describe("stable", *stable);

    // The other half: `process_line` DOES normalize, so the differential is a differential.
    EXPECT_FALSE(carries_escape(normalized->template_str))
        << "process_line let an escape byte through to the event. Stage 1 is unconditional at "
           "that door (ADR-21.D4: 'This door IS the guarantee'), so this is the normalized door "
           "failing, not the stable one.\n  input : \""
        << visible(kAnsiWrapped) << "\"\n  " << describe("process_line", *normalized);

    EXPECT_NE(stable->template_str, normalized->template_str)
        << "the two doors produced the SAME template for an ESC-bearing line — they cannot both "
           "be honoring their contract.\n  "
        << describe("stable      ", *stable) << "\n  " << describe("process_line", *normalized);
}

// ── ARM 2 — WHAT THE DIFFERENCE IS ────────────────────────────────────────────────────────────
// A differential says the doors disagree. This names the disagreement as stage 1 exactly.

TEST(StableDoorDoesNotNormalize, ProcessLineIsTheStableDoorWithStageOneInFrontOfIt)
{
    // The normalized view must outlive every read of the event that views it: `normalize` writes
    // an ESC-bearing line into this scratch buffer, and the stable door copies nothing.
    std::string scratch;
    const std::string_view stage_one_output{normalize(kAnsiWrapped, scratch).bytes()};

    ASSERT_FALSE(carries_escape(stage_one_output))
        << "stage 1 left an escape byte in its own output — this arm's premise is broken before "
           "any door is called.\n  input : \""
        << visible(kAnsiWrapped) << "\"\n  output: \"" << visible(stage_one_output) << '"';

    Door normalizing_door;
    Door stable_door;

    const auto through_process_line{normalizing_door.tokenizer.process_line(kAnsiWrapped)};
    const auto through_stable_door{stable_door.tokenizer.process_stable_line(stage_one_output)};

    ASSERT_TRUE(through_process_line.has_value())
        << "process_line declined the raw line: " << through_process_line.error();
    ASSERT_TRUE(through_stable_door.has_value())
        << "process_stable_line declined the pre-normalized line: " << through_stable_door.error();

    // The composition, stated as an equality. It reds in BOTH directions: if the stable door
    // starts normalizing, its side of this equation normalizes twice and the two stop matching on
    // any line where stage 1 is not idempotent-at-the-byte-level; if process_line stops
    // normalizing, its side keeps the escape runs and the two diverge immediately.
    EXPECT_EQ(through_process_line->template_str, through_stable_door->template_str)
        << "process_line is specified as stage 1 followed by the same parse the stable door "
           "performs. The two are no longer the same composition.\n  "
        << describe("process_line(raw)          ", *through_process_line) << "\n  "
        << describe("stable(normalize(raw))     ", *through_stable_door);

    EXPECT_EQ(through_process_line->level, through_stable_door->level)
        << "same content bytes, different level — the level channel has grown a dependence on "
           "which DOOR produced the content, which no ADR-21 door declares.\n  "
        << describe("process_line(raw)          ", *through_process_line) << "\n  "
        << describe("stable(normalize(raw))     ", *through_stable_door);
}

// ── ARM 3 — THE PRICE, AT THE DOOR GRAIN ──────────────────────────────────────────────────────
// ADR-21.D4 records the exemption's cost so it is "never quoted as costless". This measures it.

TEST(StableDoorDoesNotNormalize, ThePresentationBytesReachTheLevelVerdictThroughTheStableDoor)
{
    Door stable_door;
    Door normalizing_door;

    const auto stable{stable_door.tokenizer.process_stable_line(kAnsiWrapped)};
    const auto normalized{normalizing_door.tokenizer.process_line(kAnsiWrapped)};

    ASSERT_TRUE(stable.has_value()) << "process_stable_line declined: " << stable.error();
    ASSERT_TRUE(normalized.has_value()) << "process_line declined: " << normalized.error();

    // ASSERTED FIRST, because it is what makes the level split attributable. Two doors that routed
    // to DIFFERENT strategies would disagree about the level for a reason that has nothing to do
    // with stage 1, and the arm below would then be reading a routing difference while claiming a
    // normalization one.
    ASSERT_EQ(stable->format, normalized->format)
        << "the two doors routed this line to different strategies, so nothing below is a "
           "statement about stage 1.\n  "
        << describe("stable      ", *stable) << "\n  " << describe("process_line", *normalized);

    EXPECT_TRUE(is_failing(stable->level))
        << "the stable door no longer reads this line as failing. Its answers are specified as "
           "functions of the caller's PRESENTATION bytes (ADR-21.D4), and on those bytes the "
           "escape run makes the line's head parse as a failure.\n  "
        << describe("stable", *stable);

    EXPECT_EQ(normalized->level, LogLevel::Warn)
        << "the normalized door no longer reads this line as the producer marked it. GitLab wrote "
           "WARNING: in the text and coloured it 0;33; with the escape run stripped, that is the "
           "verdict stage 1 exists to surface.\n  "
        << describe("process_line", *normalized);

    // The pair, as one statement: this is the door's PRICE. A caller that hands
    // presentation-bearing bytes to the stable door gets the silently-coarser answer, and the
    // difference is a whole alerting level on a line whose own text says the failure was
    // inconsequential.
    EXPECT_NE(stable->level, normalized->level)
        << "both doors read this line as " << to_string(stable->level)
        << ". They agree, so the exemption now costs nothing on the "
           "one input chosen because it costs something. Either stage 1 stopped mattering to the "
           "leading-level inference — in which case ADR-21.D4's recorded price needs re-deriving, "
           "not updating — or one of the doors changed.\n  "
        << describe("stable      ", *stable) << "\n  " << describe("process_line", *normalized);
}

// ── ARM 4 — THE ANTI-VACUITY CONTROL ──────────────────────────────────────────────────────────
// On an ESC-free line stage 1 is the identity, so the two doors must be indistinguishable.

TEST(StableDoorDoesNotNormalize, OnAnEscapeFreeLineTheTwoDoorsAgreeByteForByte)
{
    Door stable_door;
    Door normalizing_door;

    const auto stable{stable_door.tokenizer.process_stable_line(kEscapeFree)};
    const auto normalized{normalizing_door.tokenizer.process_line(kEscapeFree)};

    ASSERT_TRUE(stable.has_value()) << "process_stable_line declined: " << stable.error();
    ASSERT_TRUE(normalized.has_value()) << "process_line declined: " << normalized.error();

    // Every content-derived field, not just the template: a door that agreed on the template
    // while disagreeing on the level or the routed format would still be broken, and this is the
    // input on which nothing may differ.
    EXPECT_EQ(stable->template_str, normalized->template_str)
        << "the two doors disagree on a line stage 1 does not touch (`normalize` returns an "
           "escape-free input as a zero-copy fixed point), so the difference ARM 1 measures is "
           "NOT stage 1 — something else about the stable door moved.\n  "
        << describe("stable      ", *stable) << "\n  " << describe("process_line", *normalized);

    EXPECT_EQ(stable->format, normalized->format)
        << "different routed format on an escape-free line.\n  "
        << describe("stable      ", *stable) << "\n  " << describe("process_line", *normalized);

    EXPECT_EQ(stable->level, normalized->level)
        << "different level on an escape-free line.\n  " << describe("stable      ", *stable)
        << "\n  " << describe("process_line", *normalized);

    EXPECT_EQ(stable->declared_level, normalized->declared_level)
        << "different level PROVENANCE on an escape-free line.\n  "
        << describe("stable      ", *stable) << "\n  " << describe("process_line", *normalized);

    EXPECT_EQ(stable->params.size(), normalized->params.size())
        << "different masked-parameter count on an escape-free line: stable="
        << stable->params.size() << " process_line=" << normalized->params.size() << "\n  "
        << describe("stable      ", *stable) << "\n  " << describe("process_line", *normalized);
}
