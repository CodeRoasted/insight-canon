module;
#include "utils/log_macros.hpp"
#include <cstring>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// post: a logfmt-style key-equals-value record; a bare value, or a double-quoted one that may
// contain spaces.
// invariant: well-known keys map to structured fields and the rest are concatenated into the
// content as free-form pairs for the masker.
// invariant: a hand-written scanner with no regex and no per-pair string copies.
// invariant: the log macros stay TEXTUAL in the global module fragment, so no first-party
// declaration leaks through it.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{

    // invariant: a key may carry word characters, a dot and a hyphen.
    [[nodiscard]] constexpr bool is_kv_key_char(char chr) noexcept
    {
        return is_digit(chr) || is_upper(chr) || is_lower(chr) || chr == '_' || chr == '.' ||
               chr == '-';
    }
    [[nodiscard]] constexpr bool is_kv_key_start(char chr) noexcept
    {
        return is_digit(chr) || is_upper(chr) || is_lower(chr) || chr == '_';
    }
    [[nodiscard]] constexpr bool is_bare_value_char(char chr) noexcept
    {
        return !is_space(chr) && chr != ',' && chr != ';';
    }

    // post: true iff the line OPENS, after leading whitespace, with a key-equals-value token.
    // invariant: a genuine logfmt line leads with a pair; a lone trailing pair inside otherwise
    // free text does NOT, and must not be claimed here.
    // invariant: claiming it would keep only the pairs as content and drop the human-readable
    // message, fragmenting the template per value.
    // invariant: such a line belongs to the raw-text fallback, which preserves the full line and
    // infers the leading level.
    [[nodiscard]] constexpr bool leads_with_kv_pair(std::string_view line) noexcept
    {
        std::size_t pos{0};
        const std::size_t len{line.size()};
        while (pos < len && is_space(line[pos]))
            ++pos;
        if (pos >= len || !is_kv_key_start(line[pos]))
            return false;
        while (pos < len && is_kv_key_char(line[pos]))
            ++pos;
        return pos + 1U < len && line[pos] == '=' && is_bare_value_char(line[pos + 1U]) &&
               line[pos + 1U] != '=';
    }

} // namespace

// invariant: mapping well-known keys onto structured fields is inherently branch-heavy, and the
// branch count IS the well-known key set.
// note: the directive below is measured LOAD-BEARING: 0 findings with it, 1 without.
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

    // invariant: the content buffer is sized to an upper bound — the line plus one separator per
    // pair — so it is allocated once in the arena and never grown.
    const std::size_t content_cap{line.size() + pairs.size() + 1U};
    char* const content_buf{static_cast<char*>(arena.allocate(content_cap, 1U))};
    const std::span<char> content_span{content_buf, content_cap};
    std::size_t write_pos{0};
    bool first_content{true};

    for (const auto& [key, value] : pairs)
    {
        if (!parsed_line.timestamp.has_value())
        {
            if (key == "ts" || key == "timestamp" || key == "time" || key == "@timestamp")
            {
                parsed_line.timestamp = EventTime::parsed(try_parse_timestamp(value));
                continue;
            }
        }
        if (parsed_line.level == LogLevel::Unknown)
        {
            if (key == "level" || key == "severity" || key == "loglevel" || key == "log_level")
            {
                parsed_line.level = EventLevel::declared(try_parse_level(value));
                continue;
            }
        }
        if (parsed_line.component.empty())
        {
            if (key == "component" || key == "source" || key == "logger" || key == "service" ||
                key == "module")
            {
                parsed_line.component = value;
                continue;
            }
        }
        // invariant: a message key writes its VALUE only, with no key prefix, because the message
        // is the record's content rather than one of its pairs.
        // invariant: and ONLY when nothing has been written yet — a message key arriving after
        // another unclaimed pair is DROPPED rather than appended.
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
        // invariant: when no content was written the whole line is handed to the masker, so a
        // record whose pairs were all consumed as fields still templates on something.
        parsed_line.content = line;
    }
    else
    {
        parsed_line.content = {content_buf, write_pos};
    }

    INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                      "strategy=KeyValue parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level.value()),
                      parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat KVStrategy::format() const noexcept
{
    return LogFormat::KeyValue;
}

double KVStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr int kHighConfidencePairCount{3};
    static constexpr int kMediumConfidencePairCount{2};
    static constexpr int kLowConfidencePairCount{1};
    static constexpr double kHighConfidence{0.90};
    static constexpr double kMediumConfidence{0.70};
    static constexpr double kLowConfidence{0.30};
    static constexpr double kNoConfidence{0.0};

    // invariant: a line that does not OPEN with a pair is free text with at most an embedded
    // assignment, so it defers to the raw-text fallback and its message and level survive.
    if (!leads_with_kv_pair(line))
        return kNoConfidence;
    const std::size_t count{count_kv_pair_signatures(line, kHighConfidencePairCount)};
    if (count >= kHighConfidencePairCount)
        return kHighConfidence;
    if (count == kMediumConfidencePairCount)
        return kMediumConfidence;
    if (count == kLowConfidencePairCount)
        return kLowConfidence;
    return kNoConfidence;
}

// post: the key-value pairs of the line, each a view into it; an empty value is skipped.
// invariant: the scan is inherently branch-heavy because the value grammar has two forms and the
// key grammar is a character class.
// note: the directive below is measured LOAD-BEARING: 0 findings with it, 1 without.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::vector<KVStrategy::KVPair> KVStrategy::extract_pairs(std::string_view line)
{
    std::vector<KVPair> pairs;
    std::size_t pos{0};
    const std::size_t len{line.size()};

    while (pos < len)
    {
        if (!is_kv_key_start(line[pos]))
        {
            ++pos;
            continue;
        }

        const std::size_t k_start{pos};
        while (pos < len && is_kv_key_char(line[pos]))
            ++pos;
        const std::size_t k_end{pos};

        if (pos >= len || line[pos] != '=')
            continue;
        ++pos;

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
                ++pos;
        }
        else
        {
            while (pos < len && is_bare_value_char(line[pos]))
                ++pos;
            v_end = pos;
        }

        if (v_end == v_start)
            continue;

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
