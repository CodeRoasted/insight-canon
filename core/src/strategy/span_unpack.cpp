module;
#include "strategy/simdjson_scratch.hpp"
#include <simdjson.h>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;

// invariant: the record-source layer's one-to-N step — an OTLP export document is walked and
// re-emitted as N CANONICAL flat-span records.
// invariant: byte-form-identical to what the lab emits for the same spans, so the flat-span parser
// is authored ONCE and only ever sees the flat shape.
// invariant: the strategy interface stays one-to-one: this is a PRE-tokenization unpack, not a
// one-to-N strategy.
// invariant: the simdjson entities stay TEXTUAL in the global module fragment and are TU-local, so
// no third-party declaration leaks through the module.
// refs: ADR-3.D4, ADR-29, SRC-D-OTEL-18, SRC-D-OTEL-18a
namespace insight::tokenization
{

namespace
{

    // post: the canonical string enum name for a span-kind integer; out of range yields the
    // internal kind, which is the lab's default.
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
            return "SPAN_KIND_INTERNAL";
        }
    }

    // post: the canonical string enum name for a status-code integer — only the error code maps,
    // and everything else folds to unset.
    // invariant: the OK code folds to unset because declared-outranks-inferred maps both to the
    // same canon level.
    [[nodiscard]] std::string_view status_code_name(std::int64_t code) noexcept
    {
        return code == 2 ? "STATUS_CODE_ERROR" : "STATUS_CODE_UNSET";
    }

    // post: the canonical string for a value that may be an enum in integer OR string form.
    // invariant: the type is probed FIRST, because an on-demand value is a single-use cursor and a
    // failed string read must not pre-consume it before the integer read.
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
                return as_string;
        }
        else if (type == simdjson::ondemand::json_type::number)
        {
            std::int64_t as_int{};
            if (value.get_int64().get(as_int) == simdjson::SUCCESS)
                return from_int(as_int);
        }
        return string_default;
    }

    // post: an OWNED copy of the resource's service name, because the caller reuses it across every
    // span of that resource.
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

    // invariant: field order and serialization match the lab's own span seam EXACTLY — string
    // ids, name and times pass through as their raw JSON, quotes and escaping byte-preserved.
    // invariant: kind and status are normalized to the string enum; the resource service name is
    // injected FIRST, then the span's own attributes verbatim.
    // refs: SRC-D-OTEL-18a
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
        std::string_view span_attributes;
        std::string_view span_links;

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
        // invariant: a span link is the DECLARED cross-trace edge — the argument the whole OTEL
        // subject was minted on — so it is the ONE field this unpack may not drop.
        // invariant: dropping it made the document path silently delete the fact that distinguishes
        // this subject from a dialect, and the byte-equivalence golden went green over it.
        // invariant: the golden went green because its fixture carried no link, which is the shape
        // of a gate that cannot fail on the thing it exists for.
        // invariant: carried VERBATIM and positioned after the parent id and before the name,
        // because that is where the lab writes it — a different slot is not byte-equivalence.
        // invariant: empty means nothing written, matching the lab's own rule, so a span without
        // links stays byte-identical to the pre-links form.
        // refs: ADR-29.D1, ADR-29.D2, DN-29.D7
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
        // invariant: the span's own attributes are merged VERBATIM after the injected service name,
        // by splicing the raw array's interior when it is non-empty.
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

