module;

module insight.semantic.github;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;

// github_strategy.cpp — the GitHub-Actions dialect CODE TIER (ADR 0024 §2.3): the format strategy +
// the echoed-source provenance hook + the dialect's private byte primitives (is_github_actions_prefix,
// kGhaPrefixLen, the command-echo SGR grammar). Relocated from canon core (src/strategy/github_actions.cpp
// + the GHA-specific halves of src/scan/…): all GHA knowledge lives HERE now, so canon core is
// semantic-unaware (SP-1). Self-contained: only api (LogFormat/LogLevel/ArenaAllocator + utils
// timestamp/level parsers) + spi (IFormatStrategy/ParsedLine) + std — never a sealed detail shard.
//
// Byte logic ported VERBATIM from the pre-split canon so the composed pipeline is byte-identical
// (G-SP-1). The universal sub-primitives (ISO date / time digit shapes) are re-homed here as private
// helpers rather than depending on core's sealed scan shard.

namespace insight::semantic::github
{
namespace
{

// ── Dialect line-prefix detection (was src/scan/canon.detail.scan.cppm is_github_actions_prefix) ──
[[nodiscard]] constexpr bool is_digit(char chr) noexcept
{
    return static_cast<unsigned>(chr) - '0' < 10U;
}
[[nodiscard]] constexpr bool is_space(char chr) noexcept { return chr == ' ' || chr == '\t'; }

constexpr std::size_t kGhaPrefixLen{28U}; // "YYYY-MM-DDTHH:MM:SS.fffffffZ"

// "YYYY-MM-DD" at offset `pos`.
[[nodiscard]] constexpr bool match_iso_date_at(std::string_view str, std::size_t pos) noexcept
{
    if (pos + 10U > str.size())
        return false;
    return is_digit(str[pos]) && is_digit(str[pos + 1]) && is_digit(str[pos + 2]) &&
           is_digit(str[pos + 3]) && str[pos + 4] == '-' && is_digit(str[pos + 5]) &&
           is_digit(str[pos + 6]) && str[pos + 7] == '-' && is_digit(str[pos + 8]) &&
           is_digit(str[pos + 9]);
}

// "HH:MM:SS" at offset `pos`.
[[nodiscard]] constexpr bool match_time_at(std::string_view str, std::size_t pos) noexcept
{
    if (pos + 8U > str.size())
        return false;
    return is_digit(str[pos]) && is_digit(str[pos + 1]) && str[pos + 2] == ':' &&
           is_digit(str[pos + 3]) && is_digit(str[pos + 4]) && str[pos + 5] == ':' &&
           is_digit(str[pos + 6]) && is_digit(str[pos + 7]);
}

// "YYYY-MM-DDTHH:MM:SS" — RFC 3339 prefix (T separator).
[[nodiscard]] constexpr bool is_rfc3339_prefix(std::string_view str) noexcept
{
    return match_iso_date_at(str, 0) && str.size() > 10U && str[10] == 'T' && match_time_at(str, 11U);
}

// "YYYY-MM-DDTHH:MM:SS.fffffffZ" — GHA / Azure Pipelines line prefix (exactly 7 fractional digits +
// 'Z'). A strict subset of RFC 3339 so the strategy outranks Syslog only on genuine GHA lines.
[[nodiscard]] constexpr bool is_github_actions_prefix(std::string_view str) noexcept
{
    constexpr std::size_t kDotAt{19U};
    constexpr std::size_t kFracAt{20U};
    constexpr std::size_t kFracLen{7U};
    constexpr std::size_t kZAt{27U};
    if (str.size() < kGhaPrefixLen)
        return false;
    if (!is_rfc3339_prefix(str))
        return false;
    if (str[kDotAt] != '.')
        return false;
    for (std::size_t pos{kFracAt}; pos < kFracAt + kFracLen; ++pos)
        if (!is_digit(str[pos]))
            return false;
    if (str[kZAt] != 'Z')
        return false;
    return str.size() == kGhaPrefixLen || is_space(str[kGhaPrefixLen]);
}

// ── Level lift (was github_actions.cpp level_from_message) — walks the PACKAGE'S level-lift rows ──
// Data-driven: the prefixes/levels live in kLevelLifts (github.cppm), also serialized into
// semantic_identity. First-match walk in declared order (no nested prefixes) == the pre-split ordered
// if-chain, byte-identical. The marker is KEPT in the templated content; only the level is lifted.
[[nodiscard]] insight::LogLevel level_from_message(std::string_view message) noexcept
{
    for (const LevelLiftRow& row : kLevelLifts)
        if (message.starts_with(row.prefix))
            return row.level;
    return insight::LogLevel::Unknown;
}

// ── Echoed-source detection (was src/scan/… kCommandEchoSgrParams / is_echoed_source_line) ──
constexpr unsigned char kEsc{0x1bU};
inline constexpr std::array<std::string_view, 2> kCommandEchoSgrParams{std::string_view{"36;1"},
                                                                       std::string_view{"1;36"}};
inline constexpr std::array<std::string_view, 3> kSgrResetParams{std::string_view{"0"},
                                                                 std::string_view{}, std::string_view{"39"}};

// Parse a CSI `\x1b[<params>m` at `pos`; on success advance `pos` past the final `m` and return the
// parameter substring. nullopt when `pos` is not such a sequence. Pure byte walk (F5).
// NOLINTNEXTLINE(bugprone-exception-escape)
[[nodiscard]] std::optional<std::string_view> parse_sgr_params(std::string_view line,
                                                               std::size_t& pos) noexcept
{
    if (pos + 1U >= line.size() || static_cast<unsigned char>(line[pos]) != kEsc || line[pos + 1U] != '[')
        return std::nullopt;
    const std::size_t params_begin{pos + 2U};
    std::size_t cur{params_begin};
    while (cur < line.size() && (is_digit(line[cur]) || line[cur] == ';'))
        ++cur;
    if (cur >= line.size() || line[cur] != 'm')
        return std::nullopt;
    const std::string_view params{line.substr(params_begin, cur - params_begin)};
    pos = cur + 1U;
    return params;
}

// ── The dialect format strategy (was GitHubActionsStrategy) ──
class GitHubActionsStrategy final : public insight::tokenization::IFormatStrategy
{
  public:
    [[nodiscard]] std::expected<insight::tokenization::ParsedLine, std::string>
    parse(std::string_view line, insight::tokenization::ArenaAllocator& arena) const override
    {
        if (!is_github_actions_prefix(line))
            return std::unexpected(
                std::string("GitHubActionsStrategy: missing GHA timestamp prefix"));

        // Everything past the fixed-width timestamp; drop the separator space + any GHA indentation.
        std::string_view content{line.substr(kGhaPrefixLen)};
        while (!content.empty() && is_space(content.front()))
            content.remove_prefix(1U);

        // A timestamp-only line is a blank line: decline it (make_event drops it, never an empty "").
        if (content.empty())
            return std::unexpected(
                std::string("GitHubActionsStrategy: blank GHA line (timestamp only)"));

        insight::tokenization::ParsedLine parsed;
        parsed.raw_line = line;
        parsed.timestamp = insight::utils::parse_iso8601(line.substr(0U, kGhaPrefixLen));
        // A workflow-command marker is authoritative when present; otherwise the body is raw stdout —
        // fall back to the same leading-level / failure-cue inference RawTextStrategy uses.
        parsed.level = level_from_message(content);
        if (parsed.level == insight::LogLevel::Unknown)
            parsed.level = insight::utils::infer_leading_log_level(content);
        parsed.component = {}; // GHA lines carry no component / tag
        parsed.content = arena.store_string(content);
        return std::expected<insight::tokenization::ParsedLine, std::string>{parsed};
    }

