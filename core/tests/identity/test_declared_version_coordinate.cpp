// invariant: the core applies a DECLARED version coordinate as a mechanism: the introducer is a
// parameter, so every arm below runs with introducers no shipped dialect declares.
// invariant: a version is masked whole by the one version mask in the class and carried verbatim
// in the discriminant, whatever its bytes are; the head takes the same rules as any name.
// invariant: homed beside the canonicalizer's own contract, because the subject is the core's
// mechanism and no dialect package.
#include <gtest/gtest.h>

import insight.canon.test;

namespace
{

using insight::tokenization::IntentMarker;
using insight::tokenization::IntentMarkerKind;

// post: a recognized Step marker for `name` whose declared version is what `introducer` selects,
// built as the recognizer builds it.
[[nodiscard]] IntentMarker marker_of(std::string_view name, std::string_view introducer)
{
    const std::string_view version{insight::one_token_version_of(name, introducer)};
    return {.kind = IntentMarkerKind::Step,
            .name = name,
            .discriminant = insight::discriminant_of(name, version),
            .child_order = insight::tokenization::ChildOrder::Ordered,
            .version = version};
}

struct Declared
{
    std::string_view name;
    std::string_view introducer;
    std::string_view version;
    std::string_view class_;
    std::string_view instance;
};

constexpr std::array kDeclared{
    // note: a colon and a two-byte introducer: the mechanism holds no byte of its own.
    Declared{.name = "registry/tool:9f86d081884c7d659a2f",
             .introducer = ":",
             .version = "9f86d081884c7d659a2f",
             .class_ = "registry/tool:vX",
             .instance = "9f86d081884c7d659a2f"},
    Declared{.name = "lib/parser==main",
             .introducer = "==",
             .version = "main",
             .class_ = "lib/parser==vX",
             .instance = "main"},
    // note: the LAST introducer selects, and everything after it is the version, separators too.
    Declared{.name = "scope#pkg#release/2.13",
             .introducer = "#",
             .version = "release/2.13",
             .class_ = "scope#pkg#vX",
             .instance = "release/2.13"},
    // note: a space inside makes it two tokens, so it has no version and keeps its class.
    Declared{.name = "build (linux)/tool#abc",
             .introducer = "#",
             .version = "",
             .class_ = "build (M)/tool#abc",
             .instance = "(linux)"},
    // note: a span in the head opens the envelope, and the version closes it.
    Declared{.name = "build(linux)/tool#abc",
             .introducer = "#",
             .version = "abc",
             .class_ = "build(M)/tool#vX",
             .instance = "(linux)/tool#abc"},
    // note: a version the three rules would also claim gives the class it always gave.
    Declared{.name = "owner/action#v4",
             .introducer = "#",
             .version = "v4",
             .class_ = "owner/action#vX",
             .instance = "v4"},
};

} // namespace

// invariant: for every declared name, the version is the bytes after the last introducer, the
// class masks it whole, and the discriminant carries it verbatim.
TEST(DeclaredVersionCoordinate, TheVersionIsMaskedWholeInTheClassAndCarriedInTheDiscriminant)
{
    for (const auto& [name, introducer, version, class_, instance] : kDeclared)
    {
        const IntentMarker marker{marker_of(name, introducer)};
        EXPECT_EQ(marker.version, version) << "version of \"" << name << "\"";
        EXPECT_EQ(insight::canonicalize_intent(marker), class_) << "class of \"" << name << "\"";
        EXPECT_EQ(marker.discriminant, instance) << "instance of \"" << name << "\"";
    }
}

// invariant: the one-token shape is the guard: a name holding an intent trim byte, no introducer,
// or nothing after its last introducer has no version, and an empty introducer selects nothing.
// invariant: surrounding trim bytes are not part of the name, so they do not make it two tokens.
TEST(DeclaredVersionCoordinate, OnlyAOneTokenNameWithBytesAfterItsIntroducerHasAVersion)
{
    EXPECT_EQ(insight::one_token_version_of("pull image#abc", "#"), "") << "two tokens";
    EXPECT_EQ(insight::one_token_version_of("pull\timage#abc", "#"), "") << "a tab inside";
    EXPECT_EQ(insight::one_token_version_of("owner/action", "#"), "") << "no introducer";
    EXPECT_EQ(insight::one_token_version_of("owner/action#", "#"), "") << "nothing after it";
    EXPECT_EQ(insight::one_token_version_of("owner/action#abc", ""), "") << "an empty introducer";
    EXPECT_EQ(insight::one_token_version_of("  owner/action#abc\t", "#"), "abc")
        << "trim bytes around a one-token name";
}

// invariant: a marker with no version takes the undeclared path, byte for byte: its class is
// canonicalize_intent of its name and its discriminant is discriminant_of its name.
TEST(DeclaredVersionCoordinate, AMarkerWithNoVersionIsTheUndeclaredPath)
{
    for (const std::string_view name :
         {"owner/action#0123456789abcdef", "build (linux, 3.12)", "deploy v2 to staging", "make"})
    {
        const IntentMarker marker{.kind = IntentMarkerKind::Step,
                                  .name = name,
                                  .discriminant = insight::discriminant_of(name, {}),
                                  .child_order = insight::tokenization::ChildOrder::Ordered,
                                  .version = {}};
        EXPECT_EQ(insight::canonicalize_intent(marker), insight::canonicalize_intent(name))
            << "class of \"" << name << "\"";
        EXPECT_EQ(marker.discriminant, insight::discriminant_of(name))
            << "instance of \"" << name << "\"";
    }
}
