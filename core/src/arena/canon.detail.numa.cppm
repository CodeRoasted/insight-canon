// invariant: libnuma is LOADED at run time and never linked, so no build needs its header and no
// binary carries an LGPL dependency; a host without `libnuma.so.1` runs the portable allocator.
// refs: DN-142.D16
export module insight.canon.detail.numa;
import insight.canon.internal;
import insight.canon.api;
export namespace insight::tokenization::numa
{

// invariant: every outcome but `Loaded` leaves the table empty, so a caller never holds a partial
// binding; `Unsupported` is a platform with no run-time loader at all.
enum class LoadOutcome : std::uint8_t
{
    Loaded,
    NotFound,
    SymbolMissing,
    Unavailable,
    Unsupported,
};

// invariant: the pointer types are spelled from libnuma's stable `libnuma.so.1` ABI and no
// libnuma prototype is declared, so no name here can collide with or link to libnuma's own.
struct Library
{
    using AvailableFn = int (*)();
    using AllocateLocalFn = void* (*)(std::size_t);
    using AllocateOnNodeFn = void* (*)(std::size_t, int);
    using FreeFn = void (*)(void*, std::size_t);
    using NodeCountFn = int (*)();

    LoadOutcome outcome{LoadOutcome::Unsupported};
    // note: the loader's own words for a failed outcome - `dlerror()`'s text or the missing name.
    std::string reason;
    AvailableFn available{nullptr};
    AllocateLocalFn allocate_local{nullptr};
    AllocateOnNodeFn allocate_on_node{nullptr};
    FreeFn free{nullptr};
    NodeCountFn node_count{nullptr};

    [[nodiscard]] bool usable() const noexcept
    {
        return outcome == LoadOutcome::Loaded;
    }
};

// post: the outcome's name, as the loader's debug line and a test's failure message print it.
[[nodiscard]] std::string_view outcome_name(LoadOutcome outcome) noexcept;

// post: `Loaded` with all five functions bound and `numa_available() >= 0`, or another outcome
// with an empty table; a found library whose binding failed is closed again.
// invariant: all or nothing - one missing symbol is `SymbolMissing`, never a partial table.
[[nodiscard]] Library load(const char* soname);

// post: the process's one binding of `libnuma.so.1`, loaded on first use and never closed.
// invariant: the outcome is logged once, at debug level, on the arena logger.
[[nodiscard]] const Library& process_library();

// post: true when canon was compiled under ASan, MSan or TSan, read from the compiler's own macros.
// invariant: a RUNTIME query, so it reports the flags canon's translation units were compiled with.
[[nodiscard]] bool sanitizer_build() noexcept;

// post: `requested` with its kind kept only when `library` is usable, the build is not a sanitizer
// build and, for `Fixed`, the node is one the library reports; `Disabled` otherwise, node -1.
// invariant: an uninstrumented library's pages would read as initialised under MSan, so a
// sanitizer build never takes the NUMA path.
[[nodiscard]] ArenaNumaPolicy resolve(ArenaNumaPolicy requested, const Library& library) noexcept;

// post: a block from libnuma for an active `resolved` policy (`numa_alloc_local` for `Auto`,
// `numa_alloc_onnode` for `Fixed`), or nullptr, in which case the caller takes the portable path.
// pre: `resolved` is `resolve`'s answer for this same `library`.
[[nodiscard]] std::byte* allocate_block(const Library& library, const ArenaNumaPolicy& resolved,
                                        std::size_t bytes, std::size_t alignment) noexcept;

} // namespace insight::tokenization::numa
