// invariant: this interface holds DECLARATIONS only; the class bodies live in src/*.cpp impl units,
// and det_math is the exception because inline-in-interface IS its guarantee.
// invariant: a consumer compiles det_math's bodies under its own -ffp-contract=off, which is what
// makes the emitted double bit-identical.
// refs: ADR-3.D4
module;
// invariant: this exported unit references NO build-config macro; one that did would force that
// macro PUBLIC and let two consumers of one module disagree on its content.
// invariant: canon's spdlog elision level stays PRIVATE to canon's own build.
#include "det/det_int128.hpp"
#include <fmt/core.h>
#include <fmt/format.h>
#include <spdlog/common.h>
#include <spdlog/logger.h>

export module insight.canon.api;
import insight.canon.internal;

export namespace insight
{

using Timestamp = std::chrono::system_clock::time_point;
using Duration = std::chrono::system_clock::duration;

using EventID = uint64_t;

// invariant: the identifier of the canonicalization CONTRACT, not of the package — a release that
// does not touch the rules must not move it, and a rules change must.
// invariant: every MetaLog producer defaults to this, so old and new documents become incomparable
// at the wire spec's 2.4 gate: re-derive, never migrate.
// refs: ADR-2.D5, ADR-2.D9, SRC-D-TID-16, SRC-D-TID-9, SRC-D-TID-22
// note: the generation ledger is technical_docs/canonicalization_generations.md.
inline constexpr std::string_view kCanonicalizationVersion{"stateless-masks-15"};

// invariant: the first 16 bytes of SHA-256 over the masked template_str, carried as a fixed-size
// POD; the 34-byte "h:"+hex string materialises only at the serialize seam.
// invariant: canon owns it because identity IS the hash UNDER kCanonicalizationVersion, so the
// identity and its comparability version belong in one place.
// refs: SRC-D-TIR-1, SRC-D-TID-9, SRC-D-TID-16
struct TemplateId
{
    std::array<std::uint8_t, 16> bytes{};
    // invariant: the DEFAULTED byte-lexicographic order reproduces the old "h:"+hex string order
    // exactly, because hex is order-preserving and the prefix is constant.
    // invariant: every vector<TemplateId> sort in merge_behavior and diff relies on that identity
    // — a hand-rolled operator< silently re-orders published goldens.
    auto operator<=>(const TemplateId&) const = default;
    bool operator==(const TemplateId&) const = default;
};

// invariant: a fixed-width 128-bit key for an n-gram SEQUENCE, so a map keys on a scalar instead of
// rehashing and recomparing a variable-length sequence on every operation.
// invariant: NEVER serialized — the n-gram maps emit their output sorted by the sequence, not by
// this id, so a fast non-crypto hash is correct here.
// invariant: order-sensitive: [a,b] and [b,a] are distinct n-grams and take distinct ids.
// refs: SRC-D-TIR-4
struct NgramId
{
    std::array<std::uint8_t, 16> bytes{};
    auto operator<=>(const NgramId&) const = default;
    bool operator==(const NgramId&) const = default;
};

// post: render(template_id_of(s)) is byte-identical to the former string id for every s.
// refs: SRC-D-TIR-1
[[nodiscard]] TemplateId template_id_of(std::string_view canonical_template) noexcept;
// post: "h:" + 32 lowercase hex — the ONLY place the id string materialises.
[[nodiscard]] std::string render(TemplateId template_id);
// note: a TEST and fixture helper only — no product path parses an id back.
// refs: SRC-D-TIR-1
[[nodiscard]] TemplateId parse_template_id(std::string_view rendered);

// post: a transient 128-bit content key over the sequence's id bytes — never serialized,
// order-sensitive.
// refs: SRC-D-TIR-4
[[nodiscard]] NgramId ngram_id_of(const std::vector<TemplateId>& sequence) noexcept;

// invariant: the comparability identity of the recognizer and marker rule set is the composed
// semantic_identity — a CONTENT hash over the rows packages ship, never a hand-bumped label.
// invariant: a rule change is a package version bump, so a new semantic_identity, so
// re-segment-or-refuse; kCanonicalizationVersion enters that composed hash as a component.
// post: the intent CLASS — the version, matrix and shard tokens masked out, the structural name
// kept, so matrix legs and retries of ONE job collapse to one class.
// invariant: instances are separated DOWNSTREAM by ordinal and are never eaten here; this is a
// distinct rule set from the value masker, which keeps what identity must collapse.
// invariant: deterministic, ASCII-safe, no float, no regex, no cross-line state; cold path, so it
// returns an owned string.
// refs: ADR-17, SRC-II-1, SRC-II-2, SRC-II-6, SRC-II-7, BIB:intent_identity
[[nodiscard]] std::string canonicalize_intent(std::string_view name);

// post: a VIEW into name — canon's intent trim bytes removed from both ends, everything else
// verbatim: no allocation, no masking, no normalization.
// invariant: THREE consumers need that one byte set and the set may have exactly one definition —
// the class masks, the instance keeps, and a report renders.
// invariant: a carriage return is in the trim set because a Windows runner emits CRLF, not for
// tidiness; a report trimming a different set would show one intent two ways.
// refs: DN-38.D1
[[nodiscard]] std::string_view trimmed_intent_name(std::string_view name) noexcept;

// post: the matrix tuple rendered into the display name, returned VERBATIM as a view; empty when
// the name carries no tuple.
// invariant: kept RAW rather than masked or ordinal-numbered because matrix axes are stable by
// DECLARATION, so raw keys pair exactly across runs.
// invariant: the exact complement of canonicalize_intent — the class masks the tuple, the
// discriminant keeps it.
// refs: ADR-18, SRC-II-9
[[nodiscard]] std::string_view discriminant_of(std::string_view name) noexcept;

// post: byte-identical to template_id_of(canonicalize_intent(name)); one call keeps intent_id
// co-located with its comparability version.
// invariant: a STRUCTURAL grouping key derived from the marker, never a retained value.
// refs: SRC-II-1, SRC-D-OTEL-1
[[nodiscard]] TemplateId intent_id_of(std::string_view name);

// invariant: location recognition lives on the FACADE, not here — it walks composed location rows
// a package ships, and the compose module imports this one.
// invariant: the three match families are canon ALGORITHMS; the file-naming vocabulary they match
// is package DATA.
// note: the stream rendering below is a diagnostic only; the wire path is render().
// refs: SRC-II-8, BIB:intent_identity
inline std::ostream& operator<<(std::ostream& out, const TemplateId& template_id)
{
    return out << render(template_id);
}

// invariant: the OTEL hex ids are hashed to fixed-width scalar PODs at the strategy seam, carried
// in memory, and NEVER serialized — the per-transaction hex is a cardinality bomb.
// invariant: a zero value means ABSENT, and the hash forces non-zero on any non-empty input, so
// absent and present can never collide.
// invariant: what is NOT built is consuming a declared edge as GROUND TRUTH rather than folding it
// into the inferred graph.
// refs: ADR-29, ADR-29.O1, SRC-D-OTEL-1, SRC-D-TIR-4
struct TraceId
{
    std::uint64_t value{};
    auto operator<=>(const TraceId&) const = default;
    bool operator==(const TraceId&) const = default;
};
struct SpanId
{
    std::uint64_t value{};
    auto operator<=>(const SpanId&) const = default;
    bool operator==(const SpanId&) const = default;
};

// invariant: every field is consumed downstream and none is serialized; present is true iff the
// record carried a trace_id, which is the graph-scoping key.
// invariant: span_id and parent_span_id carry the DECLARED causal vertex and edge, and the
// trace-scoping path does not read them.
// refs: ADR-29.D2, SRC-D-OTEL-1
struct OtelTraceContext
{
    bool present{false};
    bool has_parent{false};
    // invariant: a SPAN record declares causality and metalog routes it to the observed DAG; a log
    // record with trace context carries positional causality and goes to the adjacency ring.
    // refs: SRC-D-OTEL-11
    bool is_span{false};
    TraceId trace_id{};
    SpanId span_id{};
    SpanId parent_span_id{};
};

// post: fnv1a-64 of the OTEL hex — same hex, same id, on any run; byte-only, so cross-stdlib
// bit-identical, and no float.
// invariant: 0 is reserved for ABSENT, so a non-empty hex hashing to 0 is bumped to 1.
// invariant: the hex itself is consumed and discarded, never retained.
[[nodiscard]] constexpr std::uint64_t otel_id_hash(std::string_view hex) noexcept
{
    constexpr std::uint64_t kFnvOffsetBasis{0xcbf29ce484222325ULL};
    constexpr std::uint64_t kFnvPrime{0x00000100000001b3ULL};
    std::uint64_t hash{kFnvOffsetBasis};
    for (const char character : hex)
    {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(character));
        hash *= kFnvPrime;
    }
    return hash == 0U ? 1U : hash;
}
[[nodiscard]] constexpr TraceId trace_id_from_hex(std::string_view hex) noexcept
{
    return TraceId{otel_id_hash(hex)};
}
[[nodiscard]] constexpr SpanId span_id_from_hex(std::string_view hex) noexcept
{
    return SpanId{otel_id_hash(hex)};
}

// invariant: OTLP fields are SCHEMA-DECLARED rather than data-learned, so they need no registry and
// stay core as a structured catalog rather than scattered inline predicates.
// invariant: the three trace keys route to consumed structural metadata, dropped from the template
// and never tokenized; severity_number routes to the LogLevel band.
// refs: ADR-17, SRC-D-OTEL-1, SRC-D-OTEL-4a, SRC-D-TID-6
enum class OtelFieldClass : std::uint8_t
{
    TraceId,
    SpanId,
    ParentSpanId,
    SeverityNumber,
};

struct OtelFieldDescriptor
{
    OtelFieldClass field_class;
    std::string_view key;
};

// invariant: keys are EXACT because OTLP is a declared schema.
// invariant: the span kind field is ABSENT deliberately — it is categorical, so carrying it needs
// a value_counts channel canon does not have.
// invariant: routing it through an existing channel would fabricate an ordinal or smuggle a
// vocabulary into a semantic-unaware core, so the honest state is absent.
// refs: SRC-D-OTEL-18b
inline constexpr std::array<OtelFieldDescriptor, 4> kOtelFieldCatalog{{
    {.field_class = OtelFieldClass::TraceId, .key = "traceId"},
    {.field_class = OtelFieldClass::SpanId, .key = "spanId"},
    {.field_class = OtelFieldClass::ParentSpanId, .key = "parentSpanId"},
    {.field_class = OtelFieldClass::SeverityNumber, .key = "severityNumber"},
}};

