// det_int128.hpp — portable 128-bit integer for the det_math fixed-point core.
//
// det_math (canon.api.cppm, namespace insight::det) needs 128-bit integer intermediates:
// the repeated-squaring log2 squares a ~2^43 mantissa (→ ~2^86), det_ln multiplies two
// ~2^46/2^40 fixed-point values, and FixedReducer accumulates a Σ that can exceed 64 bits before
// the final round_div. gcc/clang provide `__int128` natively; MSVC does NOT (no 128-bit integer
// type at all). Because det_math is `constexpr`, the MSVC fallback cannot use _umul128/_udiv128
// (not constexpr) — it must be pure C++.
//
// DETERMINISM CONTRACT (the whole reason this file exists): the emitted canonical digest must be
// BIT-IDENTICAL across gcc/clang/MSVC. So:
//   - On gcc/clang, `u128`/`i128` ARE `unsigned __int128`/`__int128` — the proven path is byte-for-
//     byte untouched; the existing golden (det_public_proof.sh) cannot move.
//   - On MSVC, they are a pure-C++ constexpr struct implementing EXACTLY the operations det_math
//     uses, with two's-complement semantics identical to `__int128` (limb-based mul, restoring long
//     division, subtraction as a + ~b + 1). A strict value-equivalent, not an approximation.
//   - INSIGHT_DET_FORCE_PORTABLE_INT128 forces the struct even on gcc/clang, so a Linux unit test
//     (test_det_math.cpp) asserts struct ≡ native bit-for-bit — the MSVC-equivalence oracle that
//     runs WITHOUT an MSVC box. This is how we MEASURE cross-OS bit-identity instead of assuming it.
//
// Scope is deliberately minimal: only the operators the det core AND its downstream consumers
// (metalog's HLL harmonic-sum + stats accumulators) invoke are provided — native `__int128` supplies
// them ALL for free, so a missing one only ever surfaces on MSVC. Adding an op is a consumer-driven
// change that MUST also extend the oracle (test_det_int128_portable.cpp), so the gap can't recur
// silently; it moves a digest golden only if it changes an existing computed value (a new operator on
// a new consumer path does not — canon's det_public_proof golden is untouched by +=/<).
#ifndef INSIGHT_CANON_DET_INT128_HPP
#define INSIGHT_CANON_DET_INT128_HPP

#include <cstdint>

#if (defined(__SIZEOF_INT128__) || defined(__GNUC__) || defined(__clang__)) && \
    !defined(INSIGHT_DET_FORCE_PORTABLE_INT128)
#define INSIGHT_DET_HAS_NATIVE_INT128 1
#endif

namespace insight::det::detail
{

#ifdef INSIGHT_DET_HAS_NATIVE_INT128

// Native path (gcc/clang): the proven types, unchanged. The portable structs below are not even
// compiled — det_math behaves exactly as it did before this header existed.
using u128 = unsigned __int128;
using i128 = __int128;

#else

// ── Portable constexpr 128-bit (MSVC, or forced for the equivalence test) ──────────────────────
// Little-endian two's-complement: lo = low 64 bits, hi = high 64 bits. Only the det_math op set is
// implemented. Every member is constexpr. Signed ops are layered on the unsigned core.

struct u128
{
    std::uint64_t lo{0};
    std::uint64_t hi{0};

    constexpr u128() noexcept = default;
    constexpr u128(std::uint64_t low, std::uint64_t high) noexcept : lo{low}, hi{high} {}
    // Implicit widening, matching `unsigned __int128`, so det_math call sites are byte-identical
    // (u64 counts, the `u128{1}` literals, `static_cast<u128>(value)`). Single u64 ctor only —
    // an extra `int` overload makes `u128{1ULL}` ambiguous (both viable).
    constexpr u128(std::uint64_t value) noexcept : lo{value} {} // NOLINT(google-explicit-constructor)

