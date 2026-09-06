
// invariant: the stable door's EXEMPTION, stated POSITIVELY.
// invariant: the tokenizer carries two raw-byte producer doors — the normalizing one performs
// stage-1 ingest normalization unconditionally, and the stable one performs NO stage 1 at all.
// invariant: that is deliberate, so the bytes reaching the returned event are the caller's bytes
// UNCHANGED.
// invariant: every answer downstream of that door — template, level, role, marker — is
// therefore a function of the caller's PRESENTATION bytes.
// invariant: WHY THIS FILE HAD TO BE WRITTEN, and it is the whole reason the arms take the shape
// they do: WE DO NOT DO X HAS NO FAILING INSTANCE UNTIL SOMEONE WRITES ONE.
// invariant: the ruling enumerates the four producer doors and their obligations in PROSE, and says
// so itself — a gate sees the absent constructor, the friend census and the peel door.
// invariant: it sees NO facade door at all, and nothing asserted that one door normalizes and the
// other does not.
// invariant: so the day someone SIMPLIFIES the stable door by routing it through the normalizing
// one.
// invariant: the wrong fix the ruling names by hand precisely because it is plausible — every
// existing canon test stays GREEN.
// invariant: the events still parse, still template and still carry a level; this file is the
// failing instance that day.
// invariant: HOMED as a UNIT test on the shelf that mirrors the source directory where both doors
// are defined.
// invariant: the discriminator is what the FIXTURE must control, and here it is exactly two things
// — the input BYTES and WHICH DOOR they enter by.
// invariant: both are literals and a function call, nothing else is needed and nothing else may
// vary, which is why this is NOT an integration home.
// invariant: a seam test would ask the generator to emit escape-bearing lines, so the seam would
// SUPPLY the bytes and the arm would read a generator's output instead of controlling it.
// invariant: no fact on the far side of any seam enters this property.
// invariant: it is also NOT homed beside the doors census even though the subject is the same
// ruling — that file's subject is the TYPE and its scope is the type's PRODUCERS.
// invariant: the tokenizer facade is outside that file by construction, and this file's subject is
// the FACADE's two doors.
// invariant: same ruling, DISJOINT apparatus — merging them would put a source census and a byte
// differential in one file and blur which claim a red belongs to.
// invariant: ARM 1 is THE DIFFERENTIAL — one escape-bearing line through both doors, where the
// stable door's event still carries the escape bytes and the other's does not.
// invariant: that is the arm that reds if the stable door ever starts normalizing.
// invariant: ARM 2 is WHAT THE DIFFERENCE IS, because a differential alone says only that the two
// doors disagree.
// invariant: it names the disagreement as stage 1 EXACTLY — the normalizing door equals the
// stable door with stage 1 in front of it, which is the composition the ruling describes.
// invariant: ARM 3 is THE PRICE at the door grain — the ruling records that a caller handing
// presentation-bearing bytes to the stable door gets a silently-coarser answer.
// invariant: it calls that the door's PRICE rather than a defect, and this arm makes the price a
// MEASURED FACT on the level channel instead of a sentence.
// invariant: THE PRICE MOVED on 2026-09-02 — it used to show on a corpus warning line whose
// escape run pushed the level word past stage 1's byte head, splitting the two doors' verdicts.
// invariant: with stage 1's budget a TOKEN count the escape runs are DELIMITERS, the level word is
// token 3 on both byte strings, and the two doors AGREE on that line.
// invariant: the arm now pins that agreement as a BOUNDARY, and where the price still shows is
// stage 2, whose cue head remains a RAW-BYTE budget.
// invariant: so on a line whose failure cue starts inside 128 stripped bytes and outside 128 raw
// bytes, the stable door reads no failure and the normalizing door reads an alerting level.
// invariant: it asserts the routed format is IDENTICAL first, or a level split would be
// attributable to strategy routing rather than to stage 1.
// invariant: ARM 4 is THE ANTI-VACUITY CONTROL, and without it the three arms above prove less than
// they look.
// invariant: on an ESCAPE-FREE line the two doors must agree byte-for-byte on every content-derived
// field.
// invariant: that is what makes ARM 1 a statement about STAGE 1 rather than about the stable door
// mangling bytes somehow.
// invariant: a door broken for any unrelated reason — a missing trim, a stray copy, a different
// strategy — reds HERE, on the input where stage 1 is the identity.
// invariant: FALSIFIABILITY was measured on 2026-09-01 with three mutations, each applied, measured
// and REVERTED, with both touched sources confirmed byte-identical afterwards.
// invariant: THE ARMS PARTITION — no two mutations red the same set, which is what says the four
// are four properties and not one property written four times.
// invariant: routing the stable door through the normalizing parse — the plausible wrong fix the
// ruling names — reds ARM 1 and ARM 3, and leaves ARM 2 and ARM 4 GREEN, correctly.
// invariant: with the stable door normalizing, ARM 2 normalizes an already-normalized line, which
// is a FIXED POINT, so its equality still holds; ARM 4's input has no escape byte to eat.
// invariant: ARM 1 IS THE ONLY ARM THAT CATCHES THAT MUTATION ON ITS OWN, which is why it is
// written as a direct byte assertion rather than as a comparison.
// invariant: deleting stage 1 at the door that owes it unconditionally reds three of the four —
// ARM 1's second half, ARM 2 on both assertions, and ARM 3's normalizing half.
// invariant: ARM 4 stays green there too, because its input is escape-free so stage 1 is the
// identity on it under that mutation as well.
// invariant: the third mutation is an INSTRUMENT mutation and not a plausible product regression,
// and it is here because A CONTROL THAT HAS NEVER FIRED IS A CONTROL ARMED AT ZERO.
// invariant: it makes the stable door diverge by ONE BYTE for a reason having nothing to do with
// normalization, and it reds ARM 2 and ARM 4 while ARM 1 and ARM 3 stay GREEN.
// invariant: THAT IS THE WHOLE REASON ARM 4 EXISTS — a stable door broken for a non-stage-1
// reason passes the escape test and is caught only by the control.
// invariant: falsifiability was RE-MEASURED on 2026-09-02 against the new ARM 3 input, the same
// three mutations run over the WHOLE core binary rather than this file alone.
// invariant: the partition is unchanged and it is the whole SUITE's rather than this file's —
// under each mutation the only reds among the whole binary are the arms named here.
// invariant: NO OTHER TEST GUARDS THESE PROPERTIES.
// invariant: the line grain versus the classifier grain — a sibling arm pins how the CLASSIFIER
// reads that corpus line on both byte strings since the token budget.
// invariant: this arm is a claim about which bytes the DOOR puts in front of the classifier, taken
// on the input where the classifier still answers differently.
// invariant: neither file can see the other's property.
// invariant: the classifier arm stays green if BOTH doors start normalizing, and this one stays
// green if the classifier's verdicts change, as long as they still differ.
// invariant: determinism — string literals only, and ONE arena and ONE tokenizer per door
// invocation.
// invariant: a shared parser latches a sticky strategy, so sharing one would make an arm's routing
// depend on the previous arm's input.
// invariant: no seed, no RNG, no worker pool, no wall clock, no float; every assertion is on bytes,
// an enum or a boolean.
// refs: ADR-16.D7, ADR-21.D1, ADR-21.D4
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