// invariant: a declared, registry-free catalog of numeric fields whose VALUE is ordinal, recognized
// by EXACT top-level field name — no suffix match, no pattern, no value guessing.
// invariant: a hit is captured as a consumed-not-tokenized observation and NEVER a param; metalog
// bins it per schedule into the W1 carrier.
// invariant: names are unit-explicit, so each value's unit is unambiguous at the key.
// invariant: the SCHEDULE a field bins onto is a VERSIONED catalog, and its stable string id is the
// eidos diff's comparability key.
// refs: SRC-D-W1-2, SRC-D-W1-3, SRC-D-W1-4, SRC-D-W1-5, SRC-D-W1-8, SRC-D-TID-6, SRC-D-TID-14
enum class OrdinalSchedule : std::uint8_t
{
    DurationLog2Ns,
    SizeLog2Bytes,
};

// invariant: canon carries the stable schedule id and the bin count and NEVER bins — the ladder
// map itself lives metalog-side, which owns binning.
// refs: SRC-D-W1-2
struct OrdinalScheduleSpec
{
    OrdinalSchedule schedule;
    std::string_view schedule_id;
    std::uint32_t bin_count;
};
inline constexpr std::array<OrdinalScheduleSpec, 2> kOrdinalScheduleCatalog{{
    {.schedule = OrdinalSchedule::DurationLog2Ns,
     .schedule_id = "dur-log2-ns-v1",
     .bin_count = 48U},
    {.schedule = OrdinalSchedule::SizeLog2Bytes,
     .schedule_id = "size-log2-bytes-v1",
     .bin_count = 48U},
}};

[[nodiscard]] constexpr std::string_view ordinal_schedule_id(OrdinalSchedule schedule) noexcept
{
    for (const auto& spec : kOrdinalScheduleCatalog)
        if (spec.schedule == schedule)
            return spec.schedule_id;
    return {};
}
[[nodiscard]] constexpr std::uint32_t ordinal_schedule_bins(OrdinalSchedule schedule) noexcept
{
    for (const auto& spec : kOrdinalScheduleCatalog)
        if (spec.schedule == schedule)
            return spec.bin_count;
    return 0U;
}

