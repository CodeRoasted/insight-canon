module;
// refs: ADR-3.D4
#include "utils/log_macros.hpp"

module insight.canon;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.strategy;
import insight.canon.detail.mask;
import insight.canon.detail.parse;

// refs: SRC-D-TID-11, ADR-16.D5
// invariant: the template is a pure function of the line's own masked tokens — no clustering
// state and no cross-line learning, so the identity is run-independent.
// invariant: the arena is external; every string_view on a CanonicalEvent points into it and is
// valid until reset or destruction.
namespace insight::tokenization
{

namespace
{

    constexpr std::size_t kProgressLogInterval{1000};

    // note: first breach then every Nth: a broken strategy must not log per line.
    constexpr std::size_t kEmptyProjectionWarnEvery{100};

    // invariant: SPDLOG_ACTIVE_LEVEL is canon's PRIVATE build definition, kept off the public api
    // surface so the level macro never leaks onto a consumer's command line.
    constexpr bool kDebugLogsEnabled{SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG};

} // namespace

struct Tokenizer::Impl
{
    // note: the tokenizer shares the caller-managed arena so its string_views stay stable.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    ArenaAllocator& arena;
    // refs: ADR-17
    // note: borrowed, not owned — the composed vocabulary must outlive the Tokenizer.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-const-or-ref-data-members)
    const insight::semantic::ComposedSemantics& composed;
    LogParser parser;
    MaskConfig config;
    EventID next_id{0};
    std::size_t produced{0};
    std::size_t empty_projections{0};

    Impl(ArenaAllocator& arena_ref, MaskConfig mask_config,
         const insight::semantic::ComposedSemantics& composed_ref)
        : arena{arena_ref}, composed{composed_ref}, parser{arena_ref, composed_ref},
          config{mask_config}
    {
    }