    [[nodiscard]] insight::LogFormat format() const noexcept override
    {
        return insight::LogFormat::GitHubActions;
    }

    [[nodiscard]] double confidence(std::string_view line) const noexcept override
    {
        static constexpr double kGitHubActionsConfidence{0.92};
        static constexpr double kNoConfidence{0.0};
        return is_github_actions_prefix(line) ? kGitHubActionsConfidence : kNoConfidence;
    }
};

} // namespace

std::unique_ptr<insight::tokenization::IFormatStrategy> make_strategy()
{
    return std::make_unique<GitHubActionsStrategy>();
}

// True iff `line` is an echoed-source line (D-PROV-1): after an optional leading GHA timestamp +
// separating space, the ENTIRE visible content is a SINGLE SGR-wrapped span — a command-echo SGR
// (`36;1`/`1;36`), a content run, and a closing reset (`0`/empty/`39`) — no un-wrapped visible bytes
// outside the span. Operates on the RAW line (ANSI intact). Byte-exact state machine (F5). Ported
// verbatim from the pre-split canon scan shard; the composed provenance hook LogParser consults.
bool is_echoed_source(std::string_view line) noexcept
{
    std::size_t pos{0};
    if (is_github_actions_prefix(line))
    {
        pos = kGhaPrefixLen;
        if (pos < line.size() && is_space(line[pos]))
            ++pos;
    }
    const std::optional<std::string_view> open{parse_sgr_params(line, pos)};
    if (!open || std::ranges::find(kCommandEchoSgrParams, *open) == kCommandEchoSgrParams.end())
        return false;
    while (pos < line.size() && static_cast<unsigned char>(line[pos]) != kEsc)
        ++pos;
    if (pos >= line.size())
        return false;
    const std::optional<std::string_view> close{parse_sgr_params(line, pos)};
    if (!close || std::ranges::find(kSgrResetParams, *close) == kSgrResetParams.end())
        return false;
    while (pos < line.size() && (is_space(line[pos]) || line[pos] == '\r' || line[pos] == '\n'))
        ++pos;
    return pos == line.size();
}

} // namespace insight::semantic::github