// invariant: every scale factor is a power of ten (or 1), so the decimal-to-fixed-point parse is
// EXACT and reads the JSON number's decimal TEXT rather than a double.
// invariant: a get_double()-then-cast would be the forbidden float-to-int on the
// deterministic-content path.
// refs: SRC-D-W1-3
struct OrdinalFieldDescriptor
{
    std::string_view key;
    OrdinalSchedule schedule;
    std::int64_t scale_to_canonical;
};
inline constexpr std::int64_t kNanosPerMicro{1'000};
inline constexpr std::int64_t kNanosPerMilli{1'000'000};
inline constexpr std::int64_t kNanosPerSecond{1'000'000'000};
inline constexpr std::array<OrdinalFieldDescriptor, 15> kOrdinalFieldCatalog{{
    // invariant: computed by the flat-span parser as endTime minus startTime, already integer
    // nanoseconds; the key also self-matches a literal field if a log carries one.
    // refs: ADR-29, SRC-D-OTEL-12
    {.key = "span_duration_ns",
     .schedule = OrdinalSchedule::DurationLog2Ns,
     .scale_to_canonical = 1},
    {.key = "latency_ms",
     .schedule = OrdinalSchedule::DurationLog2Ns,
     .scale_to_canonical = kNanosPerMilli},
    {.key = "duration_ms",
     .schedule = OrdinalSchedule::DurationLog2Ns,
     .scale_to_canonical = kNanosPerMilli},
    {.key = "elapsed_ms",
     .schedule = OrdinalSchedule::DurationLog2Ns,
     .scale_to_canonical = kNanosPerMilli},
    {.key = "response_time_ms",
     .schedule = OrdinalSchedule::DurationLog2Ns,
     .scale_to_canonical = kNanosPerMilli},
    {.key = "latency_us",
     .schedule = OrdinalSchedule::DurationLog2Ns,
     .scale_to_canonical = kNanosPerMicro},
    {.key = "duration_us",
     .schedule = OrdinalSchedule::DurationLog2Ns,
     .scale_to_canonical = kNanosPerMicro},
    {.key = "latency_ns", .schedule = OrdinalSchedule::DurationLog2Ns, .scale_to_canonical = 1},
    {.key = "duration_ns", .schedule = OrdinalSchedule::DurationLog2Ns, .scale_to_canonical = 1},
    {.key = "duration_seconds",
     .schedule = OrdinalSchedule::DurationLog2Ns,
     .scale_to_canonical = kNanosPerSecond},
    {.key = "elapsed_seconds",
     .schedule = OrdinalSchedule::DurationLog2Ns,
     .scale_to_canonical = kNanosPerSecond},
    {.key = "response_bytes", .schedule = OrdinalSchedule::SizeLog2Bytes, .scale_to_canonical = 1},
    {.key = "request_bytes", .schedule = OrdinalSchedule::SizeLog2Bytes, .scale_to_canonical = 1},
    {.key = "size_bytes", .schedule = OrdinalSchedule::SizeLog2Bytes, .scale_to_canonical = 1},
    {.key = "payload_bytes", .schedule = OrdinalSchedule::SizeLog2Bytes, .scale_to_canonical = 1},
}};

[[nodiscard]] constexpr const OrdinalFieldDescriptor*
match_ordinal_field(std::string_view key) noexcept
{
    for (const auto& descriptor : kOrdinalFieldCatalog)
        if (descriptor.key == key)
            return &descriptor;
    return nullptr;
}

// pre: scale MUST be a power of ten, or 1.
// post: the value in the schedule's canonical unit, or nullopt on a malformed, negative,
// exponent-bearing or overflowing token — the observation is then omitted.
// invariant: integer and decimal-string arithmetic only, never via double; fractional digits beyond
// the scale's decimal places truncate, deterministically.
// refs: SRC-D-W1-3
[[nodiscard]] constexpr std::optional<std::int64_t>
parse_decimal_scaled(std::string_view text, std::int64_t scale) noexcept
{
    constexpr std::string_view kTrim{" \t\r\n,}]"};
    while (!text.empty() && kTrim.find(text.front()) != std::string_view::npos)
        text.remove_prefix(1);
    while (!text.empty() && kTrim.find(text.back()) != std::string_view::npos)
        text.remove_suffix(1);
    if (text.empty())
        return std::nullopt;
    constexpr std::int64_t kIntMax{std::numeric_limits<std::int64_t>::max()};
    constexpr std::int64_t kRadix{10};
    std::size_t idx{0};
    std::int64_t value{0};
    bool any_digit{false};
    for (; idx < text.size() && text[idx] >= '0' && text[idx] <= '9'; ++idx)
    {
        any_digit = true;
        const std::int64_t digit{text[idx] - '0'};
        if (value > (kIntMax - digit) / kRadix)
            return std::nullopt;
        value = (value * kRadix) + digit;
    }
    if (scale != 0 && value > kIntMax / scale)
        return std::nullopt;
    value *= scale;
    if (idx < text.size() && text[idx] == '.')
    {
        ++idx;
        std::int64_t frac_scale{scale / kRadix};
        for (; idx < text.size() && text[idx] >= '0' && text[idx] <= '9'; ++idx)
        {
            any_digit = true;
            if (frac_scale == 0)
                continue;
            const std::int64_t contribution{(text[idx] - '0') * frac_scale};
            if (value > kIntMax - contribution)
                return std::nullopt;
            value += contribution;
            frac_scale /= kRadix;
        }
    }
    if (idx != text.size() || !any_digit)
        return std::nullopt;
    return value;
}

// invariant: consumed-not-tokenized — carried on CanonicalEvent beside params and trace, and
// NEVER serialized as a param.
// invariant: field_name is the catalog's STATIC key, stable for the program lifetime and not
// arena-backed, which is what lets a diff row attribute to it.
// refs: SRC-D-W1-3
struct OrdinalObservation
{
    std::string_view field_name;
    OrdinalSchedule schedule;
    std::int64_t value{0};
};

enum class LogLevel : uint8_t
{
    Trace,
    Debug,
    Info,
    Warn,
    Error,
    Fatal,
    Unknown
};

[[nodiscard]] constexpr std::string_view to_string(LogLevel level) noexcept
{
    using namespace std::literals;

    switch (level)
    {
    case LogLevel::Trace:
        return "Trace"sv;
    case LogLevel::Debug:
        return "Debug"sv;
    case LogLevel::Info:
        return "Info"sv;
    case LogLevel::Warn:
        return "Warn"sv;
    case LogLevel::Error:
        return "Error"sv;
    case LogLevel::Fatal:
        return "Fatal"sv;
    default:
        return "Unknown"sv;
    }
}

// invariant: a level and the PROVENANCE of that level are assigned together and are not
// independently settable — a bool beside a LogLevel is two fields a writer can set one of.
// invariant: canon has a DECLARED layer (schema field, syslog priority, OTLP severity_number, a
// dialect's announced marker) and, where nothing is declared, a CONTENT-INFERENCE layer.
// invariant: the inference layer reads WORDS and is meant to guess; this type is the ERROR TERM, so
// a guess never arrives downstream spelled identically to a fact.
// invariant: inferred is the DEFAULT species and the default-constructed state, so a writer who
// forgets UNDER-claims; the opposite default would silently promote a guess to a fact.
// invariant: there is deliberately NO implicit conversion from LogLevel — constraining WRITERS is
// the point, and readers keep == against a bare LogLevel and value().
// refs: DN-32.D3, DN-29.D14, ADR-22.D3
class EventLevel
{
  public:
    EventLevel() = default;

    // post: the INFERRED species — a leading word that parses as a level, or a failure cue in the
    // head; the default species and what the whole content-inference layer produces.
    [[nodiscard]] static constexpr EventLevel inferred(LogLevel value) noexcept
    {
        EventLevel out;
        out.value_ = value;
        return out;
    }

    // post: the DECLARED species — a position whose MEANING is the level, never content that
    // resembles one; the same species of fact as a run's outcome.
    [[nodiscard]] static constexpr EventLevel declared(LogLevel value) noexcept
    {
        EventLevel out;
        out.value_ = value;
        out.declared_ = true;
        return out;
    }

    [[nodiscard]] constexpr LogLevel value() const noexcept
    {
        return value_;
    }

    // invariant: true only for an actually-declared, actually-present level; Unknown is this type's
    // absence and an absent level is never declared, so this cannot disagree with value().
    // invariant: that is what lets declared(parse_log_level(bytes)) stay one honest expression when
    // the declared field held a token canon does not know.
    [[nodiscard]] constexpr bool is_declared() const noexcept
    {
        return declared_ && value_ != LogLevel::Unknown;
    }

    // note: read forwarding — a comparison against a bare LogLevel reads as it always did.
    [[nodiscard]] friend constexpr bool operator==(EventLevel lhs, LogLevel rhs) noexcept
    {
        return lhs.value_ == rhs;
    }
    [[nodiscard]] friend constexpr bool operator==(EventLevel lhs, EventLevel rhs) noexcept
    {
        return lhs.value_ == rhs.value_ && lhs.declared_ == rhs.declared_;
    }

  private:
    LogLevel value_{LogLevel::Unknown};
    bool declared_{false};
};

// note: the algebra asserted where the type is; a test can be deleted, this cannot.
static_assert(!EventLevel{}.is_declared(),
              "the default-constructed species must be UNDECLARED — a writer that forgets must "
              "under-claim, never promote a guess to a fact");
static_assert(EventLevel{}.value() == LogLevel::Unknown, "the default species must be absent");
static_assert(!EventLevel::inferred(LogLevel::Error).is_declared(),
              "a level read out of message content is never declared, whatever it reads");
static_assert(EventLevel::declared(LogLevel::Error).is_declared());
static_assert(!EventLevel::declared(LogLevel::Unknown).is_declared(),
              "an ABSENT level is never declared — so `declared(parse_log_level(bytes))` stays one "
              "honest expression when the declared field held a token canon does not know");
static_assert(EventLevel::inferred(LogLevel::Error) == LogLevel::Error,
              "read forwarding: `== LogLevel::X` must ignore provenance, or every existing reader "
              "silently changes meaning");
static_assert(!(EventLevel::inferred(LogLevel::Error) == EventLevel::declared(LogLevel::Error)),
              "two EventLevels are equal only if BOTH halves agree — the whole point is that these "
              "two are not the same fact");
static_assert(std::is_trivially_copyable_v<EventLevel>,
              "CanonicalEvent/ParsedLine are hot-path aggregates; this must stay a 2-byte value");

// invariant: a RUN-level verdict — never a field on CanonicalEvent, and never a cube dimension:
// outcome is the run LABEL, not an axis.
// invariant: the five categories are UNIVERSAL; the per-dialect strings that name them are
// semantic-package data.
// invariant: Unstable is neither Success nor Failure and is NEVER folded into either; Aborted means
// the log is INCOMPLETE and is never read as pass or fail.
// refs: ADR-17
enum class RunOutcome : std::uint8_t
{
    Unknown = 0,
    Success,
    Failure,
    Unstable,
    Aborted,
};

[[nodiscard]] constexpr std::string_view to_string(RunOutcome outcome) noexcept
{
    using namespace std::literals;

    switch (outcome)
    {
    case RunOutcome::Success:
        return "SUCCESS"sv;
    case RunOutcome::Failure:
        return "FAILURE"sv;
    case RunOutcome::Unstable:
        return "UNSTABLE"sv;
    case RunOutcome::Aborted:
        return "ABORTED"sv;
    default:
        return "UNKNOWN"sv;
    }
}

// post: true iff the verdict got strictly worse on the pass-fail axis Success < Unstable < Failure.
// invariant: Aborted and Unknown are EXCLUDED because they are not points on that axis, so no
// transition touching them is ever an outcome regression.
// invariant: deterministic integer compare, and the single-source predicate the CLI gate, the check
// and the comment verdict all read.
[[nodiscard]] constexpr bool outcome_regressed(RunOutcome baseline, RunOutcome changed) noexcept
{
    constexpr auto axis_rank{[](RunOutcome outcome) noexcept -> int
                             {
                                 switch (outcome)
                                 {
                                 case RunOutcome::Success:
                                     return 0;
                                 case RunOutcome::Unstable:
                                     return 1;
                                 case RunOutcome::Failure:
                                     return 2;
                                 default:
                                     return -1;
                                 }
                             }};
    const int baseline_rank{axis_rank(baseline)};
    const int changed_rank{axis_rank(changed)};
    return baseline_rank >= 0 && changed_rank >= 0 && changed_rank > baseline_rank;
}

// post: the OTEL severity_number folds into canon's six levels by integer division; below 1 clamps
// to Trace, above 24 clamps to Fatal.
// invariant: integer-only, no float.
// invariant: this is the DECLARED severity channel for OTEL inputs and outranks the failure lexicon
// when present.
// invariant: canon keeps its own six-level model and DISCARDS the raw 1-24 number — the 24-band
// granularity is deliberately not inherited.
// refs: ADR-29, SRC-D-OTEL-1, SRC-D-OTEL-8
[[nodiscard]] constexpr LogLevel
log_level_from_severity_number(std::int64_t severity_number) noexcept
{
    if (severity_number < 1)
        return LogLevel::Trace;
    static constexpr std::int64_t kBandWidth{4};
    switch ((severity_number - 1) / kBandWidth)
    {
    case 0:
        return LogLevel::Trace;
    case 1:
        return LogLevel::Debug;
    case 2:
        return LogLevel::Info;
    case 3:
        return LogLevel::Warn;
    case 4:
        return LogLevel::Error;
    default:
        return LogLevel::Fatal;
    }
}

enum class LogFormat : uint8_t
{
    Syslog,
    JSON,
    KeyValue,
    CLF,
    Log4j,
    SparkHDFS,
    BGL,
    AndroidLogcat,
    ApacheError,
    WindowsCBS,
    HealthApp,
    Proxifier,
    HPC,
    CloudWatch,
    SystemdJournal,
    IISW3C,
    RFC5424,
    NginxError,
    // invariant: a first-class CI input — without its own member these lines are mis-claimed by
    // Syslog on the RFC3339 prefix and shredded into empty templates.
    GitHubActions,
    // invariant: the Jenkins member is GONE — its bracket stamp is DECLARED catalogue transport
    // and its two verdict legs are dialect-gated rows core walks under a jenkins dialect.
    // invariant: no wire field and no wire token carries this enum's numeric values, so a member
    // shift is invisible outside a single build graph.
    GitLab,
    // invariant: a core REPRESENTATION format, not a dialect — it names no ecosystem.
    // invariant: an RFC-3339 prefix is evidence of a TIMESTAMP and not of syslog; splitting it out
    // keeps the event time while level and component become what the bytes actually say.
    // refs: DN-43.D4
    Rfc3339Text,
    // invariant: selected only when no structured strategy matches a NON-EMPTY line, so the
    // tokenizer never silently drops a line.
    // invariant: what the code binds is that Unknown stays LAST — the detector sizes an array
    // indexed by this enum at Unknown + 1, so a member appended after it under-sizes that array.
    // note: a new member goes immediately before RawText by CONVENTION, not by constraint.
    // refs: OPS-2
    RawText,
    Unknown
};

[[nodiscard]] constexpr std::string_view to_string(LogFormat fmt) noexcept
{
    using namespace std::literals;

    switch (fmt)
    {
    case LogFormat::Syslog:
        return "Syslog"sv;
    case LogFormat::JSON:
        return "JSON"sv;
    case LogFormat::KeyValue:
        return "KeyValue"sv;
    case LogFormat::CLF:
        return "CLF"sv;
    case LogFormat::Log4j:
        return "Log4j"sv;
    case LogFormat::SparkHDFS:
        return "SparkHDFS"sv;
    case LogFormat::BGL:
        return "BGL"sv;
    case LogFormat::AndroidLogcat:
        return "AndroidLogcat"sv;
    case LogFormat::ApacheError:
        return "ApacheError"sv;
    case LogFormat::WindowsCBS:
        return "WindowsCBS"sv;
    case LogFormat::HealthApp:
        return "HealthApp"sv;
    case LogFormat::Proxifier:
        return "Proxifier"sv;
    case LogFormat::HPC:
        return "HPC"sv;
    case LogFormat::CloudWatch:
        return "CloudWatch"sv;
    case LogFormat::SystemdJournal:
        return "SystemdJournal"sv;
    case LogFormat::IISW3C:
        return "IISW3C"sv;
    case LogFormat::RFC5424:
        return "RFC5424"sv;
    case LogFormat::NginxError:
        return "NginxError"sv;
    case LogFormat::GitHubActions:
        return "GitHubActions"sv;
    case LogFormat::GitLab:
        return "GitLab"sv;
    case LogFormat::Rfc3339Text:
        return "Rfc3339Text"sv;
    case LogFormat::RawText:
        return "RawText"sv;
    default:
        return "Unknown"sv;
    }
}

// invariant: what a LINE does in the sequence, as opposed to what a token inside it MEANS — two
// orthogonal ontologies and two registries, which is what avoids conflating them.
// invariant: these roles are ANNOUNCED by a marker the line carries, and are NEVER derived from
// graph position, which is a structural-layer output rather than a role.
enum class StructuralRole : uint8_t
{
    None = 0,
    GroupBegin,
    GroupEnd,
    Terminator
};

[[nodiscard]] constexpr std::string_view to_string(StructuralRole role) noexcept
{
    using namespace std::literals;

    switch (role)
    {
    case StructuralRole::GroupBegin:
        return "GroupBegin"sv;
    case StructuralRole::GroupEnd:
        return "GroupEnd"sv;
    case StructuralRole::Terminator:
        return "Terminator"sv;
    default:
        return "None"sv;
    }
}

} // namespace insight

// invariant: the content is already a uniform cryptographic hash, so the first 8 bytes ARE a good
// size_t — no mixing, no allocation.
// invariant: reachable to importers of this module, so an unordered_map keyed on it resolves
// downstream; a specialization need not be exported, importing the module suffices.
// refs: SRC-D-TIR-1
namespace std
{
template <> struct hash<insight::TemplateId>
{
    [[nodiscard]] std::size_t operator()(const insight::TemplateId& template_id) const noexcept
    {
        std::size_t out{};
        std::memcpy(&out, template_id.bytes.data(), sizeof out);
        return out;
    }
};

// invariant: already a uniform 128-bit hash, so the first 8 bytes ARE a good size_t.
// invariant: the n-gram maps re-sort their output, so unordered iteration order is not a
// determinism surface here.
// refs: ADR-16, SRC-D-TIR-4
template <> struct hash<insight::NgramId>
{
    [[nodiscard]] std::size_t operator()(const insight::NgramId& ngram_id) const noexcept
    {
        std::size_t out{};
        std::memcpy(&out, ngram_id.bytes.data(), sizeof out);
        return out;
    }
};

// invariant: value is already an fnv1a hash of the OTEL hex, so it IS a good size_t — no mixing.
// refs: SRC-D-OTEL-1
template <> struct hash<insight::TraceId>
{
    [[nodiscard]] std::size_t operator()(const insight::TraceId& trace_id) const noexcept
    {
        return static_cast<std::size_t>(trace_id.value);
    }
};
// invariant: keys the per-window span-to-template map metalog resolves observed edges through at
// window close.
// refs: SRC-D-OTEL-11
template <> struct hash<insight::SpanId>
{
    [[nodiscard]] std::size_t operator()(const insight::SpanId& span_id) const noexcept
    {
        return static_cast<std::size_t>(span_id.value);
    }
};
} // namespace std

// invariant: IEEE add, subtract, multiply, divide and sqrt are already cross-machine deterministic;
// the two divergence sources are transcendentals and float sum order.
// invariant: this removes both — the logarithms are pure integer, and the reduction accumulates
// in a signed 128-bit integer, which is exact and associative.
// invariant: det is an API-ONLY domain: inline-in-interface IS the determinism guarantee, because a
// consumer must compile these bodies under its own -ffp-contract=off.
// invariant: it therefore has no src or detail unit by design, and its tests having no src
// counterpart is the expected shape rather than drift.
export namespace insight::det
{

// invariant: native 128-bit on gcc and clang, a pure-C++ constexpr equivalent on MSVC, with the
// same two's-complement semantics either way — that is the cross-OS determinism leg.
using u128 = detail::u128;
using i128 = detail::i128;

// invariant: a stored value v represents v over 2^kFracBits; 40 fractional bits is far below the
// tolerances the entropy tests assert.
// invariant: it also keeps every magnitude produced under 2^53, so the final fixed-to-double
// conversion is EXACT — which is what makes the emitted double bit-identical.
inline constexpr unsigned int kFracBits{40U};
inline constexpr std::int64_t kOne{static_cast<std::int64_t>(std::uint64_t{1} << kFracBits)};

// note: a constant with a documented derivation; the det_math test pins it.
inline constexpr std::int64_t kLn2Fixed{762123384786};

// pre: value >= 1 — in these reductions log2 is applied only to counts, totals and products of
// positive integers; 0 is a caller bug, mapped to 0 to keep the function total.
// post: round(log2(value) * 2^kFracBits) — pure integer arithmetic, round-to-nearest, no libm,
// identical on every compiler and architecture.
[[nodiscard]] constexpr std::int64_t det_log2_fixed(std::uint64_t value) noexcept
{
    if (value <= 1U)
        return 0;

    const unsigned msb{static_cast<unsigned>(63 - std::countl_zero(value))};

    // invariant: working in Q(kFracBits + kGuard) is what lets the kFracBits-bit fraction round to
    // nearest.
    constexpr unsigned kGuard{2U};
    constexpr unsigned kWork{kFracBits + kGuard};
    u128 mantissa{(static_cast<u128>(value) << kWork) >> msb};
    const u128 two_work{u128{1} << (kWork + 1U)};

    // invariant: squaring the mantissa doubles its log2, and the carry out of [1,2) is the next
    // fraction bit.
    std::uint64_t frac{0};
    for (unsigned bit{0}; bit < kWork; ++bit)
    {
        mantissa = (mantissa * mantissa) >> kWork;
        frac <<= 1U;
        if (mantissa >= two_work)
        {
            frac |= 1U;
            mantissa >>= 1U;
        }
    }
    // invariant: log2 of a non-power-of-two is irrational, so an exact half never occurs and
    // round-half-up equals round-to-nearest-even here.
    const std::int64_t frac_rne{
        static_cast<std::int64_t>((frac + (std::uint64_t{1} << (kGuard - 1))) >> kGuard)};
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(msb) << kFracBits) + frac_rne;
}

// post: ln(value) in the same fixed-point scale, computed as log2(value) times ln(2).
[[nodiscard]] constexpr std::int64_t det_ln_fixed(std::uint64_t value) noexcept
{
    const i128 product{i128{det_log2_fixed(value)} * i128{kLn2Fixed}};
    const u128 rounded{static_cast<u128>(product) + u128{std::uint64_t{1} << (kFracBits - 1U)}};
    return static_cast<std::int64_t>(rounded >> kFracBits);
}

// post: an EXACT conversion — int64 to double is lossless below 2^53 and the divisor is a power
// of two, so no rounding enters and the result is bit-identical everywhere.
[[nodiscard]] constexpr double fixed_to_double(std::int64_t fixed) noexcept
{
    return static_cast<double>(fixed) / static_cast<double>(kOne);
}

// pre: den > 0 (a count or a total); num may be negative, because KL and JS terms can be.
// post: round-half-up integer division, for the final normalisation.
[[nodiscard]] constexpr std::int64_t round_div(i128 num, std::int64_t den) noexcept
{
    const i128 den128{den};
    if (num >= i128{0})
        return static_cast<std::int64_t>((num + (den128 / i128{2})) / den128);
    return -static_cast<std::int64_t>(((-num) + (den128 / i128{2})) / den128);
}

// invariant: all accumulation is in a signed 128-bit INTEGER — exact and associative — so the
// result depends neither on summation order nor on float rounding.
// invariant: the caller adds terms in the canonical sorted-by-key order; for integer terms that
// order is immaterial to the value, and the contract keeps the discipline explicit.
class FixedReducer
{
  public:
    // post: adds weight times log2(value) — the defining term of entropy, KL and JS once
    // reformulated into integer-ratio form.
    constexpr void add_weighted_log2(std::uint64_t weight, std::uint64_t value) noexcept
    {
        // invariant: the weight widens VALUE-PRESERVING to 128-bit, matching a native cast and NOT
        // going through int64, which would sign-flip at 2^63 and above.
        acc_ += static_cast<i128>(u128{weight}) * i128{det_log2_fixed(value)};
    }

    constexpr void add_fixed(i128 term) noexcept
    {
        acc_ += term;
    }

    [[nodiscard]] constexpr i128 raw() const noexcept
    {
        return acc_;
    }

    // post: the accumulated sum normalised by a positive denominator and converted to bits — one
    // rounding division back to the fixed scale, then one exact fixed-to-double divide.
    [[nodiscard]] constexpr double normalized_bits(std::int64_t denom) const noexcept
    {
        return fixed_to_double(round_div(acc_, denom));
    }

  private:
    i128 acc_{0};
};

} // namespace insight::det

export namespace insight::tokenization
{

// invariant: every string view points into the arena passed to the Tokenizer, so every lifetime is
// bounded by that arena's reset or destruction.
// invariant: params is a span over an arena-allocated array, which keeps the event a fixed-size POD
// with zero per-event heap allocations on the hot path.
struct CanonicalEvent
{
    EventID id{};
    Timestamp timestamp;
    // invariant: true iff the producer DECLARED the time in a schema event-time field; false for
    // every line whose time was parsed from ambiguous bytes or absent.
    // invariant: a declared time outranks a transport observation stamp and a parsed one does not,
    // which is the difference the pipeline needs.
    // invariant: written ONLY in make_event beside the timestamp, off one field carrying both, so
    // the pair cannot be split on the way here; consumed in memory and never serialized.
    // invariant: NOT derivable from trace.is_span and it must never be re-derived that way — an
    // OTLP LOG record carries a declared time with is_span false and often no trace at all.
    // refs: DN-29.D12
    bool declared_timestamp{false};
    LogLevel level{LogLevel::Unknown};
    // invariant: true iff the level came from a position whose MEANING is the level; false for
    // every inferred or absent level.
    // invariant: a claim resting on the inference layer carries an error term a declared one does
    // not, and a consumer must see that WITHOUT re-reading the words.
    // invariant: re-deriving it downstream is how a tool-specific vocabulary denylist gets built,
    // and that denylist rots on the next tool.
    // invariant: written ONLY in make_event beside the level, off one field carrying both; consumed
    // in memory and never serialized.
    // invariant: the one-ness of that write site is held by a LINT rather than by the type, because
    // correct-today-and-held-by-discipline was the sentence that was wrong four times before.
    // refs: DN-32.D3, DN-29.D14
    bool declared_level{false};
    // invariant: observability metadata, NOT deterministic MetaLog content — downstream may group
    // or correlate by it; Unknown when no strategy matched.
    LogFormat format{LogFormat::Unknown};
    // invariant: the low-card FUNCTIONAL SOURCE and a cube dimension — never the node identity,
    // which is host.
    // refs: ADR-19.D4
    std::string_view component;
    // invariant: the high-card node IDENTITY — kept for correlation and grouping, but HORS-CUBE:
    // never a cube dimension. Empty when the format carries none.
    // refs: ADR-19.D4
    std::string_view host;
    std::string_view template_str;
    std::span<const std::string_view> params;
    // invariant: what this line DOES in the sequence, orthogonal both to template_str (what the
    // line IS) and to the semantic class of the tokens inside it.
    StructuralRole structural_role{StructuralRole::None};
    // invariant: consumed in memory and NEVER serialized, so the MetaLog wire shape is unchanged;
    // present is false for every non-OTEL input, so the cost is zero there.
    // refs: ADR-29, SRC-D-OTEL-1, SRC-D-OTEL-11
    OtelTraceContext trace{};
    // invariant: consumed-not-tokenized — metalog bins these per schedule into the W1 carrier and
    // they are NEVER params.
    // invariant: a span over arena-allocated storage, EMPTY for every non-ordinal line, so the hot
    // path pays nothing input-conditionally.
    // refs: SRC-D-W1-3
    std::span<const OrdinalObservation> ordinals;
    // invariant: the span ids this span DECLARES a cross-trace edge to; each resolves metalog-side,
    // by span id and across traces, into the SAME distilled topology as intra-trace parentage.
    // invariant: a span over arena-allocated storage, EMPTY for every span without links and every
    // non-span line; never retained, never serialized.
    // refs: ADR-29, SRC-D-OTEL-9, SRC-D-OTEL-21
    std::span<const SpanId> linked_span_ids;
    // invariant: the line is echoed program or script SOURCE rather than an observed runtime event,
    // recognized at the ANSI strip layer by the command-echo SGR wrapper.
    // invariant: consumed in memory and never serialized — it already demoted the level in the
    // parser, and metalog skips the level-blind salience tier for an all-echoed template.
    // refs: SRC-D-PROV-1
    bool echoed_source{false};
    // invariant: EMPTY when the parse recognized at least one declared role; NON-EMPTY when it
    // recognized none, holding a witness key that WAS present in the input.
    // invariant: the strategy writes it on ParsedLine and make_event copies it HERE, because this
    // is where the guarantee binds — a consumer never sees the intermediate.
    // invariant: the record path therefore never silently emits an event for input it understood
    // nothing of — a marker stopping at the intermediate would leave the event unmarked.
    // invariant: a view into arena-stable bytes, consumed in memory and never serialized.
    // invariant: a STATEMENT and never a verdict — a marked event is still analysed, and no
    // consumer may drop it on this field alone.
    // refs: DN-29.D16
    std::string_view no_role_witness_key;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

// invariant: a mask rule may classify a SYNTACTIC token class only — decidable from the token's
// own bytes, one token, no lexicon; varying WORDS stay literal.
// invariant: that boundary is what stops the masker becoming a vocabulary — every widening that
// needs a list of words belongs to the value-class registry, which no package populates.
// invariant: byte-only single-token rules are also the only shape that is cross-stdlib identical.
// invariant: the Drain clustering knobs are gone with the clustering — a stateless masker has no
// tree, no similarity match and no cluster cap to bound.
// refs: SRC-D-TID-14, SRC-D-TID-3, ADR-17
struct MaskConfig
{
    // invariant: structurally variable tokens are replaced before the masked template is formed, so
    // they never fossilise into the template identity.
    // invariant: there is deliberately NO hex knob — its acceptor required a leading 0, so every
    // token it accepted was already digit-leading and the knob was inert over ALL inputs.
    // invariant: the IP knob stays because its grammar admits a leading bracket, and a bracketed
    // token is not digit-leading, so it genuinely gates.
    // refs: DN-27
    bool mask_ip_addresses{true};
    // invariant: when set, a line whose NATIVE component is empty takes its recognized test-file as
    // component — populating the cube WHERE axis above the empty native tier, never faking it.
    // invariant: OFF by default so every existing path is byte-identical; the gated block moves no
    // default-path output and no golden.
    // invariant: it does NOT keep the canonicalization generation — the generation names the
    // RULES FUNCTION over the whole config space, not the default slice of it.
    // refs: SRC-II-8, BIB:intent_identity
    bool recognize_test_where{false};
};

} // namespace insight::tokenization

// invariant: canon core is semantic-unaware — it owns the recognition ALGORITHM and the semantic
// packages own the rule ROWS.
// invariant: the registry CLASSES live on the facade's composed walkers; the result TYPES stay
// here, because the spi rows and every downstream consumer reference them.
// refs: ADR-17, SRC-SP-1
export namespace insight::tokenization
{

// invariant: the RESET-class markers that open a behavioural quantum on the stripped content stream
// — a job banner names the job-scoped parent, a step banner opens the step within it.
// invariant: the payload is the RAW name; canonicalize_intent turns it into the alignment CLASS,
// which is what pairs a matrix leg across runs.
// invariant: nothing CLOSES a quantum — it runs until the next RESET-class marker opens one, or
// until the stream ends.
// invariant: the step banner is DIALECT-GATED because its prefix is runner-specific and would
// misfire on an ordinary content line elsewhere, unlike the universal group markers.
// invariant: it is CHANNEL-gated too, and that gate carries the measurement: in the annotated
// channel the same prefix is ordinary prose, so an ungated row mints phantom step quanta.
// invariant: measured on real annotated bytes at 9.05 % of 22 030 logs — 7 752 lines over 62
// distinct payloads, every one prose — and confirmed end to end to fabricate a vanished phase.
// invariant: a phantom quantum fails to align rather than silently mispairing, which is why the
// residual cost is a low-severity pair and never a wrong match.
// invariant: deterministic, ASCII-safe, no cross-line state.
// refs: SRC-II-2, SRC-II-6, BIB:intent_identity, STU-4
// refs: F-SRC-insight-canon:github.dialect.yaml
enum class IntentMarkerKind : std::uint8_t
{
    None = 0,
    Job,
    Step
};

// invariant: how a level's sibling nodes are matched across two runs — a DECLARED property of the
// dialect level, never a runtime heuristic.
// invariant: a parallel-by-construction level takes a set match, so a completion interleave is
// invisible.
// invariant: a sequential-by-declaration level takes an order-respecting match, so a transposition
// IS a signal.
// invariant: it is package DATA on a marker row, so it enters the composed semantic_identity.
// refs: ADR-18, ADR-17
enum class ChildOrder : std::uint8_t
{
    Ordered = 0,
    Unordered
};

struct IntentMarker
{
    IntentMarkerKind kind{IntentMarkerKind::None};
    std::string_view name;
    // invariant: the raw discriminant kept VERBATIM and never masked — the stable declared
    // coordinate separating co-occurring siblings. Empty when the name carries no tuple.
    // refs: ADR-18, SRC-II-9
    std::string_view discriminant;
    ChildOrder child_order{ChildOrder::Ordered};
    auto operator<=>(const IntentMarker&) const = default;
    bool operator==(const IntentMarker&) const = default;
};

} // namespace insight::tokenization

export namespace insight::tokenization
{

struct ArenaNumaPolicy
{
    enum class Kind : std::uint8_t
    {
        Disabled,
        Fixed,
        Auto,
    };