// invariant: the escape byte the whole subject turns on, spelled ONCE.
constexpr char kEsc{'\x1b'};

// invariant: a REAL corpus line, byte-exact from the marker corpus.
// invariant: a sibling file carries the same literal, and it is QUOTED here rather than shared
// because the two files assert about different subjects and a shared constant would couple them.
// invariant: three escape runs — an erase-to-end-of-line, a colour set and a reset.
// invariant: the bare carriage return is the producer's own and is CONTENT, not a delimiter —
// stage 1 removes the escape runs and correctly leaves it.
constexpr std::string_view kAnsiWrapped{
    "section_end:1737226867:after_script\r\x1b[0K\x1b[0;33mWARNING: after_script failed, but job "
    "will continue unaffected: exit code 1\x1b[0;m"};

// invariant: the CONTROL input — the same SHAPE of line with no escape byte anywhere, so stage 1
// is the identity on it and normalization returns the input as a zero-copy fixed point.
// invariant: everything the two doors do to it must AGREE.
constexpr std::string_view kEscapeFree{
    "section_end:1737226867:after_script WARNING: after_script failed, but job will continue "
    "unaffected: exit code 1"};

// invariant: the PRICE input since the budget ruling — the same runner envelope in front of a
// body with NO level word among its first eight tokens and ONE failure cue.
// invariant: the cue is placed so it starts at stripped byte 118 and raw byte 129 — inside stage
// 2's cue head on the normalizing door and outside it on the stable one.
// invariant: a FIXTURE and not a corpus line, since no producer emits this exact body.
// invariant: the arm asserts BOTH offsets before reading a level, so a wrong count here reds as a
// FIXTURE error rather than as a door.
// refs: ADR-16.D7
constexpr std::string_view kCueAtTheHeadEdge{
    "section_end:1737226867:after_script\r\x1b[0K\x1b[0;33muploading artifacts for job 2092177 to "
    "the coordinator after three retry attempts failed: coordinator returned 502\x1b[0;m"};
// invariant: stage 2's cue head is QUOTED here because the constant is function-local in its own
// translation unit and cannot be linked.
// invariant: the premise assertions in ARM 3 red if it moves, which is the coupling wanted.
constexpr std::size_t kCueHeadBytes{128};

// invariant: ONE arena, ONE composition and ONE tokenizer, and declaration ORDER is load-bearing
// — the tokenizer holds a const ref to the composition, which must outlive it.
// invariant: the composition is the degenerate zero-package one, because core tests never link the
// semantic packages and no dialect row is needed here.
// invariant: the universal raw-text strategy and canon's own leading-level inference carry every
// arm below.
struct Door
{
    static constexpr std::size_t kArenaSize{1U << 20U};
    ArenaAllocator arena{kArenaSize};
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed};
};

// invariant: control characters are SPELLED OUT in failure output — a message that prints a raw
// escape into a terminal reports the sequence to the TERMINAL instead of to the reader.
// invariant: the one byte the whole file is about would then be the one byte the diagnostic cannot
// show.
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

// invariant: a failing verdict in canon's own terms — the two levels that make a line an alerting
// one.
[[nodiscard]] bool is_failing(LogLevel level)
{
    return level == LogLevel::Error || level == LogLevel::Fatal;
}

[[nodiscard]] std::string describe(std::string_view door, const CanonicalEvent& event)
{
    return std::string{door} + ": format=" + std::string{to_string(event.format)} +
           " level=" + std::string{to_string(event.level)} +
           " declared_level=" + (event.declared_level ? "true" : "false") + " template=\"" +
           visible(event.template_str) + '"';
}

} // namespace

// invariant: ARM 1, THE DIFFERENTIAL — the one arm that reds the day the stable door starts
// normalizing.
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

    // invariant: the POSITIVE half — the caller's bytes reached the event UNCHANGED.
    // invariant: the template is the masker's product over the strategy's content, and the masker
    // keeps every byte it does not mask.
    // invariant: so an escape run present in the caller's line is present here if and only if no
    // stage 1 ran.
    EXPECT_TRUE(carries_escape(stable->template_str))
        << "THE EXEMPTION IS GONE: the stable door's event carries NO escape byte, so stage 1 ran "
           "on a path ADR-21.D4 specifies over the bytes AS SUPPLIED. If this door was routed "
           "through process_line, the echoed-source register is now silently dead on the one path "
           "that holds a single view — that is the wrong fix ADR-21.D4 names.\n  input : \""
        << visible(kAnsiWrapped) << "\"\n  " << describe("stable", *stable);

    // invariant: the OTHER half — the normalizing door DOES normalize, which is what makes the
    // differential a differential.
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

