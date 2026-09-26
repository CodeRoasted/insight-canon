module insight.canon.api;
import insight.canon.internal;

// refs: DN-108.D24, F-SRC-insight-canon:canon.api.cppm:kProjectionMembers
namespace insight::tokenization
{
namespace
{
    // note: converts to any member type, so an aggregate takes N of it iff it has N members.
    struct AnyMember
    {
        template <class Member> operator Member&() const noexcept;
    };

    template <class Aggregate, std::size_t... Slot>
    consteval bool initializes_from(std::index_sequence<Slot...> /*slots*/)
    {
        return requires { Aggregate{((void)Slot, AnyMember{})...}; };
    }

    // post: the number of members Aggregate declares, read from the one construct that sees them
    // all before reflection: aggregate initialization.
    template <class Aggregate, std::size_t Count = 0> consteval std::size_t member_count()
    {
        if constexpr (initializes_from<Aggregate>(std::make_index_sequence<Count + 1>{}))
            return member_count<Aggregate, Count + 1>();
        else
            return Count;
    }

    // invariant: the nested aggregates below are rendered field by field, so each is pinned beside
    // the event itself.
    constexpr std::size_t kTraceContextFields{6};
    constexpr std::size_t kOrdinalObservationFields{3};
    constexpr std::size_t kIdFields{1};

    static_assert(member_count<CanonicalEvent>() == kProjectionMembers.size(),
                  "CanonicalEvent gained or lost a member: render it in render_projection and "
                  "name it in kProjectionMembers (DN-108.D24)");
    static_assert(member_count<OtelTraceContext>() == kTraceContextFields,
                  "OtelTraceContext changed: render the new field in the trace column");
    static_assert(member_count<OrdinalObservation>() == kOrdinalObservationFields,
                  "OrdinalObservation changed: render the new field in the ordinals column");
    static_assert(member_count<TraceId>() == kIdFields && member_count<SpanId>() == kIdFields,
                  "a trace or span id gained a field: render it in the trace column");

    // post: the column of the member called name, or one past the last for a name that is not a
    // member, which std::get then refuses at compile time.
    // invariant: derived from the name, so a reordered name list moves the index with it.
    consteval std::size_t column(std::string_view name)
    {
        std::size_t idx{0};
        while (idx < kProjectionMembers.size() && kProjectionMembers[idx] != name)
            ++idx;
        return idx;
    }

    constexpr char kListSeparator{'|'};
    constexpr char kFieldSeparator{','};
    constexpr char kEscape{'\\'};

    void append_escaped(std::string& out, std::string_view text)
    {
        for (const char chr : text)
        {
            switch (chr)
            {
            case '\\':
                out.append("\\\\");
                break;
            case '\t':
                out.append("\\t");
                break;
            case '\n':
                out.append("\\n");
                break;
            case '\r':
                out.append("\\r");
                break;
            case kListSeparator:
                out.push_back(kEscape);
                out.push_back(kListSeparator);
                break;
            default:
                out.push_back(chr);
            }
        }
    }

    template <std::integral Integer> void append_integer(std::string& out, Integer value)
    {
        constexpr std::size_t kDigits{std::numeric_limits<Integer>::digits10 + 2};
        std::array<char, kDigits> digits{};
        // assert: the buffer holds the widest value of the type plus its sign, so to_chars cannot
        // fail here.
        const std::to_chars_result written{
            std::to_chars(digits.data(), digits.data() + digits.size(), value)};
        out.append(digits.data(), written.ptr);
    }

    void append_flag(std::string& out, bool value)
    {
        out.push_back(value ? '1' : '0');
    }

    void append_count(std::string& out, std::size_t count)
    {
        append_integer(out, count);
    }
} // namespace

void render_projection(const CanonicalEvent& event, ProjectionColumns& out)
{
    for (std::string& text : out)
        text.clear();

    append_integer(std::get<column("id")>(out), event.id);
    append_integer(std::get<column("timestamp_ns")>(out),
                   event.timestamp.time_since_epoch().count());
    append_flag(std::get<column("declared_timestamp")>(out), event.declared_timestamp);
    std::get<column("level")>(out).append(to_string(event.level));
    append_flag(std::get<column("declared_level")>(out), event.declared_level);
    std::get<column("format")>(out).append(to_string(event.format));
    append_escaped(std::get<column("component")>(out), event.component);
    append_escaped(std::get<column("host")>(out), event.host);
    append_escaped(std::get<column("template_str")>(out), event.template_str);

    std::string& params{std::get<column("params")>(out)};
    append_count(params, event.params.size());
    for (const std::string_view param : event.params)
    {
        params.push_back(kListSeparator);
        append_escaped(params, param);
    }

    std::get<column("structural_role")>(out).append(to_string(event.structural_role));

    std::string& trace{std::get<column("trace")>(out)};
    append_flag(trace, event.trace.present);
    trace.push_back(kFieldSeparator);
    append_flag(trace, event.trace.has_parent);
    trace.push_back(kFieldSeparator);
    append_flag(trace, event.trace.is_span);
    trace.push_back(kFieldSeparator);
    append_integer(trace, event.trace.trace_id.value);
    trace.push_back(kFieldSeparator);
    append_integer(trace, event.trace.span_id.value);
    trace.push_back(kFieldSeparator);
    append_integer(trace, event.trace.parent_span_id.value);

    std::string& ordinals{std::get<column("ordinals")>(out)};
    append_count(ordinals, event.ordinals.size());
    for (const OrdinalObservation& observation : event.ordinals)
    {
        ordinals.push_back(kListSeparator);
        append_escaped(ordinals, observation.field_name);
        ordinals.push_back(kFieldSeparator);
        ordinals.append(ordinal_schedule_id(observation.schedule));
        ordinals.push_back(kFieldSeparator);
        append_integer(ordinals, observation.value);
    }

    std::string& linked{std::get<column("linked_span_ids")>(out)};
    append_count(linked, event.linked_span_ids.size());
    for (const SpanId span : event.linked_span_ids)
    {
        linked.push_back(kListSeparator);
        append_integer(linked, span.value);
    }

    append_flag(std::get<column("echoed_source")>(out), event.echoed_source);
    append_escaped(std::get<column("no_role_witness_key")>(out), event.no_role_witness_key);
}

} // namespace insight::tokenization
