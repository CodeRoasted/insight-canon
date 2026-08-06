// insight.canon.api — the public DATA + API surface of canon (ADR-3.D4). The former
// api/insight/**/*.hpp content (types, det_math, canonical_event, mask_config,
// structural_role_registry, arena_allocator, tokenizer_engine, failure_lexicon, time_utils, logger
// accessors) lives here. std comes from insight.canon.internal; spdlog (3rd-party, the logger
// accessor signatures) is a textual GMF include. det_math is header-only integer math — it stays
// INLINE here (consumers compile it under the package's -ffp-contract=off, the determinism
// guarantee). Class IMPLEMENTATIONS stay in src/*.cpp impl units (byte-identical .a); this
// interface holds only their declarations.
module;
// NB: this exported unit deliberately references NO build-config macro (SPDLOG_ACTIVE_LEVEL et al).
// It is recompiled by every consumer, so any -D-dependent code here would force that macro PUBLIC
// and diverge per consumer. Canon's spdlog elision level stays PRIVATE to canon's build; the one
// gate that needed it (kDebugLogsEnabled) lives in the build-only tokenizer/parser impl units.
#include "det/det_int128.hpp" // portable 128-bit for det_math (native __int128 on gcc/clang; pure-C++ on MSVC)
#include <fmt/core.h>      // fmt::format_string (log_message template)
#include <fmt/format.h>    // fmt::format (log_message template)
#include <spdlog/common.h> // spdlog::level::level_enum, spdlog::source_loc, SPDLOG_LEVEL_* — 3rd-party
#include <spdlog/logger.h> // spdlog::logger (logger accessor return types + log_message)

export module insight.canon.api;
import insight.canon.internal; // std + global C fixed-width types

