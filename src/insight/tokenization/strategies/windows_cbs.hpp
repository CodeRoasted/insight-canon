#pragma once

#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include "insight/utils/result.hpp"

namespace insight::tokenization
{

/// Parses Windows CBS/CSI log format:
///   "2016-09-28 04:30:30, Info    CBS    Loaded Servicing Stack ..."
class WindowsCBSStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] insight::Result<ParsedLine> parse(std::string_view line,
                                                    ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization
