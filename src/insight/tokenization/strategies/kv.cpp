module;
#include <cstring>
#include "insight/utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.detail;
import insight.canon.internal;
import insight.canon.api;

// src/1_tokenization/strategies/kv.cpp
//
// KVStrategy — parses key=value log lines (Logfmt-style).
//
// Supported syntax:
//   key=value          bare value (no spaces/commas/semicolons)
//   key="quoted value" double-quoted value (may contain spaces)
//
// Well-known keys are mapped to structured fields; the rest are concatenated
// into the content string as free-form pairs for Drain ingestion.
//
// Hand-written KV scanner: zero RE2, zero per-pair string copies.




namespace insight::tokenization
{

namespace
{

    constexpr int kHighConfidencePairCount{3};
    constexpr int kMediumConfidencePairCount{2};
    constexpr int kLowConfidencePairCount{1};
    constexpr double kNoConfidence{0.0};
    constexpr double kLowConfidence{0.30};
    constexpr double kMediumConfidence{0.70};
    constexpr double kHighConfidence{0.90};

    // Key-valid chars: \w + '.' + '-'
    [[nodiscard]] constexpr bool is_kv_key_char(char chr) noexcept
    {
        return detail::is_digit(chr) || detail::is_upper(chr) || detail::is_lower(chr) ||
               chr == '_' || chr == '.' || chr == '-';
    }
    [[nodiscard]] constexpr bool is_kv_key_start(char chr) noexcept
    {
        return detail::is_digit(chr) || detail::is_upper(chr) || detail::is_lower(chr) ||
               chr == '_';
    }
    [[nodiscard]] constexpr bool is_bare_value_char(char chr) noexcept
    {
        return !detail::is_space(chr) && chr != ',' && chr != ';';
    }

    // True iff `line` opens (after leading whitespace) with a `key=value` token.
    // Genuine logfmt lines lead with a pair (`ts=`, `level=`, `key=`); a lone
    // trailing pair inside otherwise free text (e.g. "INFO checkout completed
    // order=42") does NOT — and must NOT be claimed by KV, which keeps only the
    // pairs as content and would drop the human-readable message, fragmenting
    // the template per value. Such lines belong to the raw-text fallback, which
    // preserves the full line and infers the leading level.
    [[nodiscard]] constexpr bool leads_with_kv_pair(std::string_view line) noexcept
    {
        std::size_t pos{0};
        const std::size_t len{line.size()};
        while (pos < len && detail::is_space(line[pos]))
            ++pos;
        if (pos >= len || !is_kv_key_start(line[pos]))
            return false;
        while (pos < len && is_kv_key_char(line[pos]))
            ++pos;
        return pos + 1U < len && line[pos] == '=' && is_bare_value_char(line[pos + 1U]) &&
               line[pos + 1U] != '=';
    }

} // namespace

// Mapping well-known keys to structured fields is inherently branch-heavy.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::expected<ParsedLine, std::string> KVStrategy::parse(std::string_view line,
                                                         ArenaAllocator& arena) const
{
    const auto pairs{extract_pairs(line)};
    if (pairs.empty())
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=KeyValue no_pairs");
        return std::unexpected(std::string("KVStrategy: no key=value pairs found"));
    }

    ParsedLine parsed_line;
    parsed_line.raw_line = line;

    // Allocate content buffer in arena (upper bound: line.size() + separators).
    const std::size_t content_cap{line.size() + pairs.size() + 1U};
    char* const content_buf{static_cast<char*>(arena.allocate(content_cap, 1U))};
    const std::span<char> content_span{content_buf, content_cap};
    std::size_t write_pos{0};
    bool first_content{true};

    for (const auto& [key, value] : pairs)
    {
        // Timestamp keys
        if (!parsed_line.timestamp.has_value())
        {
            if (key == "ts" || key == "timestamp" || key == "time" || key == "@timestamp")
            {
                parsed_line.timestamp = try_parse_timestamp(value);
                continue;
            }
        }
        // Level keys
        if (parsed_line.level == LogLevel::Unknown)
        {
            if (key == "level" || key == "severity" || key == "loglevel" || key == "log_level")
            {
                parsed_line.level = try_parse_level(value);
                continue;
            }
        }
        // Component / service keys
        if (parsed_line.component.empty())
        {
            if (key == "component" || key == "source" || key == "logger" || key == "service" ||
                key == "module")
            {
                parsed_line.component = value; // direct slice; line is arena-stable
                continue;
            }
        }
        // Message keys → write value only (no key=prefix)
        if (key == "msg" || key == "message" || key == "log" || key == "text" || key == "body")
        {
            if (first_content)
            {
                std::memcpy(content_span.subspan(write_pos).data(), value.data(), value.size());
                write_pos += value.size();
                first_content = false;
            }
            continue;
        }
        // All remaining pairs: "key=value"
        if (!first_content)
            content_span[write_pos++] = ' ';
        std::memcpy(content_span.subspan(write_pos).data(), key.data(), key.size());
        write_pos += key.size();
        content_span[write_pos++] = '=';
        std::memcpy(content_span.subspan(write_pos).data(), value.data(), value.size());
        write_pos += value.size();
        first_content = false;
    }

    if (write_pos == 0)
    {
        // No content written — fall back to the full line for Drain.
        parsed_line.content = line;
    }
    else
    {
        parsed_line.content = {content_buf, write_pos};
    }

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=KeyValue parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat KVStrategy::format() const noexcept
{
    return LogFormat::KeyValue;
}

