module;
#include "utils/log_macros.hpp"

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: an Apache httpd error-log record — a bracketed full date, a bracketed level, then any
// number of further bracketed sections before the message.
// invariant: a hand-written scanner: no regex and, on the SUCCESS path, no string copies — a
// decline builds an error message.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{
    // post: the WORD characters of a bracket's interior, so a compound level yields its first
    // segment only.
    // invariant: that reproduces the retired regex exactly — its word class stopped at the first
    // non-word byte, and this replicates the same cut rather than inventing a new one.
    // note: the directive below is LOAD-BEARING: the position argument is 0, so this cannot throw.
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

    // invariant: the guard above is the ONLY exit — the predicate proved the head bracket closes,
    // so the timestamp take is total and the empty-result guard it replaces is gone.
    // refs: DN-43.D16
    std::string_view rest{line};
    const std::string_view raw_ts{sv_take_bracketed_or_none(rest)};

    sv_skip_ws(rest);
    // invariant: the level bracket is OPTIONAL and unproven, so an unclosed one declines the field
    // and leaves its bytes in content instead of swallowing the remainder.
    // refs: DN-43.D11
    const std::string_view level_bracket{sv_take_bracketed_or_none(rest)};
    const std::string_view level_word{extract_level_word(level_bracket)};

    sv_skip_ws(rest);
    // invariant: any number of further bracketed sections may follow the level, so they are skipped
    // as a group rather than enumerated.
    // invariant: the skip stops at the first section that does NOT close — those bytes reach no
    // field, so removing them would delete what no predicate validated.
    // assert: sv_take_bracketed_or_none either shortens `rest` or leaves it identical, so the size
    // comparison terminates the loop in every case.
    // refs: DN-43.D11
    while (!rest.empty() && rest[0] == '[')
    {
        const std::size_t before{rest.size()};
        (void)sv_take_bracketed_or_none(rest);
        if (rest.size() == before)
            break;
        sv_skip_ws(rest);
    }

    ParsedLine parsed_line;
    parsed_line.raw_line = line;
    parsed_line.timestamp = EventTime::parsed(utils::parse_apache_error_ts(raw_ts));
    parsed_line.level = EventLevel::declared(utils::parse_log_level(level_word));
    parsed_line.component = "httpd";
    parsed_line.content = rest;

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=ApacheError parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level.value()),
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
