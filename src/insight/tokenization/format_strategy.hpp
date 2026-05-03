#pragma once

#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/arena_allocator.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include "insight/utils/result.hpp"

namespace insight::tokenization
{

class IFormatStrategy
{
  public:
    IFormatStrategy() = default;
    IFormatStrategy(const IFormatStrategy&) = delete;
    IFormatStrategy& operator=(const IFormatStrategy&) = delete;
    IFormatStrategy(IFormatStrategy&&) = delete;
    IFormatStrategy& operator=(IFormatStrategy&&) = delete;
    virtual ~IFormatStrategy() = default;

    // Parse a single log line.
    //
    // The input string_view must remain valid for the duration of the call
    // (raw_line in the result borrows from it). Owned scalar fields
    // (component, content) are copied into the supplied arena via
    // ArenaAllocator::store_string(); their string_views remain valid until
    // the arena is reset or destroyed.
    [[nodiscard]] virtual insight::Result<ParsedLine> parse(std::string_view line,
                                                            ArenaAllocator& arena) const = 0;

    [[nodiscard]] virtual LogFormat format() const noexcept = 0;

    // Returns a [0,1] confidence score that this strategy matches the line.
    // Used by FormatDetector for majority-vote detection.  Must be O(1).
    [[nodiscard]] virtual double confidence(std::string_view line) const noexcept = 0;
};

} // namespace insight::tokenization
