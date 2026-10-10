// invariant: the NUMA arena's OVERHEAD against the portable allocator, block size by block size; a
// one-node host measures no locality benefit, only what the libnuma path costs.
// refs: DN-142.D16
#include <benchmark/benchmark.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

import insight.canon;

namespace
{

namespace tok = insight::tokenization;

inline constexpr std::size_t kChunkBytes{64};
inline constexpr int kTouchByte{0x5A};

// post: the policy a benchmark argument names: 0 is `Disabled`, 1 is `Auto`.
[[nodiscard]] tok::ArenaNumaPolicy policy_of(std::int64_t argument)
{
    return argument == 0 ? tok::ArenaNumaPolicy{.kind = tok::ArenaNumaPolicy::Kind::Disabled}
                         : tok::ArenaNumaPolicy{};
}

// post: one block's whole life per iteration — acquired, every byte touched, released.
// note: the libnuma path pays an mmap, its page faults and a munmap where operator new may reuse.
void bm_arena_block_lifecycle(benchmark::State& state)
{
    const tok::ArenaNumaPolicy policy{policy_of(state.range(0))};
    const auto block_bytes{static_cast<std::size_t>(state.range(1))};
    std::size_t numa_blocks{0};
    for (auto _ : state)
    {
        tok::ArenaAllocator arena{block_bytes, policy};
        void* storage{arena.allocate(block_bytes, 1)};
        std::memset(storage, kTouchByte, block_bytes);
        benchmark::DoNotOptimize(storage);
        numa_blocks = arena.numa_block_count();
    }
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(block_bytes));
    state.counters["numa_blocks"] = static_cast<double>(numa_blocks);
}

// post: the steady state per iteration — one arena filled in 64-byte chunks, then reset.
void bm_arena_alloc_reset(benchmark::State& state)
{
    const tok::ArenaNumaPolicy policy{policy_of(state.range(0))};
    const auto block_bytes{static_cast<std::size_t>(state.range(1))};
    tok::ArenaAllocator arena{block_bytes, policy};
    const std::size_t chunks{block_bytes / kChunkBytes};
    for (auto _ : state)
    {
        for (std::size_t chunk{0}; chunk < chunks; ++chunk)
        {
            void* storage{arena.allocate(kChunkBytes, 1)};
            std::memset(storage, kTouchByte, kChunkBytes);
            benchmark::DoNotOptimize(storage);
        }
        arena.reset();
    }
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) *
                            static_cast<std::int64_t>(chunks * kChunkBytes));
    state.counters["numa_blocks"] = static_cast<double>(arena.numa_block_count());
}

void block_sizes(benchmark::internal::Benchmark* benchmark)
{
    static constexpr std::int64_t kKiB{1024};
    for (const std::int64_t policy : {0, 1})
    {
        for (const std::int64_t block : {4 * kKiB, 64 * kKiB, 1024 * kKiB, 4096 * kKiB,
                                         16384 * kKiB, 24576 * kKiB, 32768 * kKiB, 65536 * kKiB})
            benchmark->Args({policy, block});
    }
    benchmark->ArgNames({"auto", "block"});
}

} // namespace

BENCHMARK(bm_arena_block_lifecycle)->Apply(block_sizes);
BENCHMARK(bm_arena_alloc_reset)->Apply(block_sizes);
