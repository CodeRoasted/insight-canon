import runpy
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain


required_conan_version = ">=2.28"


class InsightCanonProofConan(ConanFile):
    name = "insight_canon_proof"
    version = "1.10.7"
    package_type = "application"
    license = "Apache-2.0"
    url = "https://github.com/CodeRoasted/insight-canon"
    description = (
        "InSight Canon determinism proof, ship-leg step (DN-142.D5 (4)): the det_proof fixture "
        "linked against the STORED insight_canon and the five semantic packages, the proof "
        "driver's reporting gate and the showcase view gate. Never published. The N-ways cell "
        "matrix of scripts/det_public_proof.sh stays the tag-time determinism harness, which "
        "recompiles canon in its own tree from the same proof/CMakeLists.txt."
    )
    settings = "os", "arch", "compiler", "build_type"

    # THE EXPORT IS SHAPED AS THE REPOSITORY. The proof driver and both gates find each other by
    # their place in insight-canon (scripts/ beside proof/), so the recipe's root is the repository
    # (layout() below) and every input is exported at its repository path: proof/'s own sources and
    # corpus, the driver the reporting gate sources, the showcase script the view gate drives, and
    # the gates themselves. Each through malf's tracked-files helper (DN-142.D4 (b)): a file git
    # does not track is never exported.
    _EXPORTS = (
        ("CMakeLists.txt", "proof/CMakeLists.txt"),
        ("det_proof.cpp", "proof/det_proof.cpp"),
        ("corpus", "proof/corpus"),
        ("../scripts/det_public_proof.sh", "scripts/det_public_proof.sh"),
        ("../scripts/samples_showcase.sh", "scripts/samples_showcase.sh"),
        ("../scripts/tests", "scripts/tests"),
    )

    def export_sources(self):
        export_tracked = runpy.run_path(self.conf.get("user.malf:recipe_exports"))["export_tracked"]
        for source, destination in self._EXPORTS:
            export_tracked(self, source, destination)

    def layout(self):
        # The recipe sits in proof/ and its sources span the repository (above): conan's documented
        # subfolder layout, identical on the desk and in the create.
        self.folders.root = ".."
        self.folders.source = "proof"

    def requirements(self):
        self.requires("insight_canon/1.10.7")
        self.requires("insight_semantic_github/1.10.7")
        self.requires("insight_semantic_gitlab/1.10.7")
        self.requires("insight_semantic_jenkins/1.10.7")
        self.requires("insight_semantic_package_managers/1.10.7")
        self.requires("insight_semantic_test_frameworks/1.10.7")
        # `--digest` hashes each member column with canon's own SHA-256; canon links it PRIVATE.
        self.requires("picosha2/1.0.0")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generator = "Ninja"
        tc.generate()
        deps = CMakeDeps(self)
        deps.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
        runpy.run_path(self.conf.get("user.malf:recipe_tests"))["run_tests"](self)

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # Application package: nothing to link downstream; the artefact is the det_proof executable.
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
