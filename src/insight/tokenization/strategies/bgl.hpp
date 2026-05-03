#pragma once

#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include "insight/utils/result.hpp"

namespace insight::tokenization
{

/// Parses BlueGene/L (BGL) and Thunderbird supercomputer log formats:
///   BGL:         "- 1117838570 2005.06.03 R02-M1 ... RAS KERNEL INFO msg"
///   Thunderbird: "- 1131566461 2005.11.09 dn228 Nov 9 12:01:01 dn228 crond:
///   msg"
class BGLStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] insight::Result<ParsedLine> parse(std::string_view line,
                                                    ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization
