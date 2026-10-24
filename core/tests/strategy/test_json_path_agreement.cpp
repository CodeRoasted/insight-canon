// invariant: one JSON record reads one way on canon's two JSON readers: the escape-free byte reader
// and the simdjson reader that a backslash anywhere in the line routes to.
// invariant: each fixture is escape-free and flat, so it takes the byte reader; its twin appends
// one unrelated member holding a backslash, so it takes the simdjson reader.
// invariant: the two agree on every role, ordinal, trace member and marker, and through the
// tokenizer on the template id.
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
    out += " ordinals=[";
    for (const OrdinalObservation& observation : parsed.ordinals)
        out += std::string{observation.field_name} + ":" + std::to_string(observation.value) + " ";
    out += "] trace=" + std::to_string(static_cast<int>(parsed.trace.present)) +
           std::to_string(static_cast<int>(parsed.trace.is_span)) + "," +
           std::to_string(parsed.trace.trace_id.value) + "," +
           std::to_string(parsed.trace.span_id.value);
    out += " links=" + std::to_string(parsed.linked_span_ids.size());
    out += " marker=\"" + std::string{parsed.no_role_witness_key} + "\"";
    return out;
}

struct Fixture
{
    std::string_view what;
    std::string_view line;
    // invariant: true when no message role is chosen, so the content is the raw line and the
    // twin's template differs from the fixture's by construction.
    bool content_is_the_line{false};
};

