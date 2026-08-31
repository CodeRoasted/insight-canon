module;
#include "strategy/simdjson_scratch.hpp" // textual: TU-local simdjson entities (ADR-3.D4 family)
#include <simdjson.h>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;

// src/strategy/span_unpack.cpp
//
// OTEL span-export DOCUMENT unpack (ADR-29, SRC-D-OTEL-18 / SRC-D-OTEL-18a): the
// record-source layer's 1→N step. An OTLP/JSON `resourceSpans` trace export (shape 1) is walked
// and re-emitted as N CANONICAL flat-span records (shape 2) — byte-form-identical to what the
// LogCraft lab emits for the same spans, so the flat-span parser (json.cpp) is authored ONCE and
// only ever sees shape 2, and shape-1 ≡ shape-2 is a golden-tested property. `IFormatStrategy`
// stays 1:1 (SRC-D-OTEL-18): this is a PRE-tokenization unpack, not a 1→N strategy.

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
    // lab's fmt_otel_json span seam exactly (SRC-D-OTEL-18a): string ids/name/times pass through as
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
        std::string_view span_links;      // raw JSON of the span's links[] (verbatim)

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
            else if (key == "links")
                read_raw_json_or_keep(field.value(), span_links);
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
        // ── Span Links: the one field this unpack may not drop (DN-29.D7) ──────────────────────
        // A Span Link is the DECLARED cross-trace Régime-B edge (ADR-29.D2) — the argument the
        // whole OTEL subject was minted on (ADR-29.D1). Dropping it here made the document path
        // silently delete the fact that distinguishes this subject from a dialect, and the
        // byte-equivalence golden went green over it because its fixture carried no link.
        //
        // Carried VERBATIM, and positioned after parentSpanId / before name because that is where
        // the lab writes it (logcraft `fmt_otel_span`) — the flat-span parser is authored once
        // against the lab's byte form, so a re-serializer that carries links in a different slot
        // is not shape-1 ≡ shape-2. Empty ⇒ nothing written, matching the lab's own
        // empty-links rule, so a span without links stays byte-identical to the pre-links form.
        if (span_links.size() > 2 && span_links.front() == '[' && span_links.back() == ']')
        {
            out += R"(,"links":)";
            out += span_links;
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

// The probe COMPARES the root object's first key against a closed set; it does not SEARCH the line
// for a key. That is a measured choice, and BOTH its numbers belong here — the rejected form's and
// the accepted form's.
//
// THE BASELINE ALL THREE FIGURES ARE A FRACTION OF: `Tokenizer::process_line` over the non-OTEL
// nested-JSON workload (`BM_TokenizationThroughputNestedJson`, insight-canon/benchmarks), 1000 lines per
// iteration, 2 independent builds x 7 repetitions, clang-21/libc++ release, one box. That
// population — structured JSON that reaches the simdjson slow path and carries no OTEL field — is
// the one charged for this probe, and the percentages below are of ITS per-line tokenize cost,
// not of a window, a pipeline stage, or the product path.
//
//   HEAD, no probe on this path      1286 / 1321 us per 1000 lines   (stddev 14.9 / 11.7)
//   bounded WINDOW search + refusal        1440 us                   (stddev 12.9)  +10.5%
//   first-key COMPARE + refusal      1354 / 1363 us                  (stddev 12.1 / 7.4)  +4.2%
//
//   * The WINDOW form — `substr(0, 256).contains("\"resourceSpans\"")` — meets ADR-29.D7's letter
//     and is the worst arm. Its needle begins with `"` and structured JSON is dense in `"`, so a
//     naive substring search restarts on nearly every byte of the window.
//   * Comparing the first key is O(1): two whitespace skips and a bounded prefix compare, with no
//     window constant to justify.
//
// THE ACCEPTED FORM IS NOT FREE, AND IT WAS ACCEPTED ON JUDGMENT. +4.2% is above this bench's
// build-to-build spread (~2.7%, measured by rebuilding HEAD twice), so it is a real cost, paid to
// convert a silent wrong answer into a refusal. No threshold was pre-registered for it — the
// measurement ran AFTER the rewrite, so there was no declared budget for it to come in under, and
// calling it "within budget" would invent one after the fact.
//
// ── The closed set, its DATED premise, and what breaks when OTLP grows a top-level field ─────
//
// JSON object key order is NOT semantically significant. "The first key is resourceSpans" is
// therefore a statement about what PRODUCERS emit, never about what the format guarantees — which
// is exactly why this is a closed SET rather than one hard-coded compare.
//
// THE PREMISE, DATED, because an assumed premise is not auditable and a dated one is (DN-29.D15):
//   * Claim: OTLP's `ExportTraceServiceRequest` carries exactly ONE top-level field
//     (`resource_spans`), so in any export of that message `"resourceSpans"` is the first key.
//   * Established: 2026-08-06, from the OTLP wire contract as known to the author.
//   * NOT established by: a fetched or vendored schema artifact. No opentelemetry-proto is
//     vendored anywhere in this workspace, so nothing in-repo can re-derive or re-check this.
//     Stated so a reader weighs the claim by its actual evidence rather than by its confidence.
//
// WHAT BREAKS IF OTLP ADDS A SIBLING FIELD AND A PRODUCER EMITS IT FIRST: this probe (L1) stops
// claiming a real export, so the hard refusal does not fire. It does NOT follow that the export is
// then read silently, and that is the whole point of DN-29.D15's layering:
//
//   L1  record, pre-parse   this predicate, against the dated set   hard REFUSAL
//   L2  record, post-parse  zero recognized roles (json.cpp)        WARN naming the keys seen
//   L3  acquisition         broad, over-triggering                  UNPACK
//
// L2 does not depend on this premise AT ALL — it knows nothing about OTLP, so the same schema
// change cannot defeat both. A stale premise therefore degrades to: L1 stops recognising, L2
// diagnoses, L3 still unpacks. Lost capability with a log line, never a wrong answer.
//
// ALL THREE LAYERS ARE LIVE. L3's consumer is the acquisition entry
// (Tokenizer::unpack_span_document → InsightPipeline::ingest_line, DN-29.D6(a)), so the sentence
// above is STATE and no longer intention: a non-canonical export IS unpacked on the file/CLI path
// today. Adding a member here is a one-line fix if OTLP moves, and it changes only which layer
// catches it — never whether one does.
//
// DO NOT VENDOR AN opentelemetry-proto TO "FIX" THE UNVERIFIABLE PREMISE. That takes a real
// dependency to support a comment, and L2 already removes the consequence that would justify it.
constexpr std::array<std::string_view, 1> kExportFirstKeys{R"("resourceSpans")"};

bool is_otel_span_document(std::string_view line) noexcept
{
    static constexpr std::string_view kWhitespace{" \t\n\r"};
    const std::size_t open{line.find_first_not_of(kWhitespace)};
    if (open == std::string_view::npos || line[open] != '{')
        return false;
    const std::size_t key{line.find_first_not_of(kWhitespace, open + 1)};
    if (key == std::string_view::npos)
        return false;
    // Built from the pointer for the reason json.cpp's `first_top_level_key` states: `substr`
    // can throw and this body is `noexcept`. `key` came back from a non-npos find, so
    // `key < line.size()` and the tail is well-formed.
    const std::string_view first_key{line.data() + key, line.size() - key};
    for (const std::string_view accepted : kExportFirstKeys)
        if (first_key.starts_with(accepted))
            return true;
    return false;
}

// one coherent traversal of the OTLP resourceSpans→scopeSpans→spans nesting emitting one line per
// span; splitting the nested walk fragments a single-responsibility unpacker.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
// ── L3 (DN-29.D15) — the ACQUISITION-side check: broad, and deliberately over-triggering ───────
// It must NOT be L1's O(1) first-key compare, and the asymmetry is the whole ruling. The record
// path is charged for its probe on every JSON line of every stream, the overwhelming majority of
// which are not OTEL, so it is tuned for PRECISION and pays for it with a dated premise about key
// order. This path is reached only once something already decided to hand it a whole document; it
// holds the entire input by definition (ADR-22.D5) and is not the hot path, so thoroughness is
// free and RECALL is what matters.
//
// The error directions are opposite, which is why one predicate could not serve both: here a false
// positive costs one walk that finds nothing and returns 0, while a false negative is a conformant
// export silently not unpacked. An unbounded scan is the right instrument in exactly this place and
// the wrong one in the other — the cost that condemns it on the record path is not charged here.
//
// It is also what makes the closed set's dated premise survivable: if OTLP grows a top-level field
// and a producer emits it first, L1 stops recognising, L2 marks the line, and THIS still unpacks.
[[nodiscard]] bool is_otel_span_document_broad(std::string_view document) noexcept
{
    return document.contains(R"("resourceSpans")");
}

std::size_t unpack_otel_spans(std::string_view document, std::vector<std::string>& out)
{
    if (!is_otel_span_document_broad(document))
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
