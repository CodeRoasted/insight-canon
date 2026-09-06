// invariant: the emitted canonical digest is BIT-IDENTICAL across gcc, clang and MSVC.
// note: det_math is constexpr, so the MSVC fallback cannot use _umul128/_udiv128 and is pure C++.
// refs: ADR-3.D4, BIB:determinism_model
#ifndef INSIGHT_CANON_DET_INT128_HPP
#define INSIGHT_CANON_DET_INT128_HPP

#include <cstdint>

#if (defined(__SIZEOF_INT128__) || defined(__GNUC__) || defined(__clang__)) &&                     \
    !defined(INSIGHT_DET_FORCE_PORTABLE_INT128)
#define INSIGHT_DET_HAS_NATIVE_INT128 1
#endif

namespace insight::det::detail
{

#ifdef INSIGHT_DET_HAS_NATIVE_INT128

using u128 = unsigned __int128;
using i128 = __int128;

#else

/***************************************************************************************************
D-LSRC-9 — the portable 128-bit operator set is consumer-driven; adding one extends the oracle
Native `unsigned __int128` supplies every operator for free, so a member missing from the portable
struct compiles on gcc and on clang and fails only on MSVC, where nobody builds daily. The set below
is therefore exactly what `det_math` and its downstream consumers invoke, and an operator is added
only with a consumer that invokes it. The same change MUST extend the equivalence oracle,
F-SRC-insight-canon:test_det_int128_portable.cpp, which forces this struct on a Linux leg and
requires it equal native `__int128` over a value sweep — that is what stops the gap recurring
silently. Adding an operator moves no digest golden by itself; only a change to an existing computed
value does.
***************************************************************************************************/
// invariant: little-endian two's-complement — lo is the low 64 bits, hi the high 64.
struct u128
{
    std::uint64_t lo{0};
    std::uint64_t hi{0};

    constexpr u128() noexcept = default;
    constexpr u128(std::uint64_t low, std::uint64_t high) noexcept : lo{low}, hi{high} {}
    // invariant: exactly ONE converting constructor — a second int overload makes `u128{1ULL}`
    // ambiguous.
    // note: implicit widening matches `unsigned __int128`, keeping call sites byte-identical.
    constexpr u128(std::uint64_t value) noexcept : lo{value} {}

    [[nodiscard]] constexpr bool operator>=(const u128& o) const noexcept
    {
        return hi != o.hi ? hi > o.hi : lo >= o.lo;
    }
    // note: metalog's HLL small-range branch needs `<`, which the native type gives free.
    [[nodiscard]] constexpr bool operator<(const u128& o) const noexcept
    {
        return hi != o.hi ? hi < o.hi : lo < o.lo;
    }
    // note: the proof digest's decimal serializer needs == and != for its magnitude loop.
    [[nodiscard]] constexpr bool operator==(const u128& o) const noexcept
    {
        return lo == o.lo && hi == o.hi;
    }
    [[nodiscard]] constexpr bool operator!=(const u128& o) const noexcept
    {
        return !(*this == o);
    }