double KVStrategy::confidence(std::string_view line) const noexcept
{
    // A line that does not OPEN with a key=value pair is free text with at most
    // an embedded assignment — not logfmt. Defer to the raw-text fallback so the
    // message and leading level survive.
    if (!leads_with_kv_pair(line))
        return kNoConfidence;
    const std::size_t count{detail::count_kv_pair_signatures(line, kHighConfidencePairCount)};
    if (count >= kHighConfidencePairCount)
        return kHighConfidence;
    if (count == kMediumConfidencePairCount)
        return kMediumConfidence;
    if (count == kLowConfidencePairCount)
        return kLowConfidence;
    return kNoConfidence;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::vector<KVStrategy::KVPair> KVStrategy::extract_pairs(std::string_view line)
{
    std::vector<KVPair> pairs;
    std::size_t pos{0};
    const std::size_t len{line.size()};

    while (pos < len)
    {
        // Advance to potential key start (must be word char).
        if (!is_kv_key_start(line[pos]))
        {
            ++pos;
            continue;
        }

        // Scan forward over key chars: [\w.\-]+
        const std::size_t k_start{pos};
        while (pos < len && is_kv_key_char(line[pos]))
            ++pos;
        const std::size_t k_end{pos};

        // Must be followed immediately by '='
        if (pos >= len || line[pos] != '=')
            continue;
        ++pos; // skip '='

        // Extract value: quoted or bare.
        std::size_t v_start{pos};
        std::size_t v_end{};
        if (pos < len && line[pos] == '"')
        {
            v_start = pos + 1U;
            ++pos;
            while (pos < len && line[pos] != '"')
                ++pos;
            v_end = pos;
            if (pos < len)
                ++pos; // skip closing '"'
        }
        else
        {
            while (pos < len && is_bare_value_char(line[pos]))
                ++pos;
            v_end = pos;
        }

        if (v_end == v_start)
            continue; // empty value — skip

        pairs.push_back({.key = line.substr(k_start, k_end - k_start),
                         .value = line.substr(v_start, v_end - v_start)});
    }
    return pairs;
}

std::optional<Timestamp> KVStrategy::try_parse_timestamp(std::string_view value)
{
    if (auto parsed_ts{utils::parse_iso8601(value)})
        return parsed_ts;
    if (auto parsed_ts{utils::parse_bsd_syslog_ts(value)})
        return parsed_ts;
    return std::nullopt;
}

LogLevel KVStrategy::try_parse_level(std::string_view value)
{
    return utils::parse_log_level(value);
}

} // namespace insight::tokenization
