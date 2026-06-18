// Module interface for the MSVC 14.52 gather spike (.github/workflows/msvc-14_52-spike.yml).
// Exports a struct with a DEFAULTED operator== — the exact construct that ICE'd the
// stock 14.44 toolset when consumed across a translation unit. Synthetic, not eidos code.
export module geom;

export struct Point {
    int x;
    int y;
    bool operator==(const Point& other) const = default;
};