// ──────── from api/insight/core/types.hpp ────────
export namespace insight
{

// ── Time ──
using Timestamp = std::chrono::system_clock::time_point;
using Duration = std::chrono::system_clock::duration;

// ── Identifiers ──
using EventID = uint64_t;

// ── Canonicalization-contract version (SRC-D-TID-16) ──
// The single canon-owned identifier of the canonicalization CONTRACT — the masking
// rules that turn a raw line into its `template_str` (the stateless per-line masker +
// the F13 class set). Every MetaLog producer DEFAULTS to this (MetaLogConfig), so a
// rules change is one edit HERE and impossible to skip: bump it and old/new metalogs
// become incomparable at the §2.4 gate (re-derive, never migrate — SRC-D-TID-9). It names
// the rules generation, NOT the package version (decoupled — a patch release that does
// not touch the masking rules must NOT change it). Bump the suffix on any output-affecting
// canonicalization change. Generations: -1 = stateless masker + F13; -2 = OTEL-awareness
// (severity-from-severity_number + trace-context routing + the trace-scoped graph —
// ADR-29 D-OTEL-2, unconditional); -3 = currency-marker numerics
// (SRC-D-TID-22 — `$463`/`total=$463` mask to `$<*>`/`total=$<*>`); -4 =
// the 1.6.4 masking batch (detection_provenance_and_legibility.md): SRC-D-MSK-1 generalized
// diagnostic-composite masking (per-`:`/`/`-segment digit-leading rule — collapses the
// Chromium/glog prefix `[PID:DATE/TIME:LEVEL:file.cc:line]`, subsumes source-location), SRC-D-MSK-2
// ephemeral-root path catalog (`/tmp/…` → `/tmp/<*>`), and SRC-D-MSK-3 JSON nested-`fields`
// component/level descent (a cube-axis change folded into the same bump). The -4 content changes
// ONLY for inputs carrying a diagnostic-composite / ephemeral-root token or a nested-`fields` JSON
// record; every other document is byte-identical except this version string. -5 = D-OTEL-15
// (`4e46af0`); -6 = the SRC-D-MSK-4 batch — canon ephemeral-root masking + the lexicon-context
// precision fix (`9c5db20`); -7 = SRC-D-NOTE-1, the NOTE register: a failure word inside a compiler
// note's message (`<path>:<line>:<col>: note: … failed:`) no longer confers a failure verdict, so
// the serialized `dominant_level` of a gcc/clang cascade's note lines moves Error → Unknown.
// `template_str` and `template_id` do NOT move under -7 — the masker is untouched — but
// `dominant_level` is serialized and gates NewErrorPattern and diff polarity, so it is an
// output-affecting canonicalization change and takes the bump. -8 = SRC-D-MSK-5 bracket_timestamp
// (bibles/jenkins_dialect.md §4, ADR-23 erratum 2 — "the bracket is the entire difference"): a
// WHOLE-token bracketed RFC3339 full datetime (`[2026-06-23T15:11:09.020Z]`) masks to `[<*>]`
// instead of falling through to literal KEEP; `template_str`/`template_id` move ONLY for lines
// carrying that token class, every other document is byte-identical except this version string.
inline constexpr std::string_view kCanonicalizationVersion{"stateless-masks-8"};

// ── Template identity (insight_perf_template_id.md SRC-D-TIR-1) ──
// The structural identity of a canonicalised template: the first 16 bytes of
// SHA-256(masked template_str), carried as a fixed-size POD through the whole
// metalog/eidos domain. The 34-byte "h:"+hex string is materialised only at the
// serialize seam (render()). Owned by canon because identity IS "the hash under
// kCanonicalizationVersion" (SRC-D-TID-9/SRC-D-TID-16) — identity and its comparability
// version belong in one place.
struct TemplateId
{
    std::array<std::uint8_t, 16> bytes{}; // first 16 bytes of SHA-256(masked template_str)
    // Defaulted byte-lexicographic order REPRODUCES the old "h:"+hex string order exactly
    // (hex is order-preserving, the "h:" prefix is constant) — the golden-preserving
    // invariant every vector<TemplateId> sort in merge_behavior/diff relies on. Do NOT
    // hand-roll operator<.
    auto operator<=>(const TemplateId&) const = default;
    bool operator==(const TemplateId&) const = default;
};

// NgramId (SRC-D-TIR-4(2)): a fixed-width 128-bit key for an n-gram SEQUENCE
// (std::vector<TemplateId>), so metalog's merge_behavior / diff_ngram_delta key on a
// scalar instead of rehashing+recomparing the variable-length sequence on every map op.
// NEVER serialized — a purely in-memory keying optimisation (the n-gram maps emit their
// output sorted by the sequence, not by this id), so a fast non-crypto hash is correct;
// 128 bits keeps collisions ~0 for window-bounded n-gram cardinality, so the map needs no
// sequence compare. Order-sensitive: [a,b] and [b,a] are distinct n-grams → distinct ids.
struct NgramId
{
    std::array<std::uint8_t, 16> bytes{};
    auto operator<=>(const NgramId&) const = default;
    bool operator==(const NgramId&) const = default;
};

// SHA-256 the canonical (masked) template; the first 16 bytes are the id. Same content
// as the former MetaLogEngine::compute_template_id (spec §3.2) — render(template_id_of(s))
// is byte-identical to the old string for every s (SRC-D-TIR-1 invariant 2).
[[nodiscard]] TemplateId template_id_of(std::string_view canonical_template) noexcept;
// Wire rendering: "h:" + 32 lowercase hex. The ONLY place the id string materialises.
[[nodiscard]] std::string render(TemplateId template_id);
// Inverse of render() — a TEST / fixture helper only (fixtures construct synthetic ids).
// NOT on any product path: the wire is a one-way terminal render (SRC-D-TIR-1 §1).
[[nodiscard]] TemplateId parse_template_id(std::string_view rendered);

// 128-bit content key for an n-gram sequence (SRC-D-TIR-4(2)). Fast non-crypto combine over
// the sequence's id bytes; transient (never serialized), order-sensitive.
[[nodiscard]] NgramId ngram_id_of(const std::vector<TemplateId>& sequence) noexcept;

// ── Intent identity (bibles/intent_identity.md §2-§4, SRC-II-1/SRC-II-6/SRC-II-7) ──
// kIntentRegistryVersion RETIRED (ADR-17): it was a dead constant (zero downstream readers),
// and its job — the SRC-II-7 comparability identity of the recognizer/marker rule set — is now
// discharged by the composed `semantic_identity` (insight::semantic::ComposedSemantics), a CONTENT
// hash over the actual marker/role/level rows the packages ship (content, not a hand-bumped label).
// A rule change is a package version bump → a new semantic_identity → re-segment-or-refuse, wired
// for real on the MetaLogDocument RulesetIdentity block. The intent-canonicalization ALGORITHM
// stays below (canonicalize_intent / discriminant_of); kCanonicalizationVersion (the core masking
// generation) remains and enters the composed hash as a component.

// Canonicalize a marker payload (a job or step name) to its intent CLASS: mask the
// version-bearing / matrix / shard tokens that vary across homologous runs, keep the
// structural name — so matrix legs and retries of ONE job collapse to one class
// (instances are separated downstream by ordinal, per §5.3 multiplicity, NEVER eaten
// here — SRC-II-2). This is the canon.detail.mask templating discipline REAPPLIED to
// identifiers (§5.1 detail 1); it is a distinct rule set from the value masker (which
// keeps `(1/<*>` to distinguish, where identity must collapse `(1/10)`→`(M)` to align).
// Deterministic, ASCII-safe, no float, no regex, no cross-line state. Cold path (few
// markers per log) — returns an owned string.
[[nodiscard]] std::string canonicalize_intent(std::string_view name);

// The raw INSTANCE DISCRIMINANT (ADR-18, SRC-II-9 — the third role on the identity spine): the
// matrix tuple rendered into a job/step display name (`Test (ubuntu-latest, Node 24.x)` →
// `(ubuntu-latest, Node 24.x)`), returned VERBATIM (a view into `name`). This is the STABLE
// declared parallelism coordinate that separates co-occurring siblings within one identity class —
// kept raw (NOT masked, NOT an appearance ordinal) because matrix axes are stable by declaration,
// so raw keys pair exactly across runs. Empty when the name carries no tuple (the aligner then
// falls to a retry ordinal — the declared-runs-on source is stripped from Sift's input stream, so
// it is inert here). The complement of canonicalize_intent: the class MASKS the tuple to `(M)`, the
// discriminant KEEPS it verbatim.
[[nodiscard]] std::string_view discriminant_of(std::string_view name) noexcept;

// The intent identity: the 16-byte SHA-256 of the canonicalized name — a STRUCTURAL
// grouping key derived from the marker, never a retained value (SRC-II-1, the O1/SRC-D-OTEL-1
// discipline verbatim). Byte-identical to template_id_of(canonicalize_intent(name));
// one call keeps `intent_id` co-located with its comparability version.
[[nodiscard]] TemplateId intent_id_of(std::string_view name);

// Location recognition (bibles/intent_identity.md §8, SRC-II-8) moved to the facade
// (insight::recognize_location over a ComposedSemantics) — it walks the composed location rows the
// test_frameworks package ships, so it cannot live in api (which the compose module imports). The
// three LocationMatchKind families (jest/vitest/playwright `.test.`/`.spec.`; pytest `test_*.py`;
// go/ruby `_test.go`/`_spec.rb`) are canon algorithms; the file-naming vocabulary is package data.

// Stream rendering (ADL) so a TemplateId prints as "h:"+hex in logs / test diagnostics
// (the "verbose on failure" rule). Not a product wire path — that is render() at the seam.
inline std::ostream& operator<<(std::ostream& out, const TemplateId& template_id)
{
    return out << render(template_id);
}

// ── OTEL trace context (ADR-29, SRC-D-OTEL-1) ──
// The OTEL hex ids (traceId/spanId/parentSpanId) are hashed to fixed-width scalar PODs at
// the strategy seam — the SRC-D-TIR-4 hash-to-POD discipline — carried IN-MEMORY on the
// CanonicalEvent, CONSUMED by the structural layer (trace_id scopes the n-gram graph per
// trace; span_id/parent_span_id feed metalog's observed causal DAG, which IS built and tested
// — SRC-D-OTEL-11 resolves each declared parent edge into that same n-gram graph at window
// close) and NEVER serialized into the MetaLog (OR1 — the per-transaction-unique hex would be
// a cardinality bomb). What is NOT built is Régime B proper (ADR-29.O1): consuming a declared
// edge as ground truth rather than folding it into the inferred graph. A zero `value` means
// "absent"; the hash forces non-zero on any non-empty input so absent ≠ present.
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

// The per-record OTEL trace context extracted by the strategy layer (SRC-D-OTEL-1). All fields
// are consumed downstream and never serialized. `present` is true iff the record carried a
// trace_id (the graph-scoping key); span_id/parent_span_id carry the DECLARED causal
// vertex/edge (ADR-29.D2) and are unread by the trace-scoping path.
struct OtelTraceContext
{
    bool present{false};    // the record carried a trace_id (the graph-scoping key)
    bool has_parent{false}; // a parent_span_id was present (declared edge; usually absent on logs)
    bool is_span{false}; // SRC-D-OTEL-11: a SPAN record (declared causality → observed edge), not a
                         // log record with trace context (positional causality → the adjacency
                         // ring). Set by the flat-span parser; false on the OTEL-log path. Metalog
                         // routes spans to the observed DAG, never the adjacency ring.
    TraceId trace_id{};  // the transaction grouping key
    SpanId span_id{};    // the declared causal vertex identity
    SpanId parent_span_id{}; // the declared causal edge span→parent
};

// fnv1a-64 of the OTEL hex string → a scalar id (content-addressed: same hex → same id,
// any run, byte-only → cross-stdlib bit-identical, no float). 0 is reserved for "absent",
// so a non-empty hex that hashes to 0 is bumped to 1. The hex is otherwise discarded
// (consumed, not retained — OR1).
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

// ── Declared OTEL field-map catalog (SRC-D-OTEL-4a) ──
// The four schema-declared OTLP fields canon recognizes, as a structured (class → recognizer
// key) catalog — the christened ValueClassRegistry (ADR-17) is the composed view over THIS
// catalog + kOrdinalFieldCatalog + the KEEP lexicons + the package ValueClassRow seat
// (ComposedSemantics::value_classes). These UNIVERSAL value concepts stay core (the ratified rule);
// this is the SRC-D-TID-6 "now" tier (a declared contract with hardcoded strategies, like the
// JsonStrategy key lists) — NOT a package extension. OTEL fields are
// schema-declared (never data-learned), so they need no registry. Each class routes to its
// declared layer (SRC-D-OTEL-1): the three trace keys → consumed structural metadata (dropped
// from the template, never tokenized); severity_number → the LogLevel band.
enum class OtelFieldClass : std::uint8_t
{
    TraceId,        // → OtelTraceContext::trace_id (the graph-scoping key)
    SpanId,         // → OtelTraceContext::span_id (the declared causal vertex)
    ParentSpanId,   // → OtelTraceContext::parent_span_id (the declared causal edge)
    SeverityNumber, // → LogLevel band (declared > inferred)
};

struct OtelFieldDescriptor
{
    OtelFieldClass field_class;
    std::string_view key; // the OTLP/JSON top-level key
};

// The OTLP/JSON top-level keys per the OpenTelemetry Log Data Model. Keys are exact (OTLP is
// a declared schema); a structured catalog, not scattered inline predicates.
//
// SRC-D-OTEL-18b: the span `kind` field is ABSENT, deliberately and not by oversight. It is a
// categorical value, so carrying it would need a categorical-field→value_counts channel canon does
// not have; routing it through any existing channel would either fabricate an ordinal or smuggle a
// vocabulary into the semantic-unaware core. It is not load-bearing for the structural exhibits,
// so the honest state is ABSENT — never a placeholder. Adding the channel is what unblocks it.
inline constexpr std::array<OtelFieldDescriptor, 4> kOtelFieldCatalog{{
    {.field_class = OtelFieldClass::TraceId, .key = "traceId"},
    {.field_class = OtelFieldClass::SpanId, .key = "spanId"},
    {.field_class = OtelFieldClass::ParentSpanId, .key = "parentSpanId"},
    {.field_class = OtelFieldClass::SeverityNumber, .key = "severityNumber"},
}};

// ── Declared ordinal-field catalog (W1 ordinal channel, §4A.4 SRC-D-W1-2/SRC-D-W1-3/SRC-D-W1-8) ──
// The "now" tier (SRC-D-TID-6): a declared, registry-free catalog of structured numeric fields
// whose VALUE is ordinal (metric structure — magnitude + distance), recognized by EXACT top-level
// field name in the JsonStrategy field-route (mirror kOtelFieldCatalog). A declared-key hit is
// captured as a consumed-not-tokenized ordinal observation (CanonicalEvent.ordinals) — NEVER a
// param — which metalog bins per schedule into the W1 carrier (TopKEntry.ordinal_histograms). A
// UNIVERSAL value class → stays core (this catalog); arbitrary/client ordinals await a package
// ValueClassRow (the ValueClassRegistry seat, ADR-17 — no package ships one; we do
// not build dormant vocabulary — the SRC-D-TID-14 anti-monster boundary). EXACT keys only — uniform
// across the fast/slow JSON paths, no suffix/pattern matching and no value-syntax guessing (the
// SRC-D-W1-5 mis-route hazard). Unit-explicit names only,
// so each value's unit is unambiguous (SRC-D-W1-3).

// Which ordinal SCHEDULE (canonical unit + log ladder) a field bins onto. The schedule is a
// versioned catalog (SRC-D-W1-4): its stable string id is the eidos diff's comparability key.
enum class OrdinalSchedule : std::uint8_t
{
    DurationLog2Ns, // log2-octave ladder over nanoseconds (durations / latencies)
    SizeLog2Bytes,  // log2-octave ladder over bytes (payload / response sizes)
};

// Per-schedule wire identity + bin count B (the frozen, versioned ladder — SRC-D-W1-2). The ladder
// MAP itself (bin = clamp(bit_width(value)−1, 0, B−1)) lives metalog-side — it owns binning; canon
// only needs the stable string id (carried to the wire) and the B, and never bins.
struct OrdinalScheduleSpec
{
    OrdinalSchedule schedule;
    std::string_view schedule_id; // stable + versioned; mismatch ⇒ no W1 (the eidos diff gate)
    std::uint32_t bin_count;      // B — fixed, small, bounds the carrier (SRC-D-W1-2)
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

// A declared ordinal field: exact name → schedule + the integer factor scaling the field's declared
// unit to the schedule's CANONICAL unit (ns for durations, bytes for sizes). Every factor is a
// power of ten (or 1) so the decimal→fixed-point parse is EXACT (SRC-D-W1-3 pin: the value is
// parsed from the JSON number's decimal TEXT, never via double — a get_double()→cast would be the
// forbidden float→int on the deterministic-content path).
struct OrdinalFieldDescriptor
{
    std::string_view key; // EXACT top-level JSON key
    OrdinalSchedule schedule;
    std::int64_t scale_to_canonical; // ×factor: field unit → canonical unit (power of 10, or 1)
};
inline constexpr std::int64_t kNanosPerMicro{1'000};
inline constexpr std::int64_t kNanosPerMilli{1'000'000};
inline constexpr std::int64_t kNanosPerSecond{1'000'000'000};
inline constexpr std::array<OrdinalFieldDescriptor, 15> kOrdinalFieldCatalog{{
    // OTEL span wall-duration (ADR-29, SRC-D-OTEL-12): endTimeUnixNano −
    // startTimeUnixNano, already integer ns. Computed by the flat-span parser and emitted as
    // this declared ordinal on the shipped DurationLog2Ns ladder (activates W1 + latency_shift
    // on traces). The key also self-matches a literal span_duration_ns field if a log carries one.
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

// Parse a non-negative JSON numeric token's decimal TEXT to an int64 in the schedule's canonical
// unit, multiplying by `scale` (a power of ten, or 1). Integer/decimal-string arithmetic only —
// NEVER via double (SRC-D-W1-3 determinism pin). Returns nullopt on a malformed / negative /
// exponent / overflowing token (the observation is then omitted — omit-when-absent). Fractional
// digits beyond the scale's decimal places are truncated (deterministic). `scale` MUST be a power
// of ten or 1.
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
            return std::nullopt; // integer-part overflow ⇒ omit
        value = (value * kRadix) + digit;
    }
    if (scale != 0 && value > kIntMax / scale)
        return std::nullopt; // ×scale overflow ⇒ omit
    value *= scale;
    if (idx < text.size() && text[idx] == '.')
    {
        ++idx;
        std::int64_t frac_scale{scale / kRadix};
        for (; idx < text.size() && text[idx] >= '0' && text[idx] <= '9'; ++idx)
        {
            any_digit = true;
            if (frac_scale == 0)
                continue; // beyond the schedule's precision ⇒ truncate
            const std::int64_t contribution{(text[idx] - '0') * frac_scale};
            if (value > kIntMax - contribution)
                return std::nullopt;
            value += contribution;
            frac_scale /= kRadix;
        }
    }
    if (idx != text.size() || !any_digit)
        return std::nullopt; // trailing junk / lone '.' / exponent / sign ⇒ omit
    return value;
}

// A recognized ordinal observation (W1, SRC-D-W1-3): the matched declared field, its schedule, and
// the value parsed to the canonical-unit int64. Consumed-not-tokenized — carried on CanonicalEvent
// parallel to the params/trace, NEVER serialized as a param. `field_name` is the catalog's static
// key (stable for the program lifetime — no arena), surfaced on the diff row for `attributable_to`.
struct OrdinalObservation
{
    std::string_view field_name; // the declared catalog key (e.g. "latency_ms")
    OrdinalSchedule schedule;
    std::int64_t value{0}; // canonical-unit fixed integer (ns for durations, bytes for sizes)
};

// ── Enums ──
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

// ── Run outcome (ADR-17 / insight_run_outcome_model.md §2) ──
// The CI run's terminal verdict — a run-level sibling of LogLevel, NOT a per-event field (never on
// CanonicalEvent) and NOT a cube dimension (OUTCOME is the run LABEL, never an axis of the cube).
// Universal outcome CATEGORIES; the strings that name them per dialect are semantic-package data
// (OutcomeTokenRow, insight.canon.spi). The five span the CI outcome space: Jenkins/GHA/GitLab all
// map into them. UNSTABLE (ran, partially failed, continued) is neither Success nor Failure and is
// NEVER folded into either; ABORTED means the log is INCOMPLETE and is never read as pass or fail.
enum class RunOutcome : std::uint8_t
{
    Unknown = 0, // no verdict observed (absence / legacy / unmapped token) — the default
    Success,     // the run completed clean
    Failure,     // the run failed as a whole (a hard build break)
    Unstable,    // the run RAN and partially failed but continued — flaky/partial, NOT Failure
    Aborted,     // the run was cancelled / timed out / did not complete — the log is INCOMPLETE
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

// §6.1: did the run verdict get strictly WORSE on the pass↔fail axis Success < Unstable < Failure?
// Aborted/Unknown are EXCLUDED — they are not points on that axis (an aborted run is inconclusive,
// an unknown one carries no verdict), so any transition touching them is never an outcome
// regression. Deterministic integer compare; the single-source predicate the CLI gate, the check
// and the comment verdict read.
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
                                     return -1; // Aborted / Unknown — off the axis
                                 }
                             }};
    const int baseline_rank{axis_rank(baseline)};
    const int changed_rank{axis_rank(changed)};
    return baseline_rank >= 0 && changed_rank >= 0 && changed_rank > baseline_rank;
}

// ── OTEL severity_number → LogLevel band (ADR-29 SRC-D-OTEL-1) ──
// The OpenTelemetry severity_number (1–24) folds into canon's existing 6-level LogLevel
// by integer division: band = (n-1)/4 → Trace(1–4)/Debug(5–8)/Info(9–12)/Warn(13–16)/
// Error(17–20)/Fatal(21–24). Integer-only (no float — D-OTEL-3); n<1 clamps to Trace,
// n>24 clamps to Fatal. This is the *declared* severity channel for OTEL inputs and wins
// over the failure_lexicon when present (declared > inferred); canon keeps its own 6-level
// model and DISCARDS the raw 1–24 number (MetaLog ≠ OTEL — SRC-D-OTEL-8). The 24-band
// granularity is deliberately not inherited.
[[nodiscard]] constexpr LogLevel
log_level_from_severity_number(std::int64_t severity_number) noexcept
{
    if (severity_number < 1)
        return LogLevel::Trace;
    static constexpr std::int64_t kBandWidth{4}; // 24 OTEL numbers / 6 canon levels
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
        return LogLevel::Fatal; // band >= 5 (severity_number >= 21), clamped
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
    // GitHub Actions / Azure Pipelines logs: every line prefixed with an
    // RFC3339 UTC timestamp at 100-ns (7-fraction-digit) precision + 'Z', then
    // the raw message (which may carry ##[group]/##[error]/##[warning] workflow
    // commands). A first-class CI input — without it these lines are mis-claimed
    // by Syslog (RFC3339 prefix) and shredded into empty templates.
    GitHubActions,
    // `Jenkins` EXITED here at T5 5.2 (the GHA T4 purification, one dialect over): the dialect's
    // detection strategy died — the Timestamper bracket stamp is DECLARED catalogue transport
    // (`bracket-rfc3339-line-prefix`), and the `[Pipeline] `/`Finished: ` legs are dialect-gated
    // rows walked by core under `--dialect jenkins`. Freeze-agnostic re-verified at the exit: no
    // wire field and no wire token carries this enum's numeric values, so the member shift below
    // is invisible outside a single build graph.
    // GitLab CI job-trace dialect (ADR-17 registration; the strategy code lives in
    // insight_semantic_gitlab). Line-selective: a fixed-width `<RFC3339> NNC[ +]` runner transport
    // prefix (stamped traces), `section_start:` section markers, and the terminal
    // `Job succeeded` / `ERROR: Job failed…` verdict line; other trace output falls through.
    GitLab,
    // Catch-all for unstructured text (CI / pytest / build logs). Selected only
    // when no structured strategy matches a non-empty line, so the tokenizer
    // never silently drops a line. Keep immediately before Unknown.
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
    case LogFormat::RawText:
        return "RawText"sv;
    default:
        return "Unknown"sv;
    }
}

