// invariant: one CloudWatch line reads one way on the CloudWatch strategy's two readers: the
// escape-free byte reader and the simdjson reader that a backslash anywhere in the line routes to.
// invariant: each fixture is escape-free and flat, so it takes the byte reader; its twin appends
// one unrelated member holding a backslash, so it takes the simdjson reader.
// invariant: both readers read only the strategy's declared keys — `timestamp`, `level`,
// `logGroup` then `logStream`, `message` — and agree, through the tokenizer, on the template id.
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

namespace
{

constexpr std::string_view kUnrelatedEscapedMember{R"(,"zz":"a\\b")"};

// post: the fixture with the unrelated escaped member appended before its closing brace.
[[nodiscard]] std::string escaped_twin(std::string_view fixture)
{
    const std::size_t close{fixture.rfind('}')};
    return std::string{fixture.substr(0, close)} + std::string{kUnrelatedEscapedMember} +
           std::string{fixture.substr(close)};
}

// post: true iff the fixture is one the byte reader takes: one flat object, no backslash.
[[nodiscard]] bool takes_the_byte_reader(std::string_view fixture)
{
    return !fixture.contains('\\') && !fixture.contains('[') &&
           fixture.find('{', fixture.find('{') + 1) == std::string_view::npos;
}

// post: every member of a parse a reader assigns, as one line; content that is the whole raw
// line reads `<whole line>`, since the twin's raw line differs by construction.
[[nodiscard]] std::string describe(const ParsedLine& parsed)
{
    std::string out;
    out += "time=";
    out += parsed.timestamp.has_value()
               ? std::to_string(parsed.timestamp->time_since_epoch().count())
               : std::string{parsed.timestamp.has_stamp() ? "<yearless>" : "<absent>"};
    out += parsed.timestamp.is_declared() ? " (declared)" : "";
    out += " level=" + std::string{to_string(parsed.level.value())};
    out += parsed.level.is_declared() ? " (declared)" : "";
    out += " component=\"" + std::string{parsed.component} + "\"";
    out += " content=" + (parsed.content == parsed.raw_line
                              ? std::string{"<whole line>"}
                              : "\"" + std::string{parsed.content} + "\"");
    return out;
}

struct Fixture
{
    std::string_view what;
    std::string_view line;
    // invariant: true when no `message` member is read, so the content is the raw line and the
    // twin's template differs from the fixture's by construction.
    bool content_is_the_line{false};
};

const std::array kFixtures{
    Fixture{.what = "a wider message name: `msg` is not the declared `message`",
            .line = R"({"logGroup":"g","msg":"a"})",
            .content_is_the_line = true},
    Fixture{.what = "a wider level name: `severity` is not the declared `level`",
            .line = R"({"logGroup":"g","severity":"error","message":"m"})"},
    Fixture{.what = "a wider component name: `service` is not a declared component key",
            .line = R"({"service":"svc","logGroup":"g","message":"m"})"},
    Fixture{.what = "component precedence: `logGroup` outranks `logStream` whatever the order",
            .line = R"({"logStream":"s","logGroup":"g","message":"m"})"},
    Fixture{.what = "a string `timestamp` is not a millisecond count",
            .line = R"({"timestamp":"2026-03-23T08:30:00Z","logGroup":"g","message":"m"})"},
    Fixture{.what = "a wider time name: a numeric `ts` is not the declared `timestamp`",
            .line = R"({"ts":1700000000000,"logGroup":"g","message":"m"})"},
    Fixture{.what = "a wider time name: a string `time` is not the declared `timestamp`",
            .line = R"({"time":"2026-03-23T08:30:00Z","logGroup":"g","message":"m"})"},
    Fixture{.what = "a compound spelling is not the declared top-level name",
            .line = R"({"logGroup":"g","log.message":"x","message":"m"})"},
    Fixture{.what = "an empty `message` is the message",
            .line = R"({"logGroup":"g","message":""})"},
    Fixture{.what = "an empty `logGroup` is the component",
            .line = R"({"logGroup":"","logStream":"s","message":"m"})"},
    Fixture{.what = "a zero `timestamp` is the epoch",
            .line = R"({"timestamp":0,"logGroup":"g","message":"m"})"},
    Fixture{.what = "a fractional `timestamp` is not a millisecond count",
            .line = R"({"timestamp":1700000000000.5,"logGroup":"g","message":"m"})"},
    Fixture{.what = "an exponent `timestamp` is not a millisecond count",
            .line = R"({"timestamp":1.7e12,"logGroup":"g","message":"m"})"},
    Fixture{.what = "a duplicate key: the first in document order",
            .line = R"({"logGroup":"g","message":"a","message":"b"})"},
    Fixture{.what = "the full production shape",
            .line = R"({"timestamp":1705312200000,"level":"WARN","message":"timeout",)"
                    R"("logGroup":"/aws/lambda/f","logStream":"2024/01/15/abc"})"},
};

// invariant: one arena, one zero-package composition and one tokenizer per line, so no sticky
// format state crosses from one line to the next.
struct Door
{
    static constexpr std::size_t kArenaSize{1U << 20U};
    ArenaAllocator arena{kArenaSize};
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed, StreamContext{}};
};

} // namespace

TEST(CloudWatchPathAgreement, EveryFixtureTakesTheByteReaderAndItsTwinTheSimdjsonReader)
{
    for (const Fixture& fixture : kFixtures)
    {
        EXPECT_TRUE(takes_the_byte_reader(fixture.line))
            << fixture.what << ": the fixture is not escape-free and flat, so it never reaches the "
            << "byte reader and the comparisons below are vacuous\n  " << fixture.line;
        EXPECT_TRUE(escaped_twin(fixture.line).contains('\\')) << fixture.what;
    }
}

