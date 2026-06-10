module;
#include <cstring>
#include <numa.h>
#include "insight/utils/log_macros.hpp" // textual macro layer (§11.9)

module insight.canon.api;
import insight.canon.internal;

#ifdef INSIGHT_HAS_NUMA
#endif


namespace insight::tokenization
{

namespace
{
    constexpr std::size_t kGrowthFactor{2};
    constexpr std::size_t kInitialBlockReserve{4};

    [[nodiscard]] constexpr bool is_power_of_two(std::size_t value) noexcept
    {
        return value != 0 && (value & (value - 1U)) == 0;
    }

    [[nodiscard]] std::uintptr_t align_up(std::uintptr_t value, std::size_t alignment) noexcept
    {
        assert(is_power_of_two(alignment));
        const auto mask{static_cast<std::uintptr_t>(alignment - 1U)};
        return (value + mask) & ~mask;
    }

    [[nodiscard]] bool numa_runtime_available() noexcept
    {
#ifdef INSIGHT_HAS_NUMA
        static const bool available{::numa_available() >= 0};
        return available;
#else
        return false;
#endif
    }

    [[nodiscard]] ArenaNumaPolicy resolve_policy(ArenaNumaPolicy policy) noexcept
    {
#ifdef INSIGHT_HAS_NUMA
        if (policy.kind == ArenaNumaPolicy::Kind::Auto && numa_runtime_available())
        {
            const int node{::numa_preferred()};
            if (node >= 0)
            {
                policy.kind = ArenaNumaPolicy::Kind::Fixed;
                policy.node = node;
            }
            else
            {
                policy.kind = ArenaNumaPolicy::Kind::Disabled;
                policy.node = -1;
            }
        }
        if (policy.active() && !numa_runtime_available())
        {
            policy.kind = ArenaNumaPolicy::Kind::Disabled;
            policy.node = -1;
        }
#else
        if (policy.active())
        {
            policy.kind = ArenaNumaPolicy::Kind::Disabled;
            policy.node = -1;
        }
#endif
        return policy;
    }

