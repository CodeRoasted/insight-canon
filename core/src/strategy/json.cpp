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
    // invariant: resolved at compile time, so a catalog that lost the span duration key fails to
    // build instead of silently dropping every span's duration.
    constexpr const OrdinalFieldDescriptor& kSpanDurationField{
        *match_ordinal_field("span_duration_ns")};

    // invariant: these are canon's OWN names for the four roles it reads, and they are deliberately
    // NOT a registry of vendor field names.
    // refs: F-SRC-insight-canon:simdjson_scratch.hpp:compound_key_name
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
    // refs: F-SRC-insight-canon:simdjson_scratch.hpp:compound_key_name, ADR-17.D1, ADR-17.D12
    enum class JsonRole : std::uint8_t
    {
        Timestamp,
        Level,
        Component,
        Message,
    };
    constexpr std::size_t kJsonRoleCount{4};

    // invariant: a name's role and its position in that role's vocabulary, the position being the
    // name's precedence within the role.
    struct JsonRoleName
    {
        JsonRole role;
        std::uint8_t position;
    };

    // post: the position of `name` in `vocabulary`, or nullopt.
    [[nodiscard]] constexpr std::optional<std::uint8_t>
    position_in(std::string_view name, std::span<const std::string_view> vocabulary) noexcept
    {
        const auto found{std::ranges::find(vocabulary, name)};
        if (found == vocabulary.end())
            return std::nullopt;
        return static_cast<std::uint8_t>(std::distance(vocabulary.begin(), found));
    }

    // post: the role a BARE name carries and its precedence within that role, or nullopt.
    [[nodiscard]] constexpr std::optional<JsonRoleName> role_of(std::string_view name) noexcept
    {
        if (const auto position{position_in(name, kTimestampKeys)})
            return JsonRoleName{.role = JsonRole::Timestamp, .position = *position};
        if (const auto position{position_in(name, kLevelKeys)})
            return JsonRoleName{.role = JsonRole::Level, .position = *position};
        if (const auto position{position_in(name, kComponentKeys)})
            return JsonRoleName{.role = JsonRole::Component, .position = *position};
        if (const auto position{position_in(name, kMessageKeys)})
            return JsonRoleName{.role = JsonRole::Message, .position = *position};
        return std::nullopt;
    }

    // invariant: a member's rank is its TIER, then its name's position in the role's vocabulary.
    // invariant: an exact top-level name outranks every compound spelling, so a compound key can
    // ADD a role and never move one.
    // invariant: the compound tier holds a dotted top-level key's resolved name and a descended
    // member's bare name alike, so the two wire spellings of one document rank the same.
    constexpr std::uint8_t kExactTier{0};
    constexpr std::uint8_t kCompoundTier{1};
    constexpr std::uint8_t kTierStride{8};
    static_assert(std::max({kTimestampKeys.size(), kLevelKeys.size(), kComponentKeys.size(),
                            kMessageKeys.size()}) <= kTierStride);

    // post: true iff the record is OTEL, which the caller uses to route the message to the nested
    // OTLP body.
    // invariant: the severity number runs AFTER the level-string route, so it OVERRIDES it —
    // declared outranks inferred.
    // invariant: trace ids are hashed to scalar PODs and never retained as values.
    // invariant: kept separate so the strategy's own parse stays within its complexity budget.
    // refs: F-SRC-insight-canon:canon.api.cppm:OtelTraceContext
    // refs: F-SRC-insight-canon:canon.api.cppm:kOtelFieldCatalog
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
                // refs: ADR-29.D5
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
    // refs: ADR-29.D7
    constexpr std::uint64_t kWarnEveryNRoleless{1000};

    // post: a view into the caller's own line — never into the padded scratch buffer, which the
    // next line overwrites, and never into a local, which would dangle the moment this returns.
    // invariant: the caller made those bytes arena-stable before calling parse, so the view
    // outlives the event.
    // invariant: the FIRST top-level key is the witness — the same O(1) walk the document probe
    // does, no cursor spent, and one key that was genuinely present answers what arrived.
    // invariant: the full key list is the diagnostic's job, never the value's.
    // refs: ADR-29.D7
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
    // invariant: it gives a COUNT and never a key, because a host journal keeps this log and a key
    // is a byte of the input.
    [[nodiscard]] std::string top_level_key_count_for_diagnosis(std::string_view line)
    {
        simdjson::ondemand::parser parser;
        simdjson::padded_string padded{line};
        simdjson::ondemand::document doc;
        if (parser.iterate(padded).get(doc) != simdjson::SUCCESS)
            return "<unparseable>";
        simdjson::ondemand::object root;
        if (doc.get_object().get(root) != simdjson::SUCCESS)
            return "<not a JSON object>";
        std::size_t count{0};
        if (root.count_fields().get(count) != simdjson::SUCCESS)
            return "<uncountable>";
        return std::to_string(count);
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
    // refs: ADR-29.D2
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

    // invariant: the depth-0 fields a span record is mapped from, gathered in one forward pass.
    struct SpanFields
    {
        std::string_view start_nano;
        std::string_view end_nano;
        std::string_view name_view;
        std::string_view service_name;
        bool is_error{false};
        std::vector<SpanId> linked;
    };

    // post: `is_error` is set from the status object's `code` (ERROR anywhere in it); an
    // unreadable status or code leaves it untouched.
    void read_span_status(simdjson::simdjson_result<simdjson::ondemand::value> value,
                          bool& is_error) noexcept
    {
        simdjson::ondemand::object status_obj;
        if (value.get_object().get(status_obj) != simdjson::SUCCESS)
            return;
        std::string_view code;
        if (status_obj.find_field_unordered("code").get_string().get(code) == simdjson::SUCCESS)
            is_error = code.contains("ERROR");
    }

    // post: the `stringValue` of every readable `service.name` attribute, in order, the last one
    // winning; anything unreadable is skipped.
    void read_service_name(simdjson::simdjson_result<simdjson::ondemand::value> value,
                           std::string_view& service_name) noexcept
    {
        simdjson::ondemand::array attributes;
        if (value.get_array().get(attributes) != simdjson::SUCCESS)
            return;
        for (auto element : attributes)
        {
            simdjson::ondemand::object attr;
            if (element.get_object().get(attr) != simdjson::SUCCESS)
                continue;
            std::string_view attr_key;
            if (attr.find_field_unordered("key").get_string().get(attr_key) != simdjson::SUCCESS ||
                attr_key != "service.name")
                continue;
            simdjson::ondemand::object value_obj;
            if (attr.find_field_unordered("value").get_object().get(value_obj) == simdjson::SUCCESS)
                read_string_or_keep(value_obj.find_field_unordered("stringValue"), service_name);
        }
    }

    // invariant: each link declares a cross-trace edge to another span; metalog
    // resolves them by span id ACROSS traces into the distilled service topology.
    // invariant: the link's own trace id and attributes are consumed-not-retained, like
    // the parent context.
    // refs: ADR-29.D2
    void read_link_span_ids(simdjson::simdjson_result<simdjson::ondemand::value> value,
                            std::vector<SpanId>& linked)
    {
        simdjson::ondemand::array links_array;
        if (value.get_array().get(links_array) != simdjson::SUCCESS)
            return;
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

    // post: one depth-0 span field read into `fields` or the trace context by its key; a key the
    // span mapping does not name, or a value of the wrong type, changes nothing.
    void read_span_field(std::string_view key,
                         simdjson::simdjson_result<simdjson::ondemand::value> value,
                         SpanFields& fields, OtelTraceContext& trace)
    {
        std::string_view hex;
        if (key == "startTimeUnixNano")
            read_string_or_keep(value, fields.start_nano);
        else if (key == "endTimeUnixNano")
            read_string_or_keep(value, fields.end_nano);
        else if (key == "name")
            read_string_or_keep(value, fields.name_view);
        else if (key == "traceId")
        {
            if (value.get_string().get(hex) == simdjson::SUCCESS)
            {
                trace.present = true;
                trace.trace_id = trace_id_from_hex(hex);
            }
        }
        else if (key == "spanId")
        {
            if (value.get_string().get(hex) == simdjson::SUCCESS)
                trace.span_id = span_id_from_hex(hex);
        }
        else if (key == "parentSpanId")
        {
            if (value.get_string().get(hex) == simdjson::SUCCESS)
            {
                trace.has_parent = true;
                trace.parent_span_id = span_id_from_hex(hex);
            }
        }
        else if (key == "status")
            read_span_status(value, fields.is_error);
        else if (key == "attributes")
            read_service_name(value, fields.service_name);
        else if (key == "links")
            read_link_span_ids(value, fields.linked);
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
    // refs: ADR-17.D13, F-SRC-insight-canon:canon.api.cppm:DurationLog2Ns
    // refs: F-SRC-insight-canon:canon.detail.strategy.cppm:unpack_otel_spans
    // refs: F-SRC-insight-canon:canon.api.cppm:kOtelFieldCatalog
    [[nodiscard]] bool parse_otel_span(simdjson::ondemand::object& root, ParsedLine& parsed_line,
                                       ArenaAllocator& arena)
    {
        SpanFields fields;
        for (auto field : root)
        {
            std::string_view key;
            if (field.unescaped_key().get(key) == simdjson::SUCCESS)
                read_span_field(key, field.value(), fields, parsed_line.trace);
        }

        // invariant: the confirmation is FREE — the start value is empty exactly when the key the
        // probe promised is not at depth 0.
        // invariant: checked BEFORE the mapping — applying one over fields that were never found
        // is how an unparsed line acquires a plausible event time.
        // invariant: a span with no declared event time is not a span we parsed.
        // refs: ADR-29.D5
        if (fields.start_nano.empty())
            return false;

        parsed_line.trace.is_span = true;
        // invariant: DECLARED — the producer's own statement of when the span began, which is why
        // it outranks a transport stamp where a merely PARSED time does not.
        // invariant: the span flag is set here too: DECLARED causality routes the record to the
        // observed DAG rather than to the adjacency ring.
        // refs: ADR-29.D5, F-SRC-insight-metalog:metalog.cppm:record_span
        if (const auto declared_start{utils::parse_unix_nano_timestamp(fields.start_nano)})
            parsed_line.timestamp = EventTime::declared(*declared_start);
        else
            parsed_line.timestamp = EventTime::parsed(std::nullopt);
        parsed_line.level =
            EventLevel::declared(fields.is_error ? LogLevel::Error : LogLevel::Info);
        if (!fields.service_name.empty())
            parsed_line.component = arena.store_string(fields.service_name);
        parsed_line.content =
            arena.store_string(fields.name_view.empty() ? parsed_line.raw_line : fields.name_view);

        // invariant: the duration becomes the declared ordinal; an end before the start yields a
        // ZERO duration, the smallest bin, and never a negative one.
        // refs: F-SRC-insight-canon:canon.api.cppm:DurationLog2Ns
        const std::int64_t start_value{parse_span_nano(fields.start_nano)};
        const std::int64_t end_value{parse_span_nano(fields.end_nano)};
        const std::int64_t duration_ns{end_value > start_value ? end_value - start_value : 0};
        const std::array<OrdinalObservation, 1> observation{
            {{.field_name = kSpanDurationField.key,
              .schedule = kSpanDurationField.schedule,
              .value = duration_ns}}};
        parsed_line.ordinals = store_ordinals(observation, arena);

        parsed_line.linked_span_ids = store_span_ids(fields.linked, arena);
        return true;
    }

    // post: a span over arena-stable copies of the matched observations; empty in means empty out
    // with no allocation, so a non-ordinal line stays zero-cost.
    // invariant: the observations' field names view the declared catalog's STATIC keys, stable for
    // the program lifetime, so only the small POD array is arena-copied.
    // refs: F-SRC-insight-canon:canon.api.cppm:OrdinalObservation
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

    // post: true iff the member is one the OTEL readers own: a log record's event time, a span's
    // start time, or a key of the OTEL field catalog.
    // invariant: the fast reader declines a line carrying one, so the OTEL readers, which only the
    // simdjson reader runs, see every line they could claim.
    // refs: ADR-29.D5, F-SRC-insight-canon:canon.api.cppm:kOtelFieldCatalog
    [[nodiscard]] constexpr bool is_otel_member(std::string_view key) noexcept
    {
        return key == "timeUnixNano" || key == "startTimeUnixNano" ||
               std::ranges::any_of(kOtelFieldCatalog, [key](const OtelFieldDescriptor& descriptor)
                                   { return descriptor.key == key; });
    }

    // invariant: THE ONE ASSIGNMENT RULE of a JSON record's roles and ordinals, fed by the byte
    // reader and the simdjson reader alike, so an unrelated member's bytes never move a role.
    // invariant: a role takes the string member of lowest rank, the first in document order on a
    // tie; a non-string member offers no role, and an unparseable chosen value leaves it absent.
    // invariant: an ordinal is the first numeric member of its exact catalog name, and the
    // observations are emitted in catalog order.
    // refs: ADR-17.D12, F-SRC-insight-canon:canon.api.cppm:OrdinalObservation
    class JsonRecordSink
    {
      public:
        [[nodiscard]] bool member(std::string_view key, JsonScalar scalar) noexcept
        {
            if (is_otel_member(key))
                return false;
            if (scalar.kind == JsonScalarKind::Number)
                capture_ordinal(key, scalar.text);
            else if (scalar.kind == JsonScalarKind::String)
            {
                const std::string_view name{compound_key_name(key)};
                offer(name, name.size() == key.size() ? kExactTier : kCompoundTier, scalar.text);
            }
            return true;
        }

        void descended_member(std::string_view key, JsonScalar scalar) noexcept
        {
            if (scalar.kind == JsonScalarKind::String)
                offer(key, kCompoundTier, scalar.text);
        }

        // post: the chosen roles and the captured ordinals written to `parsed_line`, the content
        // being the chosen message or else the whole line; true iff a message was chosen.
        // invariant: every value is parsed or arena-stored here, so the views it holds may dangle
        // once this returns.
        [[nodiscard]] bool apply(ParsedLine& parsed_line, std::string_view line,
                                 ArenaAllocator& arena) const
        {
            if (const JsonRoleChoice& stamp{choice(JsonRole::Timestamp)}; stamp.chosen())
            {
                parsed_line.timestamp = EventTime::parsed(utils::parse_iso8601(stamp.value));
                if (!parsed_line.timestamp)
                    parsed_line.timestamp =
                        EventTime::yearless(utils::parse_bsd_syslog_ts(stamp.value));
            }
            if (const JsonRoleChoice& level{choice(JsonRole::Level)}; level.chosen())
                parsed_line.level = EventLevel::declared(utils::parse_log_level(level.value));
            if (const JsonRoleChoice& component{choice(JsonRole::Component)}; component.chosen())
                parsed_line.component = arena.store_string(component.value);
            const JsonRoleChoice& message{choice(JsonRole::Message)};
            parsed_line.content = arena.store_string(message.chosen() ? message.value : line);
            if (ordinal_count_ == 0)
                return message.chosen();

            std::array<OrdinalObservation, kOrdinalFieldCatalog.size()> matched{};
            std::size_t matched_count{0};
            for (std::size_t index{0}; index < kOrdinalFieldCatalog.size(); ++index)
            {
                const std::optional<std::int64_t>& value{ordinal_values_[index]};
                if (!value.has_value())
                    continue;
                const OrdinalFieldDescriptor& descriptor{kOrdinalFieldCatalog[index]};
                matched[matched_count++] = OrdinalObservation{
                    .field_name = descriptor.key, .schedule = descriptor.schedule, .value = *value};
            }
            parsed_line.ordinals = store_ordinals(
                std::span<const OrdinalObservation>{matched.data(), matched_count}, arena);
            return message.chosen();
        }

      private:
        [[nodiscard]] const JsonRoleChoice& choice(JsonRole role) const noexcept
        {
            return choices_[static_cast<std::size_t>(role)];
        }

        void offer(std::string_view name, std::uint8_t tier, std::string_view value) noexcept
        {
            if (const auto role_name{role_of(name)})
                choices_[static_cast<std::size_t>(role_name->role)].offer(
                    static_cast<std::uint8_t>((tier * kTierStride) + role_name->position), value);
        }

        // invariant: the decimal TEXT becomes the int64 — there is no float-to-int anywhere on
        // this path.
        // refs: F-SRC-insight-canon:canon.api.cppm:parse_decimal_scaled
        void capture_ordinal(std::string_view key, std::string_view text) noexcept
        {
            const OrdinalFieldDescriptor* const descriptor{match_ordinal_field(key)};
            if (descriptor == nullptr)
                return;
            const auto index{static_cast<std::size_t>(descriptor - kOrdinalFieldCatalog.data())};
            if (ordinal_seen_[index])
                return;
            ordinal_seen_[index] = true;
            ordinal_values_[index] = parse_decimal_scaled(text, descriptor->scale_to_canonical);
            if (ordinal_values_[index].has_value())
                ++ordinal_count_;
        }

        std::array<JsonRoleChoice, kJsonRoleCount> choices_{};
        std::array<bool, kOrdinalFieldCatalog.size()> ordinal_seen_{};
        std::array<std::optional<std::int64_t>, kOrdinalFieldCatalog.size()> ordinal_values_{};
        std::size_t ordinal_count_{0};
    };
    static_assert(JsonMemberSink<JsonRecordSink>);

    // post: the record carries its first top-level key as the no-role witness, and the console is
    // told, rate-limited per thread.
    void mark_roleless_record(ParsedLine& parsed_line, std::string_view line)
    {
        // invariant: UNCONDITIONAL, and it must stay ABOVE the rate limit below — the marker is
        // set on EVERY role-less record and the sampling governs ONLY the log emission.
        // invariant: moving it inside the sampled branch would leave almost every role-less event
        // carrying an EMPTY marker, indistinguishable from a well-parsed record.
        // invariant: two statements about one condition, and only the console's may be sampled.
        // refs: ADR-29.D7
        parsed_line.no_role_witness_key = first_top_level_key(line);

        // invariant: ERGONOMICS, not the contract: a test asserts only that it quotes no input.
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
                             "dropped. Top-level keys: {} (total such lines={})",
                             top_level_key_count_for_diagnosis(line), roleless_count);
    }

    // post: the record is marked when it is not OTEL and yielded no role: no stamp, no level, no
    // component and no message.
    // invariant: the predicate is ALREADY COMPUTED — all four role probes have run, so this costs
    // one integer test: no needle, no window constant, no second pass.
    // refs: ADR-29.D7
    void mark_if_roleless(ParsedLine& parsed_line, std::string_view line, bool recognized_message)
    {
        if (!parsed_line.timestamp.has_stamp() && parsed_line.level == LogLevel::Unknown &&
            parsed_line.component.empty() && !recognized_message)
            mark_roleless_record(parsed_line, line);
    }

    // post: nullopt when the byte reader cannot complete — an escape, nesting, malformed input or
    // an OTEL member — so the caller falls back to full simdjson.
    // invariant: split out of parse to keep that function within its complexity budget.
    [[nodiscard]] std::optional<ParsedLine> try_fast_parse(std::string_view line,
                                                           ArenaAllocator& arena)
    {
        JsonRecordSink sink;
        if (!scan_flat_json(line, sink))
            return std::nullopt;
        ParsedLine parsed_line;
        parsed_line.raw_line = line;
        mark_if_roleless(parsed_line, line, sink.apply(parsed_line, line, arena));
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

    // invariant: for escape-free JSON objects the fast path bypasses simdjson entirely with a
    // single-pass byte scan and no heap allocation, falling back on any anomaly.
    // invariant: it runs AFTER the document refusal, which reads the first key only, so a refused
    // document is refused whatever bytes it carries.
    if (std::optional<ParsedLine> fast_parsed{try_fast_parse(line, arena)})
        return std::expected<ParsedLine, std::string>{*std::move(fast_parsed)};

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
    // refs: F-SRC-insight-canon:canon.detail.strategy.cppm:unpack_otel_spans
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
        // refs: ADR-17.D13
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

    // invariant: the roles and ordinals come from the one sink the fast path feeds, so the two
    // paths assign one record identically; the walk is the root's first read.
    JsonRecordSink sink;
    walk_json_object(root, sink);
    const bool recognized_message{sink.apply(parsed_line, line, arena)};

    // invariant: the reset fails only on bytes the structural index passed and no member grammar
    // reads (`{'k': 1}`, `{k: 1}`, `{name}`): no OTEL record, kept from the members walked so far.
    // invariant: a refusal here would drop the line, which a reading gap never earns.
    bool rewound{};
    const bool readable_again{root.reset().get(rewound) == simdjson::SUCCESS};
    if (!readable_again)
        INSIGHT_LOG_TRACE(logging::strategy_logger(),
                          "strategy=JSON malformed member past the structural index");

    // invariant: the severity number becomes the level band and the trace keys become consumed
    // structural metadata; the trace keys are top-level, so they are never tokenized.
    // invariant: they are therefore dropped from the template BY CONSTRUCTION rather than by a
    // rule.
    // refs: ADR-29, F-SRC-insight-canon:canon.api.cppm:OtelTraceContext
    // refs: F-SRC-insight-canon:canon.api.cppm:kOtelFieldCatalog
    const bool is_otel{readable_again && extract_otel_fields(root, parsed_line)};

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
        // invariant: the independent backstop against a SILENT wrong answer — the hard refusal
        // above can only refuse what it RECOGNISES, via a dated premise about the top-level key.
        // invariant: this layer cannot be defeated by the same change, because it knows nothing
        // about the schema at all.
        // invariant: a MARKER, not a refusal — a role-less object may be an application record
        // whose vocabulary we do not read yet, and refusing it would turn a gap into data loss.
        // refs: ADR-29.D7, ADR-17.D12, MEM:synthetic-gate-vacuity-vs-judgment
        mark_if_roleless(parsed_line, line, recognized_message);
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
