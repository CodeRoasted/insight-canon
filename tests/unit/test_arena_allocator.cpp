// NOLINTBEGIN
// Unit tests: allow short identifiers and test-specific patterns
// tests/1_tokenization/test_arena_allocator.cpp
//
// Unit tests for ArenaAllocator.
// Coverage: construction, allocation, alignment, store_string,
//           reset, owns, move semantics, copy-deletion, accessors.

#include <cstdint>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#include "insight/tokenization/arena_allocator.hpp"

using insight::tokenization::ArenaAllocator;

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

TEST(ArenaAllocator_Construction, ZeroCapacityThrows)
{
    EXPECT_THROW(ArenaAllocator{0}, std::invalid_argument);
}

TEST(ArenaAllocator_Construction, ValidCapacitySucceeds)
{
    EXPECT_NO_THROW(ArenaAllocator{1024});
}

TEST(ArenaAllocator_Construction, InitialStateIsEmpty)
{
    ArenaAllocator arena{1024};
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.capacity(), 1024u);
    EXPECT_EQ(arena.initial_block_size(), 1024u);
    EXPECT_EQ(arena.block_count(), 1u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Allocation — basic
// ─────────────────────────────────────────────────────────────────────────────

TEST(ArenaAllocator_Allocate, ZeroSizeReturnsNull)
{
    ArenaAllocator arena{64};
    EXPECT_EQ(arena.allocate(0), nullptr);
    // Bump pointer must not advance for zero-size requests.
    EXPECT_EQ(arena.used(), 0u);
}

TEST(ArenaAllocator_Allocate, NonZeroSizeReturnsNonNull)
{
    ArenaAllocator arena{64};
    EXPECT_NE(arena.allocate(16), nullptr);
}

TEST(ArenaAllocator_Allocate, AllocatedMemoryIsWritable)
{
    ArenaAllocator arena{64};
    auto* p{
        static_cast<std::uint32_t*>(arena.allocate(sizeof(std::uint32_t), alignof(std::uint32_t)))};
    ASSERT_NE(p, nullptr);
    *p = 0xDEAD'BEEF;
    EXPECT_EQ(*p, 0xDEAD'BEEF);
}

TEST(ArenaAllocator_Allocate, SequentialAllocationsDoNotOverlap)
{
    ArenaAllocator arena{128};
    constexpr std::size_t size{32};
    auto* a{static_cast<std::byte*>(arena.allocate(size, 1))};
    auto* b{static_cast<std::byte*>(arena.allocate(size, 1))};
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    // b must begin at or after the end of a's region.
    EXPECT_GE(b, a + size);
}

TEST(ArenaAllocator_Allocate, ExactCapacitySucceeds)
{
    constexpr std::size_t cap{64};
    ArenaAllocator arena{cap};
    EXPECT_NO_THROW(static_cast<void>(arena.allocate(cap, 1)));
    EXPECT_EQ(arena.used(), cap);
}

TEST(ArenaAllocator_Allocate, OverInitialCapacityGrows)
{
    ArenaAllocator arena{64};
    void* p = nullptr;
    EXPECT_NO_THROW(p = arena.allocate(128, 1));
    EXPECT_NE(p, nullptr);
    EXPECT_GT(arena.capacity(), 64u);
    EXPECT_GT(arena.block_count(), 1u);
    EXPECT_TRUE(arena.owns(p));
}

TEST(ArenaAllocator_Allocate, PartialFillThenGrowRetainsExistingAllocation)
{
    ArenaAllocator arena{32};
    // Consume 24 bytes, leaving 8 bytes free.
    void* first = arena.allocate(24, 1);
    void* second = nullptr;
    EXPECT_NO_THROW(second = arena.allocate(16, 1));
    EXPECT_TRUE(arena.owns(first));
    EXPECT_TRUE(arena.owns(second));
    EXPECT_GT(arena.block_count(), 1u);
}

// ─────────────────────────────────────────────────────────────────────────────
// Alignment
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
// Returns true when the pointer satisfies the given power-of-two alignment.
bool is_aligned(const void* ptr, std::size_t alignment) noexcept
{
    return (reinterpret_cast<std::uintptr_t>(ptr) & (alignment - 1u)) == 0u;
}
} // namespace

TEST(ArenaAllocator_Alignment, AlignOne)
{
    ArenaAllocator arena{128};
    // Force a non-aligned offset, then request alignment == 1.
    static_cast<void>(arena.allocate(3, 1));
    void* p = arena.allocate(8, 1);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned(p, 1));
}

