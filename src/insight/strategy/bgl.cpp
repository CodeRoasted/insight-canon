module;
#include "insight/utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// src/1_tokenization/strategies/bgl.cpp
//
// BGLStrategy — parses BlueGene/L (BGL) and Thunderbird supercomputer logs.
//
// BGL:         "- 1117838570 2005.06.03 R02-M1-N0 addr1 addr2 RAS KERNEL LEVEL msg"
// Thunderbird: "- 1131566461 2005.11.09 dn228 Nov 9 12:01:01 dn228/dn228 crond[2915]: msg"
//
// Hand-written scanner: zero RE2, zero string copies.




namespace insight::tokenization
{

namespace
{
    constexpr std::string_view::size_type kMinimumCandidateLength{20};
    constexpr double kNoConfidence{0.0};
    constexpr double kBglConfidence{0.90};

} // namespace

std::expected<ParsedLine, std::string> BGLStrategy::parse(std::string_view line,
                                                          ArenaAllocator& /*arena*/) const
{
    if (!is_bgl_prefix(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=BGL parse miss");
        return std::unexpected(
            std::string("BGLStrategy: line does not match BGL or Thunderbird format"));
    }

    // Common prefix: "- epoch date node"
    std::string_view rest{line};
    rest.remove_prefix(1U); // skip '-'
    sv_skip_ws(rest);

    const std::string_view epoch{sv_take_token(rest)}; // epoch digits
    (void)sv_take_token(rest);                         // skip date
    const std::string_view node{sv_take_token(rest)};  // node

    // Save position after node: this is where Thunderbird message starts.
    const std::string_view after_node{rest};

    // Skip addr1, addr2 — check for BGL "RAS KERNEL" signature.
    (void)sv_take_token(rest); // addr1 / first field
    (void)sv_take_token(rest); // addr2 / second field

    // BGL-specific: "RAS KERNEL LEVEL msg"
    if (rest.size() >= 3U && rest[0] == 'R' && rest[1] == 'A' && rest[2] == 'S' &&
        (rest.size() < 4U || is_space(rest[3])))
    {
        (void)sv_take_token(rest); // consume "RAS"
        (void)sv_take_token(rest); // consume "KERNEL"
        const std::string_view level_sv{sv_take_token(rest)};

        ParsedLine parsed_line;
        parsed_line.raw_line = line;
        parsed_line.timestamp = utils::parse_epoch_timestamp(epoch);
        parsed_line.level = utils::parse_log_level(level_sv);
        parsed_line.component = node;
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=BGL parsed component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level),
                          parsed_line.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed_line};
    }

    // Thunderbird / generic BGL: rest after node is the message.
    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = utils::parse_epoch_timestamp(epoch);
    parsed_line.level = LogLevel::Unknown;
    parsed_line.component = node;
    parsed_line.content = after_node;
    INSIGHT_LOG_DEBUG(
        logging::strategy_logger(), "strategy=BGL parsed component={} level={} has_timestamp={}",
        parsed_line.component, to_string(parsed_line.level), parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat BGLStrategy::format() const noexcept
{
    return LogFormat::BGL;
}

double BGLStrategy::confidence(std::string_view line) const noexcept
{
    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (is_bgl_prefix(line))
        return kBglConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
