// Module interface for the MSVC 14.52 gather spike (.github/workflows/msvc-14_52-spike.yml).
// A trivial exported C++23 module with a defaulted operator== — a FUNCTIONAL smoke that the
// gathered toolset compiles named modules. This is NOT a bug discriminator: the eidos
// special-member/double-free miscompile resists synthetic reproduction (bugs.md: "every
// source-side fix peels another layer"), so validating the actual fix stays a private
// Heph spike against eidos's own code. Synthetic, not eidos code.
export module geom;

export struct Point {
    int x;
    int y;
    bool operator==(const Point& other) const = default;
};
