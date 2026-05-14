#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include "insight/core/types.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include <expected>

namespace insight::tokenization
{

class KVStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;

  private:
    // Views into the original (arena-stable) line; values are un-quoted slices.
    struct KVPair
    {
        std::string_view key;
        std::string_view value;
    };
    [[nodiscard]] static std::vector<KVPair> extract_pairs(std::string_view line);
    [[nodiscard]] static std::optional<Timestamp> try_parse_timestamp(std::string_view value);
    [[nodiscard]] static LogLevel try_parse_level(std::string_view value);
};

} // namespace insight::tokenization
