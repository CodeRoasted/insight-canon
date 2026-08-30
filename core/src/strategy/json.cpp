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

    // ── The four role-name vocabularies (moved off the class, SRC-D-ECS-1) ─────────────────────
    // These are canon's OWN names for the four roles it reads. They are deliberately NOT a
    // registry of vendor field names, and SRC-D-ECS-1 below is what keeps them that way.
    constexpr std::array<std::string_view, 5> kTimestampKeys{"timestamp", "ts", "@timestamp",
                                                             "time", "datetime"};
    constexpr std::array<std::string_view, 4> kLevelKeys{"level", "severity", "loglevel",
                                                         "log_level"};
    constexpr std::array<std::string_view, 5> kMessageKeys{"message", "msg", "log", "text", "body"};
    constexpr std::array<std::string_view, 5> kComponentKeys{"component", "source", "logger",
                                                             "service", "module"};

    // ── SRC-D-ECS-1 — canon reads two SHAPES of compound key, and ZERO new field names ─────────
    //
    // THE RULE, and the form IS the decision: a compound key is resolved to the name AFTER ITS
    // SINGLE DOT, and that name is matched against the vocabularies above. `log.level` → `level`.
    // `"log":{"level":…}` → `level`. Nothing named `log.level` is added anywhere, and nothing may
    // be.
    //
    // WHY NOT A FIELD NAME, NOT EVEN ONE. Adding `"log.level"` to kLevelKeys would read as the
    // cheaper fix and it is the one that must not happen: it converts a language feature into a
    // vendor dialect, and canon's core tier is not a dialect (ADR-17.D1 — dialects are semantic
    // PACKAGES, composed, never core's business). One vendor name is the back door to an ECS
    // package living in core, and the second name is then unarguable. Learning the SHAPE instead
    // is convergence-proof: it covers pino, Serilog, Bunyan, GELF and OTLP bodies at once, and it
    // costs nothing when the next vocabulary appears because there is no list to extend.
    //
    // ⚠ THE FIELD POSITION ONLY — the NAMESPACE position is REFUSED, and this is a DECLARED
    // LIMITATION, not an oversight (DN-30.D11). `log.level` → `level` and `log.logger` → `logger`
    // resolve because the FIELD segment is a role name. `service.name` does NOT resolve: its role
    // word is the NAMESPACE (`service`), and canon reads that position for nobody.
    //
    // THE REASON IS STRUCTURAL, NOT A TUNING FAILURE. Under every predicate expressible over the
    // four vocabularies above, `service.name` and `source.ip` are the SAME shape — a role-word
    // namespace followed by a field that is in no list. The information that would separate them
    // (`name` denotes an identity, `ip` denotes an address) is not present in the instrument, so
    // no rule written here can admit one and refuse the other.
    //
    // AND THE CONSEQUENCE IS NOT A CARDINALITY RISK — IT IS A CATEGORY ERROR THE STRUCT ALREADY
    // NAMES. `ParsedLine::component` is declared the LOW-CARD FUNCTIONAL SOURCE and is a cube
    // dimension; `host` is the high-card node identity and is deliberately HORS-CUBE. Admitting
    // the namespace position would put `source.ip` — a host-class value — into the field that
    // declares it is not one, on a cube dimension. `host.ip`, `client.address` and
    // `service.node.name` are all ordinary ECS, and that counterexample space is UNMEASURED.
    //
    // Two escapes were considered and refused, recorded so they are not re-proposed: keying on
    // "exactly one string child" FAILS on LogCraft's own ECS (`"service":{"name":…,"type":…}` has
    // two) and ADMITS the trap (`"source":{"ip":…,"port":54321}` has one, `port` being numeric) —
    // wrong on the target and dangerous on the counterexample.
    //
    // REFUSED, NOT UNKNOWN. The reopening condition is a measurement, not an argument: the
    // DN-29.D16 legibility marker can report, from a real stream, the cardinality a
    // namespace-carried resolution would place on the WHERE axis. Until that number exists this
    // stays a boundary — and a red arm is not a licence to add the missing word to a vocabulary.
    //
    // BOUNDED AT EXACTLY ONE LEVEL, both shapes. One dot, one descent. Unbounded descent would
    // make an arbitrarily nested document's `level` field anywhere in the tree a severity claim,
    // which is a different and much weaker statement than "this producer namespaced its top-level
    // fields". The bound is what makes the shape a grammar rather than a search.
    enum class JsonRole : std::uint8_t
    {
        None,
        Timestamp,
        Level,
        Component,
        Message
    };

    [[nodiscard]] constexpr bool name_in(std::string_view name,
                                         std::span<const std::string_view> vocabulary) noexcept
    {
        for (const std::string_view candidate : vocabulary)
            if (name == candidate)
                return true;
        return false;
    }

    // The role a BARE name carries, or None. Order matches parse()'s own precedence.
    [[nodiscard]] constexpr JsonRole role_of(std::string_view name) noexcept
    {
        if (name_in(name, kTimestampKeys))
            return JsonRole::Timestamp;
        if (name_in(name, kLevelKeys))
            return JsonRole::Level;
        if (name_in(name, kComponentKeys))
            return JsonRole::Component;
        if (name_in(name, kMessageKeys))
            return JsonRole::Message;
        return JsonRole::None;
    }

    // Shape 1 (`compound_key_name`) is defined in simdjson_scratch.hpp, NOT here: the escape-free
    // fast scanner classifies keys too, and parse() returns early on a fast-path hit, so the rule
    // must be the same object at both sites or a flat namespaced line silently keeps the old
    // behaviour.

    // Fill ONE role from a compound key's resolved name — but only if that role is still MISSING.
    // The exact-name pass always wins, which is the property that keeps an already-readable line
    // byte-identical: a compound key can add a role, never move one.
    void fill_missing_role(JsonRole role, std::string_view value, ParsedLine& parsed_line,
                           ArenaAllocator& arena, bool& recognized_message)
    {
        switch (role)
        {
        case JsonRole::Timestamp:
            if (!parsed_line.timestamp.has_value())
            {
                parsed_line.timestamp = EventTime::parsed(utils::parse_iso8601(value));
                if (!parsed_line.timestamp)
                    parsed_line.timestamp = EventTime::parsed(utils::parse_bsd_syslog_ts(value));
            }
            break;
        case JsonRole::Level:
            if (parsed_line.level == LogLevel::Unknown)
                parsed_line.level = EventLevel::declared(utils::parse_log_level(value));
            break;
        case JsonRole::Component:
            if (parsed_line.component.empty())
                parsed_line.component = arena.store_string(value);
            break;
        case JsonRole::Message:
            if (!recognized_message)
            {
                parsed_line.content = arena.store_string(value);
                recognized_message = true;
            }
            break;
        case JsonRole::None:
            break;
        }
    }

    // The compound-key pass (SRC-D-ECS-1). ONE forward walk over the root object: each string
    // value is offered under its single-dot resolved name (shape 1), and each object value is
    // descended EXACTLY ONCE with its children offered under their own bare names (shape 2). No key
    // name is consulted to decide whether to descend — the VALUE's type decides, which is what
    // makes this a grammar rather than a list.
    //
    // It subsumes the hard-coded `"fields"` descent this replaces: that was a single vendor name
    // (app loggers and LogCraft nest under `fields`) doing by enumeration what shape 2 now does by
    // structure. Removing it is a name REMOVED from core, not added.
    void route_compound_keys(simdjson::ondemand::object& root, ParsedLine& parsed_line,
                             ArenaAllocator& arena, bool& recognized_message)
    {
        for (auto field : root)
        {
            std::string_view key;
            if (field.unescaped_key().get(key) != simdjson::SUCCESS)
                continue;
            auto value{field.value()};
            simdjson::ondemand::json_type type{};
            if (value.type().get(type) != simdjson::SUCCESS)
                continue;

            if (type == simdjson::ondemand::json_type::object)
            {
                simdjson::ondemand::object child;
                if (value.get_object().get(child) != simdjson::SUCCESS)
                    continue;
                for (auto sub : child)
                {
                    std::string_view sub_key;
                    if (sub.unescaped_key().get(sub_key) != simdjson::SUCCESS)
                        continue;
                    // Bare name only: the descent is the one level, so a dotted key INSIDE it
                    // would be a second level of compounding and is deliberately not read.
                    if (const JsonRole role{role_of(sub_key)}; role != JsonRole::None)
                    {
                        std::string_view sub_value;
                        if (sub.value().get_string().get(sub_value) == simdjson::SUCCESS)
                            fill_missing_role(role, sub_value, parsed_line, arena,
                                              recognized_message);
                    }
                }
                continue;
            }

            if (type != simdjson::ondemand::json_type::string)
                continue;
            if (const JsonRole role{role_of(compound_key_name(key))}; role != JsonRole::None)
            {
                std::string_view text;
                if (value.get_string().get(text) == simdjson::SUCCESS)
                    fill_missing_role(role, text, parsed_line, arena, recognized_message);
            }
        }
    }

    // Route the declared OTEL field-map (SRC-D-OTEL-4a) over the parsed OTLP object:
    // severity_number → the LogLevel band (declared > inferred — it runs after the level-string
    // route, so it overrides), and traceId/spanId/parentSpanId → the consumed trace context.
    // Returns true iff the record is OTEL (a severityNumber or traceId was present), which the
    // caller uses to route the message to the nested body.stringValue. Trace ids are hashed to
    // scalar PODs and never retained as values (OR1). Kept separate so JsonStrategy::parse stays
    // within its complexity budget.
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
                // DECLARED (DN-29.D12 rung 1): OTLP `timeUnixNano` is the LOG record's schema
                // field for when the event happened — not content that resembles a time. This is
                // the second of exactly two declared-time sites, and it is the one that made
                // `is_span` unusable as the marker: an OTLP log record carries a declared time
                // with is_span == false.
                parsed_line.timestamp = EventTime::declared(*timestamp);
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
                    parsed_line.level =
                        EventLevel::declared(log_level_from_severity_number(severity_number));
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

    // L2's rate limit (DN-29.D15). First occurrence, then every Nth: an entirely unreadable stream
    // would otherwise flood the log from the hot path, and the tenth identical line carries nothing
    // the first did not. Same principle as LogParser's failure warning and the n-gram cap notice.
    constexpr std::uint64_t kWarnEveryNRoleless{1000};

    // The witness key L2 puts ON the returned value (DN-29.D16). Returns a view into `line` itself
    // — never into simdjson's padded buffer, which is thread-local scratch the next line
    // overwrites, and never into a local, which would dangle the moment this returns. `line` is the
    // bytes LogParser already made arena-stable before calling parse(), so the view outlives the
    // event.
    //
    // The FIRST top-level key is the witness: extracting it is the same O(1) walk the document
    // probe does (skip whitespace, expect `{`, take the quoted key), it needs no simdjson cursor,
    // and one key that was genuinely present answers "what arrived?" — which is the question a
    // reader has. The full key list is the WARN's job, not the value's.
    [[nodiscard]] std::string_view first_top_level_key(std::string_view line) noexcept
    {
        // Non-empty by construction so the marker is never silently blank on a line that failed to
        // yield one: an empty marker MEANS "roles were recognized", and a blank here would assert
        // exactly the opposite of what happened. Static storage, so the view is always valid.
        static constexpr std::string_view kUnreadable{"<no readable top-level key>"};
        static constexpr std::string_view kWhitespace{" \t\n\r"};
        const std::size_t open{line.find_first_not_of(kWhitespace)};
        if (open == std::string_view::npos || line[open] != '{')
            return kUnreadable;
        const std::size_t quote{line.find('"', open + 1)};
        if (quote == std::string_view::npos)
            return kUnreadable;
        const std::size_t end{line.find('"', quote + 1)};
        if (end == std::string_view::npos || end == quote + 1)
            return kUnreadable;
        // Built from the pointer, not `substr`: `substr` throws when its offset exceeds the
        // size, and a potentially-throwing call inside a `noexcept` body is a `std::terminate`
        // that only a proof rules out. The proof holds here — `end` came back from a find that
        // STARTED at `quote + 1` and is not npos, so `quote + 1 <= end < line.size()` — and this
        // form states it instead of leaving every later reader to re-derive it.
        return std::string_view{line.data() + quote + 1, end - quote - 1};
    }

    // Name the object's top-level keys for L2's diagnostic. COLD PATH ONLY — called after the line
    // is already known to have yielded no role, and behind the rate limit, so a re-walk is free
    // where it matters and never paid where it does not. Uses its OWN parser rather than the
    // thread-local scratch, whose cursor is still held by the caller's live document. Output is
    // bounded in both key count and key length: a diagnostic that can be made arbitrarily large by
    // its own input is a second defect, not an aid.
    [[nodiscard]] std::string top_level_keys_for_diagnosis(std::string_view line)
    {
        constexpr std::size_t kMaxKeys{8};
        constexpr std::size_t kMaxKeyChars{40};
        simdjson::ondemand::parser parser;
        simdjson::padded_string padded{line};
        simdjson::ondemand::document doc;
        if (parser.iterate(padded).get(doc) != simdjson::SUCCESS)
            return "<unparseable>";
        simdjson::ondemand::object root;
        if (doc.get_object().get(root) != simdjson::SUCCESS)
            return "<not a JSON object>";
        std::string out;
        std::size_t shown{0};
        for (auto field : root)
        {
            std::string_view key;
            if (field.unescaped_key().get(key) != simdjson::SUCCESS)
                continue;
            if (shown == kMaxKeys)
            {
                out += ", ...";
                break;
            }
            if (shown > 0)
                out += ", ";
            out += key.substr(0, kMaxKeyChars);
            ++shown;
        }
        return out;
    }

    // Forward decl: parse_otel_span (just below) stores its span_duration_ns ordinal through this,
    // which is defined further down alongside the other ordinal helpers.
    [[nodiscard]] std::span<const OrdinalObservation>
    store_ordinals(std::span<const OrdinalObservation> observations, ArenaAllocator& arena);

    // True iff the raw line is a flat OTLP/JSON span (D-OTEL-10 shape 2 / SRC-D-OTEL-18) — detected
    // by the span-specific startTimeUnixNano key (logs carry timeUnixNano). A cheap raw-byte check,
    // no simdjson cursor spent.
    //
    // It does NOT exclude a resourceSpans EXPORT DOCUMENT, and that is not an omission: parse()
    // refuses one above, before this ever runs, so by the time control arrives here a document is
    // already impossible. The exclusion used to live here as `&& !line.contains("resourceSpans")`
    // and was dropped as REDUNDANT, not as expensive — `&&` short-circuits, so on a non-OTEL line
    // the first scan already returned false and the second one never ran at all. It cost only the
    // lines that genuinely carry `startTimeUnixNano`. Recorded because the opposite was believed,
    // and measured false.
    //
    // The ordering in parse() is what the removal rests on, and it is load-bearing: this predicate
    // is true of a document too, because a document carries `startTimeUnixNano` inside its spans.
    [[nodiscard]] bool is_otel_span_line(std::string_view line) noexcept
    {
        return line.contains(R"("startTimeUnixNano")");
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

    // Parse a flat OTLP/JSON span (D-OTEL-10 / SRC-D-OTEL-18) in ONE forward pass over the object —
    // the on-demand idiom that descends into status/attributes inline with no rewind. The §13.1
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
    // ── CONFIRM OR DECLINE (DN-29.D17) — returns false when the nomination was wrong ───────────
    // A cheap probe NOMINATES; the parser it routes to CONFIRMS its own precondition or DECLINES.
    // `is_otel_span_line` is a raw byte scan and matches `startTimeUnixNano` at ANY DEPTH; this
    // walk matches at DEPTH 0 ONLY. On an export document the two disagree — the key is real but
    // nested inside the spans — and a `void` return had no way to say so, so the caller emitted a
    // confident record for a line it had not parsed.
    //
    // Declining is NOT an exclusion rule for documents. A key-name denylist names one intruder and
    // rots on the next envelope; this enumerates nothing, so it holds for every probe, parser and
    // format that ever routes here. The rule is "do not emit what you did not parse."
    [[nodiscard]] bool parse_otel_span(simdjson::ondemand::object& root, ParsedLine& parsed_line,
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

        // THE CONFIRMATION, and it is free: `start_nano` is empty exactly when the key the probe
        // promised is not at depth 0. Checked BEFORE the mapping below, because applying a mapping
        // over fields that were never found is how an unparsed line acquires a plausible event
        // time, an Info level and a content fallback — the confident record this ruling ends.
        // A span with no declared event time is not a span we parsed (ADR-29.D5's corollary).
        if (start_nano.empty())
            return false;

        // Apply the mapping. Event time + duration are integer ns (D-OTEL-3, by construction).
        // SRC-D-OTEL-11: declared causality → the observed DAG
        parsed_line.trace.is_span = true;
        // DECLARED (DN-29.D12 rung 1): `startTimeUnixNano` is the producer's own statement of
        // when the span began. Rung 1 is why this must outrank a transport stamp, which a merely
        // PARSED time does not.
        if (const auto declared_start{utils::parse_unix_nano_timestamp(start_nano)})
            parsed_line.timestamp = EventTime::declared(*declared_start);
        else
            parsed_line.timestamp = EventTime::parsed(std::nullopt);
        parsed_line.level = EventLevel::declared(is_error ? LogLevel::Error : LogLevel::Info);
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
        return true;
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
    // pattern); the value's raw decimal TOKEN → int64 (never get_double() — the SRC-D-W1-3 pin).
    // MUST run before the OTLP body descent below (which spends the on-demand cursor).
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
            parsed_line.timestamp = EventTime::parsed(utils::parse_iso8601(fast.timestamp_str));
            if (!parsed_line.timestamp)
                parsed_line.timestamp =
                    EventTime::parsed(utils::parse_bsd_syslog_ts(fast.timestamp_str));
        }
        if (!fast.level_str.empty())
            parsed_line.level = EventLevel::declared(utils::parse_log_level(fast.level_str));
        if (!fast.component_str.empty())
            parsed_line.component = arena.store_string(fast.component_str);
        parsed_line.content = fast.message_str.empty() ? arena.store_string(line)
                                                       : arena.store_string(fast.message_str);
        parsed_line.ordinals = extract_ordinals_fast(fast, arena); // W1 (SRC-D-W1-3)
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=JSON fast_path component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level.value()),
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

    // An OTLP `resourceSpans` EXPORT DOCUMENT is REFUSED here, and the refusal is the feature.
    //
    // Without it the document falls through to the generic log-record route below and yields ONE
    // plausible-looking event for an export carrying N spans — no error, no diagnostic. That is
    // the silent-wrong-answer class ADR-29.D5 names as the one failure this product may not ship,
    // and a refusal a caller can see is strictly better than a number a caller cannot doubt.
    //
    // Refused rather than unpacked because document mode is ACQUISITION-tier, never record-tier:
    // this entry is frame-oriented (the SHM plane carries a fixed 4096-byte payload) and an export
    // has no declared size, so unpacking here would put an unbounded object inside a
    // bounded-memory instrument — the same refusal ADR-29.D3 makes about retaining a `trace_id`.
    // A collector-shaped acquisition fans the export out and puts flat SPANS on the wire, which is
    // what ADR-29.D7's "a span enters as a record" already says.
    //
    // ⚠ THIS MUST PRECEDE is_otel_span_line: that predicate tests only for `startTimeUnixNano`,
    // and a document carries that key inside its spans.
    if (is_otel_span_document(line))
        return std::unexpected(
            std::string("JsonStrategy: OTLP resourceSpans export DOCUMENT on the record-oriented "
                        "entry — document mode is acquisition-tier; this wire carries one flat "
                        "span per record (ADR-29.D7)"));

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
        if (parse_otel_span(root, parsed_line, arena))
            return parsed_line;

        // DECLINED (DN-29.D17): the probe nominated on a raw byte scan, the parser looked at depth
        // 0 and the promised key was not there. Return the line to the chain instead of emitting a
        // record we did not parse — the generic path below then recognizes no role and L2 marks it,
        // which is the whole reason the marker was reachable at all on this input.
        //
        // Discard whatever the walk wrote. It matches at depth 0 only, so on an export document it
        // writes nothing — but a line with a depth-0 `traceId` and no depth-0 `startTimeUnixNano`
        // would leave a half-populated trace context, and a declined parse must leave NO trace of
        // itself. Cheap, and it makes the decline total rather than nearly total.
        parsed_line = ParsedLine{};
        parsed_line.raw_line = line;

        // The walk spent the on-demand cursor, which is forward-only and cannot rewind, so the
        // generic path below needs a fresh one. Re-iterating is a second parse of this line and it
        // is paid ONLY here — on a line that was mis-nominated, which is rare by construction. The
        // alternative (pre-checking depth 0 before the walk) costs every genuine span a second
        // pass to spare the exceptional one.
        if (scratch.parser.iterate(padded).get(doc) != simdjson::SUCCESS ||
            doc.get_object().get(root) != simdjson::SUCCESS)
        {
            INSIGHT_LOG_TRACE(logging::strategy_logger(),
                              "strategy=JSON span decline: re-iterate failed");
            return std::unexpected(
                std::string("JsonStrategy: span nomination declined and the line could not be "
                            "re-read for the generic path"));
        }
    }

    std::string_view scratch_view;
    // The fourth role probe's own result. The other three are readable off `parsed_line`, but
    // `content` is set on BOTH branches — a recognized message and the raw-line fallback are
    // indistinguishable afterwards, and treating the fallback as a role is exactly what would make
    // L2 below silent on the input it exists for.
    bool recognized_message{false};

    if (try_get_string(root, kTimestampKeys, scratch_view))
    {
        parsed_line.timestamp = EventTime::parsed(utils::parse_iso8601(scratch_view));
        if (!parsed_line.timestamp)
            parsed_line.timestamp = EventTime::parsed(utils::parse_bsd_syslog_ts(scratch_view));
    }

    if (try_get_string(root, kLevelKeys, scratch_view))
        parsed_line.level = EventLevel::declared(utils::parse_log_level(scratch_view));

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
            recognized_message = true;
        }
        else
        {
            // Fallback: arena-store the original line. We avoid re-serialising the
            // document (which would force a heap allocation); the raw bytes are
            // already stable when the caller is LogParser.
            parsed_line.content = arena.store_string(line);
        }

        // ── The COMPOUND-KEY pass (SRC-D-ECS-1) — replaces the hard-coded "fields" descent ─────
        // Runs only when a role is still missing after the exact-name lookups above, so an
        // already-readable line never reaches it and cannot be changed by it. That is what makes
        // this additive: it closes a blindness, it does not re-decide anything.
        //
        // The predecessor descended into a literally-named `"fields"` object (app loggers and
        // LogCraft nest there), which read ONE producer's namespacing convention by enumeration.
        // Shape 2 does it by structure and therefore also covers `log`, `service`, `host`, and
        // every namespace a producer invents — while shape 1 covers the same producers' flat
        // dotted spelling. Net effect on core's vocabulary: one name REMOVED, none added.
        //
        // A fresh cursor is required and it is not free: the on-demand cursor is forward-only and
        // the lookups above spent it. Paid ONLY on a line that is still missing a role — which is
        // exactly the population this exists for, and never on a canon-named line.
        if (parsed_line.component.empty() || parsed_line.level == LogLevel::Unknown ||
            !parsed_line.timestamp.has_value() || !recognized_message)
        {
            simdjson::ondemand::document compound_doc;
            simdjson::ondemand::object compound_root;
            if (scratch.parser.iterate(padded).get(compound_doc) == simdjson::SUCCESS &&
                compound_doc.get_object().get(compound_root) == simdjson::SUCCESS)
                route_compound_keys(compound_root, parsed_line, arena, recognized_message);
        }
    }

    // ── L2 — the independent backstop against a SILENT wrong answer (DN-29.D15) ─────────────
    //
    // L1 above (is_otel_span_document) is a hard refusal, but it can only refuse what it
    // RECOGNISES, and it recognises via a dated premise about OTLP's top-level key. Nothing in this
    // repo can re-derive that premise — no opentelemetry-proto is vendored anywhere — so L1 alone
    // would leave a schema change reading as a clean parse. L2 is the layer that cannot be defeated
    // by the same change, because it knows nothing about OTLP at all.
    //
    // THE PREDICATE IS ALREADY COMPUTED. By this point all four role probes have run, so "this
    // object yielded no role we understand" costs one integer test. There is no needle, no window
    // constant and no second pass on the ~100% of lines that yield a role. Naming the keys DOES
    // cost a re-walk, and it is paid only on the diagnostic path — i.e. only once we already know
    // the line was not understood.
    //
    // WHAT IT GUARANTEES, EXACTLY — the record path never SILENTLY emits a canonical event for
    // input it understood nothing of. It does NOT promise to recognise a non-canonical export;
    // it promises not to be silent about one. That is DN-29.D6(b)'s actual requirement (the word
    // is "silently"), and it holds for a re-ordered resourceSpans, for a resourceLogs envelope,
    // and for OTLP shapes that do not exist yet.
    //
    // A MARKER, NOT A REFUSAL. A role-less JSON object is not necessarily a document — it may be an
    // application record whose vocabulary we do not read yet, which is the subject DN-030 exists to
    // improve. Refusing it would convert a reading gap into data loss, and would foreclose reading
    // it better later. So the line is emitted, analysed, and MARKED.
    //
    // THE DISCHARGE IS THIS ASSIGNMENT, NOT THE LOG LINE BELOW. A WARN desilences the console; a
    // caller consuming parse() would still receive a value indistinguishable from a well-parsed
    // record, which by DN-30.O1's verb 2 is a precision-first defect rather than a declared
    // limitation. It is also untestable here — canon has no test-observable log sink, so an arm
    // written against a log line goes green the day this code is deleted (the can't-FAIL row of
    // MEM:synthetic-gate-vacuity-vs-judgment).
    if (!is_otel && !parsed_line.timestamp.has_value() && parsed_line.level == LogLevel::Unknown &&
        parsed_line.component.empty() && !recognized_message)
    {
        // ⚠ UNCONDITIONAL, AND IT MUST STAY ABOVE THE RATE LIMIT BELOW. The marker is set on EVERY
        // role-less record; the sampling governs ONLY the log emission. Moving this inside the
        // `if` would leave 999 of every 1000 role-less events carrying an EMPTY marker —
        // indistinguishable to a consumer from a well-parsed record, which is precisely the
        // console-desilenced / contract-mute state DN-29.D16 was ruled to end. Two statements about
        // one condition, and only the console's may be sampled.
        parsed_line.no_role_witness_key = first_top_level_key(line);

        // ERGONOMICS, never the contract, and no test may assert against it. Rate-limited on the
        // precedent in close_metalog_window_at: the n-gram cap warns once per WINDOW, not once per
        // drop, because a per-event warn floods from the hot path. Thread-local because `parse` is
        // const and a strategy instance is per-tokenizer, i.e. per thread.
        //
        // CONSEQUENCE OF THE THREAD-LOCAL, so nobody builds on it: "first occurrence" fires once
        // per WORKER, so diagnostic VOLUME varies with worker count. That is fine for a log and it
        // is not deterministic content — but it means no test may ever assert on the number of
        // lines emitted here. Assert on the marker, which is per-record and exact.
        thread_local std::uint64_t roleless_count{0};
        ++roleless_count;
        if (roleless_count == 1 || roleless_count % kWarnEveryNRoleless == 0)
            INSIGHT_LOG_WARN(logging::strategy_logger(),
                             "JSON object yielded NO recognized role (no timestamp, level, "
                             "component or message) — the event is emitted and MARKED, not "
                             "dropped. Top-level keys: [{}] (total such lines={})",
                             top_level_keys_for_diagnosis(line), roleless_count);
    }

    INSIGHT_LOG_TRACE(logging::strategy_logger(),
                      "strategy=JSON parsed component={} level={} has_timestamp={}",
                      parsed_line.component, to_string(parsed_line.level.value()),
                      parsed_line.timestamp.has_value());
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
