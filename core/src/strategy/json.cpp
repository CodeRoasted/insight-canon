module;
#include "strategy/simdjson_scratch.hpp"
#include "utils/log_macros.hpp"
#include <simdjson.h>

module insight.canon.detail.strategy;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.scan;

// invariant: structured JSON parsed with simdjson on-demand; the thread-local zero-alloc
// scaffolding lives in the shared scratch header.
// invariant: the simdjson entities and the log macros stay TEXTUAL in the global module fragment
// and are TU-local, so no third-party declaration leaks through the module.
// refs: ADR-3.D4
namespace insight::tokenization
{

namespace
{

    // invariant: these are canon's OWN names for the four roles it reads, and they are deliberately
    // NOT a registry of vendor field names.
    // refs: SRC-D-ECS-1
    constexpr std::array<std::string_view, 5> kTimestampKeys{"timestamp", "ts", "@timestamp",
                                                             "time", "datetime"};
    constexpr std::array<std::string_view, 4> kLevelKeys{"level", "severity", "loglevel",
                                                         "log_level"};
    constexpr std::array<std::string_view, 5> kMessageKeys{"message", "msg", "log", "text", "body"};
    constexpr std::array<std::string_view, 5> kComponentKeys{"component", "source", "logger",
                                                             "service", "module"};

    // invariant: canon reads two SHAPES of compound key and ZERO new field names — a key resolves
    // to the name AFTER ITS SINGLE DOT, matched against the vocabularies above.
    // invariant: adding a vendor's dotted spelling to a vocabulary is the cheaper-looking fix and
    // is the one that must not happen — it converts a language feature into a vendor dialect.
    // invariant: one vendor name is the back door to a schema package living in core, and the
    // second name is then unarguable; learning the SHAPE instead is convergence-proof.
    // invariant: the FIELD position only — the NAMESPACE position is REFUSED, and that is a
    // DECLARED LIMITATION rather than an oversight.
    // invariant: the reason is structural, not a tuning failure: under every predicate expressible
    // over these four vocabularies, a service name and a source address are the SAME shape.
    // invariant: the information that would separate them is not present in the instrument, so no
    // rule written here can admit one and refuse the other.
    // invariant: the consequence is a CATEGORY ERROR the struct already names — admitting the
    // namespace position would put a host-class value on a field that declares it is not one.
    // invariant: two escapes were considered and REFUSED: keying on exactly one string child fails
    // on real schemas that have two, and admits the trap where the second child is numeric.
    // invariant: REFUSED, not unknown — the reopening condition is a MEASUREMENT, and a red arm
    // is not a licence to add the missing word to a vocabulary.
    // invariant: BOUNDED at exactly one level in both shapes: one dot, one descent. Unbounded
    // descent would make a level field anywhere in a tree a severity claim.
    // invariant: the bound is what makes the shape a GRAMMAR rather than a search.
    // refs: SRC-D-ECS-1, ADR-17.D1, DN-30.D11
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

    // post: the role a BARE name carries, or None; the order matches the parse's own precedence.
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

    // post: fills ONE role from a compound key's resolved name, and ONLY if that role is still
    // MISSING.
    // invariant: the exact-name pass always wins, which is the property that keeps an
    // already-readable line byte-identical — a compound key can ADD a role, never move one.
    // invariant: the single-dot resolution is defined in the shared scratch header and NOT here,
    // because the fast scanner classifies keys too and parse RETURNS EARLY on a hit.
    // invariant: so the rule must be the same object at both sites, or a flat namespaced line
    // silently keeps the old behaviour.
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

    // invariant: ONE forward walk over the root object — each string value is offered under its
    // single-dot resolved name, each object value is descended EXACTLY ONCE.
    // invariant: no key NAME is consulted to decide whether to descend; the VALUE's type decides,
    // which is what makes this a grammar rather than a list.
    // invariant: it SUBSUMES the hard-coded single-vendor descent it replaces, so this is a name
    // REMOVED from core rather than added.
    // refs: SRC-D-ECS-1
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
                    // invariant: bare name only — a dotted key inside the descent would be a
                    // SECOND level of compounding and is deliberately not read.
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