    Kind kind{Kind::Disabled};
    int node{-1};

    [[nodiscard]] constexpr bool active() const noexcept
    {
        return kind != Kind::Disabled;
    }
};

[[nodiscard]] bool arena_numa_supported() noexcept;
[[nodiscard]] int arena_numa_node_count() noexcept;

class ArenaAllocator
{
  public:
    static constexpr std::size_t kDefaultBlockAlignment{64};

    explicit ArenaAllocator(std::size_t initial_block_size, ArenaNumaPolicy policy = {});
    ~ArenaAllocator();

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&& other) noexcept;
    ArenaAllocator& operator=(ArenaAllocator&& other) noexcept;

    [[nodiscard]] void* allocate(std::size_t size,
                                 std::size_t alignment = alignof(std::max_align_t));
    [[nodiscard]] std::string_view store_string(std::string_view str);
    void reset() noexcept;
    [[nodiscard]] std::size_t used() const noexcept;
    [[nodiscard]] std::size_t capacity() const noexcept;
    [[nodiscard]] std::size_t initial_block_size() const noexcept;
    [[nodiscard]] std::size_t block_count() const noexcept;
    [[nodiscard]] const ArenaNumaPolicy& numa_policy() const noexcept;
    [[nodiscard]] bool owns(const void* ptr) const noexcept;

  private:
    struct Block
    {
        Block() noexcept = default;
        Block(std::byte* storage, std::size_t size, std::size_t alignment,
              bool numa_allocated) noexcept;
        ~Block() noexcept;

        Block(const Block&) = delete;
        Block& operator=(const Block&) = delete;
        Block(Block&& other) noexcept;
        Block& operator=(Block&& other) noexcept;

        void reset() noexcept;

        std::byte* storage{nullptr};
        std::size_t size{0};
        std::size_t offset{0};
        std::size_t alignment{kDefaultBlockAlignment};
        bool numa_allocated{false};
    };

    [[nodiscard]] Block make_block(std::size_t bytes, std::size_t alignment) const;
    void grow_to_fit(std::size_t size, std::size_t alignment);

    std::vector<Block> blocks_;
    std::size_t active_index_{0};
    std::size_t bytes_used_{0};
    std::size_t initial_block_size_{0};
    ArenaNumaPolicy policy_{};
};

// post: true only in a POISONING build, where reset overwrites the bytes it releases instead of
// merely rewinding the bump pointer — which is what makes a use-after-reset observable.
// invariant: a RUNTIME query on purpose: the switch is private to canon's translation units, so a
// constant evaluated in a consumer's compile would report the CONSUMER's flags.
// invariant: under a rewinding reset a stale event still reads its own bytes, so a downstream
// lifetime gate cannot tell a correct lifetime from a lucky one.
// invariant: such a gate is therefore COMPILE-TIME gated on canon's exported availability
// definition and asserts this query inside it — a runtime skip exits 0 and counts as PASSED.
[[nodiscard]] bool arena_poisons_on_reset() noexcept;
} // namespace insight::tokenization

export namespace insight::utils
{

// invariant: the single source of truth shared by canon's raw-text level inference and the MetaLog
// severity signal, so the two can never disagree on what a cue is.
// invariant: a cue matches ONLY as a standalone whitespace-delimited token — punctuation trimmed,
// ASCII case-insensitive — or as a CamelCase error type, or as a fixed two-token phrase.
// invariant: a lexicon word buried INSIDE a larger token does not match; that raw-substring
// over-match promoted benign new templates to a high-severity new-error verdict.
// invariant: the lexicon partitions by benign-collision-proneness — a zero-collision token fires
// bare in prose, a collision-prone one fires ONLY when verdict-anchored.
// invariant: two precision guards keep a PASSING test from ever reading as a failure: a negated
// type name is not an error type, and a declared pass verdict demotes a bare type name.
// invariant: a line carrying a real failure cue but LED by a pass glyph is a passing test whose
// name embeds failure vocabulary, not a regression.
// invariant: scan_limit bounds the head — a token must START within it and may extend past it; 0
// scans all of the text.
// invariant: alloc-free and noexcept, a single head-bounded pass, except the rare
// error-type-without-failure-word line, which costs one extra full scan.
// refs: ADR-20.D5, SRC-D-OUT-1, SRC-D-OUT-4, DN-54
[[nodiscard]] bool contains_failure_cue(std::string_view text, std::size_t scan_limit = 0) noexcept;
[[nodiscard]] bool contains_warning_cue(std::string_view text, std::size_t scan_limit = 0) noexcept;

namespace detail
{
    // post: true iff the line's leading outcome is a PASS — a pass GLYPH anywhere in the head, or
    // a leading pass WORD as the FIRST significant token.
    // invariant: the asymmetry is the whole rule — a glyph never occurs in prose so it fires
    // anywhere in the head, while a pass word does occur in prose so it must LEAD or not fire.
    // invariant: a leading failure WORD yields false, so a genuine error line is preserved, and a
    // counted summary yields false because a number is then the first significant token.
    // invariant: promoted out of the lexicon's anonymous namespace so the explicit-level stage in a
    // SEPARATE translation unit can consult the same predicate.
    // note: internal detail; the public API stays the two cue predicates.
    // refs: ADR-20.D5, SRC-D-OUT-1, SRC-D-OUT-1b, SRC-D-OUT-2
    [[nodiscard]] bool leading_outcome_is_pass(std::string_view line) noexcept;

