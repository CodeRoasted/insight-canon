#pragma once

#include <array>
#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include <expected>

namespace insight::tokenization
{

class JsonStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string> parse(std::string_view line,
                                                    ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;

  private:
    static constexpr std::array<std::string_view, 5> kTimestampKeys{"timestamp", "ts", "@timestamp",
                                                                    "time", "datetime"};
    static constexpr std::array<std::string_view, 4> kLevelKeys{"level", "severity", "loglevel",
                                                                "log_level"};
    static constexpr std::array<std::string_view, 5> kMessageKeys{"message", "msg", "log", "text",
                                                                  "body"};
    static constexpr std::array<std::string_view, 5> kComponentKeys{"component", "source", "logger",
                                                                    "service", "module"};
};

} // namespace insight::tokenization
