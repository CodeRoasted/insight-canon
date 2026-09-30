// refs: ADR-18.D1
// invariant: a name canonicalized with NO declared version coordinate is byte-identical to what
// canon gave it before the coordinate existed: a dialect declaring none moves no class.
// invariant: the expected values are the classes and instances canon gives these names at
// stateless-masks-16, written out; a diff here is a comparability break, never a retune.
// invariant: homed beside the frozen canonicalizer contract, because the property is the core's
// undeclared path.
#include <gtest/gtest.h>

import insight.canon.test;

namespace
{

struct Undeclared
{
    std::string_view name;
    std::string_view class_;
    std::string_view instance;
};

constexpr std::array kUndeclared{
    Undeclared{.name = "actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd",
               .class_ = "actions/checkout@de0fac2e4500dabe0009e67214ff5f5447ce83dd",
               .instance = ""},
    Undeclared{.name = "actions/checkout@v4", .class_ = "actions/checkout@vX", .instance = "v4"},
    Undeclared{.name = "dtolnay/rust-toolchain@stable",
               .class_ = "dtolnay/rust-toolchain@stable",
               .instance = ""},
    Undeclared{.name = "dtolnay/rust-toolchain@1.77.2",
               .class_ = "dtolnay/rust-toolchain@vX",
               .instance = "1.77.2"},
    Undeclared{.name = "pytorch/test-infra/.github/actions/setup-uv@release/2.13",
               .class_ = "pytorch/test-infra/.github/actions/setup-uv@release/vX",
               .instance = "2.13"},
    Undeclared{.name = "deploy user@host", .class_ = "deploy user@host", .instance = ""},
};

} // namespace

// refs: ADR-18.D1
// invariant: the undeclared path masks no `@` suffix: a commit pin and a named ref stay in the
// class, and a tag or dotted number is masked by the rules that always masked it.
TEST(UndeclaredVersionCoordinate, TheCoresUndeclaredPathIsByteIdentical)
{
    for (const auto& [name, class_, instance] : kUndeclared)
    {
        EXPECT_EQ(insight::canonicalize_intent(name), class_) << "class of \"" << name << "\"";
        EXPECT_EQ(insight::discriminant_of(name), instance) << "instance of \"" << name << "\"";
    }
}
