module;
#if defined(__linux__)
#include <dlfcn.h>
#endif
// refs: ADR-3.D4
#include "utils/log_macros.hpp"

module insight.canon.detail.numa;
import insight.canon.internal;
import insight.canon.api;

namespace insight::tokenization::numa
{

namespace
{

    inline constexpr bool kSanitizerBuild{
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

    // invariant: the soname, never `libnuma.so`, which only a `-dev` package installs.
    inline constexpr const char* kSystemSoname{"libnuma.so.1"};

#if defined(__linux__)
    // post: dlerror()'s pending text, or a fixed word when the loader recorded none.
    [[nodiscard]] std::string loader_error()
    {
        const char* text{::dlerror()};
        return text != nullptr ? std::string{text} : std::string{"no loader diagnostic"};
    }

    // post: true with `slot` bound to `name` in `handle`, or false with `reason` naming the symbol.
    template <class Function>
    [[nodiscard]] bool bind(void* handle, const char* name, Function& slot, std::string& reason)
    {
        static_cast<void>(::dlerror());
        void* address{::dlsym(handle, name)};
        if (address == nullptr)
        {
            reason = std::string{name} + ": " + loader_error();
            return false;
        }
        // note: dlsym returns an object pointer, and POSIX defines its conversion to a function.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
        slot = reinterpret_cast<Function>(address);
        return true;
    }
#endif

    [[nodiscard]] Library empty_with(LoadOutcome outcome, std::string reason)
    {
        Library library;
        library.outcome = outcome;
        library.reason = std::move(reason);
        return library;
    }

    [[nodiscard]] ArenaNumaPolicy disabled() noexcept
    {
        return ArenaNumaPolicy{.kind = ArenaNumaPolicy::Kind::Disabled, .node = -1};
    }

} // namespace

std::string_view outcome_name(LoadOutcome outcome) noexcept
{
    switch (outcome)
    {
    case LoadOutcome::Loaded:
        return "loaded";
    case LoadOutcome::NotFound:
        return "not found";
    case LoadOutcome::SymbolMissing:
        return "a symbol missing";
    case LoadOutcome::Unavailable:
        return "numa_available() negative";
    case LoadOutcome::Unsupported:
        return "no run-time loader on this platform";
    }
    return "unknown outcome";
}

Library load(const char* soname)
{
#if defined(__linux__)
    static_cast<void>(::dlerror());
    void* handle{::dlopen(soname, RTLD_NOW | RTLD_LOCAL)};
    if (handle == nullptr)
        return empty_with(LoadOutcome::NotFound, loader_error());

    Library library;
    std::string reason;
    const bool bound{bind(handle, "numa_available", library.available, reason) &&
                     bind(handle, "numa_alloc_local", library.allocate_local, reason) &&
                     bind(handle, "numa_alloc_onnode", library.allocate_on_node, reason) &&
                     bind(handle, "numa_free", library.free, reason) &&
                     bind(handle, "numa_num_configured_nodes", library.node_count, reason)};
    if (!bound)
    {
        static_cast<void>(::dlclose(handle));
        return empty_with(LoadOutcome::SymbolMissing, std::move(reason));
    }
    if (library.available() < 0)
    {
        static_cast<void>(::dlclose(handle));
        return empty_with(LoadOutcome::Unavailable, "numa_available() returned a negative value");
    }
    library.outcome = LoadOutcome::Loaded;
    return library;
#else
    static_cast<void>(soname);
    return empty_with(LoadOutcome::Unsupported, "libnuma is a Linux library");
#endif
}

const Library& process_library()
{
    static const Library library{[]
                                 {
                                     Library loaded{load(kSystemSoname)};
                                     INSIGHT_LOG_DEBUG(logging::arena_logger(),
                                                       "numa loader: {} {} ({})", kSystemSoname,
                                                       outcome_name(loaded.outcome), loaded.reason);
                                     return loaded;
                                 }()};
    return library;
}

bool sanitizer_build() noexcept
{
    return kSanitizerBuild;
}

ArenaNumaPolicy resolve(ArenaNumaPolicy requested, const Library& library) noexcept
{
    if (!requested.active() || kSanitizerBuild || !library.usable())
        return disabled();
    if (requested.kind == ArenaNumaPolicy::Kind::Auto)
        return ArenaNumaPolicy{.kind = ArenaNumaPolicy::Kind::Auto, .node = -1};
    if (requested.node < 0 || requested.node >= library.node_count())
        return disabled();
    return requested;
}

std::byte* allocate_block(const Library& library, const ArenaNumaPolicy& resolved,
                          std::size_t bytes, std::size_t alignment) noexcept
{
    static constexpr std::size_t kPageAlignment{4096};
    if (!resolved.active() || !library.usable() || alignment > kPageAlignment)
        return nullptr;
    void* block{resolved.kind == ArenaNumaPolicy::Kind::Auto
                    ? library.allocate_local(bytes)
                    : library.allocate_on_node(bytes, resolved.node)};
    return static_cast<std::byte*>(block);
}

} // namespace insight::tokenization::numa
