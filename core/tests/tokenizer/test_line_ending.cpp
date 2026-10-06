// invariant: a line's ending is the maximal run of CR bytes closing the bytes a consumer hands
// canon in one call, and canon removes it at every door before anything reads the line.
// invariant: the doors are `normalize()` on the raw bytes before its escape scan, the entry of
// `LogParser::parse_line`, and the entry of `LogParser::parse_stable`.
// invariant: a CR followed by any byte, an escape byte included, is content and stays.
// refs: DN-134.D13, DN-134.D14
#include <gtest/gtest.h>

import insight.canon.test;

using insight::tokenization::ArenaAllocator;
using insight::tokenization::CanonicalEvent;
using insight::tokenization::LogParser;
using insight::tokenization::MaskConfig;
using insight::tokenization::normalize;
using insight::tokenization::Tokenizer;

namespace
{

constexpr char kCarriageReturn{'\r'};

// invariant: control bytes are spelled out, so a failure message shows the CR it is about.
[[nodiscard]] std::string visible(std::string_view bytes)
{
    std::string out;
    for (const char byte : bytes)
    {
        if (byte == kCarriageReturn)
            out += "<CR>";
        else if (byte == '\x1b')
            out += "<ESC>";
        else
            out.push_back(byte);
    }
    return out;
}

// invariant: one arena, one zero-package composition and one tokenizer per line, so no sticky
// format state crosses from one fixture to the next.
struct Door
{
    static constexpr std::size_t kArenaSize{1U << 20U};
    ArenaAllocator arena{kArenaSize};
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed, insight::tokenization::StreamContext{}};
};

[[nodiscard]] std::string describe(const std::expected<CanonicalEvent, std::string>& event)
{
    if (!event.has_value())
        return "no event (" + event.error() + ")";
    std::string out{"format=" + std::string{insight::to_string(event->format)} + " template=\"" +
                    visible(event->template_str) + "\" params=["};
    for (const auto& param : event->params)
        out += '"' + visible(param) + "\" ";
    return out + ']';
}

[[nodiscard]] std::expected<CanonicalEvent, std::string> through_process_line(std::string_view line,
                                                                              Door& door)
{
    return door.tokenizer.process_line(line);
}

} // namespace

// invariant: the one definition: the maximal CR run closing the bytes, removed as a shortened view
// of them; a CR any byte follows stays.
TEST(LineEnding, WithoutLineEndingRemovesTheMaximalFinalCarriageReturnRun)
{
    struct Case
    {
        std::string_view line;
        std::string_view want;
    };
    for (const Case& kase :
         {Case{.line = "", .want = ""}, Case{.line = "\r", .want = ""},
          Case{.line = "\r\r", .want = ""}, Case{.line = "x\r", .want = "x"},
          Case{.line = "x\r\r", .want = "x"}, Case{.line = "a\rb", .want = "a\rb"},
          Case{.line = "\rb", .want = "\rb"}, Case{.line = "x\r\x1b[K", .want = "x\r\x1b[K"},
          Case{.line = "x\r \r", .want = "x\r "}, Case{.line = "x\n\r", .want = "x\n"}})
    {
        const std::string_view got{insight::tokenization::without_line_ending(kase.line)};
        EXPECT_EQ(got, kase.want) << "without_line_ending(\"" << visible(kase.line) << "\") = \""
                                  << visible(got) << "\", expected \"" << visible(kase.want) << '"';
        EXPECT_EQ(got.data(), kase.line.data())
            << "the result is not a view of its argument: \"" << visible(kase.line) << '"';
    }
    static_assert(insight::tokenization::without_line_ending("x\r\r") == "x");
}

