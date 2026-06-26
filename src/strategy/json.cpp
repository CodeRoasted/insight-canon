module;
#include "strategy/simdjson_scratch.hpp" // textual: TU-local simdjson entities (§11.8 family)
#include "utils/log_macros.hpp"          // textual macro layer (§11.9)
#include <simdjson.h>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// src/1_tokenization/strategies/json.cpp
//
// JsonStrategy — parses structured JSON log lines using simdjson on-demand.
// See detail/simdjson_scratch.hpp for the thread-local zero-alloc scaffolding.

namespace insight::tokenization
{

namespace
{

// Route the declared OTEL field-map (D-OTEL-4a) over the parsed OTLP object: severity_number
// → the LogLevel band (declared > inferred — it runs after the level-string route, so it
// overrides), and traceId/spanId/parentSpanId → the consumed trace context. Returns true iff
// the record is OTEL (a severityNumber or traceId was present), which the caller uses to route
// the message to the nested body.stringValue. Trace ids are hashed to scalar PODs and never
// retained as values (OR1). Kept separate so JsonStrategy::parse stays within its complexity
// budget.
[[nodiscard]] bool extract_otel_fields(simdjson::ondemand::object& root, ParsedLine& parsed_line)
{
    bool is_otel{false};
    std::string_view scratch_view;
    // OTLP timeUnixNano → event-time. Not one of the four classified fields, but required so OTEL
    // inputs carry a timestamp and window like any other format (without it the whole pipeline
    // never closes a window). A quoted epoch-nanoseconds string per the OTLP/JSON mapping.
    static constexpr std::string_view kTimeUnixNanoKey{"timeUnixNano"};
    if (try_get_string(root, std::span<const std::string_view>{&kTimeUnixNanoKey, 1}, scratch_view))
        if (auto timestamp{utils::parse_unix_nano_timestamp(scratch_view)})
        {
            parsed_line.timestamp = timestamp;
            is_otel = true;
        }
    for (const auto& descriptor : kOtelFieldCatalog)
    {
        const std::span<const std::string_view> key_span{&descriptor.key, 1};
        switch (descriptor.field_class)
        {
        case OtelFieldClass::SeverityNumber:
            if (std::int64_t severity_number{}; try_get_int64(root, key_span, severity_number))
            {
                parsed_line.level = log_level_from_severity_number(severity_number);
                is_otel = true;
            }
            break;
        case OtelFieldClass::TraceId:
            if (try_get_string(root, key_span, scratch_view))
            {
                parsed_line.trace.present = true;
                parsed_line.trace.trace_id = trace_id_from_hex(scratch_view);
                is_otel = true;
            }
            break;
        case OtelFieldClass::SpanId:
            if (try_get_string(root, key_span, scratch_view))
                parsed_line.trace.span_id = span_id_from_hex(scratch_view);
            break;
        case OtelFieldClass::ParentSpanId:
            if (try_get_string(root, key_span, scratch_view))
            {
                parsed_line.trace.has_parent = true;
                parsed_line.trace.parent_span_id = span_id_from_hex(scratch_view);
            }
            break;
        }
    }
    return is_otel;
}

} // namespace

std::expected<ParsedLine, std::string> JsonStrategy::parse(std::string_view line,
                                                           ArenaAllocator& arena) const
{
    if (line.empty())
        return std::unexpected(std::string("JsonStrategy: empty line"));

    // ── Fast path ─────────────────────────────────────────────────────────────
    // For escape-free JSON objects bypass simdjson entirely: single-pass byte
    // scan with no heap allocation. Falls back to simdjson on any anomaly.
    {
        const auto fast{try_fast_json(line)};
        if (fast.has_result)
        {
            ParsedLine parsed_line;
            parsed_line.raw_line = line;
            if (!fast.timestamp_str.empty())
            {
                parsed_line.timestamp = utils::parse_iso8601(fast.timestamp_str);
                if (!parsed_line.timestamp)
                    parsed_line.timestamp = utils::parse_bsd_syslog_ts(fast.timestamp_str);
            }
            if (!fast.level_str.empty())
                parsed_line.level = utils::parse_log_level(fast.level_str);
            if (!fast.component_str.empty())
                parsed_line.component = arena.store_string(fast.component_str);
            parsed_line.content = fast.message_str.empty() ? arena.store_string(line)
                                                           : arena.store_string(fast.message_str);
            INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                              "strategy=JSON fast_path component={} level={} has_timestamp={}",
                              parsed_line.component, to_string(parsed_line.level),
                              parsed_line.timestamp.has_value());
            return std::expected<ParsedLine, std::string>{parsed_line};
        }
    }
    // ── Slow path: full simdjson (handles escapes, nested objects, etc.) ──────