    [[nodiscard]] constexpr bool operator>=(const u128& o) const noexcept
    {
        return hi != o.hi ? hi > o.hi : lo >= o.lo;
    }
    // < : metalog's HLL small-range branch (`raw < kSmallRangeThreshold`, threshold a u64 that widens
    // via the implicit u64 ctor). Mirror of >=, inverted; native `unsigned __int128` provides it free.
    [[nodiscard]] constexpr bool operator<(const u128& o) const noexcept
    {
        return hi != o.hi ? hi < o.hi : lo < o.lo;
    }
    // == / != : used by the decimal serializer's `while (magnitude != 0)` loop (the proof digest
    // prints FixedReducer::raw() this way; native `unsigned __int128` provides these built-in, so
    // the fixture code is identical on both paths).
    [[nodiscard]] constexpr bool operator==(const u128& o) const noexcept { return lo == o.lo && hi == o.hi; }
    [[nodiscard]] constexpr bool operator!=(const u128& o) const noexcept { return !(*this == o); }

    [[nodiscard]] constexpr u128 operator+(const u128& o) const noexcept
    {
        const std::uint64_t low{lo + o.lo};
        return u128{low, hi + o.hi + (low < lo ? 1ULL : 0ULL)}; // low<lo ⇒ wrap ⇒ carry
    }
    // += : metalog's HLL harmonic sum (`sum_fixed += 1<<(kHllFrac-reg)`). Same +-then-assign the
    // native type does; mirrors the existing >>= idiom.
    constexpr u128& operator+=(const u128& o) noexcept { return *this = *this + o, *this; }
    [[nodiscard]] constexpr u128 operator~() const noexcept { return u128{~lo, ~hi}; }
    [[nodiscard]] constexpr u128 operator-(const u128& o) const noexcept
    {
        return *this + (~o) + u128{1ULL}; // a - b = a + (~b + 1)
    }

    [[nodiscard]] constexpr u128 operator>>(unsigned s) const noexcept
    {
        if (s == 0)
            return *this;
        if (s >= 128U)
            return u128{0, 0};
        if (s >= 64U)
            return u128{hi >> (s - 64U), 0};
        return u128{(lo >> s) | (hi << (64U - s)), hi >> s};
    }
    [[nodiscard]] constexpr u128 operator<<(unsigned s) const noexcept
    {
        if (s == 0)
            return *this;
        if (s >= 128U)
            return u128{0, 0};
        if (s >= 64U)
            return u128{0, lo << (s - 64U)};
        return u128{lo << s, (hi << s) | (lo >> (64U - s))};
    }
    constexpr u128& operator>>=(unsigned s) noexcept { return *this = *this >> s, *this; }

    // 128×128 → low 128 bits (two's-complement wrap, matching __int128). det_math products always
    // fit in 128 bits, so the high truncation is never observed.
    //
    // Schoolbook over four 32-bit limbs into four 32-bit result limbs r0..r3 (r0 = bits 0..31, …).
    // The carry between columns can EXCEED 32 bits (a column sums up to four 32×32 products ≈ 2^66),
    // so it is threaded as a full 64-bit `carry` — the earlier bug was carrying only `>>32`, dropping
    // the overflow above bit 64 (the oracle test caught it: hi word off by a multiple of 2^32).
    [[nodiscard]] constexpr u128 operator*(const u128& o) const noexcept
    {
        const std::uint64_t a[4]{lo & 0xFFFFFFFFULL, lo >> 32, hi & 0xFFFFFFFFULL, hi >> 32};
        const std::uint64_t b[4]{o.lo & 0xFFFFFFFFULL, o.lo >> 32, o.hi & 0xFFFFFFFFULL, o.hi >> 32};
        std::uint64_t r[4]{0, 0, 0, 0}; // result limbs (low 128 bits)
        for (int i{0}; i < 4; ++i)
        {
            std::uint64_t carry{0};
            for (int j{0}; i + j < 4; ++j)
            {
                // r[i+j] currently holds a partial ≤ 32 bits + this column's running sum; keep it
                // in a 64-bit acc and split into low-32 (stored) and high (carried) each step.
                const std::uint64_t acc{r[i + j] + a[i] * b[j] + carry};
                r[i + j] = acc & 0xFFFFFFFFULL;
                carry = acc >> 32;
            }
            // carry beyond limb 3 is the >2^128 part — discarded (matches __int128 wrap).
        }
        return u128{r[0] | (r[1] << 32), r[2] | (r[3] << 32)};
    }

