#include <expected>
#include <gtest/gtest.h>
#include <string_view>

// refs: ADR-3.D4
import insight.canon;

namespace
{

constexpr std::size_t kArenaCapacity{16 * 1024};

} // namespace

TEST(InsightCanonPackage, CoreLogLevelRoundtrip)
{
    EXPECT_EQ(insight::to_string(insight::LogLevel::Info), "Info");
    EXPECT_EQ(insight::to_string(insight::LogLevel::Error), "Error");
    EXPECT_EQ(insight::to_string(insight::LogLevel::Unknown), "Unknown");
}

TEST(InsightCanonPackage, CoreResultCarriesValueAndError)
{
    std::expected<int, std::string> ok{42};
    EXPECT_TRUE(static_cast<bool>(ok));
    EXPECT_EQ(ok.value(), 42);

    std::expected<int, std::string> err{std::unexpected{"boom"}};
    EXPECT_FALSE(static_cast<bool>(err));
    EXPECT_EQ(err.error(), "boom");
}

TEST(InsightCanonPackage, CoreIso8601ParserAcceptsUtc)
{
    const auto ts{insight::utils::parse_iso8601("2026-04-24T10:00:00Z")};
    ASSERT_TRUE(ts.has_value());
}

// refs: ADR-17
TEST(InsightCanonPackage, TokenizesJsonLine)
{
    insight::tokenization::ArenaAllocator arena{kArenaCapacity};
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose({})};
    insight::tokenization::Tokenizer tokenizer{arena, {}, composed};

    constexpr std::string_view kLine{
        R"({"ts":"2024-01-15T10:30:00Z","level":"INFO","msg":"hello"})"};

    auto result{tokenizer.process_line(kLine)};
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();

    const auto& event{result.value()};
    EXPECT_FALSE(event.template_str.empty());
}

// refs: ADR-17
TEST(InsightCanonPackage, TokenizesSyslogLine)
{
    insight::tokenization::ArenaAllocator arena{kArenaCapacity};
    const insight::semantic::ComposedSemantics composed{insight::semantic::compose({})};
    insight::tokenization::Tokenizer tokenizer{arena, {}, composed};

    constexpr std::string_view kLine{"Jan 15 08:03:22 myhost sshd[1234]: Accepted password"};

    auto result{tokenizer.process_line(kLine)};
    ASSERT_TRUE(static_cast<bool>(result)) << result.error();

    const auto& event{result.value()};
    EXPECT_FALSE(event.component.empty());
}
