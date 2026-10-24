
// invariant: the internal helper shared by the JSON-shaped strategies — a thread-local parser and
// padded buffer, a span-safe loader, and field readers that spend no DOM.
// invariant: every entry point is noexcept; errors are reported through the simdjson status codes
// the caller already inspects.
#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
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
// refs: ADR-29, F-SRC-insight-canon:canon.api.cppm:OtelTraceContext
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
[[nodiscard]] inline bool get_nested_object(simdjson::ondemand::object& obj, std::string_view key,
                                            simdjson::ondemand::object& out) noexcept
{
    simdjson::ondemand::value field;
    if (obj.find_field_unordered(key).get(field) != simdjson::SUCCESS)
        return false;
    return field.get_object().get(out) == simdjson::SUCCESS;
}

// invariant: the two readers of one JSON object report every top-level member to ONE sink, so
// what a member means is decided once and the reader only decides how the bytes are read.
enum class JsonScalarKind : std::uint8_t
{
    String,
    Number,
    Literal
};

// invariant: a String's text is its unescaped value, a Number's its token without whitespace, and
// a Literal's `true`, `false` or `null`.
struct JsonScalar
{
    JsonScalarKind kind{JsonScalarKind::Literal};
    std::string_view text;
};

// invariant: a sink reads top-level scalar members in document order and a single descent's scalar
// members under their own bare names; a false return from `member` declines the fast reader.
template <class Sink>
concept JsonMemberSink = requires(Sink& sink, std::string_view key, JsonScalar scalar) {
    { sink.member(key, scalar) } -> std::same_as<bool>;
    { sink.descended_member(key, scalar) } -> std::same_as<void>;
};

// invariant: the fast reader avoids simdjson entirely for escape-free JSON objects — a single byte
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

// post: the position advanced past any JSON whitespace.
inline void skip_json_ws(std::string_view line, std::size_t& pos) noexcept
{
    while (pos < line.size() &&
           (line[pos] == ' ' || line[pos] == '\t' || line[pos] == '\r' || line[pos] == '\n'))
        ++pos;
}

// post: a compound key resolved to its LAST SEGMENT; a key with no dot is returned unchanged, so
// this is the IDENTITY on every ordinary key and needs no caller branch.
// invariant: THE BOUND IS ONE DOT, shared with the nested shape's one descent — a two-dot key
// resolves to NOTHING and is claimed by no role.
// invariant: taking the last segment at any depth would make the dotted spelling resolve while the
// equivalent nested spelling refused it — one logical document read two ways.
// refs: ADR-17.D12
[[nodiscard]] constexpr std::string_view compound_key_name(std::string_view key) noexcept
{
    const std::size_t dot{key.find('.')};
    if (dot == std::string_view::npos)
        return key;
    std::string_view tail{key};
    tail.remove_prefix(dot + 1);
    if (tail.find('.') != std::string_view::npos)
        return {};
    return tail;
}

// post: one JSON value consumed at the position and reported to the sink; false when the caller
// should abort on nesting, arrays, malformed input or the sink's decline.
template <JsonMemberSink Sink>
[[nodiscard]] bool consume_json_value(std::string_view line, std::size_t& pos, std::string_view key,
                                      Sink& sink) noexcept
{
    const char vch{line[pos]};
    if (vch == '"')
    {
        std::string_view value;
        if (!extract_json_str(line, pos, value))
            return false;
        return sink.member(key, JsonScalar{.kind = JsonScalarKind::String, .text = value});
    }
    if ((vch >= '0' && vch <= '9') || vch == '-')
    {
        const std::size_t num_start{pos};
        while (pos < line.size() &&
               ((line[pos] >= '0' && line[pos] <= '9') || line[pos] == '-' || line[pos] == '+' ||
                line[pos] == '.' || line[pos] == 'e' || line[pos] == 'E'))
            ++pos;
        return sink.member(key, JsonScalar{.kind = JsonScalarKind::Number,
                                           .text = line.substr(num_start, pos - num_start)});
    }
    if (vch == 't' || vch == 'f' || vch == 'n')
    {
        const std::size_t literal_start{pos};
        while (pos < line.size() && line[pos] >= 'a' && line[pos] <= 'z')
            ++pos;
        return sink.member(key,
                           JsonScalar{.kind = JsonScalarKind::Literal,
                                      .text = line.substr(literal_start, pos - literal_start)});
    }
    return false;
}

// pre: the position must point at the opening quote of the key.
// post: one key-value pair consumed; false when parsing should abort.
template <JsonMemberSink Sink>
[[nodiscard]] bool process_json_kv(std::string_view line, std::size_t& pos, Sink& sink) noexcept
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
    return consume_json_value(line, pos, key, sink);
}