    [[nodiscard]] constexpr u128 operator+(const u128& o) const noexcept
    {
        const std::uint64_t low{lo + o.lo};
        return u128{low, hi + o.hi + (low < lo ? 1ULL : 0ULL)};
    }
    // note: metalog's HLL harmonic sum needs +=.
    constexpr u128& operator+=(const u128& o) noexcept
    {
        return *this = *this + o, *this;
    }
    [[nodiscard]] constexpr u128 operator~() const noexcept
    {
        return u128{~lo, ~hi};
    }
    [[nodiscard]] constexpr u128 operator-(const u128& o) const noexcept
    {
        return *this + (~o) + u128{1ULL};
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
    constexpr u128& operator>>=(unsigned s) noexcept
    {
        return *this = *this >> s, *this;
    }

    // post: the low 128 bits of the product, wrapping in two's complement as `__int128` does.
    // invariant: a limb keeps only 32 bits, so acc = r + a*b + carry never exceeds 2^64 - 1.
    [[nodiscard]] constexpr u128 operator*(const u128& o) const noexcept
    {
        const std::uint64_t a[4]{lo & 0xFFFFFFFFULL, lo >> 32, hi & 0xFFFFFFFFULL, hi >> 32};
        const std::uint64_t b[4]{o.lo & 0xFFFFFFFFULL, o.lo >> 32, o.hi & 0xFFFFFFFFULL,
                                 o.hi >> 32};
        std::uint64_t r[4]{0, 0, 0, 0};
        for (int i{0}; i < 4; ++i)
        {
            std::uint64_t carry{0};
            for (int j{0}; i + j < 4; ++j)
            {
                // assert: r[i + j] holds at most 32 bits, so this sum fits a 64-bit accumulator.
                const std::uint64_t acc{r[i + j] + a[i] * b[j] + carry};
                r[i + j] = acc & 0xFFFFFFFFULL;
                carry = acc >> 32;
            }
            // assert: a carry past limb 3 is the part above 2^128 and is discarded, matching
            // `__int128` wrap.
        }
        return u128{r[0] | (r[1] << 32), r[2] | (r[3] << 32)};
    }

    // post: the quotient, or the remainder when `want_remainder` — one restoring division,
    // bit-serial.
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
    [[nodiscard]] constexpr u128 operator/(const u128& den) const noexcept
    {
        return div_or_mod(den, false);
    }
    [[nodiscard]] constexpr u128 operator%(const u128& den) const noexcept
    {
        return div_or_mod(den, true);
    }

    [[nodiscard]] explicit constexpr operator std::uint64_t() const noexcept
    {
        return lo;
    }
    [[nodiscard]] explicit constexpr operator std::int64_t() const noexcept
    {
        return static_cast<std::int64_t>(lo);
    }
};

struct i128
{
    // invariant: two's-complement — the sign is the top bit of `bits.hi`.
    u128 bits{0, 0};

    constexpr i128() noexcept = default;
    // note: implicit construction from u128 and from int64 mirrors `__int128`'s own conversions.
    constexpr i128(u128 raw) noexcept : bits{raw} {}
    constexpr i128(std::int64_t v) noexcept
        : bits{static_cast<std::uint64_t>(v), v < 0 ? ~0ULL : 0ULL}
    {
    }
    constexpr i128(int v) noexcept : i128{static_cast<std::int64_t>(v)} {}

    [[nodiscard]] constexpr bool is_negative() const noexcept
    {
        return (bits.hi >> 63) != 0;
    }
    [[nodiscard]] constexpr i128 operator-() const noexcept
    {
        return i128{(~bits) + u128{1ULL}};
    }
    [[nodiscard]] constexpr u128 magnitude() const noexcept
    {
        return is_negative() ? (-*this).bits : bits;
    }

    // post: signed order — a negative operand is the smaller; same-sign operands compare
    // unsigned.
    [[nodiscard]] constexpr bool operator>=(const i128& o) const noexcept
    {
        const bool ln{is_negative()}, rn{o.is_negative()};
        return ln != rn ? rn : bits >= o.bits;
    }

    [[nodiscard]] constexpr i128 operator+(const i128& o) const noexcept
    {
        return i128{bits + o.bits};
    }
    constexpr i128& operator+=(const i128& o) noexcept
    {
        return bits = bits + o.bits, *this;
    }

    [[nodiscard]] constexpr i128 operator*(const i128& o) const noexcept
    {
        const bool neg{is_negative() != o.is_negative()};
        const i128 mag{magnitude() * o.magnitude()};
        return neg ? -mag : mag;
    }
    // post: truncates toward zero, as `__int128` division does.
    [[nodiscard]] constexpr i128 operator/(const i128& den) const noexcept
    {
        const bool neg{is_negative() != den.is_negative()};
        const i128 q{magnitude() / den.magnitude()};
        return neg ? -q : q;
    }

    [[nodiscard]] explicit constexpr operator std::int64_t() const noexcept
    {
        return static_cast<std::int64_t>(bits.lo);
    }
    [[nodiscard]] explicit constexpr operator u128() const noexcept
    {
        return bits;
    }
};

#endif

} // namespace insight::det::detail

#endif