// What a LINE does in the sequence (its structural role), as opposed to what a
// token inside it MEANS (its SemanticClass). Two orthogonal ontologies, two
// registries — keeping them separate is what avoids the value-vs-line-role
// conflation. These roles are ANNOUNCED — the line
// declares itself via a marker (`##[group]`, `##[error]`, a non-zero exit) — never
// derived from graph position (that is a structural-layer output, not a role).
// A seed catalog; designed to grow during calibration.
enum class StructuralRole : uint8_t
{
    None = 0,   ///< no announced role (the common case)
    GroupBegin, ///< a section/group opens (`##[group]`)
    GroupEnd,   ///< a section/group closes (`##[endgroup]`)
    Terminator  ///< an outcome/failure marker (`##[error]`, error/fatal level, non-zero exit)
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

// std::hash<TemplateId> (SRC-D-TIR-1 invariant 3): the content is already a uniform
// cryptographic hash, so the first 8 bytes ARE a good size_t — no mixing, no allocation.
// Reachable to importers of this module so `unordered_map<TemplateId,…>` in metalog/eidos
// resolves it. (A specialization need not be exported; importing the module suffices.)
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

// std::hash<NgramId> (SRC-D-TIR-4(2)): keys metalog's n-gram-sequence maps. NgramId is
// already a uniform 128-bit hash, so the first 8 bytes ARE a good size_t — no mixing,
// no allocation (mirrors std::hash<TemplateId>). The maps re-sort their output, so the
// unordered iteration order is not a determinism surface (ADR-16).
template <> struct hash<insight::NgramId>
{
    [[nodiscard]] std::size_t operator()(const insight::NgramId& ngram_id) const noexcept
    {
        std::size_t out{};
        std::memcpy(&out, ngram_id.bytes.data(), sizeof out);
        return out;
    }
};

// std::hash<TraceId> (SRC-D-OTEL-1): keys O2's per-trace ring map. `value` is already an fnv1a
// hash of the OTEL hex, so it IS a good size_t — no mixing. Importing the module suffices.
template <> struct hash<insight::TraceId>
{
    [[nodiscard]] std::size_t operator()(const insight::TraceId& trace_id) const noexcept
    {
        return static_cast<std::size_t>(trace_id.value);
    }
};
// std::hash<SpanId> (SRC-D-OTEL-11): keys metalog's per-window span_id → template map for the
// close-time observed-edge resolution. `value` is already an fnv1a hash of the OTEL hex → a
// good size_t.
template <> struct hash<insight::SpanId>
{
    [[nodiscard]] std::size_t operator()(const insight::SpanId& span_id) const noexcept
    {
        return static_cast<std::size_t>(span_id.value);
    }
};
} // namespace std

// ──────── from api/insight/math/det_math.hpp ────────
// det_math — deterministic, cross-machine bit-identical fixed-point math for
// InSight's deterministic-content and significance-gate paths.
//
// Why this exists: IEEE `+ - * / sqrt` are correctly-rounded and ALREADY
// cross-machine deterministic. The only divergence sources are (a) transcendentals
// (libm `log`/`exp`/`pow` differ across implementations) and (b) float sums whose
// order or FMA-contraction the compiler may vary. This header removes both:
//
//   * det_log2_fixed / det_ln_fixed — the ONLY logarithm permitted in
//     deterministic-content/gate paths. Computed in PURE INTEGER arithmetic
//     (repeated squaring of a fixed-point mantissa), so the result bits are
//     identical on every compiler/arch. No libm, round-to-nearest.
//   * FixedReducer — accumulates Σ in a signed 128-bit INTEGER. Integer addition
//     is exact and associative, so the reduction is order-independent by
//     construction — no float rounding enters the sum. The single
//     conversion to `double` happens once, at the end, via an exact divide.
//
// Header-only; lives in insight-canon, consumed by metalog + eidos. Consuming TUs
// build with -ffp-contract=off so the trailing fixed→double divide is
// never fused; SSE (not x87 80-bit) is the x86-64 default and arm has no x87.
//
// `det` is an API-ONLY domain (like logcraft's `value` partition): inline-in-interface
// is the determinism guarantee itself — consumers must compile these bodies under their
// own -ffp-contract=off — so it has no src/ or detail unit BY DESIGN. Its tests live in
// tests/math/; that dir having no src/ counterpart is the expected shape, not drift.

