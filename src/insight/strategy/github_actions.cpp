module;

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// src/insight/tokenization/strategies/github_actions.cpp
//
// GitHubActionsStrategy — see github_actions.hpp.
//
// GHA line:  "2026-05-27T15:26:41.7842152Z   ##[error]connection refused"
//             └─ RFC3339 + 7-digit (.NET ticks) fractional + 'Z' ─┘ └ message ┘
//
// Hand-written scanner: zero RE2, zero string copies. `line` is arena-stable
// (copied by LogParser before parse()), so substrings are valid string_views;
// only the templated `content` is arena-stored.




namespace insight::tokenization
{

namespace
{
    // Above SyslogStrategy's RFC3339 score (0.80): both are candidates for an
    // RFC3339 prefix, and the precise 7-digit/'Z' GHA shape must win its lines.

    // Lift the log level from a GHA/Azure workflow-command annotation at the
    // head of the message. The marker is KEPT in the templated content (it is
    // part of the line's structure); only the level is extracted.
    [[nodiscard]] LogLevel level_from_message(std::string_view message) noexcept
    {
        if (message.starts_with("##[error]") || message.starts_with("::error::"))
            return LogLevel::Error;
        if (message.starts_with("##[warning]") || message.starts_with("::warning::"))
            return LogLevel::Warn;
        if (message.starts_with("##[debug]") || message.starts_with("::debug::"))
            return LogLevel::Debug;
        if (message.starts_with("##[notice]") || message.starts_with("::notice::"))
            return LogLevel::Info;
        return LogLevel::Unknown;
    }

} // namespace

std::expected<ParsedLine, std::string> GitHubActionsStrategy::parse(std::string_view line,
                                                                    ArenaAllocator& arena) const
{
    if (!is_github_actions_prefix(line))
        return std::unexpected(std::string("GitHubActionsStrategy: missing GHA timestamp prefix"));

    // Everything past the fixed-width timestamp; drop the separator space and
    // any GHA indentation so identical messages cluster regardless of nesting.
    std::string_view content{line.substr(kGhaPrefixLen)};
    sv_skip_ws(content);

    // A timestamp-only line is a blank line. Decline it: make_event turns the
    // unexpected into a dropped line, so it never forms an empty "" template.
    if (content.empty())
        return std::unexpected(
            std::string("GitHubActionsStrategy: blank GHA line (timestamp only)"));

    ParsedLine parsed;
    parsed.raw_line = line;
    parsed.timestamp = utils::parse_iso8601(line.substr(0U, kGhaPrefixLen));
    // A workflow-command marker (##[error]/##[warning]/…) is authoritative when
    // present. Without one, the body is effectively raw stdout — a crashing
    // subprocess prints "Segmentation fault (core dumped)" with no marker and no
    // level word — so fall back to the same leading-level / failure-cue inference
    // RawTextStrategy uses (token-aware, alloc-free; see infer_leading_log_level).
    // Without this, a bare OS/shell crash in a CI log stays Unknown and the failure
    // lexicon never reaches it — losing the dominant-level → NewErrorPattern signal
    // the diff ranks on.
    parsed.level = level_from_message(content);
    if (parsed.level == LogLevel::Unknown)
        parsed.level = utils::infer_leading_log_level(content);
    parsed.component = {}; // GHA lines carry no component / tag
    parsed.content = arena.store_string(content);
    return std::expected<ParsedLine, std::string>{parsed};
}

LogFormat GitHubActionsStrategy::format() const noexcept
{
    return LogFormat::GitHubActions;
}

double GitHubActionsStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr double kGitHubActionsConfidence{0.92};
    static constexpr double kNoConfidence{0.0};
    return is_github_actions_prefix(line) ? kGitHubActionsConfidence : kNoConfidence;
}

} // namespace insight::tokenization
