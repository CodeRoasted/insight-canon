// refs: ADR-3.D4
// invariant: the one `import std` of the canon module graph — every other canon module unit
// imports this, plain, to reach std.
export module insight.canon.internal;

export import std;

// invariant: `import std` provides std::uint64_t but not the global ::uint64_t, so the unqualified
// C fixed-width types are re-exported here.
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