export namespace insight::det
{

// 128-bit intermediates (det_int128.hpp): `unsigned __int128`/`__int128` on gcc/clang, a pure-C++
// constexpr equivalent on MSVC. Same two's-complement semantics either way, so the canonical digest
// is bit-identical across compilers (the cross-OS determinism leg).
using u128 = detail::u128;
using i128 = detail::i128;

// Fixed-point scale: a stored value v represents v / 2^kFracBits (Qk).
// 40 fractional bits → ~9.1e-13 resolution: far below the 1e-6 tolerances the
// entropy/divergence tests assert, while keeping |value·2^k| < 2^53 for every
// log2/entropy magnitude we produce (|log2| ≤ 64), so the final fixed→double
// conversion is EXACT (int64→double is lossless below 2^53; the divisor is a
// power of two). This is what makes the emitted `double` bit-identical.
inline constexpr unsigned int kFracBits{40U};
inline constexpr std::int64_t kOne{
    static_cast<std::int64_t>(std::uint64_t{1} << kFracBits)}; // 1.0 in Qk

// ln(2) in Qk, = round(0.69314718055994530942 * 2^40). Used to convert log2→ln
// without libm. A mathematical constant (documented derivation), not a magic
// number; verified against the reference vector in test_det_math.cpp.
inline constexpr std::int64_t kLn2Fixed{762123384786};

// log2(x) for a positive integer x, returned in Qk fixed-point — i.e.
// round(log2(x) · 2^kFracBits). Pure integer arithmetic, round-to-nearest,
// no libm, identical on every compiler/architecture.
//
// Precondition: x ≥ 1. In these reductions log2 is only ever applied to counts,
// totals, and products of positive integers, all ≥ 1; x == 0 is a caller bug and
// is mapped to 0 here purely to keep the function total (avoids a negative shift).
[[nodiscard]] constexpr std::int64_t det_log2_fixed(std::uint64_t value) noexcept
{
    if (value <= 1U)
        return 0; // log2(1) = 0; value == 0 is a precondition violation, mapped to 0.

    // Integer part: floor(log2(value)) = position of the most-significant set bit.
    const unsigned msb{
        static_cast<unsigned>(63 - std::countl_zero(value))}; // value ≥ 2 → msb in [1, 63]

    // Work in Q(kFracBits + kGuard) so the kFracBits-bit fraction rounds to
    // nearest. Normalised mantissa m = value / 2^msb ∈ [1, 2), held as m·2^kWork.
    constexpr unsigned kGuard{2U};
    constexpr unsigned kWork{kFracBits + kGuard};
    u128 mantissa{(static_cast<u128>(value) << kWork) >> msb}; // ∈ [2^kWork, 2^(kWork+1))
    const u128 two_work{u128{1} << (kWork + 1U)};              // 2.0 in Q(kWork)

    // Bit-by-bit log2 of the mantissa via repeated squaring (pure integer):
    // squaring m doubles log2(m); the carry out of [1,2) is the next fraction bit.
    std::uint64_t frac{0};
    for (unsigned bit{0}; bit < kWork; ++bit)
    {
        mantissa = (mantissa * mantissa) >> kWork; // m := m² , now in [1, 4)
        frac <<= 1U;
        if (mantissa >= two_work) // m ≥ 2 → emit a 1 bit and halve back into [1,2)
        {
            frac |= 1U;
            mantissa >>= 1U;
        }
    }
    // frac = floor(log2(m) · 2^kWork). Round to kFracBits bits. log2 of a
    // non-power-of-two is irrational, so an exact half never occurs and
    // round-half-up == round-to-nearest-even here.
    const std::int64_t frac_rne{
        static_cast<std::int64_t>((frac + (std::uint64_t{1} << (kGuard - 1))) >> kGuard)};
    return static_cast<std::int64_t>(static_cast<std::uint64_t>(msb) << kFracBits) + frac_rne;
}

// ln(x) for positive integer x, in Qk fixed-point. ln(x) = log2(x) · ln(2).
[[nodiscard]] constexpr std::int64_t det_ln_fixed(std::uint64_t value) noexcept
{
    // log2(value) (Qk) · ln2 (Qk) = Q2k; round back to Qk. det_log2_fixed(value) ≥ 0,
    // so the unsigned cast for the final shift is safe.
    const i128 product{i128{det_log2_fixed(value)} * i128{kLn2Fixed}};
    const u128 rounded{static_cast<u128>(product) + u128{std::uint64_t{1} << (kFracBits - 1U)}};
    return static_cast<std::int64_t>(rounded >> kFracBits);
}

// Exact conversion of a Qk fixed-point value to double: int64→double is lossless
// for |fixed| < 2^53 (true for all magnitudes here) and the divisor is a power of
// two, so the division is exact — no rounding, hence bit-identical everywhere.
[[nodiscard]] constexpr double fixed_to_double(std::int64_t fixed) noexcept
{
    return static_cast<double>(fixed) / static_cast<double>(kOne);
}

// Round-half-up integer division for the final Σ/N normalisation. den > 0
// (a count/total); num may be negative (KL/JS terms can be negative).
[[nodiscard]] constexpr std::int64_t round_div(i128 num, std::int64_t den) noexcept
{
    const i128 den128{den};
    if (num >= i128{0})
        return static_cast<std::int64_t>((num + (den128 / i128{2})) / den128);
    return -static_cast<std::int64_t>(((-num) + (den128 / i128{2})) / den128);
}

// Exact ordered reduction for Σ over a set of terms. All accumulation is
// in a signed 128-bit INTEGER: exact and associative, so the result does not
// depend on summation order or on float rounding. The caller adds terms in the
// canonical (sorted-by-key) order; for integer terms that order is immaterial to
// the value, but the contract keeps the discipline explicit and future-proof.
class FixedReducer
{
  public:
    // Add weight · log2(x) (weight, x positive integers). The defining term of
    // entropy / KL / JS once reformulated into integer-ratio form.
    constexpr void add_weighted_log2(std::uint64_t weight, std::uint64_t value) noexcept
    {
        // weight (u64) widens VALUE-PRESERVING to 128-bit (every u64 is a non-negative i128),
        // matching native `static_cast<__int128>(weight)` — NOT via int64 (would sign-flip ≥2^63).
        // static_cast (not braces): native i128 == __int128 rejects narrowing brace-init from u128.
        acc_ += static_cast<i128>(u128{weight}) * i128{det_log2_fixed(value)};
    }

    // Add an already-fixed-point (Qk) term.
    constexpr void add_fixed(i128 term) noexcept
    {
        acc_ += term;
    }

    [[nodiscard]] constexpr i128 raw() const noexcept
    {
        return acc_;
    }

    // Normalise the accumulated Σ (= value · denom, in Qk) by a positive denom and
    // convert to bits: round_div → Qk, then one exact fixed→double divide.
    [[nodiscard]] constexpr double normalized_bits(std::int64_t denom) const noexcept
    {
        return fixed_to_double(round_div(acc_, denom));
    }

  private:
    i128 acc_{0};
};

} // namespace insight::det

// ──────── from api/insight/tokenization/canonical_event.hpp ────────
export namespace insight::tokenization
{

// CanonicalEvent — Phase 1 output. Every string view points into the
// arena that was passed to the Tokenizer; lifetimes are bounded by
// `arena.reset()` or arena destruction.
//
// `params` is a span over an arena-allocated array of string_views. This
// keeps the event a fixed-size POD with zero per-event heap allocations
// on the tokenizer hot path. Downstream consumers iterate or index it
// the same way they would a vector.
struct CanonicalEvent
{
    EventID id{};
    Timestamp timestamp;
    LogLevel level{LogLevel::Unknown};
    // The format the line was ROUTED to by the strategy layer (the sticky/auto-detect winner).
    // Observability metadata — NOT deterministic MetaLog content; downstream may group/correlate
    // by it (e.g. mixed-stream router diagnostics). Unknown when no strategy matched (RawText
    // fallback aside).
    LogFormat format{LogFormat::Unknown};
    // The low-card FUNCTIONAL SOURCE (subsystem / daemon / job) — a cube dimension (F3b
    // D-F3b-1). NOT the node/host identity (that is `host`).
    std::string_view component;
    // The high-card node/host IDENTITY (F3b D-F3b-1, §5.5-class) — kept, but HORS-CUBE: a
    // field for correlation/grouping, never a cube dimension. Empty when the format has none.
    std::string_view host;
    std::string_view template_str;            // "Connection from <*> port <*>"
    std::span<const std::string_view> params; // ["192.168.1.1", "22"]
    // What this line DOES in the sequence (announced role; StructuralRole
    // registry). Orthogonal to template_str (what the line IS — its content identity)
    // and to the semantic class of tokens inside it. Consumers: phase alignment +
    // structural surprise (Phase 2/4); None for the vast majority of lines.
    StructuralRole structural_role{StructuralRole::None};
    // OTEL trace context (ADR-29 SRC-D-OTEL-1), extracted by the strategy layer for
    // OTEL inputs. CONSUMED in-memory (trace_id scopes the n-gram graph per trace;
    // span_id/parent_span_id feed metalog's observed causal DAG, which IS built and tested —
    // SRC-D-OTEL-11 folds each declared parent edge into that same n-gram graph at window close;
    // what is NOT built is Régime B proper, ADR-29.O1) and NEVER serialized — the MetaLog wire
    // shape is unchanged (OR1). `present == false` for every non-OTEL input → zero added cost.
    OtelTraceContext trace{};
    // Declared ordinal observations (W1, §4A.4 SRC-D-W1-3), captured by the strategy layer from
    // recognized structured numeric fields (kOrdinalFieldCatalog). Consumed-not-tokenized: metalog
    // bins these per schedule into TopKEntry.ordinal_histograms (the W1 carrier); they are NEVER
    // params. A span over arena-allocated storage (like `params`); EMPTY for every non-ordinal line
    // → zero added cost on the hot path (input-conditional, the OTEL D-OTEL-2a precedent).
    std::span<const OrdinalObservation> ordinals;
    // O4b Span Links (ADR-29 / SRC-D-OTEL-9, SRC-D-OTEL-21): the span_ids this
    // span DECLARES a cross-trace edge to (OTEL `links[]`). Consumed metalog-side — each resolves
    // (by span_id, across traces) into the SAME distilled service topology as intra-trace
    // parentage: component(this) → component(linked). A span over arena-allocated storage; EMPTY
    // for every span without links (and every non-span line) → zero added cost. NEVER
    // retained/serialized (the trace-context discipline).
    std::span<const SpanId> linked_span_ids;
    // Observation-provenance attribute (SRC-D-PROV-1): the line is echoed program/script SOURCE
    // (the CI harness printing a run-step body), not an observed runtime event — recognized at the
    // ANSI strip layer by the GHA command-echo SGR wrapper (Fact 1). CONSUMED in-memory — it
    // already demoted `level` to Unknown in the parser, and metalog skips the level-blind salience
    // failure-cue tier for an all-echoed template (§3.1) — and NEVER serialized: the MetaLog wire
    // shape is unchanged (like `trace`/`ordinals`). `false` for every non-echoed line.
    bool echoed_source{false};
    // ── The LEGIBILITY MARKER (DN-29.D16) — the guarantee at the PIPELINE boundary ─────────────
    // EMPTY when the parse recognized at least one declared role; NON-EMPTY when it recognized
    // none, holding a WITNESS KEY that WAS present in the input. Carried here, and not left on
    // `ParsedLine`, because this is where the guarantee binds: the record path NEVER SILENTLY emits
    // a canonical event for input it understood nothing of, and a marker a consumer cannot see
    // desilences the console rather than the contract.
    //
    // A view into arena-stable bytes; CONSUMED in-memory and NEVER serialized — the MetaLog wire
    // shape is unchanged, exactly like `trace` / `ordinals` / `echoed_source`. A DISTINCT species
    // from `echoed_source` (see canon.spi.cppm's declaration for why that separation is what keeps
    // this schema-blind).
    //
    // ⚠ A STATEMENT, NEVER A VERDICT — a marked event is still analysed. Consumers may report it,
    // count it, or route it for improvement; none may drop it on this field alone.
    std::string_view no_role_witness_key;
};

} // namespace insight::tokenization

