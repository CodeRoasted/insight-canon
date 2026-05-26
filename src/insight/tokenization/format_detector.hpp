#pragma once
#include <array>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "insight/tokenization/format_strategy.hpp"

namespace insight::tokenization
{

class FormatDetector
{
  public:
    FormatDetector();

    void register_strategy(std::unique_ptr<IFormatStrategy> strategy);

    // Returns best-matching strategy for a given line
    [[nodiscard]] IFormatStrategy* detect(std::string_view line) const;

    // Detect from a sample batch (majority vote)
    [[nodiscard]] IFormatStrategy*
    detect_from_batch(std::span<const std::string_view> sample) const;

    // Get all registered strategies
    [[nodiscard]] std::span<const std::unique_ptr<IFormatStrategy>> strategies() const noexcept;

  private:
    static constexpr std::size_t kFormatSlotCount =
        static_cast<std::size_t>(LogFormat::Unknown) + 1U;

    std::vector<std::unique_ptr<IFormatStrategy>> strategies_;
    std::vector<IFormatStrategy*> custom_strategies_;
    std::array<IFormatStrategy*, kFormatSlotCount> by_format_{};

    // Last-resort catch-all. Used only when no structured strategy scores on a
    // non-empty line, so unstructured text is templated rather than dropped.
    std::unique_ptr<IFormatStrategy> fallback_;
};

} // namespace insight::tokenization