// invariant: ARM 2 — a differential says the doors disagree, and this NAMES the disagreement as
// stage 1 exactly.
TEST(StableDoorDoesNotNormalize, ProcessLineIsTheStableDoorWithStageOneInFrontOfIt)
{
    // invariant: the normalized view must OUTLIVE every read of the event that views it —
    // normalization writes into this scratch buffer and the stable door copies nothing.
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

    // invariant: the composition stated as an EQUALITY, and it reds in BOTH directions.
    // invariant: if the stable door starts normalizing, its side normalizes twice and the two stop
    // matching on any line where stage 1 is not idempotent at the byte level.
    // invariant: if the other door stops normalizing, its side keeps the escape runs and the two
    // diverge immediately.
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

// invariant: ARM 3 — the ruling records the exemption's cost so it is never quoted as costless,
// and this MEASURES it where it still is, and pins the line it left.
// refs: ADR-21.D4
TEST(StableDoorDoesNotNormalize, ThePresentationBytesReachTheLevelVerdictThroughTheStableDoor)
{
    // invariant: PART 1 IS THE BOUNDARY — the corpus line the price USED to show on.
    // invariant: since stage 1's budget counts TOKENS the escape runs are delimiters, the level
    // word is token 3 on both byte strings, and the two doors agree on the producer's own severity.
    // invariant: a split HERE is a byte budget back in stage 1, not the exemption doing its work.
    // refs: ADR-16.D7
    {
        Door stable_door;
        Door normalizing_door;

        const auto stable{stable_door.tokenizer.process_stable_line(kAnsiWrapped)};
        const auto normalized{normalizing_door.tokenizer.process_line(kAnsiWrapped)};

        ASSERT_TRUE(stable.has_value()) << "process_stable_line declined: " << stable.error();
        ASSERT_TRUE(normalized.has_value()) << "process_line declined: " << normalized.error();
        ASSERT_EQ(stable->format, normalized->format)
            << "the two doors routed the after_script line to different strategies.\n  "
            << describe("stable      ", *stable) << "\n  " << describe("process_line", *normalized);

        EXPECT_EQ(stable->level, LogLevel::Warn)
            << "the stable door read the after_script warning as " << to_string(stable->level)
            << " — Stage 1's token budget reads the producer's WARNING at token 3 THROUGH the "
               "escape runs; a failing level here is a raw-byte budget back in Stage 1 (the "
               "escape run pushing the word out of a head), the defect ADR-16.D7 removed.\n  "
            << describe("stable", *stable);
        EXPECT_EQ(normalized->level, LogLevel::Warn)
            << "the normalized door no longer reads this line as the producer marked it. GitLab "
               "wrote WARNING: in the text and coloured it 0;33.\n  "
            << describe("process_line", *normalized);
    }

    // invariant: PART 2 IS THE PRICE, where it still shows — stage 2's cue head is a RAW-BYTE
    // budget, so an escape run in front of a cue SPENDS it.
    // invariant: the cue is inside the head once stripped and outside it raw, and the premises are
    // asserted FIRST so a miscounted fixture reds as a fixture and not as a door.
    {
        std::string scratch;
        const std::string_view stripped{normalize(kCueAtTheHeadEdge, scratch).bytes()};
        const std::size_t cue_raw{kCueAtTheHeadEdge.find("failed")};
        const std::size_t cue_stripped{stripped.find("failed")};
        ASSERT_NE(cue_raw, std::string_view::npos);
        ASSERT_NE(cue_stripped, std::string_view::npos);
        ASSERT_LT(cue_stripped, kCueHeadBytes)
            << "fixture premise: the cue must START inside the " << kCueHeadBytes
            << "-byte cue head on the STRIPPED bytes; it starts at byte " << cue_stripped;
        ASSERT_GE(cue_raw, kCueHeadBytes)
            << "fixture premise: the cue must START outside the " << kCueHeadBytes
            << "-byte cue head on the RAW bytes; it starts at byte " << cue_raw;

        Door stable_door;
        Door normalizing_door;

        const auto stable{stable_door.tokenizer.process_stable_line(kCueAtTheHeadEdge)};
        const auto normalized{normalizing_door.tokenizer.process_line(kCueAtTheHeadEdge)};

        ASSERT_TRUE(stable.has_value()) << "process_stable_line declined: " << stable.error();
        ASSERT_TRUE(normalized.has_value()) << "process_line declined: " << normalized.error();

        // invariant: ASSERTED FIRST, because it is what makes the level split ATTRIBUTABLE.
        // invariant: two doors that routed to DIFFERENT strategies would disagree about the level
        // for a reason having nothing to do with stage 1.
        // invariant: the arm below would then be reading a routing difference while claiming a
        // normalization one.
        ASSERT_EQ(stable->format, normalized->format)
            << "the two doors routed this line to different strategies, so nothing below is a "
               "statement about stage 1.\n  "
            << describe("stable      ", *stable) << "\n  " << describe("process_line", *normalized);

        EXPECT_FALSE(is_failing(stable->level))
            << "the stable door read a failure its cue head cannot reach on the caller's bytes: "
               "`failed` starts at raw byte "
            << cue_raw << ", past the " << kCueHeadBytes
            << "-byte head. Either that head grew, or the stable door normalized — the wrong fix "
               "ADR-21.D4 names.\n  "
            << describe("stable", *stable);

        EXPECT_TRUE(is_failing(normalized->level))
            << "the normalized door did not read the cue at stripped byte " << cue_stripped
            << " — stage 1 is unconditional at that door, and with the escape runs gone the cue is "
               "inside the head.\n  "
            << describe("process_line", *normalized);

        // invariant: the pair as ONE statement — this is the door's PRICE.
        // invariant: a caller handing presentation-bearing bytes to the stable door gets the
        // silently-coarser answer, and the difference is a WHOLE alerting level.
        // invariant: it now shows on stage 2's byte-bounded cue head, the one budget the ruling
        // leaves in raw bytes.
        EXPECT_NE(stable->level, normalized->level)
            << "both doors read this line as " << to_string(stable->level)
            << ". They agree, so the exemption now costs nothing on the one input chosen because "
               "it costs something. Either Stage 2's cue head stopped being a byte budget — in "
               "which case ADR-21.D4's recorded price needs re-deriving, not updating — or one of "
               "the doors changed.\n  "
            << describe("stable      ", *stable) << "\n  " << describe("process_line", *normalized);
    }
}

// invariant: ARM 4 — on an escape-free line stage 1 is the IDENTITY, so the two doors must be
// INDISTINGUISHABLE.
TEST(StableDoorDoesNotNormalize, OnAnEscapeFreeLineTheTwoDoorsAgreeByteForByte)
{
    Door stable_door;
    Door normalizing_door;

    const auto stable{stable_door.tokenizer.process_stable_line(kEscapeFree)};
    const auto normalized{normalizing_door.tokenizer.process_line(kEscapeFree)};

    ASSERT_TRUE(stable.has_value()) << "process_stable_line declined: " << stable.error();
    ASSERT_TRUE(normalized.has_value()) << "process_line declined: " << normalized.error();

    // invariant: EVERY content-derived field, not just the template — a door that agreed on the
    // template while disagreeing on the level or the routed format would still be broken.
    // invariant: this is the input on which NOTHING may differ.
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
