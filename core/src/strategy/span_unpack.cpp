module;
#include "strategy/simdjson_scratch.hpp" // textual: TU-local simdjson entities (§11.8 family)
#include <simdjson.h>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;

// src/strategy/span_unpack.cpp
//
// OTEL span-export DOCUMENT unpack (insight_otel_epic.md §13, D-OTEL-18 / D-OTEL-18a): the
// record-source layer's 1→N step. An OTLP/JSON `resourceSpans` trace export (shape 1) is walked
// and re-emitted as N CANONICAL flat-span records (shape 2) — byte-form-identical to what the
// LogCraft lab emits for the same spans, so the flat-span parser (json.cpp) is authored ONCE and
// only ever sees shape 2, and shape-1 ≡ shape-2 is a golden-tested property. `IFormatStrategy`
// stays 1:1 (D-OTEL-18): this is a PRE-tokenization unpack, not a 1→N strategy.

namespace insight::tokenization
{

namespace
{

    // OTLP SpanKind int → the canonical string enum (protojson name form, which the lab + the
    // collector file-exporter emit). Out-of-range → INTERNAL (the lab's default).
    [[nodiscard]] std::string_view span_kind_name(std::int64_t kind) noexcept
    {
        switch (kind)
        {
        case 2:
            return "SPAN_KIND_SERVER";
        case 3:
            return "SPAN_KIND_CLIENT";
        case 4:
            return "SPAN_KIND_PRODUCER";
        case 5:
            return "SPAN_KIND_CONSUMER";
        default:
            return "SPAN_KIND_INTERNAL"; // 0 UNSPECIFIED / 1 INTERNAL / unknown → INTERNAL
        }
    }

    // OTLP StatusCode int → the canonical string enum. 2 → ERROR, else UNSET (OK folds to UNSET on
    // the canon side — declared > inferred maps both to Info; §13.1).
    [[nodiscard]] std::string_view status_code_name(std::int64_t code) noexcept
    {
        return code == 2 ? "STATUS_CODE_ERROR" : "STATUS_CODE_UNSET";
    }

    // Read a value that may be an OTLP enum in int OR protojson-string form → the canonical string.
    // Probe type() first (an on-demand value is a single-use cursor — a failed get_string() must
    // not pre-consume it before get_int64()).
    [[nodiscard]] std::string_view read_enum(simdjson::ondemand::value value,
                                             std::string_view (*from_int)(std::int64_t),
                                             std::string_view string_default) noexcept
    {
        simdjson::ondemand::json_type type{};
        if (value.type().get(type) != simdjson::SUCCESS)
            return string_default;
        if (type == simdjson::ondemand::json_type::string)
        {
            std::string_view as_string;
            if (value.get_string().get(as_string) == simdjson::SUCCESS)
                return as_string; // already the SPAN_KIND_* / STATUS_CODE_* name
        }
        else if (type == simdjson::ondemand::json_type::number)
        {
            std::int64_t as_int{};
            if (value.get_int64().get(as_int) == simdjson::SUCCESS)
                return from_int(as_int);
        }
        return string_default;
    }

    // Extract `service.name` from a resource's attributes[] (the declared allowlist, §13.1).
    // Returns an owned copy — the caller reuses it across every span of this resource.
    [[nodiscard]] std::string resource_service_name(simdjson::ondemand::object& resource)
    {
        simdjson::ondemand::array attributes;
        if (resource.find_field_unordered("attributes").get_array().get(attributes) !=
            simdjson::SUCCESS)
            return {};
        for (auto element : attributes)
        {
            simdjson::ondemand::object attr;
            if (element.get_object().get(attr) != simdjson::SUCCESS)
                continue;
            std::string_view key;
            if (attr.find_field_unordered("key").get_string().get(key) != simdjson::SUCCESS ||
                key != "service.name")
                continue;
            simdjson::ondemand::object value_obj;
            if (attr.find_field_unordered("value").get_object().get(value_obj) == simdjson::SUCCESS)
            {
                std::string_view service;
                if (value_obj.find_field_unordered("stringValue").get_string().get(service) ==
                    simdjson::SUCCESS)
                    return std::string{service};
            }
        }
        return {};
    }

