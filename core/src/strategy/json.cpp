module;
#include "strategy/simdjson_scratch.hpp" // textual: TU-local simdjson entities (ADR-3.D4 family)
#include "utils/log_macros.hpp"          // textual macro layer (ADR-3.D4)
#include <simdjson.h>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan; // fast_gates predicates + sv_* scan primitives

// JsonStrategy — parses structured JSON log lines using simdjson on-demand.
// See detail/simdjson_scratch.hpp for the thread-local zero-alloc scaffolding.

namespace insight::tokenization
{

namespace
{

    // Route the declared OTEL field-map (SRC-D-OTEL-4a) over the parsed OTLP object: severity_number
    // → the LogLevel band (declared > inferred — it runs after the level-string route, so it
    // overrides), and traceId/spanId/parentSpanId → the consumed trace context. Returns true iff
    // the record is OTEL (a severityNumber or traceId was present), which the caller uses to route
    // the message to the nested body.stringValue. Trace ids are hashed to scalar PODs and never
    // retained as values (OR1). Kept separate so JsonStrategy::parse stays within its complexity
    // budget.
    [[nodiscard]] bool extract_otel_fields(simdjson::ondemand::object& root,
                                           ParsedLine& parsed_line)
    {
        bool is_otel{false};
        std::string_view scratch_view;
        // OTLP timeUnixNano → event-time. Not one of the four classified fields, but required so
        // OTEL inputs carry a timestamp and window like any other format (without it the whole
        // pipeline never closes a window). A quoted epoch-nanoseconds string per the OTLP/JSON
        // mapping.
        static constexpr std::string_view kTimeUnixNanoKey{"timeUnixNano"};
        if (try_get_string(root, std::span<const std::string_view>{&kTimeUnixNanoKey, 1},
                           scratch_view))
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

    // Forward decl: parse_otel_span (just below) stores its span_duration_ns ordinal through this,
    // which is defined further down alongside the other ordinal helpers.
    [[nodiscard]] std::span<const OrdinalObservation>
    store_ordinals(std::span<const OrdinalObservation> observations, ArenaAllocator& arena);

    // True iff the raw line is a flat OTLP/JSON span (D-OTEL-10 shape 2 / SRC-D-OTEL-18) — detected by
    // the span-specific startTimeUnixNano key (logs carry timeUnixNano), excluding a resourceSpans
    // DOCUMENT (the record-source unpack handles those before the strategy — SRC-D-OTEL-18). A cheap
    // raw-byte check, no simdjson cursor spent.
    [[nodiscard]] bool is_otel_span_line(std::string_view line) noexcept
    {
        return line.contains(R"("startTimeUnixNano")") && !line.contains(R"("resourceSpans")");
    }

    // Parse an OTLP quoted decimal-ns string → int64. Digit byte-loop (no float, no charconv dep);
    // stops at the first non-digit. Deterministic, cross-stdlib bit-identical.
    [[nodiscard]] std::int64_t parse_span_nano(std::string_view text) noexcept
    {
        std::int64_t value{0};
        for (const char character : text)
        {
            if (character < '0' || character > '9')
                break;
            value = (value * 10) + (character - '0');
        }
        return value;
    }

    // Parse a flat OTLP/JSON span (D-OTEL-10 / SRC-D-OTEL-18) in ONE forward pass over the object — the
    // on-demand idiom that descends into status/attributes inline with no rewind. The §13.1
    // mapping: name→content (the templated operation), startTimeUnixNano→event time, end−start→the
    // span_duration_ns ordinal (SRC-D-OTEL-12, integer ns by construction), status.code→level
    // (ERROR→Error else Info; declared > inferred), service.name (from attributes[])→component (the
    // WHERE tier), traceId/spanId/parentSpanId→the consumed trace context (OR1). `kind` is an
    // ABSENT diagnostic field (SRC-D-OTEL-18b — it needs a categorical-field→value_counts channel
    // canon lacks; not load-bearing for the structural exhibits). O4b Span Links
    // (SRC-D-OTEL-9): copy the collected linked span_ids into arena-stable storage (mirrors
    // store_ordinals). Empty in → empty out (no allocation) so a span without links stays
    // zero-cost.
    [[nodiscard]] std::span<const SpanId> store_span_ids(std::span<const SpanId> ids,
                                                         ArenaAllocator& arena)
    {
        if (ids.empty())
            return {};
        void* const mem{arena.allocate(ids.size() * sizeof(SpanId), alignof(SpanId))};
        auto* const dst{static_cast<SpanId*>(mem)};
        for (std::size_t i{0}; i < ids.size(); ++i)
            dst[i] = ids[i];
        return std::span<const SpanId>{dst, ids.size()};
    }

    // single-pass OTel span-object field dispatch — the branch count is OTel's field set
    // (startTimeUnixNano…status/attributes/links); a coherent deterministic parser whose else-if
    // dispatch is not safely re-expressible as a handler map.
    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    void parse_otel_span(simdjson::ondemand::object& root, ParsedLine& parsed_line,
                         ArenaAllocator& arena)
    {
        std::string_view start_nano;
        std::string_view end_nano;
        std::string_view name_view;
        std::string_view service_name;
        bool is_error{false};
        std::vector<SpanId>
            linked; // O4b Span Links (SRC-D-OTEL-9): the declared cross-trace edge targets

        for (auto field : root)
        {
            std::string_view key;
            if (field.unescaped_key().get(key) != simdjson::SUCCESS)
                continue;
            std::string_view hex;
            if (key == "startTimeUnixNano")
            {
                read_string_or_keep(field.value(), start_nano);
            }
            else if (key == "endTimeUnixNano")
            {
                read_string_or_keep(field.value(), end_nano);
            }
            else if (key == "name")
            {
                read_string_or_keep(field.value(), name_view);
            }
            else if (key == "traceId")
            {
                if (field.value().get_string().get(hex) == simdjson::SUCCESS)
                {
                    parsed_line.trace.present = true;
                    parsed_line.trace.trace_id = trace_id_from_hex(hex);
                }
            }
            else if (key == "spanId")
            {
                if (field.value().get_string().get(hex) == simdjson::SUCCESS)
                    parsed_line.trace.span_id = span_id_from_hex(hex);
            }
            else if (key == "parentSpanId")
            {
                if (field.value().get_string().get(hex) == simdjson::SUCCESS)
                {
                    parsed_line.trace.has_parent = true;
                    parsed_line.trace.parent_span_id = span_id_from_hex(hex);
                }
            }
            else if (key == "status")
            {
                simdjson::ondemand::object status_obj;
                if (field.value().get_object().get(status_obj) == simdjson::SUCCESS)
                {
                    std::string_view code;
                    if (status_obj.find_field_unordered("code").get_string().get(code) ==
                        simdjson::SUCCESS)
                        is_error =
                            code.contains("ERROR"); // STATUS_CODE_ERROR (or the int-2 form's text)
                }
            }
            else if (key == "attributes")
            {
                simdjson::ondemand::array attributes;
                if (field.value().get_array().get(attributes) == simdjson::SUCCESS)
                {
                    for (auto element : attributes)
                    {
                        simdjson::ondemand::object attr;
                        if (element.get_object().get(attr) != simdjson::SUCCESS)
                            continue;
                        std::string_view attr_key;
                        if (attr.find_field_unordered("key").get_string().get(attr_key) !=
                                simdjson::SUCCESS ||
                            attr_key != "service.name")
                            continue;
                        simdjson::ondemand::object value_obj;
                        if (attr.find_field_unordered("value").get_object().get(value_obj) ==
                            simdjson::SUCCESS)
                            read_string_or_keep(value_obj.find_field_unordered("stringValue"),
                                                service_name);
                    }
                }
            }
            else if (key == "links")
            {
                // O4b Span Links (SRC-D-OTEL-9): each link declares a cross-trace edge to another
                // span. Collect the linked span_ids; metalog resolves them (by span_id, across
                // traces) into the distilled service topology (component(this) →
                // component(linked)). The link's trace_id/attributes are consumed-not-retained like
                // the parent context.
                simdjson::ondemand::array links_array;
                if (field.value().get_array().get(links_array) == simdjson::SUCCESS)
                    for (auto element : links_array)
                    {
                        simdjson::ondemand::object link;
                        if (element.get_object().get(link) != simdjson::SUCCESS)
                            continue;
                        std::string_view link_span_hex;
                        if (link.find_field_unordered("spanId").get_string().get(link_span_hex) ==
                            simdjson::SUCCESS)
                            linked.push_back(span_id_from_hex(link_span_hex));
                    }
            }
        }

        // Apply the mapping. Event time + duration are integer ns (D-OTEL-3, by construction).
        // SRC-D-OTEL-11: declared causality → the observed DAG
        parsed_line.trace.is_span = true;
        parsed_line.timestamp = utils::parse_unix_nano_timestamp(start_nano);
        parsed_line.level = is_error ? LogLevel::Error : LogLevel::Info;
        if (!service_name.empty())
            parsed_line.component = arena.store_string(service_name);
        parsed_line.content =
            arena.store_string(name_view.empty() ? parsed_line.raw_line : name_view);

        // span_duration_ns → the declared DurationLog2Ns ordinal (SRC-D-OTEL-12). end < start (skew
        // / absent end) → 0-duration (the smallest bin), never negative.
        const std::int64_t start_value{parse_span_nano(start_nano)};
        const std::int64_t end_value{parse_span_nano(end_nano)};
        const std::int64_t duration_ns{end_value > start_value ? end_value - start_value : 0};
        if (const OrdinalFieldDescriptor* const descriptor{match_ordinal_field("span_duration_ns")})
        {
            const std::array<OrdinalObservation, 1> observation{{{.field_name = descriptor->key,
                                                                  .schedule = descriptor->schedule,
                                                                  .value = duration_ns}}};
            parsed_line.ordinals = store_ordinals(observation, arena);
        }

        // O4b Span Links (SRC-D-OTEL-9): publish the declared cross-trace edge targets (empty ⇒ no
        // allocation).
        parsed_line.linked_span_ids = store_span_ids(linked, arena);
    }

    // Copy the matched ordinal observations (W1, SRC-D-W1-3) into arena-stable storage and return a
    // span over them. Empty in → empty out (no allocation) so a non-ordinal line stays zero-cost.
    // The observations' `field_name` views point at the declared catalog's static keys (stable for
    // the program lifetime), so only the small POD array is arena-copied.
    [[nodiscard]] std::span<const OrdinalObservation>
    store_ordinals(std::span<const OrdinalObservation> observations, ArenaAllocator& arena)
    {
        if (observations.empty())
            return {};
        void* const mem{arena.allocate(observations.size() * sizeof(OrdinalObservation),
                                       alignof(OrdinalObservation))};
        auto* const dst{static_cast<OrdinalObservation*>(mem)};
        for (std::size_t i{0}; i < observations.size(); ++i)
            dst[i] = observations[i];
        return std::span<const OrdinalObservation>{dst, observations.size()};
    }

    // W1 fast-path field-route (SRC-D-W1-3): match the scanner's numeric candidates against the
    // declared catalog; each hit → a consumed ordinal observation. The decimal TEXT → int64 (no
    // float→int).
    [[nodiscard]] std::span<const OrdinalObservation>
    extract_ordinals_fast(const FastJsonResult& fast, ArenaAllocator& arena)
    {
        if (fast.numeric_field_count == 0)
            return {};
        std::array<OrdinalObservation, kFastJsonMaxNumericFields> matched{};
        std::size_t matched_count{0};
        for (std::size_t k{0}; k < fast.numeric_field_count; ++k)
        {
            const FastJsonNumericField& candidate{fast.numeric_fields[k]};
            const OrdinalFieldDescriptor* const descriptor{match_ordinal_field(candidate.key)};
            if (descriptor == nullptr)
                continue;
            const std::optional<std::int64_t> value{
                parse_decimal_scaled(candidate.text, descriptor->scale_to_canonical)};
            if (value)
                matched[matched_count++] = OrdinalObservation{.field_name = descriptor->key,
                                                              .schedule = descriptor->schedule,
                                                              .value = *value};
        }
        return store_ordinals(std::span<const OrdinalObservation>{matched.data(), matched_count},
                              arena);
    }

    // W1 slow-path field-route (SRC-D-W1-3): find_field_unordered per declared key (the OTEL-route
    // pattern); the value's raw decimal TOKEN → int64 (never get_double() — the SRC-D-W1-3 pin). MUST
    // run before the OTLP body descent below (which spends the on-demand cursor).
    [[nodiscard]] std::span<const OrdinalObservation>
    extract_ordinals_slow(simdjson::ondemand::object& root, ArenaAllocator& arena)
    {
        std::array<OrdinalObservation, kOrdinalFieldCatalog.size()> matched{};
        std::size_t matched_count{0};
        for (const auto& descriptor : kOrdinalFieldCatalog)
        {
            simdjson::ondemand::value field;
            if (root.find_field_unordered(descriptor.key).get(field) != simdjson::SUCCESS)
                continue;
            const std::optional<std::int64_t> value{
                parse_decimal_scaled(field.raw_json_token(), descriptor.scale_to_canonical)};
            if (value)
                matched[matched_count++] = OrdinalObservation{
                    .field_name = descriptor.key, .schedule = descriptor.schedule, .value = *value};
        }
        return store_ordinals(std::span<const OrdinalObservation>{matched.data(), matched_count},
                              arena);
    }

    // The escape-free fast path (no simdjson): a single byte scan. Returns nullopt when the scan
    // cannot complete (escapes / nesting / malformed) so the caller falls back to full simdjson.
    // Split out of parse() to keep it within the cognitive-complexity budget (the
    // extract_otel_fields rule).
    [[nodiscard]] std::optional<ParsedLine> try_fast_parse(std::string_view line,
                                                           ArenaAllocator& arena)
    {
        const FastJsonResult fast{try_fast_json(line)};
        if (!fast.has_result)
            return std::nullopt;
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
        parsed_line.ordinals = extract_ordinals_fast(fast, arena); // W1 (SRC-D-W1-3)
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=JSON fast_path component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level),
                          parsed_line.timestamp.has_value());
        return parsed_line;
    }

} // namespace

