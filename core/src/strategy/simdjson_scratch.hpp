
// invariant: the internal helper shared by the JSON-shaped strategies — a thread-local parser and
// padded buffer, a span-safe loader, and field readers that spend no DOM.
// invariant: every entry point is noexcept; errors are reported through the simdjson status codes
// the caller already inspects.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <simdjson.h>
#include <span>
#include <string_view>

namespace insight::tokenization
{

inline constexpr std::size_t kSimdjsonScratchCacheLine{64};
inline constexpr std::size_t kSimdjsonScratchInitialCapacity{4096};

// invariant: cache-line aligned to keep the parser away from neighbouring thread-local data.
struct alignas(kSimdjsonScratchCacheLine) JsonScratch
{
    // invariant: simdjson's parser constructor is explicit, so an explicit user-defined default
    // constructor is what makes a value-initialised thread-local wrapper well-formed.
    // note: the directive below is measured LOAD-BEARING: 0 findings with it, 2 without.
    // NOLINTNEXTLINE(readability-redundant-member-init)
    JsonScratch() noexcept : parser{}, padded{} {}
    simdjson::ondemand::parser parser;
    simdjson::padded_string padded;
};

[[nodiscard]] inline JsonScratch& json_scratch() noexcept
{
    thread_local JsonScratch scratch{};
    return scratch;
}

// post: a padded view over the thread-local buffer, with the padding region zeroed, suitable for an
// on-demand iterate.
// invariant: the buffer grows geometrically, so the allocation count is amortised logarithmic
// across the process lifetime.
[[nodiscard]] inline simdjson::padded_string_view load_padded(JsonScratch& scratch,
                                                              std::string_view line) noexcept
{
    const std::size_t needed{line.size() + simdjson::SIMDJSON_PADDING};
    if (scratch.padded.size() < needed)
    {
        std::size_t new_cap{scratch.padded.size() == 0 ? kSimdjsonScratchInitialCapacity
                                                       : scratch.padded.size() * 2};
        while (new_cap < needed)
            new_cap *= 2;
        scratch.padded = simdjson::padded_string(new_cap);
    }
    std::memcpy(scratch.padded.data(), line.data(), line.size());
    const std::span<char> buffer{scratch.padded.data(), scratch.padded.size()};
    const auto tail{buffer.subspan(line.size(), simdjson::SIMDJSON_PADDING)};
    std::memset(tail.data(), 0, tail.size());
    return simdjson::padded_string_view(scratch.padded.data(), line.size(), scratch.padded.size());
}

// post: true iff one of the keys, walked in order, held a JSON STRING value.
// invariant: each key is consumed at most once through an unordered lookup, so there is no rewind.
[[nodiscard]] inline bool try_get_string(simdjson::ondemand::object& obj,
                                         std::span<const std::string_view> keys,
                                         std::string_view& out) noexcept
{
    for (const std::string_view key : keys)
    {
        simdjson::ondemand::value field;
        if (obj.find_field_unordered(key).get(field) != simdjson::SUCCESS)
            continue;
        std::string_view value;
        if (field.get_string().get(value) == simdjson::SUCCESS)
        {
            out = value;
            return true;
        }
    }
    return false;
}

// post: true iff one of the keys, walked in order, held a JSON INTEGER value.
[[nodiscard]] inline bool try_get_int64(simdjson::ondemand::object& obj,
                                        std::span<const std::string_view> keys,
                                        std::int64_t& out) noexcept
{
    for (const std::string_view key : keys)
    {
        simdjson::ondemand::value field;
        if (obj.find_field_unordered(key).get(field) != simdjson::SUCCESS)
            continue;
        std::int64_t value{};
        if (field.get_int64().get(value) == simdjson::SUCCESS)
        {
            out = value;
            return true;
        }
    }
    return false;
}

// post: the string value when present; the output is left UNTOUCHED when the value is absent or is
// not a string.
// invariant: simdjson's result getter is warn-unused, and a void cast does NOT silence that on gcc
// or clang, unlike a standard nodiscard.
// invariant: the span pass-throughs want exactly read the field if present, else keep the value the
// caller pre-seeded — an unreadable field is a fail-safe SKIP, not an error to abort on.
// invariant: the pre-seeded default is what keeps the emitted record well-formed, and these
// adapters consume the error code so the warning dies at the root rather than being masked.
inline void read_string_or_keep(simdjson::simdjson_result<simdjson::ondemand::value> value,
                                std::string_view& out) noexcept
{
    std::string_view parsed;
    if (value.get_string().get(parsed) == simdjson::SUCCESS)
        out = parsed;
}

// post: the value's raw JSON slice with quotes and escaping byte-preserved; the output is left
// UNTOUCHED on error.
// invariant: that is what lets the export unpack pass fields through verbatim and keep its own
// JSON-literal defaults when a field is unreadable.
inline void read_raw_json_or_keep(simdjson::simdjson_result<simdjson::ondemand::value> value,
                                  std::string_view& out) noexcept
{
    std::string_view raw;
    if (value.raw_json().get(raw) == simdjson::SUCCESS)
        out = raw;
}

// post: the OTLP body string, or false when the body is absent, not an object, or carries no string
// value.
// pre: this MUST be the LAST field accessed on the object — it descends into a child, after which
// the parent cursor cannot rewind to a sibling.
// refs: ADR-29, SRC-D-OTEL-1
[[nodiscard]] inline bool try_get_otel_body(simdjson::ondemand::object& obj,
                                            std::string_view& out) noexcept
{
    simdjson::ondemand::value body_field;
    if (obj.find_field_unordered("body").get(body_field) != simdjson::SUCCESS)
        return false;
    simdjson::ondemand::object body_obj;
    if (body_field.get_object().get(body_obj) != simdjson::SUCCESS)
        return false;
    simdjson::ondemand::value string_value_field;
    if (body_obj.find_field_unordered("stringValue").get(string_value_field) != simdjson::SUCCESS)
        return false;
    std::string_view value;
    if (string_value_field.get_string().get(value) != simdjson::SUCCESS)
        return false;
    out = value;
    return true;
}

// post: the child object at the key, one level down, so the caller can read its scalars; false when
// the key is absent or is not an object.
// pre: like the body reader, this MUST be the LAST access on the object — after reading the child
// the parent cursor cannot rewind to a sibling.
// invariant: a nested object also forces the fast byte scanner to bail, so every nested-fields line
// takes this slow path.
// refs: SRC-D-MSK-3
[[nodiscard]] inline bool get_nested_object(simdjson::ondemand::object& obj, std::string_view key,
                                            simdjson::ondemand::object& out) noexcept
{
    simdjson::ondemand::value field;
    if (obj.find_field_unordered(key).get(field) != simdjson::SUCCESS)
        return false;
    return field.get_object().get(out) == simdjson::SUCCESS;
}

// invariant: the fast path avoids simdjson entirely for escape-free JSON objects — a single byte
// pass over ASCII string values, measured at about three times the on-demand speed.
// invariant: it bails on any backslash in any value, on nested objects or arrays, and on malformed
// input, and the caller MUST fall back to simdjson when it does.
// pre: the string scanner below takes a position pointing at an opening quote.
// post: it advances the position past the closing quote, and returns false on an escape sequence or
// an unterminated string, after which the position is undefined.
[[nodiscard]] inline bool extract_json_str(std::string_view line, std::size_t& pos,
                                           std::string_view& out) noexcept
{
    if (pos >= line.size() || line[pos] != '"')
        return false;
    ++pos;
    const std::size_t start{pos};
    while (pos < line.size())
    {
        const char chr{line[pos]};
        if (chr == '\\')
            return false;
        if (chr == '"')
        {
            out = line.substr(start, pos - start);
            ++pos;
            return true;
        }
        ++pos;
    }
    return false;
}

// invariant: the catalog match and the decimal-to-int64 parse happen in the strategy's module
// purview, because this global-module header cannot see the ordinal catalog.
// invariant: so the scanner only records CANDIDATES.
struct FastJsonNumericField
{
    std::string_view key;
    std::string_view text;
};

// invariant: log JSON carries a handful of numeric fields, so beyond this cap extras are ignored
// — a recognized ordinal past the cap is missed, which is accepted for the declared seed set.
// invariant: the slow path has no cap.
inline constexpr std::size_t kFastJsonMaxNumericFields{8};

struct FastJsonResult
{
    std::string_view timestamp_str;
    std::int64_t timestamp_ms{0};
    std::string_view level_str;
    std::string_view component_str;
    std::string_view message_str;
    // invariant: the numeric candidates seen in the scan, matched against the declared catalog by
    // the caller; the count never exceeds the cap.
    std::array<FastJsonNumericField, kFastJsonMaxNumericFields> numeric_fields{};
    std::size_t numeric_field_count{0};
    bool has_result{false};
};

inline constexpr std::int64_t kDecimalBase{10};

// post: the position advanced past any JSON whitespace.
inline void skip_json_ws(std::string_view line, std::size_t& pos) noexcept
{
    while (pos < line.size() &&
           (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\r' || line[pos] == '\n'))
        ++pos;
}

// post: a compound key resolved to its LAST SEGMENT; a key with no dot is returned unchanged, so
// this is the IDENTITY on every ordinary key and needs no caller branch.
// invariant: it lives in this header so the TWO key-matching sites share ONE definition — the
// escape-free fast scanner and the simdjson slow path both classify keys.
// invariant: the strategy RETURNS EARLY on a fast-path hit, so a rule applied only on the slow path
// is invisible to every flat line — exactly the shape a namespaced producer emits.
// invariant: the rule would therefore have been dead on its main population; defining it once here
// is what stops the two paths disagreeing about what a key means.
// invariant: THE BOUND IS ONE DOT, shared with the nested shape's one descent — a two-dot key
// resolves to NOTHING and is claimed by no role.
// invariant: taking the last segment at any depth would make the dotted spelling resolve while the
// equivalent nested spelling refused it — one logical document read two ways.
// refs: SRC-D-ECS-1
[[nodiscard]] inline constexpr std::string_view compound_key_name(std::string_view key) noexcept
{
    const std::size_t dot{key.find('.')};
    if (dot == std::string_view::npos)
        return key;
    const std::string_view tail{key.substr(dot + 1)};
    if (tail.find('.') != std::string_view::npos)
        return {};
    return tail;
}

inline void assign_string_field(FastJsonResult& result, std::string_view raw_key,
                                std::string_view value) noexcept
{
    // invariant: the compound SHAPE is applied BEFORE any name comparison — no vendor field name
    // is added to the comparisons below, only the key handed to them is resolved first.
    // refs: SRC-D-ECS-1
    const std::string_view key{compound_key_name(raw_key)};
    const bool no_ts = result.timestamp_str.empty() && result.timestamp_ms == 0;
    if (no_ts && (key == "ts" || key == "timestamp" || key == "@timestamp" || key == "time" ||
                  key == "datetime"))
        result.timestamp_str = value;
    else if (result.level_str.empty() &&
             (key == "level" || key == "severity" || key == "loglevel" || key == "log_level"))
        result.level_str = value;
    else if (result.component_str.empty() &&
             (key == "component" || key == "source" || key == "logger" || key == "service" ||
              key == "module" || key == "logGroup" || key == "logStream"))
        result.component_str = value;
    else if (result.message_str.empty() &&
             (key == "message" || key == "msg" || key == "log" || key == "text" || key == "body"))
        result.message_str = value;
}

// post: an integer millisecond timestamp parsed from a numeric literal, when the key is a timestamp
// key.
inline void parse_number_ts(FastJsonResult& result, std::string_view key,
                            std::string_view num) noexcept
{
    const bool no_ts = result.timestamp_str.empty() && result.timestamp_ms == 0;
    if (!no_ts || (key != "timestamp" && key != "ts"))
        return;
    std::int64_t millis{0};
    std::size_t digit_idx{0};
    const bool neg = !num.empty() && num[0] == '-';
    if (neg)
        ++digit_idx;
    for (; digit_idx < num.size() && num[digit_idx] >= '0' && num[digit_idx] <= '9'; ++digit_idx)
        millis = (millis * kDecimalBase) + static_cast<std::int64_t>(num[digit_idx] - '0');
    result.timestamp_ms = neg ? -millis : millis;
}

// post: one JSON value consumed at the position, dispatched by type; false when the caller should
// abort on nesting, arrays or malformed input.
[[nodiscard]] inline bool consume_json_value(std::string_view line, std::size_t& pos,
                                             std::string_view key, FastJsonResult& result) noexcept
{
    const char vch{line[pos]};
    if (vch == '"')
    {
        std::string_view value;
        if (!extract_json_str(line, pos, value))
            return false;
        assign_string_field(result, key, value);
    }
    else if ((vch >= '0' && vch <= '9') || vch == '-')
    {
        const std::size_t num_start{pos};
        while (pos < line.size() &&
               ((line[pos] >= '0' && line[pos] <= '9') || line[pos] == '-' || line[pos] == '+' ||
                line[pos] == '.' || line[pos] == 'e' || line[pos] == 'E'))
            ++pos;
        const std::string_view num_text{line.substr(num_start, pos - num_start)};
        parse_number_ts(result, key, num_text);
        // invariant: a numeric value is recorded as an ordinal CANDIDATE, matched against the
        // declared catalog by the caller.
        if (result.numeric_field_count < kFastJsonMaxNumericFields)
            result.numeric_fields[result.numeric_field_count++] =
                FastJsonNumericField{.key = key, .text = num_text};
    }
    else if (vch == 't' || vch == 'f' || vch == 'n')
    {
        while (pos < line.size() && line[pos] >= 'a' && line[pos] <= 'z')
            ++pos;
    }
    else
    {
        return false;
    }
    return true;
}

// pre: the position must point at the opening quote of the key.
// post: one key-value pair consumed; false when parsing should abort.
[[nodiscard]] inline bool process_json_kv(std::string_view line, std::size_t& pos,
                                          FastJsonResult& result) noexcept
{
    if (line[pos] != '"')
        return false;
    std::string_view key;
    if (!extract_json_str(line, pos, key))
        return false;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    if (pos >= line.size() || line[pos] != ':')
        return false;
    ++pos;
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    if (pos >= line.size())
        return false;
    return consume_json_value(line, pos, key, result);
}

// post: the recognized timestamp, level, component and message fields of a JSON log object, with
// unknown fields silently skipped.
// invariant: linear in line length with no heap allocation; the recognized field set is the union
// of what the JSON and CloudWatch strategies read.
[[nodiscard]] inline FastJsonResult try_fast_json(std::string_view line) noexcept
{
    FastJsonResult result;
    std::size_t pos{0};

    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    if (pos >= line.size() || line[pos] != '{')
        return result;
    ++pos;

    for (;;)
    {
        skip_json_ws(line, pos);
        if (pos >= line.size())
            return result;
        const char cur_ch{line[pos]};
        if (cur_ch == '}')
        {
            result.has_result = true;
            return result;
        }
        if (cur_ch == ',')
        {
            ++pos;
            continue;
        }
        if (!process_json_kv(line, pos, result))
            return result;
    }
}

} // namespace insight::tokenization