// post: true iff the line is one flat JSON object whose every member reached the sink and the
// sink declined none; false means the caller reads the line through simdjson instead.
// invariant: linear in line length with no heap allocation.
template <JsonMemberSink Sink>
[[nodiscard]] bool scan_flat_json(std::string_view line, Sink& sink) noexcept
{
    std::size_t pos{0};
    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t'))
        ++pos;
    if (pos >= line.size() || line[pos] != '{')
        return false;
    ++pos;

    for (;;)
    {
        skip_json_ws(line, pos);
        if (pos >= line.size())
            return false;
        const char cur_ch{line[pos]};
        if (cur_ch == '}')
            return true;
        if (cur_ch == ',')
        {
            ++pos;
            continue;
        }
        if (!process_json_kv(line, pos, sink))
            return false;
    }
}

// post: a scalar value's raw token without the whitespace simdjson's raw token carries after it.
[[nodiscard]] inline std::string_view json_token_of(simdjson::ondemand::value& value) noexcept
{
    std::string_view token{value.raw_json_token()};
    while (!token.empty() && (token.back() == ' ' || token.back() == '\t' || token.back() == '\r' ||
                              token.back() == '\n'))
        token.remove_suffix(1);
    return token;
}

// post: the scalar a simdjson value holds, or nullopt for an object, an array or an unreadable
// value.
[[nodiscard]] inline std::optional<JsonScalar>
read_json_scalar(simdjson::ondemand::value& value, simdjson::ondemand::json_type type) noexcept
{
    switch (type)
    {
    case simdjson::ondemand::json_type::string:
    {
        std::string_view text;
        if (value.get_string().get(text) != simdjson::SUCCESS)
            return std::nullopt;
        return JsonScalar{.kind = JsonScalarKind::String, .text = text};
    }
    case simdjson::ondemand::json_type::number:
        return JsonScalar{.kind = JsonScalarKind::Number, .text = json_token_of(value)};
    case simdjson::ondemand::json_type::boolean:
    case simdjson::ondemand::json_type::null:
        return JsonScalar{.kind = JsonScalarKind::Literal, .text = json_token_of(value)};
    default:
        return std::nullopt;
    }
}

// post: every scalar member of `child` reported to the sink as a descended member; deeper levels
// are not read.
template <JsonMemberSink Sink>
void walk_descended_object(simdjson::ondemand::object& child, Sink& sink) noexcept
{
    for (auto sub : child)
    {
        std::string_view sub_key;
        if (sub.unescaped_key().get(sub_key) != simdjson::SUCCESS)
            continue;
        simdjson::ondemand::value sub_value;
        simdjson::ondemand::json_type sub_type{};
        if (sub.value().get(sub_value) != simdjson::SUCCESS ||
            sub_value.type().get(sub_type) != simdjson::SUCCESS)
            continue;
        if (const auto scalar{read_json_scalar(sub_value, sub_type)})
            sink.descended_member(sub_key, *scalar);
    }
}

// post: every scalar member of `root` reported to the sink in document order, and every scalar
// member of an object member reported once as a descended member.
// invariant: the sink's decline is the fast reader's signal only, so this reader ignores it.
// pre: `root` is unread; the walk leaves its cursor at the end, and a caller reading it again
// resets it first.
template <JsonMemberSink Sink>
void walk_json_object(simdjson::ondemand::object& root, Sink& sink) noexcept
{
    for (auto field : root)
    {
        std::string_view key;
        if (field.unescaped_key().get(key) != simdjson::SUCCESS)
            continue;
        simdjson::ondemand::value value;
        simdjson::ondemand::json_type type{};
        if (field.value().get(value) != simdjson::SUCCESS ||
            value.type().get(type) != simdjson::SUCCESS)
            continue;
        if (type == simdjson::ondemand::json_type::object)
        {
            if (simdjson::ondemand::object child;
                value.get_object().get(child) == simdjson::SUCCESS)
                walk_descended_object(child, sink);
            continue;
        }
        if (const auto scalar{read_json_scalar(value, type)})
            static_cast<void>(sink.member(key, *scalar));
    }
}

// invariant: one role's choice among the members offering it: the LOWEST rank wins and an equal
// rank keeps the first offer, so the choice is a function of the member set and its order only.
struct JsonRoleChoice
{
    static constexpr std::uint8_t kNoRank{0xFF};
    std::string_view value;
    std::uint8_t rank{kNoRank};

    void offer(std::uint8_t candidate_rank, std::string_view candidate) noexcept
    {
        if (candidate_rank < rank)
        {
            rank = candidate_rank;
            value = candidate;
        }
    }

    [[nodiscard]] bool chosen() const noexcept
    {
        return rank != kNoRank;
    }
};

} // namespace insight::tokenization
