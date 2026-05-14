#pragma once

#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include <expected>

namespace insight::tokenization
{

/// Parses Apache error-log format:
///   "[Sun Dec 04 04:47:44 2005] [notice] workerEnv.init() ok"
class ApacheErrorLogStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<ParsedLine, std::string> parse(std::string_view line,
                                                    ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization
