// insight.canon.internal — the lone `import std` manifest for the canon module graph (
// ADR-3.D4). Every other canon module unit imports THIS (plain) to reach std; it is the single
// hinge where a future modular dependency graduates in. It also re-exports the GLOBAL C fixed-width
// types that the canon source uses UNQUALIFIED (`uint64_t`, `uint8_t`, …) — `import std` provides
// `std::uint64_t` but NOT the global `::uint64_t`, so we surface them here (the logcraft internal
// pattern).
export module insight.canon.internal;

export import std;

// Global unqualified C fixed-width types used across the canon source (types.hpp's
// `using EventID = uint64_t;` etc.). Re-exported so every importer sees them unqualified.
export {
    using std::int16_t;
    using std::int32_t;
    using std::int64_t;
    using std::int8_t;
    using std::uint16_t;
    using std::uint32_t;
    using std::uint64_t;
    using std::uint8_t;

    using std::ptrdiff_t;
    using std::size_t;
}
