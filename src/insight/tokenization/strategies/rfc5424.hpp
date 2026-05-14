#pragma once

#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include <expected>

namespace insight::tokenization
{

/// Parses RFC 5424 syslog format:
///   "<PRI>VERSION TIMESTAMP HOSTNAME APP-NAME PROCID MSGID [SD] MSG"
///   e.g. "<134>1 2024-01-15T10:30:00Z myhost myapp 1234 ID47 - User logged in"
class RFC5424Strategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string> parse(std::string_view line,
                                                    ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization
