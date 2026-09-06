
// invariant: the cross-compiler equivalence ORACLE for the portable 128-bit shim.
// invariant: the header gives the math layer a native wide integer on the shipped compilers and a
// pure struct on the other one.
// invariant: the determinism contract is that the two are BIT-FOR-BIT identical.
// invariant: that compiler cannot run here, but the portable struct CAN be compiled here forced,
// and asserted equal to the native type over a wide value sweep.
// invariant: that proves the struct's ARITHMETIC, which is the only thing that differs on the other
// platform; the module wiring is proven separately by the suite and the determinism proof.
// invariant: MECHANISM — the header is included ONCE, with the force macro defined before it, so
// the portable struct compiles here under its own namespace alias.
// invariant: the native half is a direct TYPEDEF rather than a second include, and this prose
// claimed a double include until 2026-09-07.
// invariant: both are then driven through the EXACT operation set the math layer uses, and equality
// is required on every sample.
// invariant: any divergence here is the divergence the other platform would have produced, and
// catching it here is the point.
// invariant: on a platform with no native wide integer there is NO CASE TO REGISTER — that branch
// is empty, and the block at it says so.
// invariant: this prose claimed the file degrades to a TAUTOLOGY that still passes, which
// contradicted that block and was false until 2026-09-07.
#include <gtest/gtest.h>

#include <array>
#include <cstdint>

#if defined(__SIZEOF_INT128__) || defined(__GNUC__) || defined(__clang__)
#define ORACLE_HAS_NATIVE 1
namespace nat
{
using u128 = unsigned __int128;
using i128 = __int128;
} // namespace nat
#endif

#define INSIGHT_DET_FORCE_PORTABLE_INT128 1
#include "det/det_int128.hpp"
namespace port = insight::det::detail;

namespace
{

#ifdef ORACLE_HAS_NATIVE

// invariant: the low and high words are extracted from each representation so the comparison is
// BIT-FOR-BIT rather than value-level.
constexpr std::uint64_t lo_of(nat::u128 v) noexcept
{
    return static_cast<std::uint64_t>(v);
}
constexpr std::uint64_t hi_of(nat::u128 v) noexcept
{
    return static_cast<std::uint64_t>(v >> 64);
}
constexpr std::uint64_t lo_of(port::u128 v) noexcept
{
    return v.lo;
}
constexpr std::uint64_t hi_of(port::u128 v) noexcept
{
    return v.hi;
}
constexpr std::uint64_t lo_of(nat::i128 v) noexcept
{
    return static_cast<std::uint64_t>(static_cast<nat::u128>(v));
}
constexpr std::uint64_t hi_of(nat::i128 v) noexcept
{
    return static_cast<std::uint64_t>(static_cast<nat::u128>(v) >> 64);
}
constexpr std::uint64_t lo_of(port::i128 v) noexcept
{
    return v.bits.lo;
}
constexpr std::uint64_t hi_of(port::i128 v) noexcept
{
    return v.bits.hi;
}

// invariant: a spread of operands — edges, powers of two, primes, and the region at or above the
// signed boundary, which is the value-preservation hazard for the signed widening of weights.
constexpr std::array<std::uint64_t, 18> kSamples{
    0ULL,         1ULL,         2ULL,         3ULL,
    7ULL,         255ULL,       256ULL,       1000ULL,
    65535ULL,     (1ULL << 32), (1ULL << 40), (1ULL << 42) + 7,
    (1ULL << 52), (1ULL << 62), (1ULL << 63), (1ULL << 63) + 12345,
    ~0ULL - 1ULL, ~0ULL};

#define EXPECT_SAME_U128(N, P)                                                                     \
    do                                                                                             \
    {                                                                                              \
        EXPECT_EQ(lo_of(N), lo_of(P)) << "lo mismatch";                                            \
        EXPECT_EQ(hi_of(N), hi_of(P)) << "hi mismatch";                                            \
    } while (0)
#define EXPECT_SAME_I128(N, P) EXPECT_SAME_U128(N, P)

TEST(DetInt128Portable, UnsignedMulShiftMatchesNative)
{
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
            // invariant: the MIXED comparison form is driven too, where the threshold widens
            // through the implicit constructor — the exact call-site shape.
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
            // invariant: the compound-add accumulator must equal the add-then-assign the native
            // type does.
            nat::u128 nacc{static_cast<nat::u128>(a)};
            port::u128 pacc{a};
            nacc += static_cast<nat::u128>(b);
            pacc += port::u128{b};
            EXPECT_SAME_U128(nacc, pacc);
        }
}

TEST(DetInt128Portable, UnsignedDivMatchesNative)
{
    // invariant: the rounding division takes a 128-bit numerator assembled from two words, divided
    // by a positive denominator.
    for (auto hi : kSamples)
        for (auto lo : {0ULL, 1ULL, 999ULL, ~0ULL})
            for (auto den : {1ULL, 2ULL, 3ULL, 1000ULL, (1ULL << 40), ~0ULL})
            {
                const nat::u128 n{(static_cast<nat::u128>(hi) << 64) | lo};
                const port::u128 p{lo, hi};
                EXPECT_SAME_U128(n / static_cast<nat::u128>(den), p / port::u128{den});
                EXPECT_SAME_U128(n % static_cast<nat::u128>(den), p % port::u128{den});
            }
}

TEST(DetInt128Portable, UnsignedEqualityMatchesNative)
{
    // invariant: equality and inequality drive the decimal serializer's magnitude loop.
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
    constexpr std::array<std::int64_t, 12> kS{0,
                                              1,
                                              -1,
                                              2,
                                              -2,
                                              1000,
                                              -1000,
                                              762123384786LL,
                                              -762123384786LL,
                                              (1LL << 46),
                                              -(1LL << 46),
                                              (1LL << 60)};
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
    // invariant: the EXACT hazard the weighted accumulator fixed — a weight at or above the
    // signed boundary must widen POSITIVE.
    for (auto w : kSamples)
    {
        const nat::i128 n{static_cast<nat::i128>(static_cast<nat::u128>(w))};
        const port::i128 p{static_cast<port::i128>(port::u128{w})};
        EXPECT_SAME_I128(n, p);
        EXPECT_FALSE(p.is_negative()) << "weight " << w << " widened negative";
    }
}

#else
// invariant: NO PLACEHOLDER CASE HERE, deliberately.
// invariant: this branch used to host a single runtime skip whose only job was to leave a visible
// mark on a platform the suite cannot cover.
// invariant: that is the FALSE-PASS shape — a harness skip exits 0, so the mark was counted as a
// passing test.
// invariant: a platform without the native type simply has NO CASE to register; the compilation
// branch above IS the statement.
// invariant: the per-platform difference stays visible in the DISCOVERED TEST COUNT rather than in
// a green line that asserted nothing.
#endif

} // namespace
// NOLINTEND
