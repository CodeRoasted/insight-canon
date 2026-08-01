module;
#include "utils/log_macros.hpp" // textual macro layer (ADR-3.D4)

module insight.canon;
import insight.canon.internal;
import insight.canon.api;
import insight.canon.detail.strategy; // ParsedLine
import insight.canon.detail.mask;     // stateless_template, StatelessTemplate
import insight.canon.detail.parse;    // LogParser

// Tokenizer: Phase 1 facade that converts raw log lines into CanonicalEvents.
//
// Data flow per line:
//   raw_line
//     →  LogParser::parse_line()  →  ParsedLine { level, timestamp, component,
//                                                  content }   (all arena-stable;
//                                                  ANSI-stripped at ingest, SRC-D-TID-11)
//     →  stateless_template(content, arena, config)
//                                →  StatelessTemplate { template_str, params[] }
//                                   (a pure function of the line's own masked tokens —
//                                    no clustering state, no cross-line learning, so the
//                                    template_id derived downstream is run-independent;
//                                    the phantom pair cannot form — stateless_template_id.md)
//     →  CanonicalEvent
//
// Ownership: the arena is external; all string_views in CanonicalEvent point
// into arena-managed memory and are valid until arena.reset() or destruction.

namespace insight::tokenization
{

namespace
{

    constexpr std::size_t kProgressLogInterval{1000};

    // canon-internal DEBUG gate for `if constexpr` elision of the progress-log block (computation +
    // log). SPDLOG_ACTIVE_LEVEL is canon's PRIVATE build-type compile def, reaching this build-only
    // impl unit via the textual log_macros.hpp include. Kept OFF the public insight.canon.api
    // surface (which every consumer recompiles) so the level macro never leaks onto a consumer's
    // command line.
    constexpr bool kDebugLogsEnabled{SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG};

} // namespace

struct Tokenizer::Impl
{
    ArenaAllocator& arena; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members): tokenizer
                           // shares the caller-managed arena for stable string_views.
    // The composed vocabulary (ADR-17): borrowed, not owned — must outlive the Tokenizer. NOLINT
    // for the same non-owning-ref reason as `arena`.
    const insight::semantic::ComposedSemantics&
        composed; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
    LogParser parser;
    MaskConfig config; // token-mask configuration for the stateless masker (mask IPv4/hex)
    EventID next_id{0};
    std::size_t produced{0};

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
            INSIGHT_LOG_WARN(logging::tokenizer_logger(), "parse failed: {}", parsed.error());
            return std::unexpected(parsed.error());
        }
        const ParsedLine& parsed_line = parsed.value();

        const StatelessTemplate match{stateless_template(parsed_line.content, arena, config)};

        CanonicalEvent event;
        event.id = next_id++;
        event.timestamp = parsed_line.timestamp.value_or(Timestamp{});
        event.level = parsed_line.level;
        event.format =
            parser.routed_format(); // the routed winner for THIS line (set by parse_line)
        event.component = parsed_line.component;
        event.host = parsed_line.host;
        event.template_str = match.template_str;
        event.params = match.params;
        // The walkers take NormalizedContent; strategy content is attested by the PARSER — the
        // object that performed stage 1 on this very line (§12.5.1(c) — the privileged mint, held
        // one line below the parse it attests). Six strategies REBUILD content into arena bytes,
        // so no public narrowing door could express these two calls.
        event.structural_role = insight::tokenization::classify(
            parser.attest(parsed_line.content),
            composed); // announced structural role (the resolved view's rows)
        // Identity-derived WHERE (bibles/intent_identity.md §8, SRC-II-8): populate an EMPTY
        // component WHERE axis with the recognized test-file. Semantic-unaware (SRC-SP-1): no
        // dialect literal — a format whose lines carry a native component already skips via
        // component.empty(); a format without one (GHA today, any future dialect tomorrow) gets the
        // identity-derived WHERE, and recognize_location returns empty on non-test content so
        // nothing is faked. Config-gated and default-OFF, so every G-SP-1 default path is
        // byte-identical (the flag is the aligned pipeline's, feeding the where_set_shift coverage
        // verdict — §5.4).
        if (config.recognize_test_where && event.component.empty())
            event.component =
                insight::recognize_location(parser.attest(parsed_line.content), composed);
        event.trace = parsed_line.trace; // OTEL trace context (SRC-D-OTEL-1): consumed by O2/O3,
                                         // never serialized; default-empty for non-OTEL inputs
        event.ordinals = parsed_line.ordinals; // W1 ordinal observations (D-W1-3): consumed by
                                               // metalog binning; empty span for non-ordinal lines
        event.linked_span_ids =
            parsed_line.linked_span_ids; // O4b Span Links (SRC-D-OTEL-9): consumed by
                                         // metalog into the service topology; empty
                                         // for lines without links
        event.echoed_source = parsed_line.echoed_source; // SRC-D-PROV-1: echoed script source, not
                                                         // a runtime event; consumed (salience tier
                                                         // gate), never serialized; false for the
                                                         // vast majority of lines

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

std::vector<std::expected<CanonicalEvent, std::string>>
Tokenizer::process_batch(std::span<const std::string_view> lines)
{
    std::vector<std::expected<CanonicalEvent, std::string>> out;
    out.reserve(lines.size());
    std::vector<std::string> span_records; // reused scratch for the document unpack
    for (auto line : lines)
    {
        // SRC-D-OTEL-18 record-source 1→N: an OTLP `resourceSpans` export is unpacked into N canonical
        // flat-span records, each tokenized 1:1 (the strategy stays 1:1). A flat span (shape 2) and
        // every non-OTEL line take the direct path — byte-identical to pre-O3.
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
std::size_t Tokenizer::lines_parsed() const noexcept
{
    return impl_->parser.lines_parsed();
}

} // namespace insight::tokenization
