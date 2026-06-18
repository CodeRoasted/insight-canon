// Consumer TU for the MSVC 14.52 gather spike — a functional smoke that the gathered
// toolset imports a module and uses its exported defaulted operator==. (No extra textual
// includes: mixing a module import with header inclusion of the same std internals trips
// a separate redefinition error that is unrelated to the toolset we are probing.)
import geom;

int main() {
    Point a{1, 2};
    Point b{1, 2};
    return (a == b) ? 0 : 1;
}
