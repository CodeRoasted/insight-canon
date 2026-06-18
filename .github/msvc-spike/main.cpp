// Consumer TU for the MSVC 14.52 gather spike. Imports the module, uses the exported
// defaulted operator==, and memcpy's the module-defined DTO — the two halves of what
// broke our consumer TUs on the stock 14.44 toolset.
import geom;
#include <cstring>

int main() {
    Point a{1, 2};
    Point b{1, 2};
    bool eq = (a == b);

    Point c{};
    std::memcpy(&c, &a, sizeof(Point));

    return (eq && (c == a)) ? 0 : 1;
}
