#pragma once

#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include <expected>

namespace insight::tokenization
{

/// Parses systemd journal export format (JSON):
///   {"__REALTIME_TIMESTAMP":"1705312200000000","PRIORITY":"6",
///    "_COMM":"nginx","MESSAGE":"Worker started"}
class SystemdJournalStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string> parse(std::string_view line,
                                                    ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization
