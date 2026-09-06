// refs: SRC-II-8
// refs: F-SRC-insight-canon:test_semantic_walkers.cpp
// invariant: the matching mechanism is canon's and the vocabulary asserted here is this package's
// kLocations rows, so the knowledge test homes in this package.
// invariant: byte-only, with no RNG, no clock and no float.
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.test_frameworks;

using insight::recognize_location;
using insight::semantic::ComposedSemantics;

namespace
{
[[nodiscard]] ComposedSemantics test_frameworks_only()
{
    const std::array manifests{insight::semantic::test_frameworks::kManifest};
    return insight::semantic::compose(manifests);
}

void expect_loc(const ComposedSemantics& composed, std::string_view content,
                std::string_view expected)
{
    // pre: recognize_location takes NormalizedContent, a precondition carried by a type that cannot
    // be constructed outside canon.
    // invariant: every probe here is an escape-free literal, so normalize() is the zero-copy fixed
    // point over this shared scratch.
    static std::string scratch;
    const std::string_view got{recognize_location(
        insight::tokenization::normalize(content, scratch).undeclared_suffix(0), composed)};
    EXPECT_EQ(got, expected) << "recognize_location(\"" << content << "\") = \"" << got
                             << "\"  expected \"" << expected << '"';
}
} // namespace

TEST(LocationFamilies, ExtractsAllFiveFamilies)
{
    const ComposedSemantics tf{test_frameworks_only()};
    expect_loc(tf, "PASS src/auth/login.test.ts", "src/auth/login.test.ts");
    expect_loc(tf, "src/components/Button.spec.tsx passed", "src/components/Button.spec.tsx");
    expect_loc(tf, "tests/test_login.py PASSED", "tests/test_login.py");
    expect_loc(tf, "app/api/login_test.py PASSED", "app/api/login_test.py");
    expect_loc(tf, "ok internal/server/handler_test.go 0.42s", "internal/server/handler_test.go");
    expect_loc(tf, "spec/models/user_spec.rb", "spec/models/user_spec.rb");
    expect_loc(tf, "test/unit/user_test.rb", "test/unit/user_test.rb");
}

TEST(LocationFamilies, StripsTrailingCoordinatesAndLeadingGlyphs)
{
    const ComposedSemantics tf{test_frameworks_only()};
    expect_loc(tf, "tests/login.test.ts:42:5", "tests/login.test.ts");
    expect_loc(tf, "pkg/net/handler_test.go::TestHandler", "pkg/net/handler_test.go");
    expect_loc(tf, "\t\tspec/models/user_spec.rb", "spec/models/user_spec.rb");
    expect_loc(tf, "\xE2\x9C\x93 src/util/date.test.js (12 ms)", "src/util/date.test.js");
}

// refs: BIB:intent_identity
// invariant: canon establishes the location START by byte class and never by a producer marker
// list, so a marker welded to a path is excluded without being named.
TEST(LocationFamilies, GluedProducerAnnotationIsNotPartOfTheLocation)
{
    const ComposedSemantics tf{test_frameworks_only()};
    expect_loc(tf,
               "##[error]fs/rc/rcserver/rcserver_test.go:104:", "fs/rc/rcserver/rcserver_test.go");
    expect_loc(tf, "##[group]src/options.spec.ts", "src/options.spec.ts");
    expect_loc(tf, "##[debug]File:/home/runner/work/echarts/echarts/test/ut/spec/util/time.test.ts",
               "/home/runner/work/echarts/echarts/test/ut/spec/util/time.test.ts");
    expect_loc(tf, "\"src/auth/login.test.ts\"", "src/auth/login.test.ts");
    expect_loc(tf, "(tests/test_login.py)", "tests/test_login.py");

    // invariant: the costly direction — a byte that IS part of a real path must never truncate
    // the label.
    expect_loc(tf, "node_modules/@scope/pkg/src/index.spec.js",
               "node_modules/@scope/pkg/src/index.spec.js");
    expect_loc(tf, "external/devinfra+/pkg/net/handler_test.go",
               "external/devinfra+/pkg/net/handler_test.go");
    expect_loc(tf, "packages/app-desktop\\integration-tests\\main.spec.ts",
               "packages/app-desktop\\integration-tests\\main.spec.ts");
}

TEST(LocationFamilies, NoFalsePositives)
{
    const ComposedSemantics tf{test_frameworks_only()};
    expect_loc(tf, "config.spec.json is valid", "");
    expect_loc(tf, "Compiling src/main.py", "");
    expect_loc(tf, "import helper from './helpers.ts'", "");
    expect_loc(tf, "", "");
}
