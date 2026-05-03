// NOLINTBEGIN Smoke test: consumes insight::canon as if external.
#include <gtest/gtest.h>
#include <string_view>

#include "insight/core/types.hpp"
#include "insight/utils/result.hpp"
#include "insight/utils/time_utils.hpp"
#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/canonical_event.hpp"
#include "insight/tokenization/tokenizer_engine.hpp"
#include "insight/sequence/sequence_engine.hpp"

namespace
{

constexpr std::size_t kArenaCapacity{16 * 1024};

insight::tokenization::CanonicalEvent make_event(insight::EventID id)
{
    insight::tokenization::CanonicalEvent ev;
    ev.id = id;
    return ev;
}

} // namespace

// ── core ──────────────────────────────────────────────────────────────────────

TEST(InsightCanonPackage, CoreLogLevelRoundtrip)
{
    EXPECT_EQ(insight::to_string(insight::LogLevel::Info), "Info");
    EXPECT_EQ(insight::to_string(insight::LogLevel::Error), "Error");
    EXPECT_EQ(insight::to_string(insight::LogLevel::Unknown), "Unknown");
}

TEST(InsightCanonPackage, CoreResultCarriesValueAndError)
{
    insight::Result<int> ok{42};
    EXPECT_TRUE(static_cast<bool>(ok));
    EXPECT_EQ(ok.value(), 42);

    insight::Result<int> err{std::string{"boom"}};
    EXPECT_FALSE(static_cast<bool>(err));
    EXPECT_EQ(err.error(), "boom");
}

TEST(InsightCanonPackage, CoreIso8601ParserAcceptsUtc)
{
    const auto ts{insight::utils::parse_iso8601("2026-04-24T10:00:00Z")};
    ASSERT_TRUE(ts.has_value());
}

// ── tokenization ──────────────────────────────────────────────────────────────

TEST(InsightCanonPackage, TokenizesJsonLine)
{
    insight::tokenization::ArenaAllocator arena{kArenaCapacity};
    insight::tokenization::Tokenizer tokenizer{arena};

    constexpr std::string_view kLine{
        R"({"ts":"2024-01-15T10:30:00Z","level":"INFO","msg":"hello"})"};

    auto result{tokenizer.process_line(kLine)};
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();

    const auto& event{result.value()};
    EXPECT_NE(event.template_id, 0U);
    EXPECT_FALSE(event.template_str.empty());
}

TEST(InsightCanonPackage, TokenizesSyslogLine)
{
    insight::tokenization::ArenaAllocator arena{kArenaCapacity};
    insight::tokenization::Tokenizer tokenizer{arena};

    constexpr std::string_view kLine{"Jan 15 08:03:22 myhost sshd[1234]: Accepted password"};

    auto result{tokenizer.process_line(kLine)};
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();

    const auto& event{result.value()};
    EXPECT_FALSE(event.component.empty());
}

// ── sequence ──────────────────────────────────────────────────────────────────

TEST(InsightCanonPackage, SequenceIngestsEventsAndExposesFlatView)
{
    insight::sequence::SequenceEngine engine;
    engine.ingest(make_event(7));
    engine.ingest(make_event(7));
    EXPECT_EQ(engine.size(), 2U);
    EXPECT_EQ(engine.unique_events(), 1U);
}

TEST(InsightCanonPackage, SequenceBuildsTransitionMatrix)
{
    insight::sequence::SequenceEngine engine;
    // Pattern: 1->2->3->1->2->3->1->2->4
    for (auto id : {1U, 2U, 3U, 1U, 2U, 3U, 1U, 2U, 4U})
        engine.ingest(make_event(id));

    EXPECT_EQ(engine.unique_events(), 4U);

    const auto edges{engine.transitions()};
    ASSERT_GE(edges.size(), 4U);
    EXPECT_EQ(edges.front().from, 1U);
    EXPECT_EQ(edges.front().to, 2U);
    EXPECT_EQ(edges.front().count, 3U);
}
// NOLINTEND
