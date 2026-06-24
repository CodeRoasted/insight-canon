// insight.canon.api — the public DATA + API surface of canon (1.5.1 unwrap, §11.9). The former
// api/insight/**/*.hpp content (types, det_math, canonical_event, mask_config,
// structural_role_registry, arena_allocator, tokenizer_engine, failure_lexicon, time_utils, logger
// accessors) lives here. std comes from insight.canon.internal; spdlog (3rd-party, the logger
// accessor signatures) is a textual GMF include. det_math is header-only integer math — it stays
// INLINE here (consumers compile it under the package's -ffp-contract=off, the determinism
// guarantee). Class IMPLEMENTATIONS stay in src/*.cpp impl units (byte-identical .a); this
// interface holds only their declarations.
module;
// SPDLOG_ACTIVE_LEVEL is a CMake -D per build type (Debug: TRACE, Release: INFO), propagated
// PUBLIC. Guard a missing definition → default TRACE (nothing elided) — mirrors log_macros.hpp.
#ifndef SPDLOG_ACTIVE_LEVEL
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
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
using WindowID = uint64_t;

// ── Canonicalization-contract version (stateless_template_id.md D-TID-16) ──
// The single canon-owned identifier of the canonicalization CONTRACT — the masking
// rules that turn a raw line into its `template_str` (the stateless per-line masker +
// the F13 class set). Every MetaLog producer DEFAULTS to this (MetaLogConfig), so a
// rules change is one edit HERE and impossible to skip: bump it and old/new metalogs
// become incomparable at the §2.4 gate (re-derive, never migrate — D-TID-9). It names
// the rules generation, NOT the package version (decoupled — a patch release that does
// not touch the masking rules must NOT change it). Bump the suffix on any masking-rule
// change (a new F13 class, the eventual SemanticClassRegistry).
inline constexpr std::string_view kCanonicalizationVersion{"stateless-masks-1"};

// ── Template identity (insight_perf_template_id.md D-TIR-1) ──
// The structural identity of a canonicalised template: the first 16 bytes of
// SHA-256(masked template_str), carried as a fixed-size POD through the whole
// metalog/eidos domain. The 34-byte "h:"+hex string is materialised only at the
// serialize seam (render()). Owned by canon because identity IS "the hash under
// kCanonicalizationVersion" (D-TID-9/D-TID-16) — identity and its comparability
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

// SHA-256 the canonical (masked) template; the first 16 bytes are the id. Same content
// as the former MetaLogEngine::compute_template_id (spec §3.2) — render(template_id_of(s))
// is byte-identical to the old string for every s (D-TIR-1 invariant 2).
[[nodiscard]] TemplateId template_id_of(std::string_view canonical_template) noexcept;
// Wire rendering: "h:" + 32 lowercase hex. The ONLY place the id string materialises.
[[nodiscard]] std::string render(TemplateId template_id);
// Inverse of render() — a TEST / fixture helper only (fixtures construct synthetic ids).
// NOT on any product path: the wire is a one-way terminal render (D-TIR-1 §1).
[[nodiscard]] TemplateId parse_template_id(std::string_view rendered);

// Stream rendering (ADL) so a TemplateId prints as "h:"+hex in logs / test diagnostics
// (the "verbose on failure" rule). Not a product wire path — that is render() at the seam.
inline std::ostream& operator<<(std::ostream& out, const TemplateId& template_id)
{
    return out << render(template_id);
}

// ── Sequences ──
using NGram = std::vector<EventID>;

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

// ─────────────────────────────────────────────────────────────────────────────
// Severity classification
// ─────────────────────────────────────────────────────────────────────────────
enum class Severity : uint8_t
{
    None = 0,
    Low = 1,
    Medium = 2,
    High = 3,
    Critical = 4
};

[[nodiscard]] constexpr std::string_view severity_to_string(Severity sev) noexcept
{
    switch (sev)
    {
        using namespace std::literals;
    case Severity::None:
        return "None"sv;
    case Severity::Low:
        return "Low"sv;
    case Severity::Medium:
        return "Medium"sv;
    case Severity::High:
        return "High"sv;
    case Severity::Critical:
        return "Critical"sv;
    default:
        return "Unknown"sv;
    }
}

} // namespace insight

// std::hash<TemplateId> (D-TIR-1 invariant 3): the content is already a uniform
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

// std::hash<std::vector<TemplateId>> (D-TIR-4): keys the n-gram-sequence maps in
// metalog's merge_behavior / diff_ngram_delta on the sequence itself. FNV-1a combine
// over each id's first-8-bytes hash. Runtime-only (bucket distribution); those maps'
// output is explicitly re-sorted, so the unordered iteration order is NOT a determinism
// surface (ADR 0008). No allocation, no per-id mixing beyond the multiply.
template <> struct hash<std::vector<insight::TemplateId>>
{
    [[nodiscard]] std::size_t
    operator()(const std::vector<insight::TemplateId>& sequence) const noexcept
    {
        constexpr std::size_t kOffsetBasis{14695981039346656037ULL};
        constexpr std::size_t kPrime{1099511628211ULL};
        const hash<insight::TemplateId> id_hash{};
        std::size_t out{kOffsetBasis};
        for (const insight::TemplateId& id : sequence)
        {
            out ^= id_hash(id);
            out *= kPrime;
        }
        return out;
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
};

} // namespace insight::tokenization

