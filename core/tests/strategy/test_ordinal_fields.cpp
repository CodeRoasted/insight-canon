// NOLINTBEGIN
// Unit tests for the W1 declared ordinal field-route (D-W1-3): JsonStrategy recognizes a
// declared structured numeric field (kOrdinalFieldCatalog) by EXACT top-level key and captures it
// as a consumed-not-tokenized CanonicalEvent.ordinals observation — value parsed from the decimal
// TEXT to a canonical-unit int64 (ns / bytes), NEVER via double (the determinism pin). Covers both
// the escape-free fast path and the simdjson slow path (escaped strings force the slow path, which
// exercises the find_field_unordered + raw_json_token route + cursor behaviour).

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
    // JSON ordinal capture is semantic-unaware — a degenerate (zero-package) composition suffices.
    // `composed` precedes `tokenizer` so it outlives the const-ref the Tokenizer holds.
    insight::semantic::ComposedSemantics composed{insight::test_support::degenerate_composition()};
    Tokenizer tokenizer{arena, MaskConfig{}, composed};

    // Find the ordinal observation for `field` in the event, or nullptr. Verbose on failure: the
    // caller asserts presence and prints the full ordinal set.
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

constexpr std::int64_t kMs{1'000'000}; // ns per ms
} // namespace

// ── Fast path (escape-free JSON) ────────────────────────────────────────────────

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
    // The ordinal is NOT a param and NOT in the template — the message templated independently.
    EXPECT_EQ(ev.template_str, "db query completed") << dump(ev);
    EXPECT_TRUE(ev.params.empty()) << dump(ev);
}

TEST_F(OrdinalFieldTest, FastPathDecimalParsedWithoutDouble)
{
    // 100.5 ms → 100_500_000 ns by integer/decimal-string arithmetic (the D-W1-3 pin).
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
        1'500'000); // 1500 µs → 1_500_000 ns
    arena.reset();
    EXPECT_EQ(
        find(tokenizer.process_line(R"({"message":"x","latency_ns":42})").value(), "latency_ns")
            ->value,
        42); // ns → ns
    arena.reset();
    EXPECT_EQ(find(tokenizer.process_line(R"({"message":"x","duration_seconds":2})").value(),
                   "duration_seconds")
                  ->value,
              2'000'000'000); // 2 s → 2e9 ns
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
    // A numeric field NOT in the declared catalog is never an ordinal observation.
    const auto result{tokenizer.process_line(R"({"message":"x","retry_count":5})")};
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result.value().ordinals.empty()) << dump(result.value());
}

TEST_F(OrdinalFieldTest, MessageBodyNumberIsNotAnOrdinal)
{
    // A latency value embedded in the MESSAGE (not a top-level key) stays a masked param — the
    // exact-key catalog never touches message-body numbers (the scenario-22 / 4A.2 boundary).
    const auto result{tokenizer.process_line(R"({"message":"db query latency 100 ms"})")};
    ASSERT_TRUE(result.has_value());
    const auto& ev{result.value()};
    EXPECT_TRUE(ev.ordinals.empty()) << dump(ev);
    EXPECT_FALSE(ev.params.empty()) << dump(ev); // 100 masked to a param
}

// ── Slow path (escaped string forces full simdjson) ─────────────────────────────

TEST_F(OrdinalFieldTest, SlowPathLatencyMsViaSimdjson)
{
    // The backslash escape in the message defeats the fast scanner → slow path.
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
    EXPECT_EQ(find(ev, "latency_ms")->value, 12 * kMs + 250'000); // 12.25 ms
    EXPECT_EQ(find(ev, "response_bytes")->value, 8192);
}

// NOLINTEND
