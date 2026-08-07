module;

module insight.semantic.gitlab;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;

// gitlab_strategy.cpp — the GitLab CI dialect CODE TIER (ADR-17): the format strategy. Like
// Jenkins and unlike GHA, a GitLab trace has no single uniform line shape across runner
// generations, so the strategy is LINE-SELECTIVE — it claims exactly the shapes this dialect marks,
// and everything else falls through (typically to RawText):
//   1. a line carrying the runner's fixed-width transport prefix
//      `<RFC3339-with-6-digit-fraction>Z <NN><O|E><' '|'+'>` — 32 bytes, PEELED, and its timestamp
//      parsed as the event time;
//   2. a line-anchored `section_start:` marker (the bare, un-stamped form older runners emit);
//   3. the terminal verdict line, recognized by walking THIS package's kOutcomeMarkers data so the
//      knowledge lives in one place (the row) — the Jenkins-strategy discipline.
//
// THE 32-BYTE PREFIX IS PEELED HERE, NOT DECLARED. It is textbook admissible transport under
// ADR-23 — a TOTAL-scope, whole-stream, fixed-width transform, the same class as GHA's 28-byte
// api-rfc3339-line-prefix — and the catalogue already ships the right KIND
// (TransportTransformKind::LinePrefixTimestamp with an EventObservationTime extract). What it does
// NOT ship is a faithful declaration of a timestamp FOLLOWED BY a stream tag, which needs a
// two-element ordered stack and therefore a new kind, a transport-catalog token, and the
// acquisition-side plumbing that decides who declares it. ADR-23 has already sequenced that work
// as T2, whose ± arms are the two legs this dialect's corpus contains. Peeling in the strategy is
// exactly where Jenkins's timestamper peel lives; this package makes T2's vehicle concrete and does
// not schedule it.
//
// Determinism: pure byte walks, no locale, no float (F5). The peel is a fixed offset, so it cannot
// depend on content.

namespace insight::semantic::gitlab
{
namespace
{

    [[nodiscard]] constexpr bool is_digit(char chr) noexcept
    {
        return static_cast<unsigned>(chr) - '0' < 10U;
    }

    // ── The runner transport prefix ──
    // `2026-07-21T18:06:18.101984Z 00O section_start:…`
    //  └──────── 27 bytes ────────┘│└┬┘│
    //                              │ │ └─ separator: ' ' on a new logical line, '+' on a
    //                              │ │    continuation of the previous one
    //                              │ └─── stream tag: two digits + 'O' (stdout) or 'E' (stderr)
    //                              └───── separator space
    //
    // MEASURED on marker_corpus_v1: 3 446 260 stamped lines, prefix width 32 bytes, with no other
    // width anywhere and exactly ONE distinct width within each of the 482 stamped traces. The
    // fraction is always 6 digits and the zone always 'Z'; the '+' continuation flag occupies the
    // same column the separator space otherwise holds, which is why both forms land on 32.
    //
    // The shape check is STRICT, and strictness is the anti-phantom guard: a Syslog or GHA line
    // also opens with an RFC3339 token, and only the fixed fraction width plus the stream tag tell
    // them apart. A producer emitting some other fraction width is DECLINED — the line falls
    // through to RawText and yields no marker, which is a fail-closed miss rather than a wrong
    // answer, and it is the same fixed-width property T2's declaration will assert.
    constexpr std::size_t kTimestampWidth{27U}; // YYYY-MM-DDTHH:MM:SS.ffffffZ
    constexpr std::size_t kFractionDigits{6U};
    constexpr std::size_t kSeparatorWidth{1U}; // the space between timestamp and stream tag
    constexpr std::size_t kStreamTagWidth{3U}; // NN + (O|E)
    constexpr std::size_t kContinuationFlagWidth{
        1U}; // ' ' (new logical line) or '+' (continuation)
    constexpr std::size_t kTransportPrefixWidth{kTimestampWidth + kSeparatorWidth +
                                                kStreamTagWidth + kContinuationFlagWidth};
    static_assert(kTransportPrefixWidth == 32U,
                  "the measured GitLab runner prefix is 32 bytes; a change here changes what is "
                  "peeled off every line of every stamped trace");

    // Are `count` bytes from `pos` all ASCII digits?
    [[nodiscard]] constexpr bool digits_at(std::string_view line, std::size_t pos,
                                           std::size_t count) noexcept
    {
        for (std::size_t idx{0}; idx < count; ++idx)
            if (!is_digit(line[pos + idx]))
                return false;
        return true;
    }

