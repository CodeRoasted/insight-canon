// Unit tests: allow short identifiers and test-specific patterns
// tests/1_tokenization/test_arena_allocator.cpp
//
// Unit tests for ArenaAllocator.
// Coverage: construction, allocation, alignment, store_string,
//           reset, owns, move semantics, copy-deletion, accessors.

#include <gtest/gtest.h>

import insight.canon.test;

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

// ── The reset-poison instrument (DN-29.D13) ──────────────────────────────────────────────────
// These do not test the arena; they test the INSTRUMENT the arena carries, because a detector
// nobody has seen detect anything is not evidence. Read a released span deliberately — the exact
// thing a use-after-reset does by accident — and require that the bytes CHANGED.
//
// Deliberately NOT written as `poisoned == 0xDD…`: that would pin the fill value, and the value is
// an implementation choice. The property is "the released bytes no longer read as what was stored",
// which is what makes a stale `string_view` observable, and it survives a change of poison byte.
//
// WHY THE SKIPS BELOW PRINT THEIR REMEDY instead of leaving it in this comment: these two arms used
// to run on every desk build, and silently stopped. Since malf-toolchain 4b1f32e the dev profile
// linux-clang21-libcxx-release configures CMAKE_BUILD_TYPE=Release, so the AUTO default
// (core/CMakeLists.txt — the instrument resolves to `$<CONFIG:Debug>`) is OFF at the desk. A skip
// that changed meaning under a toolchain repair reads exactly like the skip it always was, so the
// way out is program OUTPUT at the moment of the block, not a comment the blocked reader is not
// reading.

// The single spelling of the remedy, so the two skips cannot drift apart. The three routes differ
// in WHAT THEY BUY and not only in price, which is why the message states both halves.
constexpr const char* kArmTheInstrument{
    "ARM IT — three routes, and they do not buy the same thing. "
    "(1) Reconfigure THIS tree with -DINSIGHT_CANON_ARENA_POISON_MODE=ON: the poison alone, "
    "no new cache, cheapest if you are already built. "
    "(2) `malf test insight-canon --profile linux-clang21-libcxx-debug`: the whole desk debug "
    "leg (poison + trace logging + live assert()), no sanitizer tax, and the cheapest seat "
    "for an actual lifetime hunt. It pays a ONE-TIME profile-keyed cache and build-tree "
    "rebuild, costed in malf/README.md under 'Shared config & profiles'. "
    "(3) `malf test insight-canon --asan`: that same debug leg plus AddressSanitizer."};

TEST(ArenaAllocator_ResetPoison, InstrumentIsDeclaredAndReadableAtRuntime)
{
    // The provenance query must agree with how this TU was compiled. If these ever disagree, a
    // downstream lifetime gate is being told the wrong thing about the build it is running in,
    // and its skip/assert decision is unsound — which is worse than having no gate.
#ifdef INSIGHT_CANON_ARENA_POISON
    EXPECT_TRUE(insight::tokenization::arena_poisons_on_reset())
        << "built WITH INSIGHT_CANON_ARENA_POISON but the runtime query denies it — downstream "
           "gates would skip a check this build can actually make";
#else
    EXPECT_FALSE(insight::tokenization::arena_poisons_on_reset())
        << "built WITHOUT INSIGHT_CANON_ARENA_POISON but the runtime query claims poisoning — "
           "downstream gates would ASSERT a lifetime this build cannot observe, and pass blindly";
#endif
}

// THE TWO CASES BELOW ARE COMPILE-TIME GATED, NOT RUNTIME-SKIPPED (Founder, 2026-09-04). They
// used to open with `if (!arena_poisons_on_reset()) GTEST_SKIP()`, and the skip was honest about
// its reason while still being the wrong SHAPE: a gtest skip exits 0 and ctest counts it as
// passed, so a release build reported two green cases for a subject that cannot exist in it.
// Whether the instrument is compiled in is a property of THIS TU — `INSIGHT_CANON_ARENA_POISON`
// is set on this target by the same generator expression as on the library — so the honest
// expression is not to register the cases at all where they could not run.
//
// Nothing is lost by that. `InstrumentIsDeclaredAndReadableAtRuntime` above is UNCONDITIONAL and
// asserts BOTH directions, so a build whose runtime query disagrees with its own macro still
// reds — which was the only thing the skipping cases could have caught in an unpoisoned build.
#ifdef INSIGHT_CANON_ARENA_POISON

TEST(ArenaAllocator_ResetPoison, ReleasedBytesAreOverwrittenSoAUseAfterResetIsObservable)
{
    // A belt, not a gate: inside this #ifdef the instrument MUST be on, and the case above
    // already pins that. If it is ever false here the two disagree and this fails loudly rather
    // than measuring a rewind that never poisoned.
    ASSERT_TRUE(insight::tokenization::arena_poisons_on_reset())
        << "compiled WITH INSIGHT_CANON_ARENA_POISON but the runtime query denies it. "
        << kArmTheInstrument;

    ArenaAllocator arena{4096};
    constexpr std::string_view kStored{"GET /api/users -> 200"};
    const std::string_view held{arena.store_string(kStored)};
    ASSERT_EQ(held, kStored) << "precondition failed: store_string did not round-trip";

    arena.reset();

    // `held` is now a dangling view into released arena memory. Reading it is the defect under
    // test, performed on purpose: the instrument's whole job is to make this read WRONG.
    EXPECT_NE(held, kStored)
        << "a view into released arena memory still reads its old contents — the reset poison did "
           "NOT fire, so every use-after-reset in the codebase is invisible to every test";
}

TEST(ArenaAllocator_ResetPoison, PoisonSpansTheWholeHandedOutExtentNotJustTheFirstBytes)
{
    ASSERT_TRUE(insight::tokenization::arena_poisons_on_reset())
        << "compiled WITH INSIGHT_CANON_ARENA_POISON but the runtime query denies it. "
        << kArmTheInstrument;

    // A partial fill would leave later allocations readable and make the instrument's coverage a
    // function of WHERE in the line the stale view happened to point — a detector that fires
    // sometimes is worse than none, because its green would then be trusted.
    ArenaAllocator arena{4096};
    std::vector<std::string_view> views;
    views.reserve(32);
    for (int index{0}; index < 32; ++index)
        views.push_back(arena.store_string("payload-" + std::to_string(index)));

    arena.reset();

    for (std::size_t index{0}; index < views.size(); ++index)
    {
        const std::string expected{"payload-" + std::to_string(index)};
        EXPECT_NE(views[index], expected)
            << "allocation #" << index << " of " << views.size()
            << " survived the reset intact — the poison does not cover the full handed-out extent, "
               "so the instrument detects a use-after-reset only for some offsets";
    }
}

#endif // INSIGHT_CANON_ARENA_POISON