    // Restoring long division, bit-serial MSB→LSB. `want_remainder` selects which result the same
    // loop returns — det_math needs only the quotient (round_div, 128/positive-64); the proof
    // digest's decimal serializer also needs % (magnitude % 10) and / 10. One implementation, no
    // incomplete-type nested struct, no duplicated loop.
    [[nodiscard]] constexpr u128 div_or_mod(const u128& den, bool want_remainder) const noexcept
    {
        u128 quot{0, 0};
        u128 rem{0, 0};
        for (int bit{127}; bit >= 0; --bit)
        {
            const unsigned s{static_cast<unsigned>(bit)};
            rem = rem << 1U;
            rem.lo |= (s >= 64U ? (hi >> (s - 64U)) : (lo >> s)) & 1ULL;
            if (rem >= den)
            {
                rem = rem - den;
                if (s >= 64U)
                    quot.hi |= (1ULL << (s - 64U));
                else
                    quot.lo |= (1ULL << s);
            }
        }
        return want_remainder ? rem : quot;
    }
    [[nodiscard]] constexpr u128 operator/(const u128& den) const noexcept { return div_or_mod(den, false); }
    [[nodiscard]] constexpr u128 operator%(const u128& den) const noexcept { return div_or_mod(den, true); }

    [[nodiscard]] explicit constexpr operator std::uint64_t() const noexcept { return lo; }
    [[nodiscard]] explicit constexpr operator std::int64_t() const noexcept
    {
        return static_cast<std::int64_t>(lo);
    }
};

struct i128
{
    u128 bits{0, 0}; // two's-complement; sign = top bit of bits.hi

    constexpr i128() noexcept = default;
    constexpr i128(u128 raw) noexcept : bits{raw} {} // NOLINT(google-explicit-constructor)
    constexpr i128(std::int64_t v) noexcept          // NOLINT: sign-extend
        : bits{static_cast<std::uint64_t>(v), v < 0 ? ~0ULL : 0ULL}
    {
    }
    constexpr i128(int v) noexcept : i128{static_cast<std::int64_t>(v)} {} // NOLINT

    [[nodiscard]] constexpr bool is_negative() const noexcept { return (bits.hi >> 63) != 0; }
    [[nodiscard]] constexpr i128 operator-() const noexcept { return i128{(~bits) + u128{1ULL}}; }
    [[nodiscard]] constexpr u128 magnitude() const noexcept { return is_negative() ? (-*this).bits : bits; }

    [[nodiscard]] constexpr bool operator>=(const i128& o) const noexcept
    {
        const bool ln{is_negative()}, rn{o.is_negative()};
        return ln != rn ? rn /* neg < non-neg */ : bits >= o.bits; // same sign ⇒ unsigned order
    }

    [[nodiscard]] constexpr i128 operator+(const i128& o) const noexcept { return i128{bits + o.bits}; }
    constexpr i128& operator+=(const i128& o) noexcept { return bits = bits + o.bits, *this; }

    [[nodiscard]] constexpr i128 operator*(const i128& o) const noexcept
    {
        const bool neg{is_negative() != o.is_negative()};
        const i128 mag{magnitude() * o.magnitude()};
        return neg ? -mag : mag;
    }
    [[nodiscard]] constexpr i128 operator/(const i128& den) const noexcept
    {
        const bool neg{is_negative() != den.is_negative()};
        const i128 q{magnitude() / den.magnitude()}; // truncate toward zero, like __int128 /
        return neg ? -q : q;
    }

    [[nodiscard]] explicit constexpr operator std::int64_t() const noexcept
    {
        return static_cast<std::int64_t>(bits.lo);
    }
    [[nodiscard]] explicit constexpr operator u128() const noexcept { return bits; }
};

#endif // INSIGHT_DET_HAS_NATIVE_INT128

} // namespace insight::det::detail

#endif // INSIGHT_CANON_DET_INT128_HPP
