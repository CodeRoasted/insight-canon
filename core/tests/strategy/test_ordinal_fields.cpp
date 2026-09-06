
// invariant: the declared ordinal field-route — a declared structured numeric field is recognized
// by EXACT top-level key.
// invariant: it is captured as a consumed-not-tokenized observation rather than tokenized into the
// template.
// invariant: the value is parsed from the decimal TEXT to a canonical-unit integer, NEVER via
// double, which is the determinism pin.
// invariant: both doors are covered — the escape-free fast path and the simdjson slow path, which
// an escaped string forces and which exercises the unordered field lookup and cursor behaviour.
// refs: SRC-D-W1-3
#include <gtest/gtest.h>

import insight.canon.test;

using namespace insight;
using namespace insight::tokenization;

namespace
{
class OrdinalFieldTest : public ::testing::Test
{
  protected:
    static constexpr std::size_t kArenaSize{1u << 20};
    ArenaAllocator arena{kArenaSize};
    // invariant: JSON ordinal capture is semantic-unaware, so a degenerate zero-package composition
    // suffices.
    // invariant: the composition is declared BEFORE the tokenizer so it outlives the const-ref the
    // tokenizer holds.
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed};

    // invariant: verbose on failure — the caller asserts presence and prints the full ordinal
    // set.
    static const OrdinalObservation* find(const CanonicalEvent& ev, std::string_view field)
    {
        for (const auto& obs : ev.ordinals)
            if (obs.field_name == field)
                return &obs;
        return nullptr;
    }
    static std::string dump(const CanonicalEvent& ev)
    {
        std::string out{"ordinals=["};
        for (const auto& obs : ev.ordinals)
            out += std::string{obs.field_name} + ":" + std::to_string(obs.value) + " ";
        return out + "] template=\"" + std::string{ev.template_str} + "\"";
    }
};

constexpr std::int64_t kMs{1'000'000};
} // namespace

TEST_F(OrdinalFieldTest, FastPathLatencyMsToNanos)
{
    const auto result{tokenizer.process_line(
        R"({"level":"info","message":"db query completed","latency_ms":100})")};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    const auto* obs{find(ev, "latency_ms")};
    ASSERT_NE(obs, nullptr) << dump(ev);
    EXPECT_EQ(obs->value, 100 * kMs) << dump(ev);
    EXPECT_EQ(obs->schedule, OrdinalSchedule::DurationLog2Ns);
    // invariant: the ordinal is NOT a param and NOT in the template — the message is templated
    // independently of the captured observation.
    EXPECT_EQ(ev.template_str, "db query completed") << dump(ev);
    EXPECT_TRUE(ev.params.empty()) << dump(ev);
}

TEST_F(OrdinalFieldTest, FastPathDecimalParsedWithoutDouble)
{
    // invariant: converted by integer and decimal-string arithmetic rather than through a double.
    // refs: SRC-D-W1-3
    const auto result{tokenizer.process_line(R"({"message":"x","latency_ms":100.5})")};
    ASSERT_TRUE(result.has_value());
    const auto* obs{find(result.value(), "latency_ms")};
    ASSERT_NE(obs, nullptr) << dump(result.value());
    EXPECT_EQ(obs->value, 100 * kMs + 500'000);
}

TEST_F(OrdinalFieldTest, FastPathUnitScaling)
{
    EXPECT_EQ(
        find(tokenizer.process_line(R"({"message":"x","latency_us":1500})").value(), "latency_us")
            ->value,
        1'500'000);
    arena.reset();
    EXPECT_EQ(
        find(tokenizer.process_line(R"({"message":"x","latency_ns":42})").value(), "latency_ns")
            ->value,
        42);
    arena.reset();
    EXPECT_EQ(find(tokenizer.process_line(R"({"message":"x","duration_seconds":2})").value(),
                   "duration_seconds")
                  ->value,
              2'000'000'000);
}

TEST_F(OrdinalFieldTest, FastPathBytesSchedule)
{
    const auto* obs{find(tokenizer.process_line(R"({"message":"x","response_bytes":4096})").value(),
                         "response_bytes")};
    ASSERT_NE(obs, nullptr);
    EXPECT_EQ(obs->value, 4096);
    EXPECT_EQ(obs->schedule, OrdinalSchedule::SizeLog2Bytes);
}

TEST_F(OrdinalFieldTest, FastPathMultipleOrdinalsOneLine)
{
    const auto result{
        tokenizer.process_line(R"({"message":"x","latency_ms":10,"response_bytes":2048})")};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_EQ(ev.ordinals.size(), 2u) << dump(ev);
    ASSERT_NE(find(ev, "latency_ms"), nullptr) << dump(ev);
    ASSERT_NE(find(ev, "response_bytes"), nullptr) << dump(ev);
    EXPECT_EQ(find(ev, "latency_ms")->value, 10 * kMs);
    EXPECT_EQ(find(ev, "response_bytes")->value, 2048);
}

TEST_F(OrdinalFieldTest, FastPathNonOrdinalNumericIgnored)
{
    // invariant: a numeric field NOT in the declared catalog is never an ordinal observation.
    const auto result{tokenizer.process_line(R"({"message":"x","retry_count":5})")};
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().ordinals.empty()) << dump(result.value());
}

TEST_F(OrdinalFieldTest, MessageBodyNumberIsNotAnOrdinal)
{
    // invariant: a latency value embedded in the MESSAGE rather than at a top-level key stays a
    // masked param — the exact-key catalog never touches message-body numbers.
    const auto result{tokenizer.process_line(R"({"message":"db query latency 100 ms"})")};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_TRUE(ev.ordinals.empty()) << dump(ev);
    EXPECT_FALSE(ev.params.empty()) << dump(ev);
}

TEST_F(OrdinalFieldTest, SlowPathLatencyMsViaSimdjson)
{
    // invariant: the backslash escape in the message defeats the fast scanner and forces the slow
    // path, which is the point of this case.
    const auto result{
        tokenizer.process_line(R"({"message":"a \"quoted\" value","latency_ms":250})")};
    ASSERT_TRUE(result.has_value());
    const auto* obs{find(result.value(), "latency_ms")};
    ASSERT_NE(obs, nullptr) << dump(result.value());
    EXPECT_EQ(obs->value, 250 * kMs);
}

TEST_F(OrdinalFieldTest, SlowPathMultipleOrdinalsAndDecimal)
{
    const auto result{tokenizer.process_line(
        R"({"message":"esc \\ here","latency_ms":12.25,"response_bytes":8192})")};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    ASSERT_NE(find(ev, "latency_ms"), nullptr) << dump(ev);
    ASSERT_NE(find(ev, "response_bytes"), nullptr) << dump(ev);
    EXPECT_EQ(find(ev, "latency_ms")->value, 12 * kMs + 250'000);
    EXPECT_EQ(find(ev, "response_bytes")->value, 8192);
}