// ──────── stateless per-line masker configuration ────────
export namespace insight::tokenization
{

// Token-masking configuration for the stateless per-line masker (stateless_template).
// The Drain clustering knobs (max_depth / similarity_threshold / max_clusters) were
// removed with the clustering itself (SRC-D-TID-3) — a stateless
// masker has no tree, no similarity match, and no cluster cap to bound.
//
// SRC-D-TID-14 — THE ANTI-MONSTER BOUNDARY, the rule every mask rule is admitted against.
// A mask rule may classify a SYNTACTIC token class only (digit-leading numerics, hex runs,
// UUIDs, paths, marker+numeric composites): decidable from the token's own bytes, one token,
// no lexicon. Varying WORDS stay LITERAL — a categorical value is a KEEP-lexicon concern, and
// the lexicon is a seed that grows on calibration EVIDENCE, never on anticipation. The
// boundary is what stops the masker becoming a vocabulary: every widening that needs a list
// of words is out of scope here by construction, and belongs to the value-class registry
// (ADR-17), which no package populates. It is also why byte-only single-token rules are the only
// admissible shape — that is what makes them cross-stdlib identical.
struct MaskConfig
{
    // Structurally variable tokens are replaced with "<*>" before the masked template
    // is formed, so they never fossilise into the template identity.
    bool mask_ip_addresses{true}; // IPv4 address tokens (e.g. "192.168.1.1:")
    // NO hex knob. It was removed with its predicate (DN-027): the rule-5 acceptor required
    // a leading '0', so every token it accepted was already digit-leading — a strict subset,
    // inert over ALL inputs rather than merely on a sample. `mask_ip_addresses` stays because
    // its grammar admits a leading '[', and a bracketed token is NOT digit-leading, so it
    // genuinely gates: `[10.20.30.40]` masks on and stays literal off.
    // Identity-derived WHERE (bibles/intent_identity.md §8, SRC-II-8): when set, a GitHub-Actions
    // line whose NATIVE component is empty (GHA carries none) gets its recognize_location()
    // test-file as `component` — populating the cube WHERE axis ABOVE the empty native tier
    // (never faking it — GHA WHERE is identity-derived by construction). OFF by default so every
    // existing path is byte-identical (an additive gated block keeps the wire version: no output
    // change, no golden movement, no version bump); the batch aligned pipeline turns it on to
    // feed the where_set_shift coverage verdict (§5.4).
    bool recognize_test_where{false};
};

} // namespace insight::tokenization

// ──────── structural-role + intent-marker recognition TYPES ────────
// The StructuralRoleRegistry / IntentMarkerRegistry CLASSES (the hardcoded GHA `starts_with`
// chains) moved to the facade's COMPOSED walkers — insight::tokenization::classify / recognize over
// a ComposedSemantics (ADR-17). Canon core is semantic-unaware (SRC-SP-1): it owns the
// recognition ALGORITHM, the semantic packages own the rule ROWS. The result TYPES stay here —
// StructuralRole (above) and IntentMarkerKind / ChildOrder / IntentMarker (below): the grammar's
// vocabulary + the recognizer's return shapes, referenced by the spi rows and by every downstream
// consumer.
export namespace insight::tokenization
{

// ── Intent-marker recognition (bibles/intent_identity.md §6, the registry's segmentation
// rule class; GitHub-Actions dialect tier) ──────────────────────────────────────────────
// On the STRIPPED content stream Sift consumes (studies/004: `strip_workflow_commands`), the
// RESET-class markers that open a behavioural quantum — the segmentation anchors the aligned
// pipeline walks. A JOB banner (`Complete job name: <name>`) names the job-scoped parent; a
// STEP banner (`Run <name>`) opens the step quantum within it (the finest RESET grain, §5.3
// bottoms at job▸step). The payload is the RAW name; canonicalize_intent turns it into the
// alignment CLASS (so `test (win-msvc, …)` → `test (M)` pairs across runs).
//
// DIALECT-GATED — SRC-II-6 — see canon.spi.cppm (dialect_gate / dialect_admits) for the
// contract. Local: `Run ` is GHA-runner-specific and WOULD misfire on a "Run daemon started"
// content line elsewhere (unlike StructuralRoleRegistry's universal `##[group]` markers), which
// is why the row is gated at all. The
// residual within-GHA phantom rate (content lines beginning `Run `) is the measured 0.8%
// (studies/004 Table 2) — a phantom step-quantum simply fails to align (VANISHED+INSERTED,
// low-sev), never a silent mispair (SRC-II-2). Deterministic, ASCII-safe, no cross-line state.
enum class IntentMarkerKind : std::uint8_t
{
    None = 0, ///< the line opens no quantum (the common case)
    Job,      ///< a job-scoped parent opens (`Complete job name: <name>`)
    Step      ///< a step quantum opens (`Run <name>`)
};

// How a level's sibling nodes are matched across two runs (ADR-18, a DECLARED property of the
// dialect level — never a runtime heuristic). GHA: jobs are parallel-by-construction (Unordered →
// set/multiset match, no REORDERED, completion-interleave invisible); steps are sequential-by-YAML
// (Ordered → order-respecting nominal LCS, a transposition IS a signal). child_order is package
// DATA (an IntentMarkerRow field) → it enters the composed semantic_identity (ADR-17).
enum class ChildOrder : std::uint8_t
{
    Ordered = 0, ///< sequential by construction (steps within a job)
    Unordered    ///< parallel by construction (jobs / matrix legs under a parent)
};

struct IntentMarker
{
    IntentMarkerKind kind{IntentMarkerKind::None};
    std::string_view name; ///< raw payload (empty when kind == None); canonicalize_intent → class
    // The raw instance discriminant (ADR-18 / SRC-II-9): the matrix tuple in the display name,
    // kept VERBATIM (never masked) — the stable declared coordinate that separates co-occurring
    // siblings. Empty when the name carries no tuple. `= discriminant_of(name)`.
    std::string_view discriminant;
    ChildOrder child_order{
        ChildOrder::Ordered}; ///< how THIS marker's level matches (job=Unordered)
    auto operator<=>(const IntentMarker&) const = default;
    bool operator==(const IntentMarker&) const = default;
};

} // namespace insight::tokenization

// ──────── from api/insight/tokenization/arena_allocator.hpp ────────
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

// Was THIS canon built with the arena reset-poison instrument (arena_allocator.cpp)? True only in a
// poisoning build, where `reset()` overwrites the bytes it releases instead of merely rewinding the
// bump pointer — which is what makes a use-after-reset observable at all.
//
// A runtime query on purpose: the switch is PRIVATE to canon's translation units, so a constant
// evaluated in a consumer's own compile would report the CONSUMER's flags and be silently wrong.
// A lifetime gate downstream must SKIP on false, never pass — under a rewinding reset it cannot
// distinguish a correct lifetime from a lucky one, and a pass would assert something it did not
// test.
[[nodiscard]] bool arena_poisons_on_reset() noexcept;
} // namespace insight::tokenization

// ──────── from api/insight/utils/failure_lexicon.hpp ────────
export namespace insight::utils
{

// Token-aware failure / warning lexicon matching — the single source of truth
// shared by canon raw-text level inference (the RawTextStrategy fallback in
// infer_leading_log_level) and the MetaLog severity signal.
//
// A cue matches ONLY as a standalone, whitespace-delimited token — surrounding
// punctuation trimmed, ASCII case-insensitive — that EQUALS a lexicon word, or
// that is a CamelCase `…Error` / `…Exception` type name (OperationalError,
// ValueError, IOError, RuntimeException), or that completes a fixed two-token
// phrase ("segmentation fault" — precision-safe only as an adjacent pair).
//
// A lexicon word buried INSIDE a larger token does NOT match: a filename
// `tsc-error-report.json`, an identifier `error_handler`, or a negation
// `no errors found`. That raw-substring over-match was the bug — it promoted
// benign new templates to HIGH "New error" in the diff and inflated severity.
//
// A failure-vocab token classifies its line only IN VERDICT REGISTER (SRC-D-OUT-4): the
// lexicon partitions by benign-collision-proneness — a zero-collision token (an outcome
// verb failed/refused/…, OR a unique failure noun segfault/traceback/unhandled) self-
// anchors and fires bare in prose ("build failed", "unhandled exception"); a collision-
// prone token (error/fail/crash/timeout/fatal/panic/…) fires ONLY when verdict-anchored
// (detail::is_verdict_anchored — CAPS, a kind-slot `:` or a bracket pair, a leading ✗ fail glyph,
// the CamelCase `…Error` type form, or the `segmentation fault` phrase). A bare collision-
// prone noun in a non-verdict line (`Storing crash reports into <path>`, `watcher fail
// event`, `error_handler`) does NOT classify — the structural lesson, not a per-word denylist.
//
// Two precision guards keep a PASSING test from ever reading as a failure (the
// cardinal false-positive — a HIGH "regression" on green torches trust):
//   • a negated type name (`…NotError`/`…NoError`/`…NonError`) is NOT an error type
//     (a test named `…IsNotError` is the textbook false match); and
//   • an error-TYPE NAME alone (`…RaisesValueError`) is demoted to a non-failure when
//     the text also DECLARES a pass verdict ("Passed" / gtest "[ OK ]" / "PASSED"); and
//   • a line carrying a real failure cue but LED by a pass GLYPH (✓/✔/✅/√) is a passing
//     test whose name embeds failure vocabulary, not a regression (SRC-D-OUT-1).
//
// `scan_limit` bounds the head: a token must START within the first `scan_limit`
// chars (it may extend past them — the full word is captured). 0 = scan all of
// `text`. Alloc-free, noexcept (hot-path safe); a single head-bounded pass, except
// the rare error-type-without-failure-word line, which costs one extra full scan for
// the demoting verdict.
[[nodiscard]] bool contains_failure_cue(std::string_view text, std::size_t scan_limit = 0) noexcept;
[[nodiscard]] bool contains_warning_cue(std::string_view text, std::size_t scan_limit = 0) noexcept;

namespace detail
{
    // The shared outcome predicate (SRC-D-OUT-1b) — promoted from failure_lexicon.cpp's
    // anonymous namespace so EVERY severity-classification site can consult it: the cue
    // lexicon (contains_failure_cue) AND infer_leading_log_level's explicit-level Stage 1
    // (parse_log_level), which lives in a SEPARATE TU that could not see a TU-local symbol.
    // True iff the line's leading outcome is a PASS: a pass GLYPH (✓/✔/✅/√) anywhere in the
    // head (an unambiguous per-test verdict, so a failure WORD embedded in the test NAME
    // "✔ … failure …" is not an alert).
    //
    // SRC-D-OUT-2: a leading pass WORD (passed/ok/success/succeeded) ALSO demotes a failure word,
    // but ONLY as the FIRST SIGNIFICANT token ("ok 1 - should return error" → pass; the
    // TAP/node-runner case). A leading failure WORD ⇒ false (failure leads), so a genuine
    // "ERROR:"/"FATAL:" line is preserved; a summary "25 passed, 5 failed" ⇒ false (a number is
    // the first significant token, not "passed"), and a prose "passed" mid-line never demotes.
    // The asymmetry with the glyph is deliberate and is the whole rule: a glyph is an unambiguous
    // verdict that never occurs in prose, so it fires anywhere in the head; a pass WORD occurs in
    // prose ("passed through the proxy"), so it must LEAD or it does not fire at all.
    // Internal/detail — NOT a public product surface (the public failure-lexicon API stays
    // contains_failure_cue / contains_warning_cue); defined with the lexicon in
    // failure_lexicon.cpp.
    [[nodiscard]] bool leading_outcome_is_pass(std::string_view line) noexcept;

