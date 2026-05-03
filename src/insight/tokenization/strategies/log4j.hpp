#pragma once

#include <string_view>

#include "insight/core/types.hpp"
#include "insight/tokenization/format_strategy.hpp"
#include "insight/tokenization/parsed_line.hpp"
#include "insight/utils/result.hpp"

namespace insight::tokenization
{

/// Parses Java Log4j / Python logging formats:
///   Hadoop:    "2015-10-18 18:01:47,978 INFO [main] org.apache.hadoop: msg"
///   Zookeeper: "2015-07-29 17:41:44,747 - INFO  [QuorumPeer] - msg"
///   OpenStack: "nova-api.log 2017-05-16 00:00:00.008 25746 INFO nova.osapi
///   [req-id] msg"
class Log4jStrategy final : public IFormatStrategy
{
  public:
    [[nodiscard]] insight::Result<ParsedLine> parse(std::string_view line,
                                                    ArenaAllocator& arena) const override;
    [[nodiscard]] LogFormat format() const noexcept override;
    [[nodiscard]] double confidence(std::string_view line) const noexcept override;
};

} // namespace insight::tokenization