TEST(ArenaAllocator_Alignment, AlignFour)
{
    ArenaAllocator arena{128};
    static_cast<void>(arena.allocate(1, 1)); // offset = 1 (misaligned for 4-byte objects)
    void* p = arena.allocate(sizeof(std::uint32_t), 4);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned(p, 4));
}

TEST(ArenaAllocator_Alignment, AlignEight)
{
    ArenaAllocator arena{128};
    static_cast<void>(arena.allocate(3, 1)); // offset = 3 (misaligned for 8-byte objects)
    void* p = arena.allocate(sizeof(std::uint64_t), 8);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned(p, 8));
}

TEST(ArenaAllocator_Alignment, AlignSixteen)
{
    ArenaAllocator arena{256};
    static_cast<void>(arena.allocate(5, 1)); // offset = 5 (misaligned for 16-byte objects)
    void* p = arena.allocate(16, 16);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned(p, 16));
}

TEST(ArenaAllocator_Alignment, AlignCacheLine)
{
    ArenaAllocator arena{128};
    static_cast<void>(arena.allocate(3, 1));
    void* p = arena.allocate(64, ArenaAllocator::kDefaultBlockAlignment);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned(p, ArenaAllocator::kDefaultBlockAlignment));
}

TEST(ArenaAllocator_Alignment, DefaultAlignmentIsMaxAlignT)
{
    ArenaAllocator arena{256};
    static_cast<void>(arena.allocate(1, 1)); // disturb natural alignment
    // Default alignment argument == alignof(std::max_align_t).
    void* p = arena.allocate(sizeof(std::max_align_t));
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned(p, alignof(std::max_align_t)));
}