// the JSON strategy entry — fast-path scan, then guarded simdjson slow path routing to span vs
// log-record parse; a coherent single-responsibility parser whose early-return error handling is
// its structure.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
std::expected<ParsedLine, std::string> JsonStrategy::parse(std::string_view line,
                                                           ArenaAllocator& arena) const
{
    if (line.empty())
        return std::unexpected(std::string("JsonStrategy: empty line"));

    // ── Fast path ─────────────────────────────────────────────────────────────
    // For escape-free JSON objects bypass simdjson entirely (try_fast_parse: single-pass byte scan,
    // no heap alloc); falls back to full simdjson on any anomaly.
    if (std::optional<ParsedLine> fast_parsed{try_fast_parse(line, arena)})
        return std::expected<ParsedLine, std::string>{*std::move(fast_parsed)};
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

    // Flat OTLP/JSON span (D-OTEL-10 shape 2 / SRC-D-OTEL-18): a distinct shape (name / start+end /
    // status / service.name), parsed in its own forward pass. Routed on the raw-byte span signal
    // BEFORE spending the root cursor on the log-record field lookups below.
    if (is_otel_span_line(line))
    {
        parse_otel_span(root, parsed_line, arena);
        return parsed_line;
    }

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

    // ── OTEL/OTLP field-map (ADR-29 SRC-D-OTEL-1, the declared catalog SRC-D-OTEL-4a) ──
    // severity_number → the LogLevel band (declared > inferred) + the trace context, all
    // consumed structural metadata; the trace keys are top-level → never tokenized → dropped
    // from the template by construction (OR1). is_otel routes the message to the nested
    // OTLP body.stringValue (extract_otel_fields keeps parse() within the complexity budget).
    const bool is_otel{extract_otel_fields(root, parsed_line)};

    // W1 ordinal field-route (SRC-D-W1-3) — MUST precede the body descent below (which spends the
    // on-demand cursor); find_field_unordered per declared key, like the OTEL route above.
    parsed_line.ordinals = extract_ordinals_slow(root, arena);

    if (is_otel)
    {
        // OTLP message lives under the nested body.stringValue. MUST be the last root access
        // (it descends into a child). A malformed/absent body → arena-store the whole line.
        if (std::string_view body_value; try_get_otel_body(root, body_value))
            parsed_line.content = arena.store_string(body_value);
        else
            parsed_line.content = arena.store_string(line);
    }
    else
    {
        if (try_get_string(root, kMessageKeys, scratch_view))
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

        // ── Nested-fields fallback (SRC-D-MSK-3) ──────────────────────────────────────
        // App loggers (and LogCraft) nest custom fields under "fields":{…}, so the
        // top-level component/level lookups above miss → the cube WHERE axis goes blind
        // on JSON (bugs.md:27). When either missed, descend ONE level into "fields" and
        // read {component, level} from it. MUST be the LAST root access — get_nested_object
        // descends into a child and the parent cursor cannot rewind to a sibling afterward
        // (so it sits after the top-level message read, and only on the non-OTEL path:
        // OTEL records use resource attributes, not "fields", and already spent the cursor
        // on body). A nested object also forces try_fast_parse to bail, so every
        // nested-fields line reaches this slow path.
        if (parsed_line.component.empty() || parsed_line.level == LogLevel::Unknown)
        {
            if (simdjson::ondemand::object fields_obj;
                get_nested_object(root, "fields", fields_obj))
            {
                if (parsed_line.component.empty() &&
                    try_get_string(fields_obj, kComponentKeys, scratch_view))
                    parsed_line.component = arena.store_string(scratch_view);
                if (parsed_line.level == LogLevel::Unknown &&
                    try_get_string(fields_obj, kLevelKeys, scratch_view))
                    parsed_line.level = utils::parse_log_level(scratch_view);
            }
        }
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