// invariant: the first five rows are the disagreements the 2026-10-23 determinism audit named —
// precedence on three roles, vocabulary and the numeric `ts`; the rest close the same class.
const std::array kFixtures{
    Fixture{.what = "message precedence: `message` outranks `msg` whatever the member order",
            .line = R"({"msg":"a","message":"b"})"},
    Fixture{.what = "level precedence: `level` outranks `severity`",
            .line = R"({"severity":"warn","level":"error","message":"m"})"},
    Fixture{.what = "component precedence: `component` outranks `source`",
            .line = R"({"source":"s","component":"c","message":"m"})"},
    Fixture{.what = "vocabulary: `logGroup` is no JSON component name",
            .line = R"({"logGroup":"g","message":"m"})"},
    Fixture{.what = "a numeric `ts` offers no time and blocks no string time",
            .line = R"({"ts":1700000000,"time":"2026-03-23T08:30:00Z","message":"m"})"},
    Fixture{.what = "an exact name outranks a compound spelling of the same role",
            .line = R"({"log.level":"warn","level":"error","message":"m"})"},
    Fixture{.what = "a duplicate key: the first in document order",
            .line = R"({"msg":"a","msg":"b"})"},
    Fixture{.what = "ordinals in catalog order, whatever the member order",
            .line = R"({"message":"m","duration_ms":12,"latency_ms":3})"},
    Fixture{.what = "an ordinal past eight numeric members is still read",
            .line = R"({"a1":1,"a2":2,"a3":3,"a4":4,"a5":5,"a6":6,"a7":7,"a8":8,)"
                    R"("duration_ms":5,"message":"m"})"},
    Fixture{.what = "a record with no role is marked on both readers",
            .line = R"({"foo":"bar"})",
            .content_is_the_line = true},
    Fixture{.what = "a flat OTEL log record reads its declared time, severity and trace",
            .line = R"({"timeUnixNano":"1700000000000000000","severityNumber":17,)"
                    R"("traceId":"0102030405060708090a0b0c0d0e0f10","body":"m"})",
            .content_is_the_line = true},
    Fixture{.what = "a flat span is a span on both readers",
            .line = R"({"startTimeUnixNano":"1700000000000000000",)"
                    R"("endTimeUnixNano":"1700000000500000000","name":"op",)"
                    R"("traceId":"0102030405060708090a0b0c0d0e0f10","spanId":"0102030405060708"})"},
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

TEST(JsonPathAgreement, EveryFixtureTakesTheByteReaderAndItsTwinTheSimdjsonReader)
{
    for (const Fixture& fixture : kFixtures)
    {
        EXPECT_TRUE(takes_the_byte_reader(fixture.line))
            << fixture.what << ": the fixture is not escape-free and flat, so it never reaches the "
            << "byte reader and the comparisons below are vacuous\n  " << fixture.line;
        EXPECT_TRUE(escaped_twin(fixture.line).contains('\\')) << fixture.what;
    }
}

// assert: the byte reader and the simdjson reader assign one record identically.
TEST(JsonPathAgreement, BothReadersAssignOneRecordIdentically)
{
    JsonStrategy strategy;
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

// assert: the rule's choice, not only the readers' agreement: an exact name before a compound
// spelling, vocabulary order within a tier, document order on a tie, catalog order for ordinals.
TEST(JsonPathAgreement, TheVocabularyOrderChoosesAndDocumentOrderBreaksATie)
{
    JsonStrategy strategy;
    ArenaAllocator arena{1U << 14U};
    for (const auto& [line, content] :
         {std::pair{std::string_view{R"({"msg":"a","message":"b"})"}, std::string_view{"b"}},
          std::pair{std::string_view{R"({"msg":"a","msg":"b"})"}, std::string_view{"a"}}})
    {
        const auto parsed{strategy.parse(line, arena)};
        ASSERT_TRUE(parsed.has_value()) << line;
        EXPECT_EQ(parsed->content, content) << line << "\n  " << describe(*parsed);
    }
    const auto level{strategy.parse(R"({"severity":"warn","level":"error","message":"m"})", arena)};
    ASSERT_TRUE(level.has_value());
    EXPECT_EQ(level->level, LogLevel::Error) << describe(*level);
    const auto compound{
        strategy.parse(R"({"log.level":"warn","level":"error","message":"m"})", arena)};
    ASSERT_TRUE(compound.has_value());
    EXPECT_EQ(compound->level, LogLevel::Error)
        << "a compound spelling moved a role an exact name holds\n  " << describe(*compound);
    const auto numeric_ts{
        strategy.parse(R"({"ts":1700000000,"time":"2026-03-23T08:30:00Z","message":"m"})", arena)};
    ASSERT_TRUE(numeric_ts.has_value());
    EXPECT_TRUE(numeric_ts->timestamp.has_value())
        << "a numeric `ts` blocked the string `time`\n  " << describe(*numeric_ts);
    const auto group{strategy.parse(R"({"logGroup":"g","message":"m"})", arena)};
    ASSERT_TRUE(group.has_value());
    EXPECT_TRUE(group->component.empty()) << describe(*group);
    const auto ordinals{
        strategy.parse(R"({"message":"m","duration_ms":12,"latency_ms":3})", arena)};
    ASSERT_TRUE(ordinals.has_value());
    ASSERT_EQ(ordinals->ordinals.size(), 2U) << describe(*ordinals);
    EXPECT_EQ(ordinals->ordinals[0].field_name, "latency_ms") << describe(*ordinals);
    EXPECT_EQ(ordinals->ordinals[1].field_name, "duration_ms") << describe(*ordinals);
}

// assert: through the tokenizer, both readers give one template id and one projection.
TEST(JsonPathAgreement, BothReadersGiveOneTemplateId)
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

// assert: a malformed zone refuses the TIME and never the line: the record is emitted with its
// other roles and an absent time, on both readers.
TEST(JsonPathAgreement, AMalformedZoneLeavesTheTimeAbsentAndKeepsTheRecord)
{
    JsonStrategy strategy;
    ArenaAllocator arena{1U << 14U};
    constexpr std::string_view kLine{
        R"({"ts":"2026-03-23T12:00:00+05:3","level":"error","message":"m"})"};
    for (const std::string& line : {std::string{kLine}, escaped_twin(kLine)})
    {
        const auto parsed{strategy.parse(line, arena)};
        ASSERT_TRUE(parsed.has_value()) << line << ": " << parsed.error();
        EXPECT_FALSE(parsed->timestamp.has_stamp()) << line << "\n  " << describe(*parsed);
        EXPECT_EQ(parsed->content, "m") << line;
        EXPECT_EQ(parsed->level, LogLevel::Error) << line;
    }
}
