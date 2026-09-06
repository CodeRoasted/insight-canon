module;

module insight.semantic.gitlab;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;

// refs: ADR-17, ADR-23.D1, ADR-23.D3
// invariant: LINE-SELECTIVE — it claims a stamped line, a line-anchored `section_start:`, and the
// terminal verdict line; everything else falls through, typically to RawText.
// invariant: the 32-byte prefix is PEELED here and NOT declared: its continuation flag encodes line
// DELIMITATION, so ADR-23.D1 holds the catalogue row and this peel is that slot's price.
// note: determinism — pure byte walks, no locale, no float; the peel is a fixed offset
namespace insight::semantic::gitlab
{
namespace
{

    [[nodiscard]] constexpr bool is_digit(char chr) noexcept
    {
        return static_cast<unsigned>(chr) - '0' < 10U;
    }

    // assert: the prefix is `<RFC3339 with 6-digit fraction>Z <NN><O|E><' '|'+'>` and its width is
    // FIXED at 32 bytes — measured over 3 446 260 stamped lines with no other width anywhere.
    // note: the '+' continuation flag sits in the column the separator space otherwise holds
    constexpr std::size_t kTimestampWidth{27U};
    constexpr std::size_t kFractionDigits{6U};
    constexpr std::size_t kSeparatorWidth{1U};
    constexpr std::size_t kStreamTagWidth{3U};
    constexpr std::size_t kContinuationFlagWidth{1U};
    constexpr std::size_t kTransportPrefixWidth{kTimestampWidth + kSeparatorWidth +
                                                kStreamTagWidth + kContinuationFlagWidth};
    static_assert(kTransportPrefixWidth == 32U,
                  "the measured GitLab runner prefix is 32 bytes; a change here changes what is "
                  "peeled off every line of every stamped trace");

    [[nodiscard]] constexpr bool digits_at(std::string_view line, std::size_t pos,
                                           std::size_t count) noexcept
    {
        for (std::size_t idx{0}; idx < count; ++idx)
            if (!is_digit(line[pos + idx]))
                return false;
        return true;
    }

    // pre: `line` may be any line; the check is byte-exact position by position, with no parsing,
    // no allocation and no dependence on anything past byte 32.
    // invariant: the PUNCTUATION is tested first — the cheapest discriminator, and this runs per
    // line twice (confidence, then parse).
    // assert: strictness IS the anti-phantom guard: a Syslog or GHA line opens with RFC3339 too,
    // and only the fixed fraction width plus the stream tag tell them apart.
    [[nodiscard]] constexpr bool has_transport_prefix(std::string_view line) noexcept
    {
        if (line.size() < kTransportPrefixWidth)
            return false;
        if (line[4] != '-' || line[7] != '-' || line[10] != 'T' || line[13] != ':' ||
            line[16] != ':' || line[19] != '.' || line[26] != 'Z' || line[27] != ' ')
            return false;
        // invariant: the `E` stderr signal is DECLARED and NOT READ — mapping a stream to a
        // LogLevel is a semantic claim no study earned.
        // note: a stderr line is routinely informational; the level still comes from the level scan
        if (line[30] != 'O' && line[30] != 'E')
            return false;
        if (line[31] != ' ' && line[31] != '+')
            return false;
        return digits_at(line, 0U, 4U) && digits_at(line, 5U, 2U) && digits_at(line, 8U, 2U) &&
               digits_at(line, 11U, 2U) && digits_at(line, 14U, 2U) && digits_at(line, 17U, 2U) &&
               digits_at(line, 20U, kFractionDigits) && digits_at(line, 28U, 2U);
    }

    constexpr std::string_view kSectionPrefix{"section_start:"};

    // assert: every outcome row this package ships carries the verdict in its PREFIX, so
    // prefix-matching IS the row's match predicate and this walk is complete.
    // note: the static_assert keeps it true: a RemainderToken row would need a word check here
    static_assert(
        std::ranges::all_of(kOutcomeMarkers, [](const OutcomeMarkerRow& row) noexcept
                            { return row.shape == OutcomeMarkerShape::PrefixIsVerdict; }),
        "gitlab: a RemainderToken outcome row would need a verdict-word check here — this "
        "strategy claims a terminal line on prefix alone");

    [[nodiscard]] constexpr bool is_terminal_verdict(std::string_view content) noexcept
    {
        return std::ranges::any_of(kOutcomeMarkers, [content](const OutcomeMarkerRow& row) noexcept
                                   { return content.starts_with(row.prefix); });
    }

    // invariant: a STAMPED line is claimed regardless of its content — the peel itself is the
    // dialect knowledge, and it is what exposes every marker underneath it.
    [[nodiscard]] constexpr bool claims(std::string_view line) noexcept
    {
        return has_transport_prefix(line) || line.starts_with(kSectionPrefix) ||
               is_terminal_verdict(line);
    }

    class GitLabStrategy final : public insight::tokenization::IFormatStrategy
    {
      public:
        [[nodiscard]] std::expected<insight::tokenization::ParsedLine, std::string>
        parse(std::string_view line, insight::tokenization::ArenaAllocator& arena) const override
        {
            if (!claims(line))
                return std::unexpected(std::string("GitLabStrategy: not a GitLab-marked line"));

            std::string_view content{line};
            std::optional<insight::Timestamp> timestamp;
            if (has_transport_prefix(line))
            {
                timestamp = insight::utils::parse_iso8601(line.substr(0, kTimestampWidth));
                content = line.substr(kTransportPrefixWidth);
                // post: a prefix-only line is a blank line and is DECLINED, so no empty "" template
                // reaches the tokenizer — the GHA/Jenkins strategy discipline.
                if (content.empty())
                    return std::unexpected(
                        std::string("GitLabStrategy: blank line (transport prefix only)"));
            }

            insight::tokenization::ParsedLine parsed;
            parsed.raw_line = line;
            parsed.timestamp = insight::tokenization::EventTime::parsed(timestamp);
            // invariant: no level-lift rows, so the level comes from the same leading-level /
            // failure-cue inference RawTextStrategy uses — byte-identical to that fallback.
            parsed.level = insight::utils::infer_leading_log_level(content);
            // refs: DN-43.D8
            // invariant: EMPTY is a positive statement that this layout declares no functional
            // source — a GitLab trace line carries no component or tag.
            parsed.component = {};
            // post: content is stored VERBATIM after the peel, CR included — a trailing `\r`
            // survives on 4.96 % of non-marker stamped lines and is CONTENT there, not a delimiter.
            // note: measured 170 735 of 3 440 982; normalizing it away is an unmeasured claim
            parsed.content = arena.store_string(content);
            return std::expected<insight::tokenization::ParsedLine, std::string>{parsed};
        }

        [[nodiscard]] insight::LogFormat format() const noexcept override
        {
            return insight::LogFormat::GitLab;
        }

        [[nodiscard]] double confidence(std::string_view line) const noexcept override
        {
            // invariant: decisive on a dialect-marked line so it outranks the heuristic candidates,
            // and silent otherwise so the line falls through — the GHA/Jenkins shape.
            static constexpr double kGitLabConfidence{0.92};
            static constexpr double kNoConfidence{0.0};
            return claims(line) ? kGitLabConfidence : kNoConfidence;
        }
    };

} // namespace

std::unique_ptr<insight::tokenization::IFormatStrategy> make_strategy()
{
    return std::make_unique<GitLabStrategy>();
}

} // namespace insight::semantic::gitlab