    // post: MEMBERSHIP in the failure lexicon, never firing — case-insensitive, whole-token and
    // role-blind, so a self-anchoring word and a register-anchored one are both members.
    // invariant: the two questions differ by ORDER: the firing path consults the register only
    // AFTER a table match, so a word the table lacks is declined by nothing and is invisible.
    // invariant: NO product path calls this — it exists so a standing instrument can report which
    // of the two a residual line is without re-listing the vocabulary on its own side.
    // invariant: pure byte compare plus ASCII case fold, order-independent, so cross-stdlib
    // bit-identical.
    // refs: DN-37.D20, BIB:determinism_model
    [[nodiscard]] bool is_failure_lexicon_word(std::string_view token) noexcept;

    // pre: token MUST be a sub-view of line — the kernel recovers surrounding bytes by pointer
    // arithmetic, because caps and adjacency are pre-casefold byte facts the token alone lacks.
    // post: true iff the token carries the structural decoration CI and test tooling use to mark an
    // outcome, so a collision-prone failure token classifies its line only in verdict register.
    // invariant: the anchors are the CAPS register, a kind-slot colon or a bracket pair, a LEADING
    // fail glyph, a CamelCase error type, and the adjacent-pair phrase.
    // invariant: a leading fail glyph CONFIRMS a token and never CREATES a cue, so a glyph-only
    // line has no failure word and stays silent.
    // invariant: the multiplication sign is excluded on purpose — it doubles as a dimension
    // separator, which is the precision risk that deferred an earlier rule.
    // invariant: a trailing colon anchors a token only when every token preceding it is itself
    // colon-terminated or bracket-enclosed — the KIND SLOT.
    // invariant: a bare adjacency cannot separate a log prefix from an object key, a named
    // parameter or a quoted source string: the discriminating information is POSITIONAL.
    // invariant: the kind-slot rule is monotone-DEMOTING — it only ever removes an anchor —
    // which is what makes it admissible with no corroborating recall argument.
    // invariant: a path-line-column run needs no special case: it IS a run of colon-terminated
    // tokens, so the note register's shape is an instance of this rule rather than a sibling.
    // invariant: the walk is WHOLE-LINE and never head-bounded — bound the scan, never the claim.
    // invariant: a CamelCase error TYPE anchors ONLY in verdict register — a thrown one fires, a
    // suite NAME that merely references the type does not; the discriminator is position.
    // refs: ADR-9, ADR-20.D3, ADR-20.D5, BIB:canon_pipeline
    // refs: SRC-D-OUT-4, SRC-D-OUT-4a, SRC-D-OUT-4b, SRC-D-OUT-4c
    [[nodiscard]] bool is_verdict_anchored(std::string_view line, std::string_view token) noexcept;

    // pre: token MUST be a sub-view of line.
    // post: true iff the token is a COUNT register summary — its immediately preceding token is a
    // BARE INTEGER count that is not itself part of a numeric or temporal chain.
    // invariant: checked BEFORE the verdict anchors, because a counted noun is a summary even with
    // a trailing colon.
    // invariant: it is the symmetric dual of the disconfirming pass-and-fail summary that forced
    // the glyph gate — count-quantified outcome vocabulary is a summary, not a verdict.
    // invariant: a count-register word does NOT confer an alerting level: it caps at a warning —
    // demote, never suppress.
    // invariant: pure byte and case test, order-independent, so cross-stdlib bit-identical.
    // refs: ADR-20.D5, SRC-D-CNT-1, SRC-D-OUT-1
    [[nodiscard]] bool is_count_register(std::string_view line, std::string_view token) noexcept;

    // post: true iff the head carries a failure-lexicon word in COUNT register.
    // invariant: the firing predicate treats such words as NON-firing, so this reports their
    // presence and lets the inference cap the line at a warning, surfaced below per-item verdicts.
    // invariant: cold path — consulted only when the firing predicate is false.
    // note: internal detail, not a public product surface.
    // invariant: the NOTE register is the FOURTH register beside verdict, count and echoed-source,
    // and it has NO declaration here on purpose — no cross-TU consumer, so its kernel is private.
    // invariant: a token inside the MESSAGE of a compiler note carries no failure verdict, because
    // a note asserts none; measured at 28 of 29 ranked note findings emitted under an error label.
    // invariant: its anchor is STRUCTURAL, never a bare word — demoting on a bare word would turn
    // a labelling defect into a detection defect, which is strictly worse.
    // invariant: it is REGISTER-scoped rather than line-scoped, so a verdict anchored earlier on
    // the same line is a different author's claim and survives.
    // invariant: it DEMOTES and never suppresses — the cue does not fire, the line lands at
    // Unknown and still surfaces; the lexicon is untouched, because the defect is CONTEXT.
    // refs: ADR-20.D5, SRC-D-CNT-1, SRC-D-NOTE-1, SRC-D-PROV-1
    [[nodiscard]] bool contains_failure_summary_cue(std::string_view text,
                                                    std::size_t scan_limit = 0) noexcept;

} // namespace detail

} // namespace insight::utils

export namespace insight::utils
{

// invariant: the deterministic-content path MUST NOT read the wall clock, so a yearless timestamp
// takes an injected reference year defaulting to this constant.
// invariant: a live consumer may pass the real current year read once at stream open; batch and
// replay use the constant, so the parsed year is bit-identical across the year rollover.
// refs: BIB:determinism_model
inline constexpr int kDefaultReferenceYear{2024};

// post: an ISO 8601 or RFC 3339 UTC timestamp — with or without a fraction, with a numeric zone,
// or space-separated.
[[nodiscard]] std::optional<Timestamp> parse_iso8601(std::string_view timestamp_str) noexcept;

// post: the number of bytes consumed by a COMPLETE datetime starting at pos, or 0 when the bytes
// there do not carry one.
// invariant: ONE owner for the RFC3339 full-datetime byte grammar, and its CONSUMER SET is the
// blast radius of any change to it — four live consumers on three axes.
// invariant: two TRANSPORT consumers decide CONTENT versus TRANSPORT rather than masking, and one
// MASKING consumer tests a bracket interior with it.
// invariant: ONE MEASUREMENT consumer, in two spellings, re-implements the position logic AROUND
// this grammar, so the partition it measures moves under it.
// invariant: WIDENING it is therefore NOT a masking change — it moves a content boundary on two
// formats, and none of that is visible to a masking-focused review.
// invariant: a malformed OPTIONAL part is a hard 0 and never stop-before-it, so a consumer's
// nothing-else-follows check cannot silently accept a truncated zone.
// invariant: deliberately NOT a calendar validator — the consumers claim a token CLASS, and the
// strict character shape is already the anti-phantom guard.
// invariant: homed PUBLIC because a SEPARATE package must reach it: a package imports only the api
// and spi modules, and canon's detail shards are sealed.
// invariant: pure constexpr byte scan — no locale, no wall clock, ASCII only.
// refs: ADR-23, SRC-D-MSK-5, BIB:jenkins_dialect, BIB:determinism_model
[[nodiscard]] constexpr std::size_t rfc3339_datetime_length(std::string_view text,
                                                            std::size_t pos) noexcept
{
    constexpr std::size_t kDateLen{10U};
    constexpr std::size_t kTimeLen{8U};
    constexpr unsigned kDecimalBase{10U};
    const auto digit_at{
        [&text](std::size_t at) noexcept
        { return at < text.size() && static_cast<unsigned>(text[at]) - '0' < kDecimalBase; }};
    const std::size_t start{pos};
    if (pos + kDateLen + 1U + kTimeLen > text.size())
        return 0;
    if (!(digit_at(pos) && digit_at(pos + 1U) && digit_at(pos + 2U) && digit_at(pos + 3U) &&
          text[pos + 4U] == '-' && digit_at(pos + 5U) && digit_at(pos + 6U) &&
          text[pos + 7U] == '-' && digit_at(pos + 8U) && digit_at(pos + 9U)))
        return 0;
    pos += kDateLen;
    if (text[pos] != 'T')
        return 0;
    ++pos;
    if (!(digit_at(pos) && digit_at(pos + 1U) && text[pos + 2U] == ':' && digit_at(pos + 3U) &&
          digit_at(pos + 4U) && text[pos + 5U] == ':' && digit_at(pos + 6U) && digit_at(pos + 7U)))
        return 0;
    pos += kTimeLen;
    if (pos < text.size() && text[pos] == '.')
    {
        ++pos;
        const std::size_t frac_start{pos};
        while (digit_at(pos))
            ++pos;
        if (pos == frac_start)
            return 0;
    }
    if (pos < text.size() && text[pos] == 'Z')
        ++pos;
    else if (pos < text.size() && (text[pos] == '+' || text[pos] == '-'))
    {
        ++pos;
        if (!digit_at(pos) || !digit_at(pos + 1U))
            return 0;
        pos += 2U;
        if (pos < text.size() && text[pos] == ':')
            ++pos;
        if (!digit_at(pos) || !digit_at(pos + 1U))
            return 0;
        pos += 2U;
    }
    return pos - start;
}

// post: a yearless BSD syslog timestamp; the year is the INJECTED reference year, so no wall-clock
// read enters.
[[nodiscard]] std::optional<Timestamp>
parse_bsd_syslog_ts(std::string_view timestamp_str,
                    int reference_year = kDefaultReferenceYear) noexcept;

// post: a CLF or Combined-Log-Format timestamp.
[[nodiscard]] std::optional<Timestamp> parse_clf_timestamp(std::string_view timestamp_str) noexcept;

// post: Unix epoch SECONDS as a digit string.
[[nodiscard]] std::optional<Timestamp>
parse_epoch_timestamp(std::string_view timestamp_str) noexcept;

// post: OTLP epoch NANOSECONDS as a digit string — the OTEL event-time channel, so OTEL inputs
// window like any other format.
// invariant: integer-only, no float; sub-duration resolution truncates deterministically per
// stdlib, and a millisecond-granular producer is lossless on both.
// refs: ADR-29.D5
[[nodiscard]] std::optional<Timestamp>
parse_unix_nano_timestamp(std::string_view timestamp_str) noexcept;

// post: an HDFS compact date and time, taken as two separate six-digit fields.
[[nodiscard]] std::optional<Timestamp> parse_compact_date_time(std::string_view date,
                                                               std::string_view time) noexcept;

// post: a Spark-style short-year date and time, 19 characters.
[[nodiscard]] std::optional<Timestamp>
parse_short_year_slash(std::string_view timestamp_str) noexcept;

// post: an Apache error-log timestamp, 24 characters.
[[nodiscard]] std::optional<Timestamp>
parse_apache_error_ts(std::string_view timestamp_str) noexcept;

// post: a HealthApp compact timestamp, up to 22 characters.
[[nodiscard]] std::optional<Timestamp> parse_health_app_ts(std::string_view timestamp_str) noexcept;

// post: an ISO-like timestamp with comma or dot milliseconds; unlike the ISO 8601 parser this
// REQUIRES a space separator and REQUIRES the milliseconds.
[[nodiscard]] std::optional<Timestamp>
parse_log4j_timestamp(std::string_view timestamp_str) noexcept;

// post: a log level parsed case-insensitively from its spelled name, including the abbreviated and
// critical spellings.
[[nodiscard]] LogLevel parse_log_level(std::string_view level_str) noexcept;

// post: a level inferred from the HEAD of an unstructured line — the leading token only — for
// the raw-text fallback where no structured field carries one.
// invariant: real markers sit at the START of a line, and a benign mid-line word must not
// misclassify it; bounded and alloc-free, so it is safe on the tokenizer hot path.
// invariant: THIS is canon's content-inference layer, and the return type says so at the site that
// consults it — every value it produces is the INFERRED species.
// invariant: that includes the leading-level-word arm: a word that parses as a level is still canon
// guessing the word IS the line's level, not a producer declaring one.
// invariant: so a caller cannot mistake the guess for a declaration and no consumer has to
// re-derive the distinction by re-reading the words.
// refs: ADR-22.D3, DN-32.D3
[[nodiscard]] EventLevel infer_leading_log_level(std::string_view line) noexcept;

// post: an Nginx error-log timestamp, the same format as the Apache one.
[[nodiscard]] std::optional<Timestamp>
parse_nginx_error_ts(std::string_view timestamp_str) noexcept;
} // namespace insight::utils