// ──────── stateless per-line masker configuration ────────
export namespace insight::tokenization
{

// Token-masking configuration for the stateless per-line masker (stateless_template).
// The Drain clustering knobs (max_depth / similarity_threshold / max_clusters) were
// removed with the clustering itself (stateless_template_id.md D-TID-3) — a stateless
// masker has no tree, no similarity match, and no cluster cap to bound.
struct MaskConfig
{
    // Structurally variable tokens are replaced with "<*>" before the masked template
    // is formed, so they never fossilise into the template identity.
    bool mask_ip_addresses{true};  // IPv4 address tokens (e.g. "192.168.1.1:")
    bool mask_hex_addresses{true}; // hex address tokens  (e.g. "0xdeadbeef")
};

} // namespace insight::tokenization

// ──────── from api/insight/tokenization/structural_role_registry.hpp ────────
export namespace insight::tokenization
{

// StructuralRoleRegistry — classifies a LINE's role in the sequence (what the line
// DOES), as opposed to the SemanticClass of a token inside it (what a value MEANS).
// Two orthogonal ontologies kept in two separate registries — that separation is
// the countermeasure to value-vs-line-role conflation.
//
// Roles here are ANNOUNCED only: the line declares itself with a marker
// (`##[group]`, `##[error]`). Positional roles ("is on the dominant path") are
// DERIVED by the structural layer from the sequence graph — those are layer
// outputs, NOT registry entries; keeping that line bright is what keeps "registry"
// honest. The catalog is a seed; extend `classify` as scenarios surface markers.
//
// Bridge direction is one-way: the structural layer MAY read semantic annotations
// (a kept `EXIT_CODE != 0` is a strong Terminator candidate) — never the reverse.
// This seed recognizes the announced GitHub-Actions/Azure markers; the exit-code
// bridge and level-based refinements are deliberate later additions.
class StructuralRoleRegistry
{
  public:
    [[nodiscard]] static StructuralRole classify(std::string_view content) noexcept
    {
        if (content.starts_with("##[group]") || content.starts_with("::group::"))
            return StructuralRole::GroupBegin;
        if (content.starts_with("##[endgroup]") || content.starts_with("::endgroup::"))
            return StructuralRole::GroupEnd;
        if (content.starts_with("##[error]") || content.starts_with("::error::"))
            return StructuralRole::Terminator;
        return StructuralRole::None;
    }
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
// Two precision guards keep a PASSING test from ever reading as a failure (the
// cardinal false-positive — a HIGH "regression" on green torches trust):
//   • a negated type name (`…NotError`/`…NoError`/`…NonError`) is NOT an error type
//     (a test named `…IsNotError` is the textbook false match); and
//   • an error-TYPE NAME alone (`…RaisesValueError`) is demoted to a non-failure when
//     the text also DECLARES a pass verdict ("Passed" / gtest "[ OK ]" / "PASSED").
// Both guards override only the weak, name-based signal — an explicit failure WORD
// ("error"/"failed"/"segfault"/…) always wins, so a real failure still matches.
//
// `scan_limit` bounds the head: a token must START within the first `scan_limit`
// chars (it may extend past them — the full word is captured). 0 = scan all of
// `text`. Alloc-free, noexcept (hot-path safe); a single head-bounded pass, except
// the rare error-type-without-failure-word line, which costs one extra full scan for
// the demoting verdict.
[[nodiscard]] bool contains_failure_cue(std::string_view text, std::size_t scan_limit = 0) noexcept;
[[nodiscard]] bool contains_warning_cue(std::string_view text, std::size_t scan_limit = 0) noexcept;

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

// Parse HDFS compact date+time: date="YYMMDD", time="HHMMSS".
[[nodiscard]] std::optional<Timestamp> parse_compact_date_time(std::string_view date,
                                                               std::string_view time) noexcept;

// Parse Spark-style short-year date+time: "YY/MM/DD HH:MM:SS" (19 chars).
[[nodiscard]] std::optional<Timestamp>
parse_short_year_slash(std::string_view timestamp_str) noexcept;

// Parse BGL dotted date: "YYYY.MM.DD" (10 chars).  No time component.
[[nodiscard]] std::optional<Timestamp> parse_dotted_date(std::string_view timestamp_str) noexcept;

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

// Compile-time DEBUG gate for `if constexpr` call-site elision (tokenizer/parser progress logs).
// Mirrors the macro layer's SPDLOG_ACTIVE_LEVEL threshold. Lives in the module (importable via
// insight.canon.api) rather than the textual macro header — it is a first-party declaration, so
// per §11.4 it must not leak through the GMF-textual log_macros.hpp. The TRACE/INFO/WARN twins are
// gone (no `if constexpr` site used them; the macros gate those levels themselves).
inline constexpr bool kDebugLogsEnabled{SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_DEBUG};

namespace detail
{

    // The function INSIGHT_LOG_* expand to. Homed in the module (not the textual macro header) so
    // no first-party declaration leaks through the GMF — log_macros.hpp stays pure preprocessor + a
    // single third-party include (the logcraft canonical pattern, §11.4).
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
