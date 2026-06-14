// test_det_int128_portable.cpp — the MSVC-equivalence ORACLE for the portable 128-bit shim.
//
// det_int128.hpp gives det_math native `__int128` on gcc/clang and a pure-C++ struct on MSVC. The
// determinism contract is that the two are bit-for-bit identical. We cannot run MSVC here, but we
// CAN compile the portable struct on Linux (forced) and assert it equals native `__int128` over a
// wide value sweep — which proves the struct's arithmetic, the only thing that differs on MSVC.
// (The module wiring is proven separately by the 449 tests + det_public_proof golden.)
//
// Mechanism: include the header TWICE into two namespaces — once native, once forced-portable —
// then drive both through the exact operation set det_math uses (shift, mul, add, compare, 128/64
// div, signed mul/div) and require equality on every sample. Any divergence here is the divergence
// MSVC would have produced; catching it on Linux is the point.
//
// If this TU is built where the platform has no native __int128 at all, it degrades to a tautology
// (both halves are the portable struct) and still passes — the gcc/clang CI legs are where it bites.
// NOLINTBEGIN

#include <gtest/gtest.h>

#include <array>
#include <cstdint>

// 1) native view (only meaningful where __int128 exists)
#if defined(__SIZEOF_INT128__) || defined(__GNUC__) || defined(__clang__)
#define ORACLE_HAS_NATIVE 1
namespace nat {
using u128 = unsigned __int128;
using i128 = __int128;
} // namespace nat
#endif

// 2) forced-portable view — include the real shim with the force flag, in its own namespace.
#define INSIGHT_DET_FORCE_PORTABLE_INT128 1
#include "det/det_int128.hpp"
namespace port = insight::det::detail; // port::u128 / port::i128 are the struct

