// invariant: the pnpm install gauge is this package's vocabulary, so the arms proving which lines
// take the Progress role, and which lines sharing its bytes stay content, home with the row.
// invariant: every row is dialect-independent, so each arm holds on a stream declaring nothing and
// on one declaring a CI dialect alike.
// refs: DN-134.D9
#include <gtest/gtest.h>

import std;
import insight.canon;
import insight.semantic.package_managers;

using insight::StructuralRole;
using insight::semantic::ComposedSemantics;
using insight::tokenization::classify;

namespace
{
[[nodiscard]] insight::tokenization::NormalizedContent norm_probe(std::string_view probe)
{
    static std::string scratch;
    return insight::tokenization::normalize(probe, scratch).undeclared_suffix(0);
}

[[nodiscard]] ComposedSemantics view_of(std::string_view dialect)
{
    const std::array manifests{insight::semantic::package_managers::kManifest};
    return insight::semantic::compose(manifests).for_stream(dialect, {});
}
} // namespace

TEST(PackageManagersProgress, ThePnpmGaugeTakesTheRoleOnAnyStream)
{
    for (const std::string_view dialect :
         {insight::semantic::kAnyDialect, insight::semantic::package_managers::kDialect})
    {
        const ComposedSemantics view{view_of(dialect)};
        EXPECT_EQ(
            insight::to_string(classify(
                norm_probe("Progress: resolved 1052, reused 1009, downloaded 12, added 3"), view)),
            std::string_view{"Progress"})
            << "the gauge took no role on a stream declaring \"" << dialect << '"';
        EXPECT_EQ(insight::to_string(classify(
                      norm_probe("Progress: resolved 0, reused 0, downloaded 0, added 0 "), view)),
                  std::string_view{"Progress"});
    }
}

// invariant: each line opens with the gauge's bytes or carries them, and is content: the summary
// pnpm prints when the install ends, and the gauge behind a BuildKit or recursive-run prefix.
TEST(PackageManagersProgress, LinesSharingTheGaugeBytesStayContent)
{
    const ComposedSemantics view{view_of(insight::semantic::kAnyDialect)};
    for (const std::string_view content :
         {"Progress: resolved 1, reused 0, downloaded 1, added 1, done",
          "#12 3.4 Progress: resolved 1, reused 0, downloaded 1, added 1",
          "packages/web install$ Progress: resolved 1, reused 0, downloaded 1, added 1",
          "Progress: resolved 1, reused 0, downloaded 1",
          "Progress: resolved many, reused 0, downloaded 1, added 1"})
        EXPECT_EQ(insight::to_string(classify(norm_probe(content), view)), std::string_view{"None"})
            << "content took a role: \"" << content << '"';
}