    [[nodiscard]] std::byte* allocate_aligned_block(std::size_t bytes, std::size_t alignment)
    {
        return static_cast<std::byte*>(::operator new(bytes, std::align_val_t{alignment}));
    }
} // namespace

bool arena_numa_supported() noexcept
{
    return numa_runtime_available();
}

int arena_numa_node_count() noexcept
{
#ifdef INSIGHT_HAS_NUMA
    if (numa_runtime_available())
        return ::numa_num_configured_nodes();
#endif
    return 1;
}

ArenaAllocator::Block::Block(std::byte* block_storage, std::size_t block_size,
                             std::size_t block_alignment, bool block_numa_allocated) noexcept
    : storage{block_storage}, size{block_size}, alignment{block_alignment},
      numa_allocated{block_numa_allocated}
{
}

ArenaAllocator::Block::~Block() noexcept
{
    reset();
}

ArenaAllocator::Block::Block(Block&& other) noexcept
    : storage{other.storage}, size{other.size}, offset{other.offset}, alignment{other.alignment},
      numa_allocated{other.numa_allocated}
{
    other.storage = nullptr;
    other.size = 0;
    other.offset = 0;
    other.alignment = kDefaultBlockAlignment;
    other.numa_allocated = false;
}

ArenaAllocator::Block& ArenaAllocator::Block::operator=(Block&& other) noexcept
{
    if (this != &other)
    {
        reset();
        storage = other.storage;
        size = other.size;
        offset = other.offset;
        alignment = other.alignment;
        numa_allocated = other.numa_allocated;

        other.storage = nullptr;
        other.size = 0;
        other.offset = 0;
        other.alignment = kDefaultBlockAlignment;
        other.numa_allocated = false;
    }
    return *this;
}

void ArenaAllocator::Block::reset() noexcept
{
    if (storage == nullptr)
        return;
#ifdef INSIGHT_HAS_NUMA
    if (numa_allocated)
    {
        ::numa_free(storage, size);
    }
    else
#endif
    {
        ::operator delete(storage, std::align_val_t{alignment});
    }
    storage = nullptr;
    size = 0;
    offset = 0;
    alignment = kDefaultBlockAlignment;
    numa_allocated = false;
}

ArenaAllocator::ArenaAllocator(std::size_t initial_block_size, ArenaNumaPolicy policy)
    : initial_block_size_{initial_block_size}, policy_{resolve_policy(policy)}
{
    if (initial_block_size == 0)
        throw std::invalid_argument("ArenaAllocator: initial block size must be > 0");

    blocks_.reserve(kInitialBlockReserve);
    blocks_.push_back(make_block(initial_block_size_, kDefaultBlockAlignment));
    INSIGHT_LOG_DEBUG(logging::arena_logger(), "arena created: initial_block={} numa_active={}",
                      initial_block_size_, policy_.active());
}

ArenaAllocator::~ArenaAllocator() = default;

ArenaAllocator::ArenaAllocator(ArenaAllocator&& other) noexcept
    : blocks_{std::move(other.blocks_)}, active_index_{other.active_index_},
      bytes_used_{other.bytes_used_}, initial_block_size_{other.initial_block_size_},
      policy_{other.policy_}
{
    other.active_index_ = 0;
    other.bytes_used_ = 0;
    other.initial_block_size_ = 0;
    other.policy_ = {};
}

ArenaAllocator& ArenaAllocator::operator=(ArenaAllocator&& other) noexcept
{
    if (this != &other)
    {
        blocks_ = std::move(other.blocks_);
        active_index_ = other.active_index_;
        bytes_used_ = other.bytes_used_;
        initial_block_size_ = other.initial_block_size_;
        policy_ = other.policy_;

        other.active_index_ = 0;
        other.bytes_used_ = 0;
        other.initial_block_size_ = 0;
        other.policy_ = {};
    }
    return *this;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static): NUMA builds use policy_.
ArenaAllocator::Block ArenaAllocator::make_block(std::size_t bytes, std::size_t alignment)
{
    const std::size_t block_alignment = std::max(kDefaultBlockAlignment, alignment);
#ifdef INSIGHT_HAS_NUMA
    if (policy_.kind == ArenaNumaPolicy::Kind::Fixed && policy_.node >= 0 &&
        block_alignment <= kDefaultBlockAlignment && numa_runtime_available())
    {
        if (void* ptr = ::numa_alloc_onnode(bytes, policy_.node); ptr != nullptr)
        {
            return Block{static_cast<std::byte*>(ptr), bytes, block_alignment, true};
        }
    }
#endif
    return Block{allocate_aligned_block(bytes, block_alignment), bytes, block_alignment, false};
}

void ArenaAllocator::grow_to_fit(std::size_t size, std::size_t alignment)
{
    if (active_index_ + 1U < blocks_.size())
    {
        ++active_index_;
        blocks_[active_index_].offset = 0;
        return;
    }

    const std::size_t previous_size = blocks_.empty() ? initial_block_size_ : blocks_.back().size;
    const std::size_t growth_size = previous_size * kGrowthFactor;
    const std::size_t minimum_size = size + alignment;
    const std::size_t new_size = std::max(growth_size, minimum_size);
    blocks_.push_back(make_block(new_size, alignment));
    active_index_ = blocks_.size() - 1U;
    INSIGHT_LOG_DEBUG(logging::arena_logger(), "arena grew: blocks={} capacity={} bytes",
                      blocks_.size(), capacity());
}

void* ArenaAllocator::allocate(std::size_t size, std::size_t alignment)
{
    if (size == 0)
        return nullptr;
    if (!is_power_of_two(alignment))
        throw std::invalid_argument("ArenaAllocator: alignment must be a non-zero power of two");

    while (true)
    {
        auto& block{blocks_[active_index_]};
        const auto base{reinterpret_cast<std::uintptr_t>(block.storage)}; // NOLINT
        const auto raw{base + block.offset};
        const auto aligned{align_up(raw, alignment)};
        const auto padding{static_cast<std::size_t>(aligned - raw)};

        if (block.offset + padding + size <= block.size)
        {
            block.offset += padding + size;
            bytes_used_ += padding + size;
            INSIGHT_LOG_TRACE(logging::arena_logger(),
                              "arena alloc: size={} align={} used={} capacity={}", size, alignment,
                              bytes_used_, capacity());
            return reinterpret_cast<void*>(aligned); // NOLINT
        }
        grow_to_fit(size, alignment);
    }
}

std::string_view ArenaAllocator::store_string(std::string_view str)
{
    if (str.empty())
        return {};

    void* dest = allocate(str.size(), alignof(char));
    std::memcpy(dest, str.data(), str.size());
    return {static_cast<const char*>(dest), str.size()};
}

void ArenaAllocator::reset() noexcept
{
    for (auto& block : blocks_)
        block.offset = 0;
    active_index_ = 0;
    bytes_used_ = 0;
}

std::size_t ArenaAllocator::used() const noexcept
{
    return bytes_used_;
}

std::size_t ArenaAllocator::capacity() const noexcept
{
    std::size_t total = 0;
    for (const auto& block : blocks_)
        total += block.size;
    return total;
}

std::size_t ArenaAllocator::initial_block_size() const noexcept
{
    return initial_block_size_;
}

std::size_t ArenaAllocator::block_count() const noexcept
{
    return blocks_.size();
}

const ArenaNumaPolicy& ArenaAllocator::numa_policy() const noexcept
{
    return policy_;
}

bool ArenaAllocator::owns(const void* ptr) const noexcept
{
    if (ptr == nullptr)
        return false;
    const auto address{reinterpret_cast<std::uintptr_t>(ptr)}; // NOLINT
    return std::ranges::any_of(blocks_,
                               [address](const Block& block)
                               {
                                   const auto begin{
                                       reinterpret_cast<std::uintptr_t>(block.storage)}; // NOLINT
                                   const auto end{begin + block.size};
                                   return address >= begin && address < end;
                               });
}

} // namespace insight::tokenization