/***************************************************************************************************
D-LSRC-16 — the OTLP export probe compares a first key, and its cost and premise are stated here
THE RULE. The probe compares the root object's FIRST KEY against a closed set of accepted keys. It
does not SEARCH the line for a key, and it does not take a bounded window either. The accepted set
is a SET and not one hardcoded compare, because JSON key order is not semantically significant: "the
first key is the export field" is a statement about what PRODUCERS emit, never about what the format
guarantees.

THE COST, WITH THE WORKLOAD IT IS A FRACTION OF. A percentage is not a measurement without its
denominator, so the denominator is stated first: tokenizing the non-OTEL nested-JSON benchmark
workload, 1000 lines per iteration, 2 independent builds times 7 repetitions, clang-21 and libc++
release, one box. That population - structured JSON that reaches the simdjson slow path and carries
no OTEL field - is the one charged for this probe, and the figures below are percentages of ITS
per-line tokenize cost, not of a window, a pipeline stage or the product path. Head with no probe on
this path: 1286 and 1321 microseconds per 1000 lines. A bounded WINDOW search plus the refusal:
1440, which is +10.5 percent and the WORST arm, because its needle begins with a quote and
structured JSON is dense in quotes, so a naive substring search restarts on nearly every byte of the
window. The first-key COMPARE plus the refusal: 1354 and 1363, which is +4.2 percent - two
whitespace skips and a bounded prefix compare, with no window constant to justify.

IT WAS ACCEPTED ON JUDGMENT AND MAY NEVER BE CALLED WITHIN BUDGET. +4.2 percent is above this
bench's build-to-build spread of about 2.7 percent, measured by rebuilding head twice, so it is a
real cost, paid to convert a silent wrong answer into a refusal. No threshold was pre-registered:
the measurement ran AFTER the rewrite, so there was no declared budget for it to come in under, and
calling it within budget would invent one after the fact.

THE PREMISE, DATED, because an assumed premise is not auditable and a dated one is. CLAIM: the OTLP
trace-export request message carries exactly ONE top-level field, so in any export of that message
the export field is the first key. ESTABLISHED: 2026-08-06, from the OTLP wire contract as known to
the author. NOT ESTABLISHED BY a fetched or vendored schema artifact - no protocol definition is
vendored anywhere in this workspace, so nothing in-repo can re-derive or re-check this. Stated so a
reader weighs the claim by its actual evidence rather than by its confidence.

WHAT BREAKS IF THE SCHEMA GROWS A SIBLING FIELD AND A PRODUCER EMITS IT FIRST: this probe stops
claiming a real export, so the hard refusal does not fire. It does NOT follow that the export is
then read silently, and that is the whole point of the layering. Layer 1 is this predicate,
pre-parse on the record path, and it REFUSES. Layer 2 is zero recognized roles, post-parse in the
JSON strategy, and it WARNS naming the keys it saw. Layer 3 is the broad acquisition-side
recogniser, and it UNPACKS. Layer 2 does not depend on this premise AT ALL, so the same schema
change cannot defeat both, and a stale premise degrades to lost capability with a log line, never a
wrong answer. All three layers are live: the acquisition entry consumes layer 3 today, so a
non-canonical export IS unpacked on the file and CLI path. Adding a member to the set is a one-line
fix, and it changes only which layer catches it, never whether one does.

DO NOT VENDOR A PROTOCOL DEFINITION TO FIX THE UNVERIFIABLE PREMISE. That takes a real dependency to
support a comment, and layer 2 already removes the consequence that would justify it.
***************************************************************************************************/
// refs: DN-29.D9, ADR-29.D7, DN-29.D15, ADR-22.D5
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
    // invariant: built from the pointer for the reason the sibling witness-key helper states — a
    // substring can throw and this body is noexcept.
    // invariant: the key came back from a non-npos find, so it is within the line and the tail is
    // well-formed.
    const std::string_view first_key{line.data() + key, line.size() - key};
    for (const std::string_view accepted : kExportFirstKeys)
        if (first_key.starts_with(accepted))
            return true;
    return false;
}

// invariant: one coherent traversal of the export nesting, emitting one line per span; splitting
// the nested walk would fragment a single-responsibility unpacker.
// invariant: the ACQUISITION-side check below is broad and deliberately OVER-triggering, and it
// must NOT be the record path's O(1) first-key compare.
// invariant: the asymmetry is the whole ruling: the record path is charged for its probe on every
// JSON line of every stream, so it is tuned for PRECISION and pays with a dated premise.
// invariant: this path is reached only once something already decided to hand it a whole document,
// so thoroughness is free and RECALL is what matters.
// invariant: the error directions are OPPOSITE, which is why one predicate could not serve both.
// invariant: here a false positive costs one walk that finds nothing, while a false negative is a
// conformant export silently not unpacked.
// invariant: an unbounded scan is the right instrument in exactly this place and the wrong one in
// the other; the cost that condemns it on the record path is not charged here.
// invariant: it is also what makes the closed set's dated premise survivable — if the schema
// moves, layer 1 stops recognising, layer 2 marks the line, and THIS still unpacks.
// invariant: the acquisition entry is its live consumer, so that sentence is STATE and not
// intention.
// refs: ADR-22.D5, DN-29.D6, DN-29.D15
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
        // invariant: the resource service name is read FIRST because it precedes the scope spans in
        // the export, so the walk stays forward-only.
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