    // The verdict-register kernel (SRC-D-OUT-4) — true iff `token` carries the structural
    // decoration CI/test tooling uses to mark an outcome, so a benign-collision-prone
    // failure token classifies its line ONLY in verdict register (never in a path / config
    // line / test description). Anchors: (1) CAPS register — token's raw bytes ALL-UPPERCASE,
    // ≥2 letters (ERROR/FAILED/FATAL); (2) DELIMITER-bound — followed by `:` AND standing in the
    // line's KIND SLOT (SRC-D-OUT-4c, below), or enclosed by `[..]`/`(..)` (`[error]`, `##[error]`,
    // `(FAILED)`);
    // (3) SRC-D-OUT-4a: a LEADING fail glyph `✗`/`✕`/`✖`/`✘`/`❌` marks the line a failed verdict —
    // a line-level register that CONFIRMS the token, never CREATES a cue (a glyph-only line has no
    // failure word, and stays silent). `×` U+00D7 is excluded on purpose: it doubles as a dimension
    // separator ("1920×1080"), the precision risk that deferred D-OUT-3.
    //
    // SRC-D-OUT-4b: a CamelCase error-TYPE name (`…Error`/`…Exception`) is the fourth register
    // form, and it anchors ONLY in verdict register — a thrown `ValueError: bad input` fires, a
    // `▶`-led node:test suite NAME that merely REFERENCES the type does not. The discriminator is
    // register/position, never the token: the same identifier names a type in one line and throws
    // one in the next. (`segmentation fault` is the fifth form, an adjacent-pair phrase.) Both are
    // handled at their own sites in failure_lexicon.cpp, which cite back here.
    // PRECONDITION: `token` MUST be a sub-view of `line` (a for_each_token token) — the
    // kernel recovers the surrounding bytes by pointer arithmetic, as caps/adjacency are
    // pre-casefold byte facts the trimmed token alone does not surface. Pure byte-compare +
    // ASCII case test, order-independent ⇒ cross-stdlib + MSVC bit-identical by construction
    // (F5). The SAME kernel both severity feeders consult; internal/detail, NOT a public surface.
    //
    // SRC-D-OUT-4c — THE KIND SLOT, anchor #2's precondition. A trailing `:` anchors `token` only
    // when `token` occupies the line's KIND SLOT: every token preceding it is itself
    // colon-terminated
    // (`ld:`, `src/main.rs:`, `357:`) or bracket-enclosed (`[main]`, `(none)`, `<WORKSPACE>`). A
    // bare adjacency cannot separate a log prefix `error:` from an object key, a named parameter
    // or a quoted source string — the three are byte-identical in the token's ±1 neighbourhood, so
    // the discriminating information is POSITIONAL and no widening of the neighbourhood recovers
    // it. The rule is monotone-DEMOTING (it only ever removes an anchor, never adds one), which is
    // what makes it admissible under ADR-9 with no corroborating recall argument, and
    // `<path>:<line>:<col>:` needs no special case — it IS a run of colon-terminated tokens, so
    // SRC-D-NOTE-1's hand-parsed shape is an instance of this rule rather than a sibling of it.
    // The walk is WHOLE-LINE and never head-bounded: ADR-20 — bound the scan, never the claim.
    [[nodiscard]] bool is_verdict_anchored(std::string_view line, std::string_view token) noexcept;

    // The count-register kernel (SRC-D-CNT-1) — true iff `token` is a COUNT register summary: its
    // IMMEDIATELY-PRECEDING token (under the shared canon tokenization) is a BARE INTEGER count
    // ("1 failure", "5 failed", "HTTP 500 error") that is NOT part of a numeric/temporal chain (the
    // token before the count is not itself digit-leading — so a leading ISO timestamp
    // "2026-…T11:00:01 ERROR" is NOT count register: ERROR's predecessor `01` is a bare integer,
    // but `01`'s predecessor `00` is the `:MM` minutes, marking `01` a timestamp second). An
    // aggregate statistic, not a per-item verdict. Checked BEFORE the verdict anchors (a counted
    // noun is a summary even with a trailing colon: "1 failure:" is a summary, not Fatal). The
    // symmetric dual of the "25 passed, 5 failed" disconfirming case that forced SRC-D-OUT-1 to be
    // glyph-gated: count-quantified outcome vocab is a summary, not a verdict. A count-register
    // word does NOT confer an alerting level — it caps at Warn (demote, never suppress).
    // PRECONDITION: `token` MUST be a sub-view of `line`. Pure byte/case test, order-independent ⇒
    // cross-stdlib + MSVC bit-identical (F5).
    [[nodiscard]] bool is_count_register(std::string_view line, std::string_view token) noexcept;

    // SRC-D-CNT-1 dual — true iff the head carries a failure-lexicon word in COUNT register (a
    // summary like "1 failure" / "5 failed"). contains_failure_cue treats such words as NON-firing
    // (a count is not a per-item verdict); this reports their presence so infer_leading_log_level
    // caps the line at Warn (surfaced, below per-item verdicts). `scan_limit` bounds the head as
    // for contains_failure_cue. Internal/detail — NOT a public product surface. Cold path
    // (consulted only when contains_failure_cue is false). Pure byte/case test ⇒ cross-stdlib
    // bit-identical (F5).
    [[nodiscard]] bool contains_failure_summary_cue(std::string_view text,
                                                    std::size_t scan_limit = 0) noexcept;

    // ── The NOTE register (SRC-D-NOTE-1) — the FOURTH register, beside verdict (SRC-D-OUT-4),
    // count (SRC-D-CNT-1) and echoed-source (SRC-D-PROV-1). It has NO declaration here on purpose:
    // unlike the three above it has no cross-TU consumer, so its kernel stays private to
    // failure_lexicon.cpp, where BOTH of its consumers live (contains_failure_cue and its
    // count-summary dual). Named here so a reader enumerating the registers from this header does
    // not conclude there are three.
    //
    // WHAT IT SAYS: a token inside the MESSAGE of a compiler NOTE diagnostic — after the
    // structural diagnostic-kind position `<path>:<line>:<col>: note: ` — carries no failure
    // verdict, because a note asserts none. Measured on 3 real green→red gcc cascade pairs: 28 of
    // 29 ranked `note:` findings (96.6 %) were emitted under an ERROR-class label, naming the
    // wrong file, because `note: template argument deduction/substitution failed:` contains
    // `failed` and `failed` is SelfAnchoring by design.
    //
    // THE ANCHOR IS STRUCTURAL, NEVER A BARE WORD. Prose "Note: the deploy failed" keeps its cue:
    // demoting on a bare word would turn a LABELLING defect into a DETECTION defect, which is
    // strictly worse. It is REGISTER-scoped, not LINE-scoped — a verdict anchored EARLIER on the
    // same line (an `##[error]` wrapper) is a different author's claim and survives. And it
    // DEMOTES, NEVER SUPPRESSES: the cue does not fire, the line lands at Unknown and still
    // surfaces (the SRC-D-OUT-1 / SRC-D-PROV-1 precedent). The lexicon is untouched — the defect is
    // CONTEXT, not vocabulary.
} // namespace detail

} // namespace insight::utils

// ──────── from api/insight/utils/time_utils.hpp ────────
export namespace insight::utils
{

// Deterministic reference year for yearless timestamps (BSD syslog). The
// deterministic-content path MUST NOT read the wall clock (insight_determinism_
// model.md § Event-time, MUST 5); a yearless year comes from an injected
// reference, defaulting to this constant. A live consumer MAY pass the real
// current year (read once at stream open); batch/replay uses the constant so the
// parsed year is bit-identical across runs and across the year rollover.
inline constexpr int kDefaultReferenceYear{2024};

// Parse ISO 8601 / RFC 3339 timestamps (UTC).
// Accepted forms: "2024-01-15T10:30:00Z", "2024-01-15T10:30:00.123Z",
//                 "2024-01-15T10:30:00+05:30", "2024-01-15 10:30:00"
[[nodiscard]] std::optional<Timestamp> parse_iso8601(std::string_view timestamp_str) noexcept;

// The RFC3339 full-datetime byte GRAMMAR — one owner, two consumers (ADR-23 erratum 2's "three
// spellings of one shape", collapsed): the Jenkins strategy's `timestamper_prefix_end` delegates
// its character grammar here, and the masker's `bracket_timestamp` composite rule (SRC-D-MSK-5)
// tests a bracket interior with the same function. Homed PUBLIC (not in the mask detail) because
// the Jenkins package imports only insight.canon.api/spi — canon's detail shards are sealed, so a
// detail-homed grammar could not be delegated to (bibles/jenkins_dialect.md §4, homing note).
// Returns the number of bytes consumed by a COMPLETE datetime starting at `pos`, or 0 when the
// bytes at `pos` do not carry one. Accepted shape, byte-exact — `YYYY-MM-DDTHH:MM:SS`, optional
// `.f…` fraction, optional `Z` / `±HH:MM` / `±HHMM` zone. A malformed OPTIONAL part is a hard 0,
// never "stop before it": `2026-01-02T03:04:05+9` matches nothing, so a consumer's "nothing else
// follows" check cannot silently accept a truncated zone. Deliberately NOT a calendar validator
// (month 13 matches): the consumers claim a token CLASS, and the strict character shape is
// already the anti-phantom guard (a Proxifier `[10.20.30.40]`, an ApacheError `[Mon Oct 03 …]`,
// a bare `[12:34:56]` all fail). Pure constexpr byte scan: no locale, no wall clock, ASCII-only
// (F5).
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
    if (pos < text.size() && text[pos] == '.') // optional fraction
    {
        ++pos;
        const std::size_t frac_start{pos};
        while (digit_at(pos))
            ++pos;
        if (pos == frac_start)
            return 0; // a bare '.' is not a fraction
    }
    if (pos < text.size() && text[pos] == 'Z') // optional zone: Z
        ++pos;
    else if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) // or ±HH:MM / ±HHMM
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

// Parse BSD syslog timestamp (no year — yearless RFC3164, e.g. "Jan 15 08:03:22").
// The year is the injected `reference_year` (deterministic; no wall-clock read).
// Accepted form: "Jan  1 12:00:00" or "Jan 15 08:03:22".
[[nodiscard]] std::optional<Timestamp>
parse_bsd_syslog_ts(std::string_view timestamp_str,
                    int reference_year = kDefaultReferenceYear) noexcept;

// Parse CLF/Combined-Log-Format timestamp.
// Accepted form: "10/Oct/2000:13:55:36 -0700"
[[nodiscard]] std::optional<Timestamp> parse_clf_timestamp(std::string_view timestamp_str) noexcept;