namespace
{

#ifdef ORACLE_HAS_NATIVE

// Helpers: extract the low/high 64-bit words from each representation, to compare bit-for-bit.
constexpr std::uint64_t lo_of(nat::u128 v) noexcept { return static_cast<std::uint64_t>(v); }
constexpr std::uint64_t hi_of(nat::u128 v) noexcept { return static_cast<std::uint64_t>(v >> 64); }
constexpr std::uint64_t lo_of(port::u128 v) noexcept { return v.lo; }
constexpr std::uint64_t hi_of(port::u128 v) noexcept { return v.hi; }
constexpr std::uint64_t lo_of(nat::i128 v) noexcept { return static_cast<std::uint64_t>(static_cast<nat::u128>(v)); }
constexpr std::uint64_t hi_of(nat::i128 v) noexcept { return static_cast<std::uint64_t>(static_cast<nat::u128>(v) >> 64); }
constexpr std::uint64_t lo_of(port::i128 v) noexcept { return v.bits.lo; }
constexpr std::uint64_t hi_of(port::i128 v) noexcept { return v.bits.hi; }

// A spread of u64 operands: edges, powers of two, primes, and the ≥2^63 region (the value-
// preservation hazard for the signed-widening of weights).
constexpr std::array<std::uint64_t, 18> kSamples{
    0ULL, 1ULL, 2ULL, 3ULL, 7ULL, 255ULL, 256ULL, 1000ULL, 65535ULL,
    (1ULL << 32), (1ULL << 40), (1ULL << 42) + 7, (1ULL << 52), (1ULL << 62),
    (1ULL << 63), (1ULL << 63) + 12345, ~0ULL - 1ULL, ~0ULL};

#define EXPECT_SAME_U128(N, P)                                                      \
    do {                                                                            \
        EXPECT_EQ(lo_of(N), lo_of(P)) << "lo mismatch";                             \
        EXPECT_EQ(hi_of(N), hi_of(P)) << "hi mismatch";                             \
    } while (0)
#define EXPECT_SAME_I128(N, P) EXPECT_SAME_U128(N, P)

TEST(DetInt128Portable, UnsignedMulShiftMatchesNative)
{
    // The det_log2_fixed kernel: (a << w) >> msb, then m*m >> w, plus the ≥ compare.
    for (auto a : kSamples)
        for (unsigned w : {0U, 1U, 40U, 42U, 63U, 64U, 84U})
        {
            const nat::u128 n{(static_cast<nat::u128>(a) << w)};
            const port::u128 p{port::u128{a} << w};
            EXPECT_SAME_U128(n, p);
            EXPECT_SAME_U128(n >> 1, p >> 1U);
            EXPECT_SAME_U128(n * n, p * p);
        }
}

TEST(DetInt128Portable, UnsignedCompareMatchesNative)
{
    for (auto a : kSamples)
        for (auto b : kSamples)
        {
            EXPECT_EQ(static_cast<nat::u128>(a) >= static_cast<nat::u128>(b),
                      port::u128{a} >= port::u128{b})
                << a << " >= " << b;
            // < : metalog's HLL small-range branch. Also drive the MIXED u128 < u64 form (the
            // threshold is a u64 that widens through the implicit ctor) — the exact call-site shape.
            EXPECT_EQ(static_cast<nat::u128>(a) < static_cast<nat::u128>(b),
                      port::u128{a} < port::u128{b})
                << a << " < " << b;
            EXPECT_EQ(static_cast<nat::u128>(a) < b, port::u128{a} < b) << a << " < (u64)" << b;
        }
}

TEST(DetInt128Portable, UnsignedAddSubMatchesNative)
{
    for (auto a : kSamples)
        for (auto b : kSamples)
        {
            EXPECT_SAME_U128(static_cast<nat::u128>(a) + static_cast<nat::u128>(b),
                             port::u128{a} + port::u128{b});
            EXPECT_SAME_U128(static_cast<nat::u128>(a) - static_cast<nat::u128>(b),
                             port::u128{a} - port::u128{b});
            // += : metalog's HLL harmonic-sum accumulator. Must equal the +-then-assign native does.
            nat::u128 nacc{static_cast<nat::u128>(a)};
            port::u128 pacc{a};
            nacc += static_cast<nat::u128>(b);
            pacc += port::u128{b};
            EXPECT_SAME_U128(nacc, pacc);
        }
}

TEST(DetInt128Portable, UnsignedDivMatchesNative)
{
    // round_div divides a 128-bit numerator (a*2^64 + b) by a positive den.
    for (auto hi : kSamples)
        for (auto lo : {0ULL, 1ULL, 999ULL, ~0ULL})
            for (auto den : {1ULL, 2ULL, 3ULL, 1000ULL, (1ULL << 40), ~0ULL})
            {
                const nat::u128 n{(static_cast<nat::u128>(hi) << 64) | lo};
                const port::u128 p{lo, hi};
                EXPECT_SAME_U128(n / static_cast<nat::u128>(den), p / port::u128{den});
                EXPECT_SAME_U128(n % static_cast<nat::u128>(den), p % port::u128{den}); // serializer's %
            }
}

TEST(DetInt128Portable, UnsignedEqualityMatchesNative)
{
    // == / != drive the decimal serializer's `while (magnitude != 0)` loop.
    for (auto a : kSamples)
        for (auto b : kSamples)
        {
            const auto na{static_cast<nat::u128>(a)}, nb{static_cast<nat::u128>(b)};
            EXPECT_EQ(na == nb, port::u128{a} == port::u128{b});
            EXPECT_EQ(na != nb, port::u128{a} != port::u128{b});
        }
}

TEST(DetInt128Portable, SignedMulDivAddMatchesNative)
{
    // The det_ln / FixedReducer / round_div signed surface: i64×i64, +=, /, unary -, ≥.
    constexpr std::array<std::int64_t, 12> kS{
        0, 1, -1, 2, -2, 1000, -1000, 762123384786LL, -762123384786LL,
        (1LL << 46), -(1LL << 46), (1LL << 60)};
    for (auto a : kS)
        for (auto b : kS)
        {
            EXPECT_SAME_I128(static_cast<nat::i128>(a) * static_cast<nat::i128>(b),
                             port::i128{a} * port::i128{b});
            EXPECT_SAME_I128(static_cast<nat::i128>(a) + static_cast<nat::i128>(b),
                             port::i128{a} + port::i128{b});
            if (b != 0)
                EXPECT_SAME_I128(static_cast<nat::i128>(a) / static_cast<nat::i128>(b),
                                 port::i128{a} / port::i128{b});
            EXPECT_EQ(static_cast<nat::i128>(a) >= static_cast<nat::i128>(b),
                      port::i128{a} >= port::i128{b});
            EXPECT_SAME_I128(-static_cast<nat::i128>(a), -port::i128{a});
        }
}

TEST(DetInt128Portable, WeightWideningIsValuePreserving)
{
    // The exact hazard fixed in add_weighted_log2: a u64 weight ≥ 2^63 must widen POSITIVE.
    for (auto w : kSamples)
    {
        const nat::i128 n{static_cast<nat::i128>(static_cast<nat::u128>(w))};
        const port::i128 p{static_cast<port::i128>(port::u128{w})};
        EXPECT_SAME_I128(n, p); // weight w must widen identically (positive) on both
        EXPECT_FALSE(p.is_negative()) << "weight " << w << " widened negative";
    }
}

#else
TEST(DetInt128Portable, NoNativeInt128OnThisPlatform) { GTEST_SKIP() << "no native __int128 to compare against"; }
#endif

} // namespace
// NOLINTEND