TEST(ArenaAllocator_Alignment, UsedReflectsPaddingBytes)
{
    ArenaAllocator arena{128};
    static_cast<void>(arena.allocate(1, 1));                         // used = 1
    static_cast<void>(arena.allocate(1, alignof(std::max_align_t))); // padding + 1 byte
    // used > 2 because of alignment padding (unless max_align_t == 1).
    if constexpr (alignof(std::max_align_t) > 1)
    {
        EXPECT_GT(arena.used(), 2u);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// store_string
// ─────────────────────────────────────────────────────────────────────────────

TEST(ArenaAllocator_StoreString, EmptyStringReturnsEmptyView)
{
    ArenaAllocator arena{64};
    auto sv{arena.store_string({})};
    EXPECT_TRUE(sv.empty());
    // No bytes should have been consumed.
    EXPECT_EQ(arena.used(), 0u);
}

TEST(ArenaAllocator_StoreString, ContentIsPreserved)
{
    ArenaAllocator arena{128};
    constexpr std::string_view original{"SmartLog"};
    auto stored{arena.store_string(original)};
    EXPECT_EQ(stored, original);
    EXPECT_EQ(stored.size(), original.size());
}

TEST(ArenaAllocator_StoreString, StoredDataIsArenaOwned)
{
    ArenaAllocator arena{128};
    auto stored{arena.store_string("test payload")};
    EXPECT_TRUE(arena.owns(stored.data()));
}

TEST(ArenaAllocator_StoreString, MultipleStringsDoNotOverlap)
{
    ArenaAllocator arena{256};
    auto a{arena.store_string("alpha")};
    auto b{arena.store_string("beta")};
    // The two views must not share memory.
    if (a.data() < b.data())
    {
        EXPECT_LE(a.data() + a.size(), b.data());
    }
    else
    {
        EXPECT_LE(b.data() + b.size(), a.data());
    }
}

TEST(ArenaAllocator_StoreString, UsedIncreasedByStringLength)
{
    ArenaAllocator arena{128};
    constexpr std::string_view sv{"hello"};
    static_cast<void>(arena.store_string(sv));
    EXPECT_EQ(arena.used(), sv.size());
}

TEST(ArenaAllocator_StoreString, StoredViewPointsInsideArena)
{
    // Verify the view does NOT point into the source string's storage.
    ArenaAllocator arena{128};
    std::string source{"arena copy"};
    auto stored{arena.store_string(source)};
    EXPECT_NE(stored.data(), source.data());
    EXPECT_TRUE(arena.owns(stored.data()));
}

// ─────────────────────────────────────────────────────────────────────────────
// reset()
// ─────────────────────────────────────────────────────────────────────────────

TEST(ArenaAllocator_Reset, UsedBecomesZero)
{
    ArenaAllocator arena{64};
    static_cast<void>(arena.allocate(32));
    arena.reset();
    EXPECT_EQ(arena.used(), 0u);
}

TEST(ArenaAllocator_Reset, CapacityUnchanged)
{
    ArenaAllocator arena{64};
    static_cast<void>(arena.allocate(32));
    arena.reset();
    EXPECT_EQ(arena.capacity(), 64u);
}

TEST(ArenaAllocator_Reset, ReusesGrownBlocks)
{
    ArenaAllocator arena{32};
    static_cast<void>(arena.allocate(128, 1));
    const std::size_t grown_capacity = arena.capacity();
    const std::size_t grown_blocks = arena.block_count();

    arena.reset();
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.capacity(), grown_capacity);
    EXPECT_EQ(arena.block_count(), grown_blocks);
    EXPECT_NO_THROW(static_cast<void>(arena.allocate(128, 1)));
}

TEST(ArenaAllocator_Reset, CanAllocateFullCapacityAfterReset)
{
    constexpr std::size_t cap{64};
    ArenaAllocator arena{cap};
    static_cast<void>(arena.allocate(cap, 1));
    arena.reset();
    EXPECT_NO_THROW(static_cast<void>(arena.allocate(cap, 1)));
    EXPECT_EQ(arena.used(), cap);
}

TEST(ArenaAllocator_Reset, RepeatedCyclesRetainCorrectState)
{
    ArenaAllocator arena{16};
    for (int i{0}; i < 8; ++i)
    {
        static_cast<void>(arena.allocate(16, 1));
        arena.reset();
    }
    EXPECT_EQ(arena.used(), 0u);
    EXPECT_EQ(arena.capacity(), 16u);
}

// ─────────────────────────────────────────────────────────────────────────────
// owns()
// ─────────────────────────────────────────────────────────────────────────────

TEST(ArenaAllocator_Owns, AllocatedPointerIsOwned)
{
    ArenaAllocator arena{64};
    void* p = arena.allocate(8);
    EXPECT_TRUE(arena.owns(p));
}

TEST(ArenaAllocator_Owns, ExternalPointerIsNotOwned)
{
    ArenaAllocator arena{64};
    int x{0};
    EXPECT_FALSE(arena.owns(&x));
}

TEST(ArenaAllocator_Owns, NullptrIsNotOwned)
{
    ArenaAllocator arena{64};
    EXPECT_FALSE(arena.owns(nullptr));
}

TEST(ArenaAllocator_Owns, StoredStringDataIsOwned)
{
    ArenaAllocator arena{128};
    auto sv{arena.store_string("owns check")};
    EXPECT_TRUE(arena.owns(sv.data()));
}

TEST(ArenaAllocator_Owns, PointerPastEndIsNotOwned)
{
    ArenaAllocator arena{64};
    // Pointer one byte past the end of the buffer must not be owned.
    const std::byte* end = static_cast<const std::byte*>(arena.allocate(64, 1)) + 64;
    EXPECT_FALSE(arena.owns(end));
}

// ─────────────────────────────────────────────────────────────────────────────
// Move semantics
// ─────────────────────────────────────────────────────────────────────────────

TEST(ArenaAllocator_Move, ConstructionTransfersOwnership)
{
    ArenaAllocator src{128};
    void* p = src.allocate(16);
    const std::size_t used_before{src.used()};

    ArenaAllocator dst{std::move(src)};

    EXPECT_EQ(dst.used(), used_before);
    EXPECT_EQ(dst.capacity(), 128u);
    EXPECT_TRUE(dst.owns(p));
}

TEST(ArenaAllocator_Move, ConstructionLeavesSourceEmpty)
{
    ArenaAllocator src{128};
    static_cast<void>(src.allocate(32));

    ArenaAllocator dst{std::move(src)}; // NOLINT(bugprone-use-after-move)

    // After the move the source must report zero capacity and zero used.
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move)
    EXPECT_EQ(src.capacity(), 0u);
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move)
    EXPECT_EQ(src.used(), 0u);
}

