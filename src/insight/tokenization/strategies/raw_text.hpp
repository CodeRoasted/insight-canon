#pragma once

#include <string_view>

#include <expected>

#include "insight/core/types.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"

namespace insight::tokenization
{

// Last-resort catch-all for unstructured text (CI / pytest / build logs).
//
// The FormatDetector selects this strategy ONLY when no structured strategy
// matches a non-empty line, so the tokenizer never silently drops a line.
//
// Performance: parse() is zero-copy — `content` is a subview of the (already
// arena-stable) input, produced by trimming leading ASCII whitespace with
// pointer arithmetic. No allocation, no full-line scan. confidence() is a
// constant 0.0, which both keeps it out of the normal majority vote and stops
// LogParser's sticky fast-path from ever latching onto it (a >0 confidence
// would greedily capture every following line).
class RawTextStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string>
    parse(std::string_view line, ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization
