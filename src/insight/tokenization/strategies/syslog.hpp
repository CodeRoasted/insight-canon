#pragma once

#include <optional>
#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include <expected>

namespace insight::tokenization
{

class SyslogStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string> parse(std::string_view line,
                                                    ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;

  private:
    [[nodiscard]] static std::optional<Timestamp>
    parse_bsd_timestamp(std::string_view timestamp_str);
    [[nodiscard]] static std::optional<Timestamp>
    parse_iso_timestamp(std::string_view timestamp_str);
};

} // namespace insight::tokenization