export namespace insight::logging
{

namespace detail
{

    // invariant: the function the log macros expand to, homed in the MODULE so no first-party
    // declaration leaks through the global module fragment.
    // invariant: the macro header therefore stays pure preprocessor plus a single third-party
    // include.
    // refs: ADR-3.D4
    template <typename... Args>
    inline void log_message(const std::shared_ptr<spdlog::logger>& logger,
                            const spdlog::source_loc& source_location,
                            spdlog::level::level_enum level, fmt::format_string<Args...> format,
                            Args&&... args)
    {
        if (!logger || !logger->should_log(level))
        {
            return;
        }

        logger->log(source_location, level, fmt::format(format, std::forward<Args>(args)...));
    }

} // namespace detail

inline constexpr std::string_view kArenaLogger{"insight.arena"};
inline constexpr std::string_view kMaskLogger{"insight.mask"};
inline constexpr std::string_view kPipelineLogger{"insight.pipeline"};
inline constexpr std::string_view kDetectorLogger{"insight.detector"};
inline constexpr std::string_view kParserLogger{"insight.parser"};
inline constexpr std::string_view kStrategyLogger{"insight.strategy"};
inline constexpr std::string_view kTokenizerLogger{"insight.tokenizer"};

// invariant: the registration set lives HERE beside the names, so a name that exists but is not
// registered cannot happen without deleting it from a list three lines below.
// invariant: the impl unit's private copy HAD drifted and had lost one name, so that logger fell
// through to the default one and its warnings were emitted untagged and unroutable.
// invariant: a hand-enumerated mirror of a declaration list is the defect class; this removes the
// mirror.
inline constexpr std::array kAllLoggers{kArenaLogger,    kMaskLogger,   kPipelineLogger,
                                        kDetectorLogger, kParserLogger, kStrategyLogger,
                                        kTokenizerLogger};

// pre: call once before any logging; it is thread-safe and the first call wins.
// invariant: THE SINK IS STDERR AND IS NOT A PARAMETER — this facility serves a process class
// whose stdout is an artifact stream something downstream parses, hashes or frames.
// invariant: diagnostics on that stream corrupt it, and the corruption scales with the log level
// rather than announcing itself, so validity becomes a property of the environment.
// invariant: measured on two entry points: 9 874 log lines interleaved with 34 records, and 30
// lines interleaved into a 62 kB report — every record correct, both files unparseable.
// invariant: default_level IS the silencing axis and is the only door — a benchmark passes off to
// declare it wants no records at all, and a second spelling would reopen the closed shape.
// invariant: nothing OUTSIDE this API can quiet canon: before this call canon has nothing in the
// registry, and the accessors' fallback logger is registered under no name.
// invariant: silencing the host's default logger does not reach canon either — it used to, only
// because the accessors borrowed that logger.
// refs: DN-53.D3, DN-53.D7
void init_logging(spdlog::level::level_enum default_level = spdlog::level::info);

// post: a registered name yields its own logger; before init_logging has run they yield a
// canon-owned QUIET logger on canon's own stderr sink, never the host's default logger.
std::shared_ptr<spdlog::logger> arena_logger();
std::shared_ptr<spdlog::logger> mask_logger();
std::shared_ptr<spdlog::logger> pipeline_logger();
std::shared_ptr<spdlog::logger> detector_logger();
std::shared_ptr<spdlog::logger> parser_logger();
std::shared_ptr<spdlog::logger> strategy_logger();
std::shared_ptr<spdlog::logger> tokenizer_logger();

} // namespace insight::logging
export namespace insight::utils
{
namespace detail
{

    // invariant: delimiters are whitespace plus STRUCTURAL punctuation; identifier and path-join
    // characters are deliberately NOT delimiters.
    // invariant: so a compound filename stays a single atom while a bracketed marker or a
    // key-equals-value pair exposes its inner word.
    [[nodiscard]] constexpr bool is_token_delimiter(char chr) noexcept
    {
        switch (chr)
        {
        case ' ':
        case '\t':
        case '\n':
        case '\r':
        case '\f':
        case '\v':
        case '[':
        case ']':
        case '(':
        case ')':
        case '{':
        case '}':
        case '<':
        case '>':
        case '"':
        case '\'':
        case '`':
        case ',':
        case ':':
        case ';':
        case '|':
        case '=':
            return true;
        default:
            return false;
        }
    }

    inline constexpr unsigned kAsciiCaseBit{0x20U};
    inline constexpr unsigned kAlphabetLen{26U};
    inline constexpr unsigned kDecimalDigitLen{10U};

    [[nodiscard]] constexpr bool is_token_alnum(char chr) noexcept
    {
        const unsigned chu{static_cast<unsigned>(static_cast<unsigned char>(chr))};
        return ((chu | kAsciiCaseBit) - 'a') < kAlphabetLen || (chu - '0') < kDecimalDigitLen;
    }

    // post: the length of an ANSI escape sequence starting at pos, or 0 if none — the CSI form
    // and a bare ESC.
    // invariant: ANSI codes are formatting noise and never token content, so a sequence is consumed
    // as a DELIMITER and a level word glued to one is still extracted clean.
    [[nodiscard]] constexpr std::size_t ansi_escape_len(std::string_view text,
                                                        std::size_t pos) noexcept
    {
        if (pos >= text.size() || text[pos] != '\x1b')
            return 0U;
        // invariant: a CSI sequence terminates at its first final byte.
        constexpr unsigned kCsiFinalByteMin{0x40U};
        constexpr unsigned kCsiFinalByteMax{0x7EU};
        std::size_t end{pos + 1U};
        if (end < text.size() && text[end] == '[')
        {
            ++end;
            while (end < text.size() && (static_cast<unsigned char>(text[end]) < kCsiFinalByteMin ||
                                         static_cast<unsigned char>(text[end]) > kCsiFinalByteMax))
                ++end;
            if (end < text.size())
                ++end;
        }
        return end - pos;
    }

} // namespace detail

// post: visit is invoked for every non-empty token whose START lies within scan_limit — a token
// may extend past it, and 0 means all of the text.
// post: iteration stops early when visit returns true, and the function then returns true.
// invariant: alloc-free and single pass, and it is used by BOTH leading-level inference and the
// failure lexicon, so the two never disagree on what counts as a standalone word.
template <typename Visit>
    requires std::invocable<Visit, std::string_view> &&
             std::convertible_to<std::invoke_result_t<Visit, std::string_view>, bool>
[[nodiscard]] bool for_each_token(std::string_view text, std::size_t scan_limit, Visit visit)
{
    const std::size_t limit{scan_limit == 0U || scan_limit > text.size() ? text.size()
                                                                         : scan_limit};
    std::size_t pos{0};
    while (pos < limit)
    {
        for (;;)
        {
            if (const std::size_t esc{detail::ansi_escape_len(text, pos)}; esc != 0U)
                pos += esc;
            else if (pos < text.size() && detail::is_token_delimiter(text[pos]))
                ++pos;
            else
                break;
        }
        if (pos >= limit)
            break;
        const std::size_t begin{pos};
        while (pos < text.size() && !detail::is_token_delimiter(text[pos]) &&
               detail::ansi_escape_len(text, pos) == 0U)
            ++pos;
        std::string_view token{text.substr(begin, pos - begin)};
        while (!token.empty() && !detail::is_token_alnum(token.front()))
            token.remove_prefix(1);
        while (!token.empty() && !detail::is_token_alnum(token.back()))
            token.remove_suffix(1);
        if (!token.empty() && visit(token))
            return true;
    }
    return false;
}

} // namespace insight::utils