// assert: the byte reader and the simdjson reader assign one line identically.
TEST(CloudWatchPathAgreement, BothReadersAssignOneLineIdentically)
{
    CloudWatchStrategy strategy;
    ArenaAllocator arena{1U << 16U};
    for (const Fixture& fixture : kFixtures)
    {
        const std::string twin{escaped_twin(fixture.line)};
        const auto fast{strategy.parse(fixture.line, arena)};
        const auto slow{strategy.parse(twin, arena)};
        ASSERT_TRUE(fast.has_value()) << fixture.what << ": " << fast.error();
        ASSERT_TRUE(slow.has_value()) << fixture.what << ": " << slow.error();
        EXPECT_EQ(describe(*fast), describe(*slow))
            << fixture.what << "\n  byte reader    : " << fixture.line
            << "\n  simdjson reader: " << twin;
    }
}

// assert: the rule's choice, not only the readers' agreement: only the declared keys, `logGroup`
// first, an integer `timestamp`, the first duplicate, an escaped key under its unescaped name.
TEST(CloudWatchPathAgreement, OnlyTheDeclaredKeysAreRead)
{
    CloudWatchStrategy strategy;
    ArenaAllocator arena{1U << 16U};
    for (const std::string_view base :
         {std::string_view{R"({"logStream":"s","logGroup":"g","severity":"error","msg":"a",)"
                           R"("ts":1700000000000,"message":"m","message":"x"})"},
          std::string_view{R"({"logGroup":"g","mess\u0061ge":"m"})"}})
    {
        for (const std::string& line : {std::string{base}, escaped_twin(base)})
        {
            const auto parsed{strategy.parse(line, arena)};
            ASSERT_TRUE(parsed.has_value()) << line << ": " << parsed.error();
            EXPECT_EQ(parsed->component, "g") << line << "\n  " << describe(*parsed);
            EXPECT_EQ(parsed->level, LogLevel::Unknown) << line << "\n  " << describe(*parsed);
            EXPECT_EQ(parsed->content, "m") << line << "\n  " << describe(*parsed);
            EXPECT_FALSE(parsed->timestamp.has_stamp()) << line << "\n  " << describe(*parsed);
        }
    }
    for (const auto& [base, expected_seconds] :
         {std::pair{std::string_view{R"({"timestamp":0,"logGroup":"g","message":"m"})"},
                    std::optional<std::int64_t>{0}},
          std::pair{std::string_view{R"({"timestamp":1705312200999,"logGroup":"g","message":"m"})"},
                    std::optional<std::int64_t>{1705312200}},
          std::pair{std::string_view{R"({"timestamp":1.7e12,"logGroup":"g","message":"m"})"},
                    std::optional<std::int64_t>{}},
          std::pair{std::string_view{R"({"timestamp":"2026-03-23T08:30:00Z","logGroup":"g"})"},
                    std::optional<std::int64_t>{}}})
    {
        for (const std::string& line : {std::string{base}, escaped_twin(base)})
        {
            const auto parsed{strategy.parse(line, arena)};
            ASSERT_TRUE(parsed.has_value()) << line << ": " << parsed.error();
            const std::optional<std::int64_t> actual_seconds{
                parsed->timestamp.has_value()
                    ? std::optional<std::int64_t>{std::chrono::duration_cast<std::chrono::seconds>(
                                                      parsed->timestamp->time_since_epoch())
                                                      .count()}
                    : std::nullopt};
            EXPECT_EQ(actual_seconds, expected_seconds)
                << line << "\n  actual  : "
                << (actual_seconds ? std::to_string(*actual_seconds) : std::string{"<absent>"})
                << "\n  expected: "
                << (expected_seconds ? std::to_string(*expected_seconds) : std::string{"<absent>"});
        }
    }
}

// assert: through the tokenizer, both readers give one template id and one projection.
TEST(CloudWatchPathAgreement, BothReadersGiveOneTemplateId)
{
    for (const Fixture& fixture : kFixtures)
    {
        Door fast_door;
        Door slow_door;
        const std::string twin{escaped_twin(fixture.line)};
        const auto fast{fast_door.tokenizer.process_line(fixture.line)};
        const auto slow{slow_door.tokenizer.process_line(twin)};
        ASSERT_TRUE(fast.has_value()) << fixture.what << ": " << fast.error();
        ASSERT_TRUE(slow.has_value()) << fixture.what << ": " << slow.error();
        ProjectionColumns fast_columns;
        ProjectionColumns slow_columns;
        render_projection(*fast, fast_columns);
        render_projection(*slow, slow_columns);
        for (std::size_t member{0}; member < kProjectionMembers.size(); ++member)
        {
            const std::string_view name{kProjectionMembers[member]};
            if (fixture.content_is_the_line && (name == "template_str" || name == "params"))
                continue;
            EXPECT_EQ(fast_columns[member], slow_columns[member])
                << fixture.what << ": projection member " << name
                << " differs\n  byte reader    : " << fixture.line
                << "\n  simdjson reader: " << twin;
        }
        if (!fixture.content_is_the_line)
            EXPECT_EQ(template_id_of(fast->template_str), template_id_of(slow->template_str))
                << fixture.what << "\n  byte reader template    : " << fast->template_str
                << "\n  simdjson reader template: " << slow->template_str;
    }
}
