module;

module insight.semantic.jenkins;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.spi;

// jenkins_strategy.cpp — the Jenkins Pipeline dialect CODE TIER (ADR 0024 §2.3): the format
// strategy. Unlike GHA (a uniform per-line timestamp prefix), a Jenkins console has NO uniform
// line shape, so the strategy is LINE-SELECTIVE — it claims exactly the three dialect-marked
// shapes studies/006 keyed on, and everything else falls through (typically to RawText, exactly
// as a freestyle console does today):
//   1. a timestamper-plugin prefixed line `[<RFC3339>] <content>` (ci.jenkins.io et al.) — the
//      prefix is STRIPPED (it otherwise defeats template collapse: one template per line) and
//      parsed as the event timestamp;
//   2. a `[Pipeline] ` annotation (the stage/step skeleton the marker rows walk);
//   3. the `Finished: <RESULT>` run epilogue (the console-tail outcome source) — recognized by
//      walking THIS package's kOutcomeMarkers data, so the knowledge lives in one place (the
//      row), mirroring the GHA strategy walking its kLevelLifts.
// Byte logic mirrors the studies/006 spike recognizers, tightened to a strict RFC3339 bracket
// shape so no non-Jenkins `[…]`-prefixed line (Proxifier, ApacheError, bare `[12:34:56]`) is ever
// mis-claimed. Deterministic: pure byte walks, no locale, no float (F5).

namespace insight::semantic::jenkins
{
namespace
{

    [[nodiscard]] constexpr bool is_digit(char chr) noexcept
    {
        return static_cast<unsigned>(chr) - '0' < 10U;
    }
    [[nodiscard]] constexpr bool is_space(char chr) noexcept
    {
        return chr == ' ' || chr == '\t';
    }
    [[nodiscard]] constexpr bool is_word(char chr) noexcept
    {
        return (chr >= 'a' && chr <= 'z') || (chr >= 'A' && chr <= 'Z') || is_digit(chr) ||
               chr == '_';
    }

    // The timestamper-plugin prefix: `[` + a STRICT RFC3339 timestamp (`YYYY-MM-DDTHH:MM:SS`,
    // optional `.fff…` fraction, optional `Z` / `±HH:MM` / `±HHMM` zone) + `]`. Returns the offset
    // ONE PAST the closing `]`, or 0 when the line does not carry the prefix. Strictness is the
    // anti-phantom guard: a Proxifier `[10.20.30.40]`, an ApacheError `[Mon Oct 03 …]`, or a bare
    // `[12:34:56]` never match. The CHARACTER GRAMMAR is delegated to canon's
    // `insight::utils::rfc3339_datetime_length` — one owner, shared with the masker's D-MSK-5
    // `bracket_timestamp` rule (jenkins_retrofit_gates.md §6: the spike, this strategy and that
    // rule were three spellings of one shape; now there is one, with two consumers). This function
    // keeps only the LINE-POSITION logic: prefix-anchored, bracketed, nothing between the datetime
    // and the closing `]`.
    [[nodiscard]] constexpr std::size_t timestamper_prefix_end(std::string_view line) noexcept
    {
        if (line.empty() || line.front() != '[')
            return 0;
        const std::size_t datetime_len{insight::utils::rfc3339_datetime_length(line, 1U)};
        if (datetime_len == 0)
            return 0;
        const std::size_t close{1U + datetime_len};
        if (close >= line.size() || line[close] != ']')
            return 0;
        return close + 1U;
    }

    constexpr std::string_view kPipelinePrefix{"[Pipeline] "};

    // The run epilogue, recognized off THIS package's outcome-marker rows: `<prefix><WORD>` with a
    // single ASCII verdict word remainder (studies/006: `^Finished: (\w+)$`, strict).
    [[nodiscard]] constexpr bool is_run_epilogue(std::string_view content) noexcept
    {
        for (const OutcomeMarkerRow& row : kOutcomeMarkers)
        {
            if (!content.starts_with(row.prefix))
                continue;
            // `starts_with` guarantees `prefix.size() <= content.size()`, so this is the noexcept
            // in-place form of `substr(prefix.size())` — no `throw` path for the analyzer to flag
            // (bugprone-exception-escape).
            std::string_view token{content};
            token.remove_prefix(row.prefix.size());
            if (token.empty())
                continue;
            bool all_word{true};
            for (const char chr : token)
                if (!is_word(chr))
                {
                    all_word = false;
                    break;
                }
            if (all_word)
                return true;
        }
        return false;
    }