// invariant: escape sequences are stripped as a content normalization at canon INGEST, before
// strategy detection AND tokenization, so every downstream read sees colour-free content.
// invariant: the ordering is not a preference — the escapes interleave WITHIN and BETWEEN tokens,
// so no per-token mask downstream can reach them and a late strip has already lost.
// invariant: colour is presentation, never content; a pure byte state machine, hence cross-stdlib
// bit-identical.
// invariant: the one carve-out is the RECOGNITION path's raw read, which needs the wrapper intact
// for the echoed-source attribute.
// invariant: PUBLIC and homed here rather than in the sealed detail shard because stage 1 is an
// obligation the walkers place on their CALLERS, who cannot discharge a build-private one.
// invariant: it sits beside the token scanner's own escape length because the two are the pair that
// will have to be reconciled — one treats a run as a delimiter and handles no OSC.
// refs: ADR-21, SRC-D-TID-10, SRC-D-TID-11, SRC-D-PROV-1
export namespace insight::tokenization
{

inline constexpr unsigned char kEsc{0x1bU};
inline constexpr unsigned char kBel{0x07U};

namespace detail
{
    // invariant: a CSI body is parameters, then intermediates, then one final byte; an OSC body
    // runs to a BEL or ST terminator.
    inline constexpr unsigned char kCsiParamLo{0x30U};
    inline constexpr unsigned char kCsiParamHi{0x3fU};
    inline constexpr unsigned char kCsiInterLo{0x20U};
    inline constexpr unsigned char kCsiInterHi{0x2fU};
    inline constexpr unsigned char kCsiFinalLo{0x40U};
    inline constexpr unsigned char kCsiFinalHi{0x7eU};

    // pre: pos is the index just after the two-byte CSI introducer.
    // post: the index of the first post-sequence byte.
    [[nodiscard]] inline std::size_t scan_csi_body(std::string_view line, std::size_t pos) noexcept
    {
        const std::size_t len{line.size()};
        const auto byte_at{[&](std::size_t idx) { return static_cast<unsigned char>(line[idx]); }};
        while (pos < len && byte_at(pos) >= kCsiParamLo && byte_at(pos) <= kCsiParamHi)
            ++pos;
        while (pos < len && byte_at(pos) >= kCsiInterLo && byte_at(pos) <= kCsiInterHi)
            ++pos;
        if (pos < len && byte_at(pos) >= kCsiFinalLo && byte_at(pos) <= kCsiFinalHi)
            ++pos;
        return pos;
    }

    // pre: pos is the index just after the two-byte OSC introducer.
    // post: the index past the BEL or ST terminator, which is consumed.
    [[nodiscard]] inline std::size_t scan_osc_body(std::string_view line, std::size_t pos) noexcept
    {
        const std::size_t len{line.size()};
        const auto byte_at{[&](std::size_t idx) { return static_cast<unsigned char>(line[idx]); }};
        while (pos < len)
        {
            if (byte_at(pos) == kBel)
                return pos + 1U;
            if (byte_at(pos) == kEsc && pos + 1U < len && line[pos + 1U] == '\\')
                return pos + 2U;
            ++pos;
        }
        return pos;
    }

} // namespace detail

} // namespace insight::tokenization

// invariant: a linkage specification attaches this declaration to the GLOBAL module, which is what
// makes the sealed definition and this name ONE entity across the module boundary.
// invariant: it is EXPORTED so the sealed shard, importing this unit, redeclares THE VISIBLE entity
// rather than a twin — without the export they are two entities and the mint stops resolving.
// invariant: exporting leaks only an INCOMPLETE name: the definition is sealed and the constructor
// private, so no consumer can construct or complete it.
export extern "C++"
{
    namespace insight::tokenization
    {
        class LogParserPasskey;
    } // namespace insight::tokenization
}

export namespace insight::tokenization
{

class NormalizedContent;
class NormalizedLine;
// note: declared first so the in-class friend names THIS exported entity.
[[nodiscard]] NormalizedLine normalize(std::string_view raw_line, std::string& scratch);

// invariant: the type MEANS stage 1 ran on these bytes — canon's universal ANSI ingest
// normalization, the exact grammar the factory below owns.
// invariant: produced ONLY by that factory; there is deliberately NO constructor from a
// string_view, and that absence is the whole mechanism.
// invariant: a caller cannot reach the content walkers without having gone through stage 1 and
// cannot fake the passage — the only other producer NARROWS an object it must already hold.
// invariant: the obligation this replaces was a comment three repos of call sites were expected to
// remember, and two of three consumers broke it silently.
// refs: ADR-21.D3, ADR-21.D1
class NormalizedLine
{
  public:
    // invariant: an OUTBOUND accessor weakens nothing — reading normalized bytes destroys no
    // stage-1 evidence, where minting from arbitrary bytes would.
    [[nodiscard]] constexpr std::string_view bytes() const noexcept
    {
        return bytes_;
    }

    // post: the caller's own INFERRED stage 2 — the offset comes from a strip that is NOT a
    // declared catalogue row, which is a declared limitation placed at its one call site.
    // invariant: narrowing is the safe escape hatch by construction: both real stage-2
    // implementations only ever SHORTEN, and a suffix cannot destroy the stage-1 evidence.
    // invariant: an offset past the end yields the empty content, which is what a whole-line
    // transport line peels to.
    // refs: ADR-22, ADR-23
    [[nodiscard]] constexpr NormalizedContent undeclared_suffix(std::size_t offset) const noexcept;

  private:
    friend NormalizedLine normalize(std::string_view raw_line, std::string& scratch);
    constexpr explicit NormalizedLine(std::string_view bytes) noexcept : bytes_{bytes} {}
    std::string_view bytes_;
};

// invariant: the type MEANS stage 1 ran AND a suffix has since been taken — a declared stage 2,
// an inferred one, or the explicit statement that there is none.
// invariant: it is the only currency the three content walkers accept, and its PRODUCERS are a
// finite census where call sites are not.
// invariant: the privileged mint exists because canon's own tokenizer consumes walkers on
// strategy-REBUILT arena bytes, which no suffix door can express.
// invariant: the attestor is the object that performed stage 1 unconditionally at its one named
// site, so the guarantee stays exactly the measured defect's scope: unforgeable from OUTSIDE.
// invariant: it does NOT prove the RIGHT stage 2 ran and deliberately does not carry which one did
// — a type that knew its transport stack would be a declaration reaching the tokenizer.
// refs: ADR-21.D4, ADR-23
class NormalizedContent
{
  public:
    // note: read-only bytes, outbound only, for the same reason as the accessor on the line type.
    [[nodiscard]] constexpr std::string_view bytes() const noexcept
    {
        return bytes_;
    }

  private:
    friend class NormalizedLine;
    // invariant: the friend below is QUALIFIED on purpose — a qualified friend is a pure
    // REFERENCE to the prior global-module declaration and cannot declare a fresh entity.
    // invariant: that is how the sealed definition in the detail shard stays THIS friend.
    friend class insight::tokenization::LogParserPasskey;

    constexpr explicit NormalizedContent(std::string_view bytes) noexcept : bytes_{bytes} {}
    std::string_view bytes_;
};

// invariant: the shape guard sits at the type's own declaration, so any second member fails the
// build BEFORE any caller is recompiled.
// invariant: interposing a struct re-opened a channel a future edit could widen without touching a
// single signature; one borrowed view, trivially copyable, nothing else.
static_assert(sizeof(NormalizedContent) == sizeof(std::string_view));
static_assert(std::is_trivially_copyable_v<NormalizedContent>);
static_assert(sizeof(NormalizedLine) == sizeof(std::string_view));
static_assert(std::is_trivially_copyable_v<NormalizedLine>);

constexpr NormalizedContent NormalizedLine::undeclared_suffix(std::size_t offset) const noexcept
{
    const std::size_t clamped{offset > bytes_.size() ? bytes_.size() : offset};
    return NormalizedContent{std::string_view{bytes_.data() + clamped, bytes_.size() - clamped}};
}

// post: a NormalizedLine over the stripped bytes — CSI, SGR, OSC and bare-ESC sequences removed
// as an UNCONDITIONAL content normalization at ingest, before tokenization.
// post: a line with no ESC byte is a FIXED POINT, so the result BORROWS raw_line with no copy and
// scratch is not touched; only an ESC-bearing line rewrites into scratch.
// invariant: the returned line, every content narrowed from it, and every coordinate a walker
// slices out of THAT borrow raw_line or scratch, so both must outlive every such view.
// invariant: stage 1 is a CONSUMER's obligation and never a package's — a package that normalized
// would double-strip and then disagree with canon wherever the grammars differ.
// invariant: the caller that OWNS the ingest runs it once, BEFORE the transport peel, and that
// order is load-bearing: an escape before a transport prefix is invisible to the peel.
// invariant: it must NEVER overwrite the buffer a Tokenizer later reads — the echo wrapper the
// provenance hook needs survives only on the raw line, so stage 1 produces a DERIVED view.
// invariant: a pure byte state machine: no float, order-independent, cross-stdlib bit-identical.
// refs: ADR-17, ADR-21, SRC-D-TID-10, SRC-D-PROV-1
[[nodiscard]] inline NormalizedLine normalize(std::string_view raw_line, std::string& scratch)
{
    if (raw_line.find(static_cast<char>(kEsc)) == std::string_view::npos)
        return NormalizedLine{raw_line};
    scratch.clear();
    scratch.reserve(raw_line.size());
    const std::size_t len{raw_line.size()};
    std::size_t pos{0};
    while (pos < len)
    {
        if (static_cast<unsigned char>(raw_line[pos]) != kEsc)
        {
            scratch.push_back(raw_line[pos]);
            ++pos;
            continue;
        }
        if (pos + 1U >= len)
            break;
        const char introducer{raw_line[pos + 1U]};
        if (introducer == '[')
            pos = detail::scan_csi_body(raw_line, pos + 2U);
        else if (introducer == ']')
            pos = detail::scan_osc_body(raw_line, pos + 2U);
        else
            pos += 2U;
    }
    return NormalizedLine{scratch};
}

} // namespace insight::tokenization