// Parse Unix epoch seconds (e.g. "1117838570") to Timestamp.
[[nodiscard]] std::optional<Timestamp>
parse_epoch_timestamp(std::string_view timestamp_str) noexcept;

// Parse OTLP `timeUnixNano` — Unix epoch NANOSECONDS as a digit string (e.g.
// "1705312200000000000") to Timestamp (ADR-29.D5). Integer-only (from_chars +
// integer duration_cast, no float); the OTEL event-time channel so OTEL inputs window like any
// other format. Sub-`system_clock::duration` resolution truncates deterministically per stdlib
// (the OTLP producer emits millisecond-granular nanos → lossless on both libc++/libstdc++).
[[nodiscard]] std::optional<Timestamp>
parse_unix_nano_timestamp(std::string_view timestamp_str) noexcept;

// Parse HDFS compact date+time: date="YYMMDD", time="HHMMSS".
[[nodiscard]] std::optional<Timestamp> parse_compact_date_time(std::string_view date,
                                                               std::string_view time) noexcept;

// Parse Spark-style short-year date+time: "YY/MM/DD HH:MM:SS" (19 chars).
[[nodiscard]] std::optional<Timestamp>
parse_short_year_slash(std::string_view timestamp_str) noexcept;

// Parse Apache error-log timestamp: "Sun Dec 04 04:47:44 2005" (24 chars).
[[nodiscard]] std::optional<Timestamp>
parse_apache_error_ts(std::string_view timestamp_str) noexcept;

// Parse HealthApp compact timestamp: "YYYYMMDD-HH:MM:SS:mmm" (22 chars).
[[nodiscard]] std::optional<Timestamp> parse_health_app_ts(std::string_view timestamp_str) noexcept;

// Parse ISO-like timestamp with comma or dot milliseconds (Log4j / Windows
// CBS). Accepted: "2024-01-15 10:30:00,123" or "2024-01-15 10:30:00.123" Unlike
// parse_iso8601, this REQUIRES space separator (not T) and milliseconds.
[[nodiscard]] std::optional<Timestamp>
parse_log4j_timestamp(std::string_view timestamp_str) noexcept;

// Parse a log-level string case-insensitively.
// Recognises: trace, debug, info, warn/warning, error/err, fatal/critical/crit.
[[nodiscard]] LogLevel parse_log_level(std::string_view level_str) noexcept;

// Infer a log level from the HEAD of an unstructured line (the leading token
// only), for the raw-text fallback where no structured field carries one.
// Markers like "ERROR", "##[error]", "[WARN]", "FAILED" sit at the start of
// real logs; a benign mid-line word ("error rate" on an INFO line) must not
// misclassify it. Bounded + alloc-free — safe on the tokenizer hot path.
[[nodiscard]] LogLevel infer_leading_log_level(std::string_view line) noexcept;

// Parse Nginx error-log timestamp (same format as Apache error logs).
[[nodiscard]] std::optional<Timestamp>
parse_nginx_error_ts(std::string_view timestamp_str) noexcept;
} // namespace insight::utils

// ──────── from api/insight/utils/logger.hpp (accessors + the log_message template + the one
//          live compile-time gate; the MACROS-ONLY layer is src/insight/utils/log_macros.hpp)
//          ────────
export namespace insight::logging
{

namespace detail
{

    // The function INSIGHT_LOG_* expand to. Homed in the module (not the textual macro header) so
    // no first-party declaration leaks through the GMF — log_macros.hpp stays pure preprocessor + a
    // single third-party include (the logcraft canonical pattern, ADR-3.D4).
    template <typename... Args>
    inline void
    log_message(const std::shared_ptr<spdlog::logger>& logger,
                const spdlog::source_loc& source_location, spdlog::level::level_enum level,
                fmt::format_string<Args...> format,
                // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward) — forwarded in body
                Args&&... args)
    {
        if (!logger || !logger->should_log(level))
        {
            return;
        }

        logger->log(source_location, level, fmt::format(format, std::forward<Args>(args)...));
    }

} // namespace detail

// Module logger names.
inline constexpr std::string_view kArenaLogger{"insight.arena"};
inline constexpr std::string_view kMaskLogger{"insight.mask"};
inline constexpr std::string_view kPipelineLogger{"insight.pipeline"};
inline constexpr std::string_view kDetectorLogger{"insight.detector"};
inline constexpr std::string_view kParserLogger{"insight.parser"};
inline constexpr std::string_view kStrategyLogger{"insight.strategy"};
inline constexpr std::string_view kTokenizerLogger{"insight.tokenizer"};

// The registration set, HERE beside the names rather than hand-copied into the impl unit.
// `init_logging` iterates exactly this, so a name that exists but is not registered cannot
// happen without deleting it from a list three lines under the constant you just wrote.
// It is not merely tidier: the impl unit's private copy had drifted, and `kPipelineLogger`
// was the one it had lost — so `pipeline_logger()` fell through to spdlog's DEFAULT logger
// and every pipeline WARN (the §13 cube-collapse one, the n-gram truncation one) was
// emitted untagged, unroutable by name, and invisible to any sink attached to
// "insight.pipeline". A hand-enumerated mirror of a declaration list is the defect class;
// this removes the mirror.
inline constexpr std::array kAllLoggers{kArenaLogger,    kMaskLogger,   kPipelineLogger,
                                        kDetectorLogger, kParserLogger, kStrategyLogger,
                                        kTokenizerLogger};

// Creates all named loggers with a shared stdout colour sink. Call once before any logging
// (thread-safe; first call wins). Defined in the logger.cpp impl unit.
void init_logging(spdlog::level::level_enum default_level = spdlog::level::info);

// Per-module logger accessors (named logger when registered, else the spdlog default).
std::shared_ptr<spdlog::logger> arena_logger();
std::shared_ptr<spdlog::logger> mask_logger();
std::shared_ptr<spdlog::logger> pipeline_logger();
std::shared_ptr<spdlog::logger> detector_logger();
std::shared_ptr<spdlog::logger> parser_logger();
std::shared_ptr<spdlog::logger> strategy_logger();
std::shared_ptr<spdlog::logger> tokenizer_logger();

} // namespace insight::logging
// ──────── from src/insight/utils/token_scan.hpp ────────
export namespace insight::utils
{
namespace detail
{

    // Token delimiters: whitespace + STRUCTURAL punctuation (brackets, parens,
    // quotes, and the `: = , ; |` value/field separators). Identifier- and
    // path-join chars (`- _ . /` …) are deliberately NOT delimiters, so a compound
    // like `tsc-error-report.json` stays a single atom while a bracketed marker
    // `##[error]` or a structured value `level=error` exposes its inner word.
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

    inline constexpr unsigned kAsciiCaseBit{0x20U}; // OR-mask that folds uppercase to lowercase
    inline constexpr unsigned kAlphabetLen{26U};
    inline constexpr unsigned kDecimalDigitLen{10U};

    [[nodiscard]] constexpr bool is_token_alnum(char chr) noexcept
    {
        const unsigned chu{static_cast<unsigned>(static_cast<unsigned char>(chr))};
        return ((chu | kAsciiCaseBit) - 'a') < kAlphabetLen || (chu - '0') < kDecimalDigitLen;
    }

    // Length of an ANSI escape sequence starting at text[pos], or 0 if none. Handles
    // the CSI form `ESC [ <params> <final>` (covers the SGR colour codes CI logs wrap
    // level words in, e.g. `ESC[31mFAILED`) and a bare ESC. ANSI codes are formatting
    // noise, never token content, so a sequence is consumed as a delimiter — a level
    // or cue glued to one is still extracted as a clean word.
    [[nodiscard]] constexpr std::size_t ansi_escape_len(std::string_view text,
                                                        std::size_t pos) noexcept
    {
        if (pos >= text.size() || text[pos] != '\x1b')
            return 0U;
        // A CSI sequence terminates at its first "final byte" (ECMA-48 §5.4).
        constexpr unsigned kCsiFinalByteMin{0x40U};
        constexpr unsigned kCsiFinalByteMax{0x7EU};
        std::size_t end{pos + 1U};
        if (end < text.size() && text[end] == '[')
        {
            ++end; // CSI params + intermediates, up to the final byte
            while (end < text.size() && (static_cast<unsigned char>(text[end]) < kCsiFinalByteMin ||
                                         static_cast<unsigned char>(text[end]) > kCsiFinalByteMax))
                ++end;
            if (end < text.size())
                ++end; // include the final byte
        }
        return end - pos;
    }

} // namespace detail

// Iterate the tokens of `text` under the shared canon tokenisation (see
// is_token_delimiter): split on whitespace + structural punctuation, keep
// identifier/path-join chars inside a token, then trim each token's surrounding
// non-alphanumerics. `visit(token)` is invoked for every non-empty token whose
// START lies within the first `scan_limit` chars (0 = all of `text`); a token
// may extend past the limit (the whole word is captured). Iteration stops early
// when `visit` returns true, and for_each_token then returns true. Alloc-free,
// single pass — used by both leading-level inference and the failure lexicon so
// the two never disagree on what counts as a standalone word.
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
        for (;;) // skip delimiters and ANSI escape sequences
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
        if (!token.empty() && visit(token)) // lvalue: invoked per token
            return true;
    }
    return false;
}

} // namespace insight::utils

// ──────── from src/scan/canon.detail.scan.cppm (ANSI ingest normalization) ────────
// SRC-D-TID-11 — STRIP ANSI/CSI/SGR/OSC ESCAPE SEQUENCES AS A CONTENT NORMALIZATION AT CANON
// INGEST, BEFORE strategy detection AND tokenization, so the format prefix-match, the level
// token-scan and the `component` extraction all see colour-free content. Colour is presentation,
// never content. The ordering is not a preference: the escapes interleave WITHIN and BETWEEN
// tokens, so no per-token mask downstream can reach them — a strip that runs after tokenization
// has already lost. The implementation is a pure byte state machine, hence cross-stdlib
// bit-identical (F5). The one carve-out is the RECOGNITION path's raw read, which needs the
// wrapper intact for SRC-D-PROV-1 (canon.transport.cppm states that seam).
//
// PUBLIC, and homed here rather than in the sealed detail.scan shard for one reason: stage 1 is an
// obligation the `recognize()`/`classify()` DECLARATION places on its callers (see the precondition
// on those two in insight.canon), and a caller cannot discharge an obligation whose only
// implementation is build-private. It sits beside `insight::utils::detail::ansi_escape_len` — the
// second ANSI grammar in this unit — because the two are the pair that will have to be reconciled:
// `ansi_escape_len` treats an escape run as a token DELIMITER and handles no OSC, while the strip
// below deletes the run outright and does handle OSC. Physical adjacency is the cheapest thing
// keeping that divergence visible.
export namespace insight::tokenization
{

inline constexpr unsigned char kEsc{0x1bU}; // ESC, the escape introducer
inline constexpr unsigned char kBel{0x07U}; // BEL, an OSC terminator

namespace detail
{
    // The terminal escape-grammar byte ranges (ECMA-48): a CSI body is params then intermediates
    // then one final byte; OSC runs to a BEL or ST terminator.
    inline constexpr unsigned char kCsiParamLo{0x30U}; // CSI parameter bytes 0–9:;<=>?
    inline constexpr unsigned char kCsiParamHi{0x3fU};
    inline constexpr unsigned char kCsiInterLo{0x20U}; // CSI intermediate bytes (space..'/')
    inline constexpr unsigned char kCsiInterHi{0x2fU};
    inline constexpr unsigned char kCsiFinalLo{0x40U}; // CSI final byte ('@'..'~', incl. SGR 'm')
    inline constexpr unsigned char kCsiFinalHi{0x7eU};