// invariant: stage 1 removes the whole final CR run, and on an escape-free line its result still
// BORROWS the caller's bytes: the removal shortens a view and never copies.
TEST(LineEnding, NormalizeRemovesTheFinalCarriageReturnRunAndBorrows)
{
    struct Case
    {
        std::string_view raw;
        std::string_view want;
    };
    for (const Case& kase : {Case{.raw = "x\r", .want = "x"}, Case{.raw = "x\r\r", .want = "x"},
                             Case{.raw = "\r", .want = ""}, Case{.raw = "x", .want = "x"}})
    {
        std::string scratch{"untouched"};
        const std::string_view got{normalize(kase.raw, scratch).bytes()};
        EXPECT_EQ(got, kase.want) << "normalize(\"" << visible(kase.raw) << "\") = \""
                                  << visible(got) << "\", expected \"" << visible(kase.want) << '"';
        EXPECT_EQ(got.data(), kase.raw.data())
            << "normalize copied an escape-free line instead of borrowing it: \""
            << visible(kase.raw) << '"';
        EXPECT_EQ(scratch, "untouched") << "normalize wrote its scratch for an escape-free line: \""
                                        << visible(kase.raw) << '"';
    }
}

// invariant: the decision is taken on the RAW bytes, so a CR an erase sequence follows is a
// terminal redraw and stays, even though stage 1 then removes the sequence behind it.
TEST(LineEnding, ACarriageReturnAnEscapeFollowsIsContent)
{
    std::string scratch;
    const std::string_view got{normalize("x\r\x1b[K", scratch).bytes()};
    EXPECT_EQ(got, "x\r") << "stage 1 of x<CR><ESC>[K gave \"" << visible(got)
                          << "\": the CR is followed by an escape byte, so it is content";
}

// invariant: a CR inside a line is content at every door.
TEST(LineEnding, AnInteriorCarriageReturnStays)
{
    std::string scratch;
    EXPECT_EQ(normalize("a\rb", scratch).bytes(), "a\rb");
    EXPECT_EQ(normalize("a\rb\r", scratch).bytes(), "a\rb");
    Door door;
    const auto event{through_process_line("a\rb", door)};
    ASSERT_TRUE(event.has_value()) << describe(event);
    EXPECT_EQ(event->template_str, "a\rb") << describe(event);
}

// invariant: the stable door runs no stage 1, yet removes the ending, so the engine's streaming
// ingest and a stage-1 consumer read one line.
// invariant: it keeps every other byte as supplied, the CR before an escape included.
TEST(LineEnding, TheStableDoorRemovesTheRunAndKeepsEverythingElse)
{
    Door crlf_door;
    Door lf_door;
    const auto crlf{crlf_door.tokenizer.process_stable_line("step done (1.7s)\r\r")};
    const auto lf{lf_door.tokenizer.process_stable_line("step done (1.7s)")};
    ASSERT_TRUE(crlf.has_value()) << describe(crlf);
    ASSERT_TRUE(lf.has_value()) << describe(lf);
    EXPECT_EQ(crlf->template_str, lf->template_str)
        << "stable door, CR run kept: " << describe(crlf) << "\n  without it: " << describe(lf);
    EXPECT_TRUE(std::ranges::equal(crlf->params, lf->params))
        << "stable door, CR run kept: " << describe(crlf) << "\n  without it: " << describe(lf);

    Door redraw_door;
    const auto redraw{redraw_door.tokenizer.process_stable_line("x\r\x1b[K")};
    ASSERT_TRUE(redraw.has_value()) << describe(redraw);
    EXPECT_EQ(redraw->template_str, "x\r\x1b[K") << describe(redraw);
}