    [[nodiscard]] std::expected<CanonicalEvent, std::string>
    make_event(std::expected<ParsedLine, std::string> parsed)
    {
        if (!parsed)
        {
            // invariant: the parser is the reporter of record and logs each outcome at the level it
            // earns; this site sees only a string.
            INSIGHT_LOG_DEBUG(logging::tokenizer_logger(), "no event: {}", parsed.error());
            return std::unexpected(parsed.error());
        }
        const ParsedLine& parsed_line = parsed.value();

        // refs: ADR-16.D9
        // invariant: a strategy that empties `content` on a line that has bytes breaks the SPI
        // contract, and the line then templates to the hash of the empty string.
        // note: an honest empty body and a projection bug look alike in the bytes.
        if (parsed_line.content.empty() && !parsed_line.raw_line.empty()) [[unlikely]]
        {
            ++empty_projections;
            if (empty_projections == 1 || empty_projections % kEmptyProjectionWarnEvery == 0)
            {
                // refs: DN-43.D14
                // note: `component` separates the two readings only on a syslog-shaped grammar.
                INSIGHT_LOG_WARN(
                    logging::tokenizer_logger(),
                    "empty projection: format={} kept 0 content bytes of {} component=\"{}\" "
                    "(total={})",
                    to_string(parser.routed_format()), parsed_line.raw_line.size(),
                    parsed_line.component, empty_projections);
            }
        }

        const StatelessTemplate match{stateless_template(parsed_line.content, arena, config)};

        CanonicalEvent event;
        event.id = next_id++;
        // refs: DN-29.D14
        // invariant: time and provenance are copied together off ONE EventTime, so no edit can set
        // a declared time here and forget the marker.
        event.timestamp = parsed_line.timestamp.value_or(Timestamp{});
        event.declared_timestamp = parsed_line.timestamp.is_declared();
        // refs: DN-32.D3
        // invariant: level and provenance are copied together off ONE EventLevel — the level
        // channel's twin of the pair above.
        event.level = parsed_line.level.value();
        event.declared_level = parsed_line.level.is_declared();
        event.format = parser.routed_format();
        event.component = parsed_line.component;
        event.host = parsed_line.host;
        event.template_str = match.template_str;
        event.params = match.params;
        // invariant: strategy content is attested by the PARSER, the object that performed stage 1
        // on this line; six strategies rebuild content into arena bytes.
        event.structural_role =
            insight::tokenization::classify(parser.attest(parsed_line.content), composed);
        // refs: SRC-II-8, SRC-SP-1
        // invariant: semantic-unaware — no dialect literal; a format carrying a native component
        // skips, and an unrecognized location returns empty so nothing is faked.
        // note: config-gated and default-OFF, so every default path stays byte-identical.
        if (config.recognize_test_where && event.component.empty())
            event.component =
                insight::recognize_location(parser.attest(parsed_line.content), composed);
        // refs: SRC-D-OTEL-1, SRC-D-W1-3, SRC-D-OTEL-9, SRC-D-PROV-1
        // invariant: trace context, ordinals, span links and the echoed-source flag are CONSUMED in
        // memory and never serialized.
        event.trace = parsed_line.trace;
        event.ordinals = parsed_line.ordinals;
        event.linked_span_ids = parsed_line.linked_span_ids;
        event.echoed_source = parsed_line.echoed_source;
        // refs: DN-29.D16
        // invariant: the legibility marker crosses to the event by the same copy `component` and
        // `host` take; stopping at ParsedLine would leave a confident, unmarked event.
        event.no_role_witness_key = parsed_line.no_role_witness_key;

        ++produced;

        INSIGHT_LOG_TRACE(logging::tokenizer_logger(), "event: id={} tmpl=\"{}\" params={}",
                          event.id, event.template_str, event.params.size());

        if constexpr (kDebugLogsEnabled)
        {
            if (produced % kProgressLogInterval == 0)
            {
                INSIGHT_LOG_DEBUG(logging::tokenizer_logger(), "progress: events={}", produced);
            }
        }
        return std::expected<CanonicalEvent, std::string>{event};
    }
};

Tokenizer::Tokenizer(ArenaAllocator& arena, MaskConfig mask_config,
                     const insight::semantic::ComposedSemantics& composed)
    : impl_{std::make_unique<Impl>(arena, mask_config, composed)}
{
    INSIGHT_LOG_INFO(logging::tokenizer_logger(), "tokenizer init");
}

Tokenizer::~Tokenizer() = default;
Tokenizer::Tokenizer(Tokenizer&&) noexcept = default;
Tokenizer& Tokenizer::operator=(Tokenizer&&) noexcept = default;

std::expected<CanonicalEvent, std::string> Tokenizer::process_line(std::string_view raw_line)
{
    return impl_->make_event(impl_->parser.parse_line(raw_line));
}

std::expected<CanonicalEvent, std::string>
Tokenizer::process_stable_line(std::string_view stable_line)
{
    return impl_->make_event(impl_->parser.parse_stable(stable_line));
}

bool Tokenizer::unpack_span_document(std::string_view raw_line, std::vector<std::string>& records)
{
    // invariant: a broad recogniser, not a first-key compare — JSON does not make top-level key
    // order significant.
    if (!is_otel_span_document_broad(raw_line))
        return false;

    records.clear();
    if (unpack_otel_spans(raw_line, records) == 0)
        // note: recognised but unpacked nothing is a truncated export, not a quiet stream.
        INSIGHT_LOG_WARN(logging::tokenizer_logger(),
                         "otel span document recognised but unpacked 0 spans: bytes={}",
                         raw_line.size());
    return true;
}

std::vector<std::expected<CanonicalEvent, std::string>>
Tokenizer::process_batch(std::span<const std::string_view> lines)
{
    std::vector<std::expected<CanonicalEvent, std::string>> out;
    out.reserve(lines.size());
    std::vector<std::string> span_records;
    for (auto line : lines)
    {
        // refs: SRC-D-OTEL-18
        // invariant: an export is unpacked 1 to N and each record tokenized 1:1; a flat span and
        // every non-OTEL line take the direct path, byte-identical to it.
        if (is_otel_span_document(line))
        {
            span_records.clear();
            unpack_otel_spans(line, span_records);
            for (const auto& record : span_records)
                out.push_back(process_line(record));
            continue;
        }
        out.push_back(process_line(line));
    }
    return out;
}

std::size_t Tokenizer::events_produced() const noexcept
{
    return impl_->produced;
}
std::size_t Tokenizer::empty_projections() const noexcept
{
    return impl_->empty_projections;
}

std::size_t Tokenizer::lines_parsed() const noexcept
{
    return impl_->parser.lines_parsed();
}

} // namespace insight::tokenization
