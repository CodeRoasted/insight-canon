// Module interface for the MSVC 14.52 gather spike (.github/workflows/msvc-14_52-spike.yml).
//
// Mirrors the bug class from bugs.md (the eidos detection crash): a HEAP-OWNING DTO
// (std::string + std::map<string,string>) with a DEFAULTED operator==, exported from a
// module that uses a global-module-fragment #include. The consumer TU then implicitly
// synthesizes this type's special members; on the buggy toolsets that synthesis is wrong
// (shallow memcpy of the non-trivial map → double-free at destruction). Synthetic, not
// eidos code — the exact eidos repro stays a private Heph spike.
module;
#include <string>
#include <map>
export module geom;

export struct Doc {
    std::string id;
    std::map<std::string, std::string> templates;
    bool operator==(const Doc& other) const = default;
};
