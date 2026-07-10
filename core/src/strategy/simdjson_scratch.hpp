// Internal helper shared by the JSON-shaped strategies (json, cloudwatch,
// systemd_journal). Provides:
//   * a thread-local simdjson::ondemand::parser + padded buffer (zero per-line
//     allocation in steady state),
//   * a span-safe loader that copies a std::string_view into the padded buffer
//     and zeroes the SIMDJSON_PADDING tail,
//   * a helper to extract the first present string-valued field from a list of
//     candidate keys via find_field_unordered (no DOM materialisation).
//
// All entry points are noexcept; errors are reported by simdjson via the
// returned status codes the caller already inspects.

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

// Cache-line aligned to keep the parser away from neighbouring TLS data.
struct alignas(kSimdjsonScratchCacheLine) JsonScratch
{
    // simdjson's ondemand::parser ctor is explicit; provide an explicit
    // user-defined default ctor so callers can value-initialise the wrapper
    // and so `thread_local JsonScratch scratch{};` is well-formed.
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

// Copy `line` into the thread-local padded buffer, zero the padding region,
// and return a padded_string_view suitable for ondemand::iterate(). Buffer
// grows geometrically so allocation count is amortised O(log N) across the
// process lifetime.
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

// Walk `keys` in order; return the first one whose value is a JSON string.
// Each key is consumed at most once via find_field_unordered (no rewind).
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

// Walk `keys` in order; return the first one whose value is a JSON integer.
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

// ── Single-value readers that KEEP the caller's default on error (span seams) ──
// simdjson's `simdjson_result<T>::get(T&)` is `simdjson_warn_unused`, which on gcc/clang expands
// to `__attribute__((warn_unused_result))` — and a `(void)` cast does NOT silence that (unlike a
// C++ `[[nodiscard]]`). The span pass-throughs want exactly "read the field if present, else keep
// the value the caller pre-seeded": an unreadable field is a fail-safe SKIP, not an error to abort
// on, and the pre-seeded default keeps the emitted record well-formed. These two adapters make that
// intent explicit and consume the error_code, so the warning dies at the root rather than being
// masked. Mirrors try_get_string's discipline, for a single already-resolved value.

// Read a string-valued field into `out`; leave `out` untouched when the value is absent / not a
// string.
inline void read_string_or_keep(simdjson::simdjson_result<simdjson::ondemand::value> value,
                                std::string_view& out) noexcept
{
    std::string_view parsed;
    if (value.get_string().get(parsed) == simdjson::SUCCESS)
        out = parsed;
}

// Read a value's raw JSON slice into `out` (quotes/escaping byte-preserved); leave `out` untouched
// on error. The span-document unpack (span_unpack.cpp) passes fields through verbatim this way, so
// its JSON-literal defaults (`""`, `"0"`) survive an unreadable field and keep shape-2 well-formed.
inline void read_raw_json_or_keep(simdjson::simdjson_result<simdjson::ondemand::value> value,
                                  std::string_view& out) noexcept
{
    std::string_view raw;
    if (value.raw_json().get(raw) == simdjson::SUCCESS)
        out = raw;
}

// OTLP body extraction (insight_otel_epic.md D-OTEL-1): the OpenTelemetry Log Data Model
// nests the message under body.stringValue (`"body":{"stringValue":"…"}`). Returns the
// stringValue, or false when body is absent / not an object / carries no stringValue. MUST
// be the LAST field accessed on `obj` — it descends into a child, after which the parent
// cursor cannot rewind to a sibling (simdjson on-demand).
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

// Nested-object descent (detection_provenance_and_legibility.md D-MSK-3): app loggers (and
// LogCraft) nest custom fields under `"fields":{…}`, so a top-level component/level lookup
// misses. Get the child object at `key` (one level) so the caller can read its scalars with
// try_get_string. Returns false when `key` is absent / not an object. Like try_get_otel_body,
// this descends into a CHILD: it MUST be the LAST access on `obj` — after reading the child the
// parent cursor cannot rewind to a sibling (simdjson on-demand). A nested object also forces the
// fast byte-scanner to bail (it rejects `{`), so every nested-fields line takes this slow path.
[[nodiscard]] inline bool get_nested_object(simdjson::ondemand::object& obj, std::string_view key,
                                            simdjson::ondemand::object& out) noexcept
{
    simdjson::ondemand::value field;
    if (obj.find_field_unordered(key).get(field) != simdjson::SUCCESS)
        return false;
    return field.get_object().get(out) == simdjson::SUCCESS;
}

// ─────────────────────────────────────────────────────────────────────────────
// Fast-path byte scanner — avoids simdjson for escape-free JSON objects.
//
// For the common case where JSON log lines contain only ASCII string values
// with no escape sequences, this scanner extracts known fields in a single
// pass at ~3× the speed of simdjson::ondemand. Returns has_result=false on:
//   • any backslash in any value
//   • nested objects or arrays
//   • malformed JSON
// Callers must fall back to simdjson when has_result is false.
// ─────────────────────────────────────────────────────────────────────────────

// Scans a quoted JSON string starting at pos (which must point at the opening
// '"'). Advances pos past the closing '"'. Returns false on escape sequences
// or unterminated strings; pos is then undefined and the caller should bail.
[[nodiscard]] inline bool extract_json_str(std::string_view line, std::size_t& pos,
                                           std::string_view& out) noexcept
{
    if (pos >= line.size() || line[pos] != '"')
        return false;
    ++pos; // skip opening '"'
    const std::size_t start{pos};
    while (pos < line.size())
    {
        const char chr{line[pos]};
        if (chr == '\\')
            return false; // escape sequence: fall back to simdjson
        if (chr == '"')
        {
            out = line.substr(start, pos - start);
            ++pos; // skip closing '"'
            return true;
        }
        ++pos;
    }
    return false; // unterminated string
}

// A numeric field captured during the fast scan: its key + the raw numeric token text. The
// catalog match + decimal→int64 parse happen in JsonStrategy::parse's module purview (this GMF
// header cannot see the canon.api ordinal catalog), so the scanner only records candidates.
struct FastJsonNumericField
{
    std::string_view key;
    std::string_view text; // the raw numeric literal (digits / '.' / sign), un-trimmed
};

// Bounds the per-line numeric-candidate capture (W1 ordinal field-route). Log JSON carries a
// handful of numeric fields; beyond this cap extras are ignored (a recognized ordinal past the
// cap is missed — acceptable for the declared seed set; the slow path has no cap).
inline constexpr std::size_t kFastJsonMaxNumericFields{8};

// Output of the fast-path scanner.
struct FastJsonResult
{
    std::string_view timestamp_str; // set when timestamp field is a JSON string
    std::int64_t timestamp_ms{0};   // set (nonzero) when timestamp is a JSON integer
    std::string_view level_str;
    std::string_view component_str;
    std::string_view message_str;
    // Numeric fields seen in the scan (W1 ordinal candidates); matched against the declared
    // catalog by the caller. `numeric_field_count` ≤ kFastJsonMaxNumericFields.
    std::array<FastJsonNumericField, kFastJsonMaxNumericFields> numeric_fields{};
    std::size_t numeric_field_count{0};
    bool has_result{false}; // true iff scan completed without errors
};

// ─────────────────────────────────────────────────────────────────────────────
// Helpers for try_fast_json — each keeps its own complexity budget.
// ─────────────────────────────────────────────────────────────────────────────

inline constexpr std::int64_t kDecimalBase{10};

// Skip JSON whitespace (space, tab, CR, LF) in-place.
inline void skip_json_ws(std::string_view line, std::size_t& pos) noexcept
{
    while (pos < line.size() &&
           (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\r' || line[pos] == '\n'))
        ++pos;
}

// Assign a string value to the first matching field in result.
inline void assign_string_field(FastJsonResult& result, std::string_view key,
                                std::string_view value) noexcept
{
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

// Parse an integer millisecond timestamp from a numeric literal into result if applicable.
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

// Consume one JSON value at pos, dispatching by type.
// Returns false when the caller should abort (nested objects, arrays, or malformed input).
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
        // Record as a W1 ordinal candidate (matched against the declared catalog by the caller).
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
        // Nested object/array or unknown character — bail out; fall back to simdjson.
        return false;
    }
    return true;
}

// Process one JSON key-value pair starting at pos (must point at opening '"' of key).
// Returns false when parsing should abort.
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

// Single-pass byte scanner for JSON log objects.
//
// Recognises the fields used by JsonStrategy and CloudWatchStrategy:
//   timestamp / ts / @timestamp / time / datetime  (string or integer epoch ms)
//   level / severity / loglevel / log_level
//   component / source / logger / service / module / logGroup / logStream
//   message / msg / log / text / body
//
// Unknown fields are silently skipped. O(N) in line length; no heap allocation.
[[nodiscard]] inline FastJsonResult try_fast_json(std::string_view line) noexcept
{
    FastJsonResult result;
    std::size_t pos{0};

    // Skip leading whitespace; expect '{'
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