    // Append one span object as a canonical flat-span record. Field order + serialization match the
    // lab's fmt_otel_json span seam exactly (D-OTEL-18a): string ids/name/times pass through as
    // their raw JSON (quotes + escaping preserved, byte-faithful); kind/status are normalized to
    // the string enum; service.name (from the resource) is injected first, then the span's own
    // attributes verbatim.
    void append_canonical_span(simdjson::ondemand::object& span, std::string_view service_name,
                               std::string& out)
    {
        std::string_view trace_id{"\"\""};
        std::string_view span_id{"\"\""};
        std::string_view parent_span_id;
        bool has_parent{false};
        std::string_view name{"\"\""};
        std::string_view start_nano{"\"0\""};
        std::string_view end_nano{"\"0\""};
        std::string_view kind{"SPAN_KIND_INTERNAL"};
        std::string_view status{"STATUS_CODE_UNSET"};
        std::string_view span_attributes; // raw JSON of the span's own attributes[] (verbatim)

        for (auto field : span)
        {
            std::string_view key;
            if (field.unescaped_key().get(key) != simdjson::SUCCESS)
                continue;
            if (key == "traceId")
                read_raw_json_or_keep(field.value(), trace_id);
            else if (key == "spanId")
                read_raw_json_or_keep(field.value(), span_id);
            else if (key == "parentSpanId")
                has_parent = field.value().raw_json().get(parent_span_id) == simdjson::SUCCESS;
            else if (key == "name")
                read_raw_json_or_keep(field.value(), name);
            else if (key == "startTimeUnixNano")
                read_raw_json_or_keep(field.value(), start_nano);
            else if (key == "endTimeUnixNano")
                read_raw_json_or_keep(field.value(), end_nano);
            else if (key == "kind")
                kind = read_enum(field.value(), span_kind_name, "SPAN_KIND_INTERNAL");
            else if (key == "status")
            {
                simdjson::ondemand::object status_obj;
                if (field.value().get_object().get(status_obj) == simdjson::SUCCESS)
                {
                    simdjson::ondemand::value code;
                    if (status_obj.find_field_unordered("code").get(code) == simdjson::SUCCESS)
                        status = read_enum(code, status_code_name, "STATUS_CODE_UNSET");
                }
            }
            else if (key == "attributes")
                read_raw_json_or_keep(field.value(), span_attributes);
        }

        out += R"({"traceId":)";
        out += trace_id;
        out += R"(,"spanId":)";
        out += span_id;
        if (has_parent)
        {
            out += R"(,"parentSpanId":)";
            out += parent_span_id;
        }
        out += R"(,"name":)";
        out += name;
        out += R"(,"kind":")";
        out += kind;
        out += R"(","startTimeUnixNano":)";
        out += start_nano;
        out += R"(,"endTimeUnixNano":)";
        out += end_nano;
        out += R"(,"status":{"code":")";
        out += status;
        out += R"("},"attributes":[{"key":"service.name","value":{"stringValue":")";
        out += service_name;
        out += R"("}})";
        // Merge the span's own attributes verbatim after the injected service.name.
        // `span_attributes` is the raw `[...]`; splice its interior in when non-empty.
        if (span_attributes.size() > 2 && span_attributes.front() == '[' &&
            span_attributes.back() == ']')
        {
            const std::string_view inner{span_attributes.substr(1, span_attributes.size() - 2)};
            if (!inner.empty())
            {
                out += ',';
                out += inner;
            }
        }
        out += "]}";
    }

} // namespace

bool is_otel_span_document(std::string_view line) noexcept
{
    return line.contains(R"("resourceSpans")");
}

std::size_t unpack_otel_spans(std::string_view document, std::vector<std::string>& out)
{
    if (!is_otel_span_document(document))
        return 0;

    auto& scratch{json_scratch()};
    const auto padded{load_padded(scratch, document)};
    simdjson::ondemand::document doc;
    if (scratch.parser.iterate(padded).get(doc) != simdjson::SUCCESS)
        return 0;
    simdjson::ondemand::object root;
    if (doc.get_object().get(root) != simdjson::SUCCESS)
        return 0;
    simdjson::ondemand::array resource_spans;
    if (root.find_field_unordered("resourceSpans").get_array().get(resource_spans) !=
        simdjson::SUCCESS)
        return 0;

    std::size_t emitted{0};
    for (auto rs_element : resource_spans)
    {
        simdjson::ondemand::object resource_span;
        if (rs_element.get_object().get(resource_span) != simdjson::SUCCESS)
            continue;
        // resource.service.name FIRST (it precedes scopeSpans in the export; a forward read).
        std::string service_name;
        if (simdjson::ondemand::object resource;
            resource_span.find_field_unordered("resource").get_object().get(resource) ==
            simdjson::SUCCESS)
            service_name = resource_service_name(resource);

        simdjson::ondemand::array scope_spans;
        if (resource_span.find_field_unordered("scopeSpans").get_array().get(scope_spans) !=
            simdjson::SUCCESS)
            continue;
        for (auto ss_element : scope_spans)
        {
            simdjson::ondemand::object scope_span;
            if (ss_element.get_object().get(scope_span) != simdjson::SUCCESS)
                continue;
            simdjson::ondemand::array spans;
            if (scope_span.find_field_unordered("spans").get_array().get(spans) !=
                simdjson::SUCCESS)
                continue;
            for (auto span_element : spans)
            {
                simdjson::ondemand::object span;
                if (span_element.get_object().get(span) != simdjson::SUCCESS)
                    continue;
                std::string line;
                append_canonical_span(span, service_name, line);
                out.push_back(std::move(line));
                ++emitted;
            }
        }
    }
    return emitted;
}

} // namespace insight::tokenization
