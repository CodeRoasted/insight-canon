// invariant: this unit imports the package module and NOTHING of canon's, so naming canon's
// manifest type here resolves only through the package's `export import insight.canon.spi`.
// assert: turning the re-export into a plain import makes this unit a compile error.
#include <gtest/gtest.h>

import std;
import insight.semantic.package_managers;

static_assert(
    std::same_as<std::remove_cvref_t<decltype(insight::semantic::package_managers::kManifest)>,
                 insight::semantic::SemanticPackageManifest>,
    "kManifest's type must be nameable as canon's SemanticPackageManifest by a consumer "
    "that imports only this package");

TEST(PackageManagersModuleSurface, AConsumerImportingOnlyThePackageNamesTheManifestType)
{
    const insight::semantic::SemanticPackageManifest& manifest{
        insight::semantic::package_managers::kManifest};
    EXPECT_EQ(manifest.name, "package_managers")
        << "the manifest reached through the package-only import is not this package's: name=\""
        << manifest.name << '"';
}