    auto& scratch{json_scratch()};
    const auto padded{load_padded(scratch, line)};

    simdjson::ondemand::document doc;
    if (auto err = scratch.parser.iterate(padded).get(doc); err != simdjson::SUCCESS)
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=JSON simdjson_iterate_error={}",
                          simdjson::error_message(err));
        return std::unexpected(std::string("JsonStrategy: invalid JSON"));
    }

    simdjson::ondemand::object root;
    if (doc.get_object().get(root) != simdjson::SUCCESS)
    {
        INSIGHT_LOG_TRACE(logging::strategy_logger(), "strategy=JSON not_an_object");
        return std::unexpected(std::string("JsonStrategy: top-level not an object"));
    }

    ParsedLine parsed_line;
    parsed_line.raw_line = line;

    std::string_view scratch_view;

    if (try_get_string(root, kTimestampKeys, scratch_view))
    {
        parsed_line.timestamp = utils::parse_iso8601(scratch_view);
        if (!parsed_line.timestamp)
            parsed_line.timestamp = utils::parse_bsd_syslog_ts(scratch_view);
    }

    if (try_get_string(root, kLevelKeys, scratch_view))
        parsed_line.level = utils::parse_log_level(scratch_view);

    if (try_get_string(root, kComponentKeys, scratch_view))
        parsed_line.component = arena.store_string(scratch_view);

    // ── OTEL/OTLP field-map (insight_otel_epic.md D-OTEL-1, the declared catalog D-OTEL-4a) ──
    // severity_number → the LogLevel band (declared > inferred) + the trace context, all
    // consumed structural metadata; the trace keys are top-level → never tokenized → dropped
    // from the template by construction (OR1). is_otel routes the message to the nested
    // OTLP body.stringValue (extract_otel_fields keeps parse() within the complexity budget).
    const bool is_otel{extract_otel_fields(root, parsed_line)};

    if (is_otel)
    {
        // OTLP message lives under the nested body.stringValue. MUST be the last root access
        // (it descends into a child). A malformed/absent body → arena-store the whole line.
        if (std::string_view body_value; try_get_otel_body(root, body_value))
            parsed_line.content = arena.store_string(body_value);
        else
            parsed_line.content = arena.store_string(line);
    }
    else if (try_get_string(root, kMessageKeys, scratch_view))
    {
        parsed_line.content = arena.store_string(scratch_view);
    }
    else
    {
        // Fallback: arena-store the original line. We avoid re-serialising the
        // document (which would force a heap allocation); the raw bytes are
        // already stable when the caller is LogParser.
        parsed_line.content = arena.store_string(line);
    }

    INSIGHT_LOG_TRACE(
        logging::strategy_logger(), "strategy=JSON parsed component={} level={} has_timestamp={}",
        parsed_line.component, to_string(parsed_line.level), parsed_line.timestamp.has_value());
    return std::expected<ParsedLine, std::string>{parsed_line};
}

LogFormat JsonStrategy::format() const noexcept
{
    return LogFormat::JSON;
}

// Cheap layout check: first non-space character must be '{'. The actual JSON
// parse happens once, in parse(); the detector pipeline gates parse() behind
// this O(1) check, so the line is never traversed twice.
double JsonStrategy::confidence(std::string_view line) const noexcept
{
    static constexpr double kJsonObjectConfidence{1.0};
    static constexpr double kNoConfidence{0.0};
    for (const char current_char : line)
    {
        if (current_char == ' ' || current_char == '\t')
            continue;
        return (current_char == '{') ? kJsonObjectConfidence : kNoConfidence;
    }
    return kNoConfidence;
}

} // namespace insight::tokenization