// invariant: through process_line, a line equals itself without its ending on every projected
// member: the runner's end-action line, whose CR blocked rule K's swallow.
TEST(LineEnding, TheEndActionLineWithItsCarriageReturnEqualsItWithout)
{
    constexpr std::string_view kLine{
        "##[end-action id=build;outcome=success;conclusion=success;duration_ms=12]"};
    Door crlf_door;
    Door lf_door;
    const auto crlf{through_process_line(std::string{kLine} + "\r", crlf_door)};
    const auto lf{through_process_line(kLine, lf_door)};
    ASSERT_TRUE(crlf.has_value()) << describe(crlf);
    ASSERT_TRUE(lf.has_value()) << describe(lf);
    insight::tokenization::ProjectionColumns crlf_columns;
    insight::tokenization::ProjectionColumns lf_columns;
    insight::tokenization::render_projection(*crlf, crlf_columns);
    insight::tokenization::render_projection(*lf, lf_columns);
    for (std::size_t member{0}; member < insight::tokenization::kProjectionMembers.size(); ++member)
        EXPECT_EQ(visible(crlf_columns[member]), visible(lf_columns[member]))
            << "projection member " << insight::tokenization::kProjectionMembers[member]
            << " differs between the CRLF and the LF line";
}

// invariant: the exit-code witness: `exit 1` + CR failed the status KEEP's all-digits test, so
// rule 5 masked it and `exit 0` and `exit 1` shared one template.
TEST(LineEnding, AnExitCodeBeforeTheEndingKeepsItsStatus)
{
    Door door;
    const auto event{through_process_line("x exit 1\r", door)};
    ASSERT_TRUE(event.has_value()) << describe(event);
    EXPECT_EQ(event->template_str, "x exit 1") << describe(event);
}

// invariant: a complete wrapper shell before the ending masks as it does bare (rule S).
TEST(LineEnding, AShelledDurationBeforeTheEndingMasks)
{
    Door door;
    const auto event{through_process_line("step done (1.7s)\r", door)};
    ASSERT_TRUE(event.has_value()) << describe(event);
    EXPECT_EQ(event->template_str, "step done <*>") << describe(event);
}

// invariant: a line that is only its ending carries no event and lands on the skip counter, never
// on the failure counter.
TEST(LineEnding, ALoneCarriageReturnLineLandsOnTheSkipCounter)
{
    ArenaAllocator arena{1U << 16U};
    const auto composition{insight::test_support::degenerate_composition()};
    LogParser parser{arena, composition};
    for (const std::string_view line : {std::string_view{"\r"}, std::string_view{"\r\r"}})
    {
        const auto parsed{parser.parse_line(line)};
        ASSERT_FALSE(parsed.has_value())
            << "parse_line(\"" << visible(line) << "\") produced an event";
        EXPECT_TRUE(parsed.error().starts_with("LogParser: "))
            << "parse_line(\"" << visible(line) << "\") refused as a failure: " << parsed.error();
        const auto stable{parser.parse_stable(line)};
        ASSERT_FALSE(stable.has_value())
            << "parse_stable(\"" << visible(line) << "\") produced an event";
        EXPECT_TRUE(stable.error().starts_with("LogParser: "))
            << "parse_stable(\"" << visible(line) << "\") refused as a failure: " << stable.error();
    }
    EXPECT_EQ(parser.lines_failed(), 0U);
    EXPECT_EQ(parser.lines_parsed(), 0U);
}

// invariant: `KEY=` + CR is an empty value: the line routes as `KEY=` does, where only the CR made
// it a key-value line.
TEST(LineEnding, AnEmptyAssignmentRoutesAsItDoesWithoutTheEnding)
{
    Door crlf_door;
    Door lf_door;
    const auto crlf{through_process_line("KEY=\r", crlf_door)};
    const auto lf{through_process_line("KEY=", lf_door)};
    ASSERT_TRUE(crlf.has_value()) << describe(crlf);
    ASSERT_TRUE(lf.has_value()) << describe(lf);
    EXPECT_EQ(crlf->format, lf->format)
        << "KEY=<CR>: " << describe(crlf) << "\n  KEY=: " << describe(lf);
    EXPECT_EQ(crlf->template_str, lf->template_str)
        << "KEY=<CR>: " << describe(crlf) << "\n  KEY=: " << describe(lf);
}
