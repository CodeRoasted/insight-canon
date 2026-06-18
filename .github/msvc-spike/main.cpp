// Consumer TU for the MSVC 14.52 gather spike. Imports the heap-owning module DTO and
// exercises exactly the operations that miscompiled in eidos: the implicitly-synthesized
// copy ctor, move ctor, and destructor of a module-defined type, plus the exported
// defaulted operator==. On a mis-synthesizing toolset the moved-from / copied DTOs
// double-free at destruction (heap corruption) — a runtime failure, caught by the run.
import geom;
#include <utility>

int main() {
    Doc a{"window-1", {{"tmpl", "abc"}}};
    Doc b = a;                 // implicitly-synthesized copy ctor (cross-module)
    Doc c = std::move(b);      // implicitly-synthesized move ctor (cross-module)
    bool eq = (a == c);        // exported defaulted operator==
    return eq ? 0 : 1;
}                              // ~Doc on a/b/c — double-frees here on a buggy toolset