    // Does `line` open with the runner transport prefix? Byte-exact, position by position — no
    // parsing, no allocation, no dependence on anything past byte 32. The PUNCTUATION is tested
    // first: it is the cheapest discriminator and it rejects almost every non-GitLab line in a
    // handful of compares, which matters because this runs per line, twice (confidence + parse).
    [[nodiscard]] constexpr bool has_transport_prefix(std::string_view line) noexcept
    {
        if (line.size() < kTransportPrefixWidth)
            return false;
        if (line[4] != '-' || line[7] != '-' || line[10] != 'T' || line[13] != ':' ||
            line[16] != ':' || line[19] != '.' || line[26] != 'Z' || line[27] != ' ')
            return false;
        // The stream tag: two digits + O|E, then the continuation flag. The 'E' stderr signal is
        // DECLARED and NOT READ — mapping a stream to a LogLevel is a semantic claim no study
        // earned, and a stderr line is routinely informational. The level continues to come from
        // infer_leading_log_level, exactly as Jenkins does.
        if (line[30] != 'O' && line[30] != 'E')
            return false;
        if (line[31] != ' ' && line[31] != '+')
            return false;
        return digits_at(line, 0U, 4U) &&                              // YYYY
               digits_at(line, 5U, 2U) && digits_at(line, 8U, 2U) &&   // MM DD
               digits_at(line, 11U, 2U) && digits_at(line, 14U, 2U) && // HH MM
               digits_at(line, 17U, 2U) &&                             // SS
               digits_at(line, 20U, kFractionDigits) &&                // .ffffff
               digits_at(line, 28U, 2U);                               // the stream-tag number
    }

    constexpr std::string_view kSectionPrefix{"section_start:"};

    // Every outcome-marker row this package ships carries the verdict in its PREFIX with a
    // free-form remainder, so prefix-matching IS the row's match predicate and this walk is
    // complete. The static_assert is what keeps that true: adding a RemainderToken row without
    // teaching this function the word-remainder rule would silently make the strategy over-claim.
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

    // Does the strategy claim this line? A stamped line is claimed REGARDLESS of its content — the
    // peel itself is the dialect knowledge, and it is what exposes every marker underneath it. The
    // two bare forms cover the un-stamped runner generation, where the marker and the verdict line
    // are the only GitLab-marked shapes a trace has.
    [[nodiscard]] constexpr bool claims(std::string_view line) noexcept
    {
        return has_transport_prefix(line) || line.starts_with(kSectionPrefix) ||
               is_terminal_verdict(line);
    }

    // ── The dialect format strategy ──
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
                // A prefix-only line is a blank line: decline it (make_event drops it, never an
                // empty "" template) — the GHA/Jenkins strategy discipline.
                if (content.empty())
                    return std::unexpected(
                        std::string("GitLabStrategy: blank line (transport prefix only)"));
            }

            insight::tokenization::ParsedLine parsed;
            parsed.raw_line = line;
            parsed.timestamp = insight::tokenization::EventTime::parsed(timestamp);
            // No level-lift rows: the body is console output, so the level comes from the same
            // leading-level / failure-cue inference RawTextStrategy uses (byte-identical fallback).
            parsed.level = insight::utils::infer_leading_log_level(content);
            parsed.component = {}; // GitLab trace lines carry no component / tag
            // The content is stored VERBATIM after the peel, CR included. A trailing '\r' survives
            // on 5.05% of non-marker stamped lines (measured, marker_corpus_v1) and it is CONTENT
            // there — an in-place progress redraw, not a delimiter. Normalizing it away would be a
            // template-collapse claim this package has not measured; the marker payload's CR
            // terminator is the extractor's business and is handled in the row grammar.
            parsed.content = arena.store_string(content);
            return std::expected<insight::tokenization::ParsedLine, std::string>{parsed};
        }

        [[nodiscard]] insight::LogFormat format() const noexcept override
        {
            return insight::LogFormat::GitLab;
        }

        [[nodiscard]] double confidence(std::string_view line) const noexcept override
        {
            // Mirrors the GHA/Jenkins dialect confidence: decisive on a dialect-marked line
            // (outranks the heuristic representation candidates), silent otherwise (the line falls
            // through).
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
