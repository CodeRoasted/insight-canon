// invariant: this unit imports the package module and NOTHING of canon's, so naming canon's
// manifest type here resolves only through the package's `export import insight.canon.spi`.
// invariant: that re-export is the surface an external consumer relies on to name `kManifest`'s
// type, and no other unit in this tree reaches it — every sibling imports canon directly too.
// assert: turning the re-export into a plain import makes this unit a compile error.
#include <gtest/gtest.h>

import std;
import insight.semantic.test_frameworks;

static_assert(
    std::same_as<std::remove_cvref_t<decltype(insight::semantic::test_frameworks::kManifest)>,
                 insight::semantic::SemanticPackageManifest>,
    "kManifest's type must be nameable as canon's SemanticPackageManifest by a consumer "
    "that imports only this package");

TEST(TestFrameworksModuleSurface, AConsumerImportingOnlyThePackageNamesTheManifestType)
{
    const insight::semantic::SemanticPackageManifest& manifest{
        insight::semantic::test_frameworks::kManifest};
    EXPECT_EQ(manifest.name, "test_frameworks")
        << "the manifest reached through the package-only import is not this package's: name=\""
        << manifest.name << '"';
}
