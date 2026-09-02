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
//                                    the phantom pair cannot form — ADR-16.D5)
//     →  CanonicalEvent
//
// Ownership: the arena is external; all string_views in CanonicalEvent point
// into arena-managed memory and are valid until arena.reset() or destruction.

namespace insight::tokenization
{

namespace
{

    constexpr std::size_t kProgressLogInterval{1000};

    // Rate limit for the projection-totality warn (see make_event): first breach + every Nth after.
    // Same shape as LogParser's failure warns — a broken strategy must not turn a 31 M-line stream
    // into 31 M log records.
    constexpr std::size_t kEmptyProjectionWarnEvery{100};

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
    // Projection-totality breaches seen on this stream — see the check in make_event.
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
            // DEBUG, and the level is the whole point. THE PARSER IS THE REPORTER OF RECORD: it
            // classifies its own outcomes and logs each at the level it earns — TRACE for a line
            // that carries no event (an empty line is ordinary input, not an error), a
            // rate-limited WARN naming the strategy, its reason and the running count for a real
            // failure. This site sees only a string and cannot tell those apart, so it used to
            // reprint EVERY decline at WARN with no rate limit — an unbounded duplicate of an
            // already-bounded record. Measured on 2026-09-01, one pass of
            // `incident_episode_measure` over 4 082 GitHub CI logs: 611 704 stderr records,
            // 57.7 MB, of which 523 126 were `LogParser: empty line`. Nothing is lost by the
            // demotion — every genuine failure class still reaches WARN from the parser — and in
            // Release this line compiles out entirely (SPDLOG_ACTIVE_LEVEL=INFO).
            INSIGHT_LOG_DEBUG(logging::tokenizer_logger(), "no event: {}", parsed.error());
            return std::unexpected(parsed.error());
        }
        const ParsedLine& parsed_line = parsed.value();

        // ── The PROJECTION-TOTALITY instrument (DN-43.D6) ────────────────────────────────────
        // A strategy that emptied `content` on a line that HAS bytes broke the SPI contract, and
        // the breach is invisible downstream: the line templates to the SHA-256 prefix of the empty
        // string, a universal collision bucket that reaches the wire as an ordinary identity. A
        // rare honest occurrence (a syslog line whose body really is empty) and a catastrophic
        // silent projection bug are indistinguishable in the bytes, so the condition earns an
        // instrument rather than a rule. Nothing in canon reported this for the life of the defect;
        // that silence WAS the logging gap. Rate-limited (first + every 100th) and cold by
        // construction — two empty() tests on a path that already hashes the line.
        if (parsed_line.content.empty() && !parsed_line.raw_line.empty()) [[unlikely]]
        {
            ++empty_projections;
            if (empty_projections == 1 || empty_projections % kEmptyProjectionWarnEvery == 0)
            {
                // `component` is printed because on a SYSLOG-shaped grammar it separates the
                // two readings a bare count cannot: EMPTY means the line's body genuinely ended
                // at the header (a legitimate `cron[1]:` with nothing after it), NON-EMPTY means
                // the message body was moved onto a cube dimension — the defect shape itself.
                // IT DOES NOT GENERALISE, and reading it as if it did is how the next reader acts
                // on a false positive (DN-43.D14). A grammar with a SUBSYSTEM column fills
                // `component` from the header, so BGL's 34 470 genuinely-empty bodies each print
                // `component="KERNEL"` and read as the defect shape. The warning is a COUNT; the
                // per-strategy expectation carries the verdict.
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
        // THE ONE SITE (DN-29.D14). Time and provenance are copied together, off a single
        // EventTime that carries both — so there is no ordering in which one lands without the
        // other, and no future edit that can set a declared time here and forget the marker.
        event.timestamp = parsed_line.timestamp.value_or(Timestamp{});
        event.declared_timestamp = parsed_line.timestamp.is_declared();
        // THE ONE SITE (DN-32.D3), the level channel's twin of the two lines above and held by the
        // same species of lint. Level and provenance are copied together, off a single EventLevel.
        event.level = parsed_line.level.value();
        event.declared_level = parsed_line.level.is_declared();
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
        event.trace = parsed_line.trace; // OTEL trace context (SRC-D-OTEL-1): consumed by the
                                         // structural layer, never serialized; default-empty for
                                         // non-OTEL inputs
        event.ordinals = parsed_line.ordinals; // W1 ordinal observations (SRC-D-W1-3): consumed by
                                               // metalog binning; empty span for non-ordinal lines
        event.linked_span_ids =
            parsed_line.linked_span_ids; // O4b Span Links (SRC-D-OTEL-9): consumed by
                                         // metalog into the service topology; empty
                                         // for lines without links
        event.echoed_source = parsed_line.echoed_source; // SRC-D-PROV-1: echoed script source, not
                                                         // a runtime event; consumed (salience tier
                                                         // gate), never serialized; false for the
                                                         // vast majority of lines
        // DN-29.D16 — the legibility marker crosses to the event by the same copy `component` and
        // `host` take above. This is where L2's guarantee actually binds: a marker that stopped at
        // ParsedLine would leave the pipeline receiving a confident, unmarked event, which
        // desilences canon's internals and not the contract. Empty for every line that yielded a
        // role, i.e. for the overwhelming majority.
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
    // L3's broad recogniser, not L1's first-key compare: this door exists to catch a conformant
    // export whatever order its producer emitted the top-level keys in, and JSON does not make
    // that order significant.
    if (!is_otel_span_document_broad(raw_line))
        return false;

    records.clear();
    if (unpack_otel_spans(raw_line, records) == 0)
        // Recognised but yielded nothing: a truncated or malformed export. The line is consumed
        // and produces no event, which downstream reads as a quiet stream — the silent-wrong-answer
        // shape — so it gets a permanent record here rather than a debugger session later.
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
    std::vector<std::string> span_records; // reused scratch for the document unpack
    for (auto line : lines)
    {
        // SRC-D-OTEL-18 record-source 1→N: an OTLP `resourceSpans` export is unpacked into N
        // canonical flat-span records, each tokenized 1:1 (the strategy stays 1:1). A flat span
        // (shape 2) and every non-OTEL line take the direct path — byte-identical to the
        // pre-span-ingest path.
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