    // Does the strategy claim this line? The three dialect-marked shapes, after an optional
    // timestamper strip (a timestamper-prefixed line is claimed REGARDLESS of its content — the
    // strip is the dialect knowledge: it routes the line, extracts the event timestamp, and
    // yields the stamp-free content the marker rows walk).
    //
    // Template collapse NO LONGER depends on this strip — measured, both worlds, on the ratified
    // `jenkins-markers/v2` payload-stamped slice (the harness is
    // `tests/t5_payload_stamp_template_measurement_test.cpp`, re-runnable). Since D-MSK-5
    // `bracket_timestamp` (kCanonicalizationVersion -8) the masker claims the whole-token
    // `[<RFC3339>]` stamp to the stable normal form `[<*>]` on the RawText floor, and the §6.5
    // prefix-image triangle returned REPAIRED (2026-07-30): per stamped line,
    // template(unstripped) == "[<*>]" ⧺ M(rest), zero exceptions on 6 416 stamped lines; distinct
    // templates ±strip 3 337 vs 3 339, the +2 fully attributed (one dual-occurrence twin + the
    // bare-[<*>] cell from 134 timestamp-only lines).
    //
    // The HISTORICAL record, kept because it is why D-MSK-5 exists (adr/0053 erratum 2 — "the
    // bracket is the entire difference"): pre-fix the token fell through every mask rule to
    // literal KEEP, and the un-stripped arm measured 3 425 → 6 094 distinct templates on this
    // slice (95.9 % of the no-collapse ceiling; 7 256 → 118 376 on the whole-stream class) — the
    // adr/0013 precision-first regression the rule repaired.
    [[nodiscard]] constexpr bool claims(std::string_view line) noexcept
    {
        if (timestamper_prefix_end(line) != 0)
            return true;
        return line.starts_with(kPipelinePrefix) || is_run_epilogue(line);
    }

    // ── The dialect format strategy ──
    class JenkinsStrategy final : public insight::tokenization::IFormatStrategy
    {
      public:
        [[nodiscard]] std::expected<insight::tokenization::ParsedLine, std::string>
        parse(std::string_view line, insight::tokenization::ArenaAllocator& arena) const override
        {
            if (!claims(line))
                return std::unexpected(std::string("JenkinsStrategy: not a Jenkins-marked line"));

            std::string_view content{line};
            std::optional<insight::Timestamp> timestamp;
            if (const std::size_t prefix_end{timestamper_prefix_end(line)}; prefix_end != 0)
            {
                // The bracket interior is a strict RFC3339 timestamp — parse it as the event time.
                timestamp = insight::utils::parse_iso8601(line.substr(1U, prefix_end - 2U));
                content = line.substr(prefix_end);
                while (!content.empty() && is_space(content.front()))
                    content.remove_prefix(1U);
                // A timestamp-only line is a blank line: decline it (make_event drops it, never an
                // empty "" template) — the GHA-strategy discipline.
                if (content.empty())
                    return std::unexpected(
                        std::string("JenkinsStrategy: blank line (timestamper prefix only)"));
            }

            insight::tokenization::ParsedLine parsed;
            parsed.raw_line = line;
            parsed.timestamp = timestamp;
            // No level-lift rows (studies/006 surfaced none): the body is console output — the same
            // leading-level / failure-cue inference RawTextStrategy uses (byte-identical fallback).
            parsed.level = insight::utils::infer_leading_log_level(content);
            parsed.component = {}; // Jenkins console lines carry no component / tag
            parsed.content = arena.store_string(content);
            return std::expected<insight::tokenization::ParsedLine, std::string>{parsed};
        }

        [[nodiscard]] insight::LogFormat format() const noexcept override
        {
            return insight::LogFormat::Jenkins;
        }

        [[nodiscard]] double confidence(std::string_view line) const noexcept override
        {
            // Mirrors the GHA dialect confidence: decisive on a dialect-marked line (outranks the
            // heuristic representation candidates), silent otherwise (the line falls through).
            static constexpr double kJenkinsConfidence{0.92};
            static constexpr double kNoConfidence{0.0};
            return claims(line) ? kJenkinsConfidence : kNoConfidence;
        }
    };

} // namespace

std::unique_ptr<insight::tokenization::IFormatStrategy> make_strategy()
{
    return std::make_unique<JenkinsStrategy>();
}

} // namespace insight::semantic::jenkins
