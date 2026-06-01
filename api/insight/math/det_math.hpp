#pragma once

// det_math — deterministic, cross-machine bit-identical fixed-point math for
// InSight's deterministic-content and significance-gate paths (F5-M1/M2; see
// technical_docs/architecture/insight_determinism_model.md, "Float Hardening").
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
//     construction (F5-M2) — no float rounding enters the sum. The single
//     conversion to `double` happens once, at the end, via an exact divide.
//
// Header-only; lives in insight-canon, consumed by metalog + eidos. Consuming TUs
// build with -ffp-contract=off (F5-M3) so the trailing fixed→double divide is
// never fused; SSE (not x87 80-bit) is the x86-64 default and arm has no x87.

#include <bit>
#include <cstdint>

namespace insight::det
{

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
// Precondition: x ≥ 1. In the F5 reductions log2 is only ever applied to counts,
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
    using u128 = unsigned __int128;
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
    const __int128 product{static_cast<__int128>(det_log2_fixed(value)) * kLn2Fixed};
    const unsigned __int128 rounded{static_cast<unsigned __int128>(product) +
                                    (std::uint64_t{1} << (kFracBits - 1U))};
    return static_cast<std::int64_t>(rounded >> kFracBits);
}

// Exact conversion of a Qk fixed-point value to double: int64→double is lossless
// for |fixed| < 2^53 (true for all F5 magnitudes) and the divisor is a power of
// two, so the division is exact — no rounding, hence bit-identical everywhere.
[[nodiscard]] constexpr double fixed_to_double(std::int64_t fixed) noexcept
{
    return static_cast<double>(fixed) / static_cast<double>(kOne);
}

// Round-half-up integer division for the final Σ/N normalisation. den > 0
// (a count/total); num may be negative (KL/JS terms can be negative).
[[nodiscard]] constexpr std::int64_t round_div(__int128 num, std::int64_t den) noexcept
{
    const __int128 den128{den};
    if (num >= 0)
        return static_cast<std::int64_t>((num + (den128 / 2)) / den128);
    return -static_cast<std::int64_t>(((-num) + (den128 / 2)) / den128);
}

// Exact ordered reduction for Σ over a set of terms (F5-M2). All accumulation is
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
        acc_ += static_cast<__int128>(weight) * det_log2_fixed(value);
    }

    // Add an already-fixed-point (Qk) term.
    constexpr void add_fixed(__int128 term) noexcept
    {
        acc_ += term;
    }

    [[nodiscard]] constexpr __int128 raw() const noexcept
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
    __int128 acc_{0};
};

} // namespace insight::det