    // post: true iff the record is OTEL, which the caller uses to route the message to the nested
    // OTLP body.
    // invariant: the severity number runs AFTER the level-string route, so it OVERRIDES it —
    // declared outranks inferred.
    // invariant: trace ids are hashed to scalar PODs and never retained as values.
    // invariant: kept separate so the strategy's own parse stays within its complexity budget.
    // refs: SRC-D-OTEL-1, SRC-D-OTEL-4a
    [[nodiscard]] bool extract_otel_fields(simdjson::ondemand::object& root,
                                           ParsedLine& parsed_line)
    {
        bool is_otel{false};
        std::string_view scratch_view;
        // invariant: the OTLP event-time key is not one of the four classified fields but is
        // required, so OTEL inputs carry a timestamp and window like any other format.
        // invariant: without it the whole pipeline never closes a window; the value is a QUOTED
        // epoch-nanoseconds string per the OTLP JSON mapping.
        static constexpr std::string_view kTimeUnixNanoKey{"timeUnixNano"};
        if (try_get_string(root, std::span<const std::string_view>{&kTimeUnixNanoKey, 1},
                           scratch_view))
            if (auto timestamp{utils::parse_unix_nano_timestamp(scratch_view)})
            {
                // invariant: DECLARED — the OTLP key is the LOG record's schema field for when
                // the event happened, not content that resembles a time.
                // invariant: one of exactly TWO declared-time sites, and the one that made the span
                // flag unusable as a marker.
                // refs: DN-29.D12
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

    // invariant: first occurrence then every Nth — an unreadable stream would otherwise flood the
    // log from the hot path, and the tenth identical line adds nothing.
    // refs: DN-29.D15
    constexpr std::uint64_t kWarnEveryNRoleless{1000};

    // post: a view into the caller's own line — never into the padded scratch buffer, which the
    // next line overwrites, and never into a local, which would dangle the moment this returns.
    // invariant: the caller made those bytes arena-stable before calling parse, so the view
    // outlives the event.
    // invariant: the FIRST top-level key is the witness — the same O(1) walk the document probe
    // does, no cursor spent, and one key that was genuinely present answers what arrived.
    // invariant: the full key list is the diagnostic's job, never the value's.
    // refs: DN-29.D16
    [[nodiscard]] std::string_view first_top_level_key(std::string_view line) noexcept
    {
        // invariant: non-empty by construction, because an EMPTY marker MEANS roles were recognized
        // — a blank here would assert exactly the opposite of what happened.
        // invariant: static storage, so the view is always valid.
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
        // invariant: built from the pointer rather than a substring, because a substring throws
        // when its offset exceeds the size and a throwing call in a noexcept body terminates.
        // invariant: the proof holds — the end came back from a non-npos find that STARTED after
        // the quote — and this form states it rather than leaving it to be re-derived.
        return std::string_view{line.data() + quote + 1, end - quote - 1};
    }

    // invariant: COLD PATH only — called after the line is known to have yielded no role and
    // behind the rate limit, so the re-walk is never paid where it would matter.
    // invariant: it uses its OWN parser rather than the thread-local scratch, whose cursor is still
    // held by the caller's live document.
    // invariant: output is bounded in BOTH key count and key length — a diagnostic that its own
    // input can make arbitrarily large is a second defect, not an aid.
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

    // note: a forward declaration; the definition sits below with the other ordinal helpers.
    [[nodiscard]] std::span<const OrdinalObservation>
    store_ordinals(std::span<const OrdinalObservation> observations, ArenaAllocator& arena);

    // post: true iff the raw line is a FLAT OTLP span, detected by the span-specific start-time key
    // where a log record carries the plain one; a cheap raw-byte check, no cursor spent.
    // invariant: it does NOT exclude an export DOCUMENT, and that is no omission — parse refuses
    // one above, so a document is already impossible when control arrives.
    // invariant: the exclusion was dropped as REDUNDANT and not as expensive: the conjunction
    // short-circuits, so on a non-OTEL line the first scan already returned false.
    // invariant: recorded because the opposite was believed and measured FALSE — it cost only the
    // lines that genuinely carry the span key.
    // invariant: the ordering in parse is what the removal rests on and it is load-bearing, because
    // this predicate is TRUE of a document too: a document carries the key inside its spans.
    [[nodiscard]] bool is_otel_span_line(std::string_view line) noexcept
    {
        return line.contains(R"("startTimeUnixNano")");
    }

    // post: an OTLP quoted decimal-nanosecond string as an int64, stopping at the first non-digit.
    // invariant: a digit byte-loop with no float and no charconv dependency, so it is deterministic
    // and cross-stdlib bit-identical.
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

    // post: an arena-stable copy of the linked span ids; empty in means empty out with no
    // allocation, so a span without links stays zero-cost.
    // refs: SRC-D-OTEL-9
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

    // post: FALSE when the nomination was wrong — a cheap probe NOMINATES and the parser it
    // routes to CONFIRMS its own precondition or DECLINES.
    // invariant: ONE forward pass over the span object — the on-demand idiom that descends into
    // status and attributes inline with no rewind.
    // invariant: the mapping is name to content, start to event time, end minus start to the
    // duration ordinal, status to level, service name to component, ids to trace context.
    // invariant: the span KIND is an ABSENT diagnostic field — it needs a categorical channel
    // canon lacks and is not load-bearing for the structural exhibits.
    // invariant: the probe is a raw byte scan matching the key at ANY DEPTH; this walk matches at
    // DEPTH 0 ONLY, so on an export document the two disagree.
    // invariant: a void return had no way to say so, and the caller then emitted a confident record
    // for a line it had not parsed.
    // invariant: declining is NOT an exclusion rule for documents — a key-name denylist names one
    // intruder and rots on the next envelope, and this enumerates nothing.
    // invariant: so it holds for every probe, parser and format that ever routes here; the rule is
    // do not emit what you did not parse.
    // refs: DN-29.D17, SRC-D-OTEL-12, SRC-D-OTEL-18, SRC-D-OTEL-18b
    [[nodiscard]] bool parse_otel_span(simdjson::ondemand::object& root, ParsedLine& parsed_line,
                                       ArenaAllocator& arena)
    {
        std::string_view start_nano;
        std::string_view end_nano;
        std::string_view name_view;
        std::string_view service_name;
        bool is_error{false};
        std::vector<SpanId> linked;

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
                        is_error = code.contains("ERROR");
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
                // invariant: each link declares a cross-trace edge to another span; metalog
                // resolves them by span id ACROSS traces into the distilled service topology.
                // invariant: the link's own trace id and attributes are consumed-not-retained, like
                // the parent context.
                // refs: SRC-D-OTEL-9
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

        // invariant: the confirmation is FREE — the start value is empty exactly when the key the
        // probe promised is not at depth 0.
        // invariant: checked BEFORE the mapping — applying one over fields that were never found
        // is how an unparsed line acquires a plausible event time.
        // invariant: a span with no declared event time is not a span we parsed.
        // refs: ADR-29.D5
        if (start_nano.empty())
            return false;

        parsed_line.trace.is_span = true;
        // invariant: DECLARED — the producer's own statement of when the span began, which is why
        // it outranks a transport stamp where a merely PARSED time does not.
        // invariant: the span flag is set here too: DECLARED causality routes the record to the
        // observed DAG rather than to the adjacency ring.
        // refs: DN-29.D12, SRC-D-OTEL-11
        if (const auto declared_start{utils::parse_unix_nano_timestamp(start_nano)})
            parsed_line.timestamp = EventTime::declared(*declared_start);
        else
            parsed_line.timestamp = EventTime::parsed(std::nullopt);
        parsed_line.level = EventLevel::declared(is_error ? LogLevel::Error : LogLevel::Info);
        if (!service_name.empty())
            parsed_line.component = arena.store_string(service_name);
        parsed_line.content =
            arena.store_string(name_view.empty() ? parsed_line.raw_line : name_view);

        // invariant: the duration becomes the declared ordinal; an end before the start yields a
        // ZERO duration, the smallest bin, and never a negative one.
        // refs: SRC-D-OTEL-12
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

        parsed_line.linked_span_ids = store_span_ids(linked, arena);
        return true;
    }

    // post: a span over arena-stable copies of the matched observations; empty in means empty out
    // with no allocation, so a non-ordinal line stays zero-cost.
    // invariant: the observations' field names view the declared catalog's STATIC keys, stable for
    // the program lifetime, so only the small POD array is arena-copied.
    // refs: SRC-D-W1-3
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

    // post: the scanner's numeric candidates matched against the declared catalog, each hit a
    // consumed ordinal observation.
    // invariant: the decimal TEXT becomes the int64 — there is no float-to-int anywhere on this
    // path.
    // refs: SRC-D-W1-3
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

    // post: the same field-route on the slow path, one lookup per declared key.
    // pre: this MUST run before the OTLP body descent below, which spends the on-demand cursor.
    // invariant: the value's raw decimal TOKEN becomes the int64, never a double read — that is
    // the determinism pin.
    // refs: SRC-D-W1-3
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

    // post: nullopt when the byte scan cannot complete — an escape, nesting or malformed input
    // — so the caller falls back to full simdjson.
    // invariant: split out of parse to keep that function within its complexity budget.
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
        parsed_line.ordinals = extract_ordinals_fast(fast, arena);
        INSIGHT_LOG_DEBUG(logging::strategy_logger(),
                          "strategy=JSON fast_path component={} level={} has_timestamp={}",
                          parsed_line.component, to_string(parsed_line.level.value()),
                          parsed_line.timestamp.has_value());
        return parsed_line;
    }

} // namespace

// invariant: the strategy entry — a fast-path byte scan, then a guarded simdjson slow path
// routing to the span parse or the log-record parse.
std::expected<ParsedLine, std::string> JsonStrategy::parse(std::string_view line,
                                                           ArenaAllocator& arena) const
{
    if (line.empty())
        return std::unexpected(std::string("JsonStrategy: empty line"));

    // invariant: for escape-free JSON objects the fast path bypasses simdjson entirely with a
    // single-pass byte scan and no heap allocation, falling back on any anomaly.
    if (std::optional<ParsedLine> fast_parsed{try_fast_parse(line, arena)})
        return std::expected<ParsedLine, std::string>{*std::move(fast_parsed)};

    // invariant: an OTLP export DOCUMENT is REFUSED here, and the refusal is the feature.
    // invariant: without it the document falls through to the generic log-record route and yields
    // ONE plausible-looking event for an export carrying N spans, with no error and no diagnostic.
    // invariant: that is the silent-wrong-answer class this product may not ship, and a refusal a
    // caller can SEE is strictly better than a number a caller cannot doubt.
    // invariant: refused rather than unpacked because document mode is ACQUISITION-tier and this
    // entry is frame-oriented — unpacking would put an unbounded object in a bounded instrument.
    // invariant: a collector-shaped acquisition fans the export out and puts flat SPANS on the
    // wire, which is what the enters-as-a-record rule already says.
    // pre: this MUST precede is_otel_span_line, which tests only for the span key — and a
    // document carries that key inside its spans.
    // refs: ADR-29.D3, ADR-29.D5, ADR-29.D7
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

    // invariant: a flat span is a DISTINCT shape parsed in its own forward pass, routed on the
    // raw-byte signal BEFORE spending the root cursor on the log-record lookups below.
    // refs: SRC-D-OTEL-18
    if (is_otel_span_line(line))
    {
        if (parse_otel_span(root, parsed_line, arena))
            return parsed_line;

        // invariant: DECLINED — the probe nominated on a raw byte scan, the parser looked at
        // depth 0 and the promised key was not there, so the line returns to the chain.
        // invariant: the generic path then recognizes no role and the marker fires, which is the
        // whole reason it was reachable on this input at all.
        // invariant: whatever the walk wrote is DISCARDED — on a document it writes nothing, but
        // a depth-0 trace id would otherwise leave a half-populated context.
        // invariant: a declined parse must leave NO trace of itself, which makes the decline total
        // rather than nearly total.
        // refs: DN-29.D17
        parsed_line = ParsedLine{};
        parsed_line.raw_line = line;

        // invariant: the walk spent the on-demand cursor, which is forward-only and cannot rewind,
        // so the generic path below needs a fresh one.
        // invariant: re-iterating is a SECOND parse of this line and it is paid ONLY here — on a
        // line that was mis-nominated, which is rare by construction.
        // invariant: the alternative of pre-checking depth 0 before the walk costs every genuine
        // span a second pass to spare the exceptional one.
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
    // invariant: the fourth role probe's own result — the other three are readable off the parsed
    // line, but content is set on BOTH branches.
    // invariant: a recognized message and the raw-line fallback are indistinguishable afterwards,
    // and treating the fallback as a role would silence the marker on its own input.
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

    // invariant: the severity number becomes the level band and the trace keys become consumed
    // structural metadata; the trace keys are top-level, so they are never tokenized.
    // invariant: they are therefore dropped from the template BY CONSTRUCTION rather than by a
    // rule.
    // refs: ADR-29, SRC-D-OTEL-1, SRC-D-OTEL-4a
    const bool is_otel{extract_otel_fields(root, parsed_line)};

    // pre: the ordinal route MUST precede the body descent below, which spends the on-demand
    // cursor.
    // refs: SRC-D-W1-3
    parsed_line.ordinals = extract_ordinals_slow(root, arena);

    if (is_otel)
    {
        // pre: the body descent MUST be the last root access — it descends into a child, after
        // which the parent cursor cannot rewind to a sibling.
        // invariant: a malformed or absent body arena-stores the whole line instead.
        if (std::string_view body_value; try_get_otel_body(root, body_value))
            parsed_line.content = arena.store_string(body_value);
        else
            // invariant: the fallback arena-stores the ORIGINAL line rather than re-serialising the
            // document, which would force a heap allocation; the raw bytes are already stable.
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
            parsed_line.content = arena.store_string(line);
        }

        // invariant: this pass runs ONLY when a role is still missing after the exact-name lookups,
        // so an already-readable line never reaches it.
        // invariant: that is what makes it additive — it closes a blindness, it does not
        // re-decide anything.
        // invariant: the predecessor descended into ONE literally-named object, which read one
        // producer's namespacing convention by enumeration; the shape does it by structure.
        // invariant: net effect on core's vocabulary is one name REMOVED and none added.
        // invariant: a fresh cursor is required and is not free, and it is paid ONLY on a line
        // still missing a role — never on a canon-named line.
        // refs: SRC-D-ECS-1
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

    // invariant: the independent backstop against a SILENT wrong answer — the hard refusal above
    // can only refuse what it RECOGNISES, via a dated premise about the top-level key.
    // invariant: nothing in this repo can re-derive that premise, so the refusal alone would leave
    // a schema change reading as a clean parse.
    // invariant: this layer cannot be defeated by the same change, because it knows nothing about
    // the schema at all.
    // invariant: the predicate is ALREADY COMPUTED — all four role probes have run, so this costs
    // one integer test: no needle, no window constant, no second pass.
    // invariant: naming the keys DOES cost a re-walk, and it is paid only on the diagnostic path,
    // once we already know the line was not understood.
    // invariant: what it guarantees EXACTLY is that the record path never SILENTLY emits an event
    // for input it understood nothing of.
    // invariant: it does NOT promise to recognise a non-canonical export; it promises not to be
    // silent about one, and that holds for shapes that do not exist yet.
    // invariant: a MARKER, not a refusal — a role-less object may be an application record whose
    // vocabulary we do not read yet, and refusing it would convert a reading gap into data loss.
    // invariant: the DISCHARGE is the assignment below, never the log line: a warning desilences
    // the console while a caller would still receive an indistinguishable value.
    // invariant: it is also untestable through the log, because canon has no test-observable sink,
    // so an arm written against a log line goes green the day this code is deleted.
    // refs: DN-29.D6, DN-29.D15, DN-29.D16, DN-30, DN-30.O1
    // refs: MEM:synthetic-gate-vacuity-vs-judgment
    if (!is_otel && !parsed_line.timestamp.has_value() && parsed_line.level == LogLevel::Unknown &&
        parsed_line.component.empty() && !recognized_message)
    {
        // invariant: UNCONDITIONAL, and it must stay ABOVE the rate limit below — the marker is
        // set on EVERY role-less record and the sampling governs ONLY the log emission.
        // invariant: moving it inside the sampled branch would leave almost every role-less event
        // carrying an EMPTY marker, indistinguishable from a well-parsed record.
        // invariant: two statements about one condition, and only the console's may be sampled.
        // refs: DN-29.D16
        parsed_line.no_role_witness_key = first_top_level_key(line);

        // invariant: ERGONOMICS, never the contract, and no test may assert against it.
        // invariant: rate-limited because a per-event warn floods from the hot path; thread-local
        // because parse is const and a strategy instance is per-tokenizer, so per thread.
        // invariant: the consequence of the thread-local is that first occurrence fires once per
        // WORKER, so diagnostic VOLUME varies with worker count.
        // invariant: that is fine for a log and is not deterministic content, but no test may
        // assert on the LINE COUNT here — assert on the marker, which is per-record and exact.
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

// post: a cheap layout check — the first non-space character must open an object.
// invariant: the actual parse happens once, in parse, and the detector gates that call behind this
// O(1) check, so the line is never traversed twice.
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