TEST(ArenaAllocator_Move, AssignmentTransfersOwnership)
{
    ArenaAllocator src{256};
    void* p = src.allocate(32);

    ArenaAllocator dst{64};
    dst = std::move(src);

    EXPECT_EQ(dst.capacity(), 256u);
    EXPECT_TRUE(dst.owns(p));
}

TEST(ArenaAllocator_Move, AssignmentLeavesSourceEmpty)
{
    ArenaAllocator src{256};
    static_cast<void>(src.allocate(16));

    ArenaAllocator dst{64};
    dst = std::move(src); // NOLINT(bugprone-use-after-move)

    // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move)
    EXPECT_EQ(src.capacity(), 0u);
    // NOLINTNEXTLINE(clang-analyzer-cplusplus.Move)
    EXPECT_EQ(src.used(), 0u);
}

TEST(ArenaAllocator_Move, MoveAssignedArenaCanAllocate)
{
    ArenaAllocator src{128};
    ArenaAllocator dst{64};
    dst = std::move(src);

    void* p = nullptr;
    EXPECT_NO_THROW(p = dst.allocate(32));
    EXPECT_NE(p, nullptr);
    EXPECT_TRUE(dst.owns(p));
}

// ─────────────────────────────────────────────────────────────────────────────
// Copy semantics — compile-time enforcement
// ─────────────────────────────────────────────────────────────────────────────

TEST(ArenaAllocator_CopyDeleted, NotCopyConstructible)
{
    static_assert(!std::is_copy_constructible_v<ArenaAllocator>,
                  "ArenaAllocator must not be copy-constructible");
}

TEST(ArenaAllocator_CopyDeleted, NotCopyAssignable)
{
    static_assert(!std::is_copy_assignable_v<ArenaAllocator>,
                  "ArenaAllocator must not be copy-assignable");
}

// ─────────────────────────────────────────────────────────────────────────────
// Accessors
// ─────────────────────────────────────────────────────────────────────────────

TEST(ArenaAllocator_Accessors, UsedStartsAtZero)
{
    ArenaAllocator arena{256};
    EXPECT_EQ(arena.used(), 0u);
}

TEST(ArenaAllocator_Accessors, UsedTracksSequentialAllocations)
{
    ArenaAllocator arena{256};
    static_cast<void>(arena.allocate(10, 1));
    EXPECT_EQ(arena.used(), 10u);
    static_cast<void>(arena.allocate(5, 1));
    EXPECT_EQ(arena.used(), 15u);
    static_cast<void>(arena.allocate(1, 1));
    EXPECT_EQ(arena.used(), 16u);
}

TEST(ArenaAllocator_Accessors, CapacityMatchesConstructorArgument)
{
    constexpr std::size_t cap{4096};
    ArenaAllocator arena{cap};
    EXPECT_EQ(arena.capacity(), cap);
}

TEST(ArenaAllocator_Accessors, NumaPolicyDefaultsDisabled)
{
    ArenaAllocator arena{256};
    EXPECT_EQ(arena.numa_policy().kind, insight::tokenization::ArenaNumaPolicy::Kind::Disabled);
    EXPECT_GE(insight::tokenization::arena_numa_node_count(), 1);
}

TEST(ArenaAllocator_Accessors, UsedNeverExceedsCapacity)
{
    ArenaAllocator arena{64};
    while (arena.used() < arena.capacity())
    {
        const std::size_t remaining{arena.capacity() - arena.used()};
        static_cast<void>(arena.allocate(remaining > 8 ? 8 : remaining, 1));
    }
    EXPECT_EQ(arena.used(), arena.capacity());
}

// NOLINTEND
