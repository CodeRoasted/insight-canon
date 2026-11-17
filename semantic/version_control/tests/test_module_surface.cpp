// invariant: this unit imports the package module and NOTHING of canon's, so naming canon's
// manifest type here resolves only through the package's `export import insight.canon.spi`.
// assert: turning the re-export into a plain import makes this unit a compile error.
#include <gtest/gtest.h>

import std;
import insight.semantic.version_control;

static_assert(
    std::same_as<std::remove_cvref_t<decltype(insight::semantic::version_control::kManifest)>,
                 insight::semantic::SemanticPackageManifest>,
    "kManifest's type must be nameable as canon's SemanticPackageManifest by a consumer "
    "that imports only this package");

TEST(VersionControlModuleSurface, AConsumerImportingOnlyThePackageNamesTheManifestType)
{
    const insight::semantic::SemanticPackageManifest& manifest{
        insight::semantic::version_control::kManifest};
    EXPECT_EQ(manifest.name, "version_control")
        << "the manifest reached through the package-only import is not this package's: name=\""
        << manifest.name << '"';
}
