// invariant: white-box coverage of the run-time libnuma binding the arena allocates through - the
// all-or-nothing loader, the `Auto` default and its meaning, and the sanitizer rule.
// refs: DN-142.D16
#include <gtest/gtest.h>

import insight.canon.test;

namespace
{

using insight::tokenization::ArenaAllocator;
using insight::tokenization::ArenaNumaPolicy;
namespace numa = insight::tokenization::numa;

// note: the test TU reads the sanitizer macros itself, a second producer beside canon's own answer.
inline constexpr bool kTestCompiledUnderSanitizer{
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    true
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(memory_sanitizer) ||                         \
    __has_feature(thread_sanitizer)
    true
#else
    false
#endif
#else
    false
#endif
};

inline constexpr std::size_t kBlockBytes{4096};
inline constexpr std::size_t kNumaBlockBytes{numa::kMinimumBlockBytes};

[[nodiscard]] std::string describe(const numa::Library& library)
{
    return std::format("outcome='{}' reason='{}' sanitizer_build={}",
                       numa::outcome_name(library.outcome), library.reason,
                       numa::sanitizer_build());
}

[[nodiscard]] bool table_is_empty(const numa::Library& library)
{
    return library.available == nullptr && library.allocate_local == nullptr &&
           library.allocate_on_node == nullptr && library.free == nullptr &&
           library.node_count == nullptr;
}

[[nodiscard]] bool table_is_full(const numa::Library& library)
{
    return library.available != nullptr && library.allocate_local != nullptr &&
           library.allocate_on_node != nullptr && library.free != nullptr &&
           library.node_count != nullptr;
}

// post: a library the resolver reads as loaded, bound to no libnuma at all.
[[nodiscard]] numa::Library loaded_stand_in()
{
    numa::Library library;
    library.outcome = numa::LoadOutcome::Loaded;
    library.available = [] { return 0; };
    library.node_count = [] { return 1; };
    return library;
}

[[nodiscard]] bool numa_path_expected()
{
    return numa::process_library().usable() && !numa::sanitizer_build();
}

} // namespace

TEST(NumaLibrary, TheProcessBindingIsAllOrNothing)
{
    const numa::Library& library{numa::process_library()};
    if (library.usable())
        EXPECT_TRUE(table_is_full(library)) << describe(library);
    else
        EXPECT_TRUE(table_is_empty(library) && !library.reason.empty()) << describe(library);
}

TEST(NumaLibrary, AnAbsentSonameReportsAbsentAndYieldsPortableBlocks)
{
    const numa::Library absent{numa::load("libcoderoast-absent-numa.so.1")};
#if defined(__linux__)
    EXPECT_EQ(absent.outcome, numa::LoadOutcome::NotFound) << describe(absent);
#else
    EXPECT_EQ(absent.outcome, numa::LoadOutcome::Unsupported) << describe(absent);
#endif
    EXPECT_FALSE(absent.usable());
    EXPECT_TRUE(table_is_empty(absent)) << describe(absent);
    const ArenaNumaPolicy resolved{numa::resolve(ArenaNumaPolicy{}, absent)};
    EXPECT_EQ(resolved.kind, ArenaNumaPolicy::Kind::Disabled);
    EXPECT_EQ(resolved.node, -1);
    EXPECT_EQ(
        numa::allocate_block(absent, resolved, kBlockBytes, ArenaAllocator::kDefaultBlockAlignment),
        nullptr);
}

TEST(NumaLibrary, ALibraryLackingTheSymbolsBindsNothing)
{
    const numa::Library partial{numa::load("libc.so.6")};
#if defined(__linux__)
    EXPECT_EQ(partial.outcome, numa::LoadOutcome::SymbolMissing) << describe(partial);
    EXPECT_TRUE(partial.reason.starts_with("numa_available")) << describe(partial);
#else
    EXPECT_EQ(partial.outcome, numa::LoadOutcome::Unsupported) << describe(partial);
#endif
    EXPECT_TRUE(table_is_empty(partial)) << describe(partial);
}

TEST(NumaLibrary, CanonReportsTheSanitizerFlagsItsTestsWereCompiledWith)
{
    EXPECT_EQ(numa::sanitizer_build(), kTestCompiledUnderSanitizer);
}

