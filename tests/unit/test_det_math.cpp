// NOLINTBEGIN — Unit tests for the deterministic math primitive.
//
// Two kinds of assertion:
//   * REFERENCE VECTOR (exact ==): the bit-exact int64 outputs of det_log2_fixed
//     for a fixed input set. These are the cross-machine determinism pin — if any
//     compiler/architecture computes a different bit, `==` fails. (The same fixture
//     is also compiled across the gcc×clang×-O×-ffp-contract matrix.)
//   * ACCURACY (near): the fixed-point result is close to libm, proving the
//     primitive is not just deterministic but correct. libm is used ONLY here in
//     the test oracle, never in the primitive.

#include <gtest/gtest.h>

import std;
import insight.canon;

namespace
{
using namespace insight::det;

// ── reference vector: exact, bit-for-bit (the determinism pin) ────────────────
struct LogRef
{
    std::uint64_t x;
    std::int64_t expected_q; // exact round(log2(x) * 2^40)
};

// Generated once from det_log2_fixed and frozen. Any divergence is a determinism
// regression, not a tolerance issue — assert with ==.
constexpr LogRef kLog2Ref[]{
    {1ULL, 0LL},
    {2ULL, 1099511627776LL},
    {3ULL, 1742684699132LL},
    {5ULL, 2552986939188LL},
    {7ULL, 3086719380097LL},
    {8ULL, 3298534883328LL},
    {10ULL, 3652498566964LL},
    {100ULL, 7304997133929LL},
    {255ULL, 8789884560377LL},
    {256ULL, 8796093022208LL},
    {1000ULL, 10957495700893LL},
    {1024ULL, 10995116277760LL},
    {65535ULL, 17592161839825LL},
    {1000000ULL, 21914991401787LL},
    {4294967296ULL, 35184372088832LL},
};

TEST(DetMath, Log2ReferenceVectorIsBitExact)
{
    for (const auto& ref : kLog2Ref)
        EXPECT_EQ(det_log2_fixed(ref.x), ref.expected_q)
            << "det_log2_fixed(" << ref.x
            << ") drifted from the frozen reference — a "
               "cross-machine determinism regression. got="
            << det_log2_fixed(ref.x) << " expected=" << ref.expected_q;
}

TEST(DetMath, PowersOfTwoAreExactIntegers)
{
    // log2(2^n) == n exactly: fraction must be zero, no rounding.
    for (int n = 0; n <= 40; ++n)
        EXPECT_EQ(det_log2_fixed(std::uint64_t{1} << n), static_cast<std::int64_t>(n) * kOne)
            << "log2(2^" << n << ") not exact";
}

TEST(DetMath, ZeroAndOneMapToZero)
{
    EXPECT_EQ(det_log2_fixed(0ULL), 0LL); // precondition violation, mapped to 0 (total fn)
    EXPECT_EQ(det_log2_fixed(1ULL), 0LL);
}

TEST(DetMath, Log2IsAccurateAgainstLibm)
{
    for (std::uint64_t x : {3ULL, 5ULL, 7ULL, 10ULL, 99ULL, 1234ULL, 65535ULL, 1000000ULL})
    {
        const double approx{fixed_to_double(det_log2_fixed(x))};
        const double ref{std::log2(static_cast<double>(x))};
        EXPECT_NEAR(approx, ref, 1e-9) << "log2(" << x << ") inaccurate";
    }
}

TEST(DetMath, Ln2ConstantMatchesRoundedLn2)
{
    EXPECT_EQ(kLn2Fixed, std::llround(std::log(2.0L) * static_cast<long double>(kOne)));
}

TEST(DetMath, LnIsAccurateAgainstLibm)
{
    for (std::uint64_t x : {2ULL, 3ULL, 10ULL, 100ULL, 65535ULL, 1000000ULL})
    {
        const double approx{fixed_to_double(det_ln_fixed(x))};
        const double ref{std::log(static_cast<double>(x))};
        EXPECT_NEAR(approx, ref, 1e-9) << "ln(" << x << ") inaccurate";
    }
}

// ── FixedReducer: exact integer reduction → deterministic entropy ─────────────
TEST(DetMath, ReducerUniformDistributionEntropyIsExact)
{
    // 4 equally-likely values (25 each of 100) → entropy = log2(4) = 2.0 exactly.
    FixedReducer reducer;
    constexpr std::int64_t total{100};
    const std::int64_t log2_total{det_log2_fixed(static_cast<std::uint64_t>(total))};
    for (int i = 0; i < 4; ++i)
    {
        constexpr std::uint64_t count{25};
        reducer.add_fixed(static_cast<__int128>(count) * (log2_total - det_log2_fixed(count)));
    }
    EXPECT_DOUBLE_EQ(reducer.normalized_bits(total), 2.0);
}

TEST(DetMath, ReducerConstantDistributionEntropyIsZero)
{
    FixedReducer reducer;
    constexpr std::int64_t total{100};
    reducer.add_fixed(static_cast<__int128>(100) *
                      (det_log2_fixed(static_cast<std::uint64_t>(total)) - det_log2_fixed(100)));
    EXPECT_DOUBLE_EQ(reducer.normalized_bits(total), 0.0);
}

TEST(DetMath, ReducerOrderIndependenceIsExact)
{
    // Integer accumulation is associative/exact: summing the same terms in two
    // different orders must give the BIT-IDENTICAL double (the order-independence guarantee).
    const std::uint64_t counts[]{7, 3, 50, 1, 39};
    constexpr std::int64_t total{100};
    const std::int64_t l2n{det_log2_fixed(static_cast<std::uint64_t>(total))};
    FixedReducer forward;
    for (std::uint64_t c : counts)
        forward.add_fixed(static_cast<__int128>(c) * (l2n - det_log2_fixed(c)));
    FixedReducer reverse;
    for (auto it = std::rbegin(counts); it != std::rend(counts); ++it)
        reverse.add_fixed(static_cast<__int128>(*it) * (l2n - det_log2_fixed(*it)));
    EXPECT_EQ(forward.raw(), reverse.raw());
    EXPECT_DOUBLE_EQ(forward.normalized_bits(total), reverse.normalized_bits(total));
}

// ── helpers ───────────────────────────────────────────────────────────────────
TEST(DetMath, RoundDivRoundsHalfAwayDeterministically)
{
    EXPECT_EQ(round_div(static_cast<__int128>(10), 4), 3);   // 2.5 → 3
    EXPECT_EQ(round_div(static_cast<__int128>(9), 4), 2);    // 2.25 → 2
    EXPECT_EQ(round_div(static_cast<__int128>(-10), 4), -3); // -2.5 → -3 (symmetric)
    EXPECT_EQ(round_div(static_cast<__int128>(0), 7), 0);
}

TEST(DetMath, FixedToDoubleIsExactForPowerOfTwoScale)
{
    EXPECT_DOUBLE_EQ(fixed_to_double(kOne), 1.0);
    EXPECT_DOUBLE_EQ(fixed_to_double(kOne / 2), 0.5);
    EXPECT_DOUBLE_EQ(fixed_to_double(3 * kOne), 3.0);
    EXPECT_DOUBLE_EQ(fixed_to_double(0), 0.0);
}

} // namespace
// NOLINTEND
