module;
#include "utils/log_macros.hpp" // textual macro layer (§11.9)

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

    // Save position after node — the field right after it discriminates the two sub-formats:
    //   BGL:         "<ts2> <node2> <FACILITY> <SUBSYS> <LEVEL> <msg>" — ts2 is digit-leading
    //                (e.g. "2005-06-08-08.53.27…"), FACILITY ∈ {RAS, NULL}, SUBSYS is the dim.
    //   Thunderbird: "<Month> <Day> <Time> <host> <daemon>[pid]: <msg>" — Month is alpha.
    const std::string_view after_node{rest};

    if (!after_node.empty() && is_digit(after_node[0]))
    {
        (void)sv_take_token(rest);                             // ts2 (secondary timestamp)
        (void)sv_take_token(rest);                             // node2 (repeated node)
        (void)sv_take_token(rest);                             // FACILITY — RAS or NULL (not a dim)
        const std::string_view subsystem{sv_take_token(rest)}; // F3b: the low-card functional source
        const std::string_view level_sv{sv_take_token(rest)};

        ParsedLine parsed_line;
        parsed_line.raw_line = line;
        parsed_line.timestamp = utils::parse_epoch_timestamp(epoch);
        parsed_line.level = utils::parse_log_level(level_sv);
        parsed_line.component = subsystem; // F3b D-F3b-1: KERNEL/APP/DISCOVERY/MMCS… (the cube dim)
        parsed_line.host = node;           // F3b D-F3b-1: the node identity (hors-cube)
        parsed_line.content = rest;
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=BGL parsed component={} host={} level={} has_timestamp={}",
                          parsed_line.component, parsed_line.host, to_string(parsed_line.level),
                          parsed_line.timestamp.has_value());
        return std::expected<ParsedLine, std::string>{parsed_line};
    }

    // Thunderbird / syslog-tail: "<node> Mon DD HH:MM:SS host/host daemon[pid]: message".
    // F3b D-F3b-1/3: component = the daemon (functional source, `[pid]` stripped = identity);
    // host = the node; level = a pure-rule severity token-scan over the message (a characterized
    // proxy, blind to contextual severity — D-F3b-3). Degrades gracefully on a short tail.
    std::string_view tail{after_node};
    (void)sv_take_token(tail); // syslog month
    (void)sv_take_token(tail); // syslog day
    (void)sv_take_token(tail); // syslog HH:MM:SS
    (void)sv_take_token(tail); // syslog host (redundant with `node`)
    const std::string_view daemon{extract_syslog_tag(tail)}; // tail now = the message body

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = utils::parse_epoch_timestamp(epoch);
    parsed_line.level = utils::infer_leading_log_level(tail); // token-aware severity proxy
    parsed_line.component = daemon;
    parsed_line.host = node;
    parsed_line.content = tail;
    INSIGHT_LOG_DEBUG(
        logging::strategy_logger(),
        "strategy=BGL/syslog parsed component={} host={} level={} has_timestamp={}",
        parsed_line.component, parsed_line.host, to_string(parsed_line.level),
        parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat BGLStrategy::format() const noexcept
{
    return LogFormat::BGL;
}

double BGLStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::string_view::size_type kMinimumCandidateLength{20};
    static constexpr double kBglConfidence{0.90};
    static constexpr double kNoConfidence{0.0};

    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (is_bgl_prefix(line))
        return kBglConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