TEST(NumaLibrary, ASanitizerBuildResolvesEveryPolicyDisabled)
{
    const numa::Library stand_in{loaded_stand_in()};
    const ArenaNumaPolicy automatic{numa::resolve(ArenaNumaPolicy{}, stand_in)};
    const ArenaNumaPolicy fixed{
        numa::resolve(ArenaNumaPolicy{.kind = ArenaNumaPolicy::Kind::Fixed, .node = 0}, stand_in)};
    if (kTestCompiledUnderSanitizer)
    {
        EXPECT_EQ(automatic.kind, ArenaNumaPolicy::Kind::Disabled);
        EXPECT_EQ(fixed.kind, ArenaNumaPolicy::Kind::Disabled);
    }
    else
    {
        EXPECT_EQ(automatic.kind, ArenaNumaPolicy::Kind::Auto);
        EXPECT_EQ(fixed.kind, ArenaNumaPolicy::Kind::Fixed);
    }
}

TEST(NumaLibrary, AFixedNodeTheLibraryDoesNotReportResolvesDisabled)
{
    const numa::Library stand_in{loaded_stand_in()};
    for (const int node : {-1, 1, 7})
    {
        const ArenaNumaPolicy resolved{numa::resolve(
            ArenaNumaPolicy{.kind = ArenaNumaPolicy::Kind::Fixed, .node = node}, stand_in)};
        EXPECT_EQ(resolved.kind, ArenaNumaPolicy::Kind::Disabled) << "node=" << node;
    }
}

TEST(NumaArena, AnAutoArenaTakesTheNumaPathExactlyWhenLibnumaLoadedAndAvailable)
{
    const bool expected{numa_path_expected()};
    ArenaAllocator arena{kNumaBlockBytes};
    EXPECT_EQ(arena.numa_policy().kind,
              expected ? ArenaNumaPolicy::Kind::Auto : ArenaNumaPolicy::Kind::Disabled)
        << describe(numa::process_library());
    EXPECT_EQ(arena.numa_block_count(), expected ? 1U : 0U) << describe(numa::process_library());

    auto* grown{static_cast<std::byte*>(arena.allocate(kNumaBlockBytes + 1))};
    ASSERT_NE(grown, nullptr);
    std::memset(grown, 0x5A, kNumaBlockBytes + 1);
    EXPECT_EQ(grown[kNumaBlockBytes], std::byte{0x5A});
    EXPECT_EQ(arena.block_count(), 2U);
    EXPECT_EQ(arena.numa_block_count(), expected ? 2U : 0U) << describe(numa::process_library());
    EXPECT_EQ(insight::tokenization::arena_numa_supported(), expected);
}

TEST(NumaArena, ABlockBelowTheMeasuredThresholdTakesThePortablePath)
{
    ArenaAllocator arena{kNumaBlockBytes - 1};
    static_cast<void>(arena.allocate(kBlockBytes));
    EXPECT_EQ(arena.numa_block_count(), 0U) << describe(numa::process_library());
    EXPECT_EQ(numa::allocate_block(numa::process_library(), arena.numa_policy(),
                                   kNumaBlockBytes - 1, ArenaAllocator::kDefaultBlockAlignment),
              nullptr);
}

TEST(NumaArena, AFixedArenaOnNodeZeroTakesTheNumaPathExactlyWhenLibnumaLoadedAndAvailable)
{
    const bool expected{numa_path_expected()};
    ArenaAllocator arena{kNumaBlockBytes,
                         ArenaNumaPolicy{.kind = ArenaNumaPolicy::Kind::Fixed, .node = 0}};
    EXPECT_EQ(arena.numa_policy().kind,
              expected ? ArenaNumaPolicy::Kind::Fixed : ArenaNumaPolicy::Kind::Disabled)
        << describe(numa::process_library());
    EXPECT_EQ(arena.numa_block_count(), expected ? 1U : 0U) << describe(numa::process_library());
}

TEST(NumaArena, ADisabledArenaNeverTakesTheNumaPath)
{
    ArenaAllocator arena{kNumaBlockBytes, ArenaNumaPolicy{.kind = ArenaNumaPolicy::Kind::Disabled}};
    static_cast<void>(arena.allocate(kNumaBlockBytes + 1));
    EXPECT_EQ(arena.numa_policy().kind, ArenaNumaPolicy::Kind::Disabled);
    EXPECT_EQ(arena.block_count(), 2U);
    EXPECT_EQ(arena.numa_block_count(), 0U);
}

TEST(NumaArena, AMovedFromArenaHoldsTheResolvedDisabledPolicy)
{
    ArenaAllocator source{kNumaBlockBytes};
    ArenaAllocator target{std::move(source)};
    // note: reading the moved-from arena's policy IS the property under test.
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move)
    EXPECT_EQ(source.numa_policy().kind, ArenaNumaPolicy::Kind::Disabled);
    EXPECT_EQ(target.numa_block_count(), numa_path_expected() ? 1U : 0U);
}