    // Advance past a CSI body (params* intermediates* final?). `pos` is the index just
    // after the `ESC [` introducer; returns the index of the first post-sequence byte.
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

    // Advance past an OSC body to its BEL or ST (ESC \) terminator (consumed). `pos` is
    // the index just after the `ESC ]` introducer.
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

// The §12.5.1(c) passkey's GLOBAL-MODULE forward declaration. The key is DEFINED in the sealed
// `insight.canon.detail.parse` — a shard this public unit cannot import (detail imports api, so
// the arrow only runs the other way) — and a linkage-specification attaches a declaration to the
// GLOBAL module, which is what makes the sealed definition and this name ONE entity across the
// module boundary (the std module's own mechanism). It is EXPORTED so the shard, importing this
// unit, redeclares THE VISIBLE entity rather than a twin gcc-15 never merges — measured: a
// non-exported spelling built on clang-21 and failed friendship access on gcc-15. Exporting leaks
// only an INCOMPLETE name: the definition is sealed and the constructor private, so no consumer
// can construct or complete it — unforgeability is unchanged.
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
// Declared before the class so the in-class friend below names THIS exported entity — a
// friend-first declaration would pin `normalize` non-exported ([module.interface]).
[[nodiscard]] NormalizedLine normalize(std::string_view raw_line, std::string& scratch);

// ── STAGE 1 AS A TYPE (ADR-21.D3) ──────────────────────────────────────────────────────────────
//
// `NormalizedLine` means: STAGE 1 RAN ON THESE BYTES — canon's universal ANSI ingest normalization
// (SRC-D-TID-11), the exact grammar `normalize()` below owns. Produced ONLY by `normalize()`; there
// is deliberately NO constructor from `std::string_view`, because that absence is the whole
// mechanism: a caller cannot reach the content walkers (`classify`/`recognize`/
// `recognize_location`, which take `NormalizedContent`) without having gone through stage 1, and
// cannot fake the passage — the only other producer NARROWS an object it must already hold. The
// obligation this replaces was a comment three repos of call sites were expected to remember; two
// of three consumers broke it silently at a measured cost of 1 077 of 3 193 GitLab markers
// (ADR-21). The same argument that made the transport boundary a type (canon.transport §4:
// "by construction, not by review") makes this one one.
class NormalizedLine
{
  public:
    // The normalized bytes, read-only. An OUTBOUND accessor weakens nothing: reading normalized
    // bytes destroys no stage-1 evidence; minting from arbitrary bytes would. (Do not "harden"
    // this away — the seam's byte-readers have no other expressible edit, §12.5.2.)
    [[nodiscard]] constexpr std::string_view bytes() const noexcept
    {
        return bytes_;
    }

    // The suffix-narrowing door — the caller's own INFERRED stage 2 (a transport strip canon was
    // never told about). Named for what it states: the offset comes from a strip that is NOT a
    // declared catalogue row, which is ADR-22's declared limitation placed at its one
    // call site, greppable, instead of in a comment three repos away. (The DECLARED stage 2 is
    // `TransportStack::peel(NormalizedLine)`, which composes this door with a catalogue row's
    // width.) Narrowing is the safe escape hatch by construction: both real stage-2
    // implementations only ever SHORTEN, and a suffix cannot destroy the stage-1 evidence — which
    // is precisely what a `string_view` constructor would destroy. An `offset` past the end
    // yields the empty content (a whole-line transport line peels to nothing).
    [[nodiscard]] constexpr NormalizedContent undeclared_suffix(std::size_t offset) const noexcept;

  private:
    friend NormalizedLine normalize(std::string_view raw_line, std::string& scratch);
    constexpr explicit NormalizedLine(std::string_view bytes) noexcept : bytes_{bytes} {}
    std::string_view bytes_;
};

// `NormalizedContent` means: stage 1 ran, AND a suffix has since been taken (a stage 2 — declared
// or inferred — or the explicit `undeclared_suffix(0)` statement that there is none). This is the
// only currency `classify()`/`recognize()`/`recognize_location()` accept.
//
// Producers — the door census the deleted P2 lint's value collapsed into (doors are finite where
// call sites are not):
//   * `NormalizedLine::undeclared_suffix(offset)` — public, the caller's inferred stage 2;
//   * `TransportStack::peel(const NormalizedLine&)` — public, the declared stage 2
//     (insight.canon.transport; composes the door above with a catalogue row);
//   * `LogParserPasskey` — the ONE privileged mint (§12.5.1 exit (c)), sealed in
//     `insight.canon.detail.parse` with a private constructor and LogParser its single friend.
//     It exists because canon's own tokenizer consumes walkers on strategy-REBUILT arena bytes
//     (six of 22 strategies assemble `content` rather than narrowing the line), which no suffix
//     door can express — and the attestor is the object that PERFORMED stage 1 unconditionally at
//     its one named site (SRC-D-TID-11), so the boundary guarantee stays exactly the measured
//     defect's scope: unforgeable from OUTSIDE `insight.canon`.
//
// WHAT THE TYPE DOES NOT PROVE (§12.3, load-bearing): it does not prove the RIGHT stage 2 ran and
// deliberately does not carry which one did — a NormalizedContent that knew its transport stack
// would be a declaration reaching the tokenizer, the exact channel canon.transport §4 closes.
class NormalizedContent
{
  public:
    // Read-only bytes — outbound only; see the accessor note on NormalizedLine::bytes().
    [[nodiscard]] constexpr std::string_view bytes() const noexcept
    {
        return bytes_;
    }

  private:
    friend class NormalizedLine; // the suffix door (and, through it, the declared peel)
    // QUALIFIED on purpose: a qualified friend is a pure REFERENCE to the prior (global-module,
    // extern "C++") declaration above — it cannot declare a fresh module-attached entity, which
    // is how the sealed definition in insight.canon.detail.parse stays THIS friend on both
    // gcc-15 and clang-21 (an unqualified spelling bound differently across the two).
    friend class insight::tokenization::LogParserPasskey; // the §12.5.1(c) mint

    constexpr explicit NormalizedContent(std::string_view bytes) noexcept : bytes_{bytes} {}
    std::string_view bytes_;
};

// The §12.2 shape guard, at the type's own declaration so any second member fails the build
// BEFORE any caller is recompiled. This is what restores by-construction on the identity path:
// interposing a struct re-opened a channel a future edit could widen without touching a single
// signature, and these two lines close it — one borrowed view, trivially copyable, nothing else.
static_assert(sizeof(NormalizedContent) == sizeof(std::string_view));
static_assert(std::is_trivially_copyable_v<NormalizedContent>);
static_assert(sizeof(NormalizedLine) == sizeof(std::string_view));
static_assert(std::is_trivially_copyable_v<NormalizedLine>);

constexpr NormalizedContent NormalizedLine::undeclared_suffix(std::size_t offset) const noexcept
{
    const std::size_t clamped{offset > bytes_.size() ? bytes_.size() : offset};
    return NormalizedContent{std::string_view{bytes_.data() + clamped, bytes_.size() - clamped}};
}

// ── STAGE 1 — the factory (the former `strip_escape_sequences(string_view, string&)`; the
// out-parameter form is REMOVED, not deprecated: §12.1 — what canon owes is a stage-1 factory
// with a RETURN TYPE, and two spellings of one behaviour is this document's defect class) ───────
//
// Strip CSI / SGR / OSC and bare-ESC terminal escape sequences from a line as an UNCONDITIONAL
// content normalization at canon ingest — BEFORE tokenization. Colour is presentation, never
// content (SRC-D-TID-10); the escapes interleave within/between tokens (`\x1b[31mERROR\x1b[0m`) so
// a per-token mask cannot reach them — they must die here. A pure byte state machine: no float,
// order-independent → cross-stdlib bit-identical.
//
// FAST PATH, now inside the factory (the gate LogParser used to carry at its call site): a line
// with no ESC byte is a FIXED POINT of the strip, so the result BORROWS the caller's `raw_line`
// with no copy and `scratch` is not touched (`find` is the SIMD any-ESC scan). Only an
// ESC-bearing line rewrites into `scratch` (cleared first; result ≤ input, so a reused scratch is
// allocation-free in steady state).
//
// LIFETIME: the returned NormalizedLine (and every NormalizedContent narrowed from it, and every
// coordinate a walker slices out of THAT — an IntentMarker's name views the handed content,
// ADR-18) borrows `raw_line` or `scratch`; both must outlive every such view.
//
// ⚠ STAGE 1 IS A CONSUMER'S OBLIGATION, NEVER A PACKAGE'S. A semantic package's strategy MUST NOT
// call this: normalization inside a strategy is refused by ADR-17, and a package that
// normalized would double-strip content canon already stripped, then disagree with canon on any
// line where the two grammars differ. The caller that OWNS the ingest runs it once, before the
// transport peel (stage 2). The order is load-bearing: an escape sitting BEFORE a transport
// prefix is invisible to the peel unless the strip ran first.
//
// ⚠ AND IT MUST NEVER OVERWRITE THE BUFFER A Tokenizer LATER READS. The strip is destructive of
// exactly the bytes some provenance hooks need: GHA's command-echo SGR wrapper survives ONLY on
// the raw line (SRC-D-PROV-1, log_parser.cpp), so normalizing a caller's line storage in place
// would silently kill the echoed-source demotion. Stage 1 produces a DERIVED view for recognition;
// the raw line stays raw.
[[nodiscard]] inline NormalizedLine normalize(std::string_view raw_line, std::string& scratch)
{
    if (raw_line.find(static_cast<char>(kEsc)) == std::string_view::npos)
        return NormalizedLine{raw_line}; // fixed point: zero-copy, scratch untouched
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
            break; // a lone trailing ESC — drop it
        const char introducer{raw_line[pos + 1U]};
        if (introducer == '[')
            pos = detail::scan_csi_body(raw_line, pos + 2U);
        else if (introducer == ']')
            pos = detail::scan_osc_body(raw_line, pos + 2U);
        else
            pos += 2U; // a simple two-byte ESC sequence (charset select, reset, …)
    }
    return NormalizedLine{scratch};
}

} // namespace insight::tokenization
