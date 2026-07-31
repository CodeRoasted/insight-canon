module;
#include "utils/log_macros.hpp" // textual macro layer (ADR-3.D4)

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// ApacheErrorLogStrategy — parses Apache httpd error-log format:
//   "[Sun Dec 04 04:47:44 2005] [notice] workerEnv.init() ok ..."
//   "[Sun Dec 04 04:47:44 2005] [error] [client 10.0.0.1] mod_jk child ..."
//
// Hand-written scanner: zero RE2, zero string copies.

namespace insight::tokenization
{

namespace
{
    // Extract the level word from a bracket like "error", "warn", "php:error".
    // RE2 pattern was `\[(\w+)\]` — \w+ stops at non-word chars, so for
    // "[php:error]" RE2 captured "php". We replicate: take word chars only.
    // substr(0, idx) has pos arg 0, always <= size, so this noexcept body cannot throw.
    // NOLINTNEXTLINE(bugprone-exception-escape)
    [[nodiscard]] constexpr std::string_view extract_level_word(std::string_view bracket) noexcept
    {
        std::size_t idx{0};
        while (idx < bracket.size() && (is_lower(bracket[idx]) || is_upper(bracket[idx]) ||
                                        is_digit(bracket[idx]) || bracket[idx] == '_'))
            ++idx;
        return bracket.substr(0, idx);
    }

} // namespace

std::expected<ParsedLine, std::string>
ApacheErrorLogStrategy::parse(std::string_view line, ArenaAllocator& /*arena*/) const
{
    if (!is_apache_error_prefix(line))
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=ApacheError parse miss");
        return std::unexpected(
            std::string("ApacheErrorLogStrategy: line does not match Apache error-log format"));
    }

    std::string_view rest{line};
    // "[Dow Mon DD HH:MM:SS YYYY]"
    const std::string_view raw_ts{sv_take_bracketed(rest)};
    if (raw_ts.empty())
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=ApacheError parse miss (no ts)");
        return std::unexpected(
            std::string("ApacheErrorLogStrategy: line does not match Apache error-log format"));
    }

    sv_skip_ws(rest);
    // "[error]" or "[error:debug]" or "[php:error]"
    const std::string_view level_bracket{sv_take_bracketed(rest)};
    const std::string_view level_word{extract_level_word(level_bracket)};

    sv_skip_ws(rest);
    // Skip any number of extra bracketed sections: [pid N], [client IP], etc.
    while (!rest.empty() && rest[0] == '[')
    {
        (void)sv_take_bracketed(rest);
        sv_skip_ws(rest);
    }

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = utils::parse_apache_error_ts(raw_ts);
    parsed_line.level = utils::parse_log_level(level_word);
    parsed_line.component = "httpd";
    parsed_line.content = rest;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=ApacheError parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat ApacheErrorLogStrategy::format() const noexcept
{
    return LogFormat::ApacheError;
}

double ApacheErrorLogStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr std::string_view::size_type kMinimumCandidateLength{27};
    static constexpr double kApacheErrorConfidence{0.88};
    static constexpr double kNoConfidence{0.0};

    if (line.size() < kMinimumCandidateLength)
        return kNoConfidence;
    if (is_apache_error_prefix(line))
        return kApacheErrorConfidence;
    return kNoConfidence;
}

} // namespace insight::tokenization
