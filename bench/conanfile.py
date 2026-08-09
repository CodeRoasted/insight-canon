import os
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain


required_conan_version = ">=2.28"


class InsightCanonBenchConan(ConanFile):
    name = "insight_canon_bench"
    version = "1.9.3"
    package_type = "application"
    license = "Apache-2.0"
    url = "https://github.com/CodeRoasted/insight-canon"
    description = (
        "InSight Canon composed benchmark harness (ADR 0024 / SRC-SP-5). Measures the tokenization "
        "hot path under the COMPOSED semantic set (github + test_frameworks) against the "
        "degenerate core-only composition, in one binary — the per-line-cost gate for every "
        "composition-mechanism change. A leaf package by necessity: linking the vocabulary "
        "packages from the core build would invert the SRC-SP-1 / R1 dependency arrow (core must "
        "never depend on a semantic package), so the perf gate lives where the composition "
        "point does — above them all."
    )
    settings = "os", "arch", "compiler", "build_type"

    exports_sources = "CMakeLists.txt", "src/*"

    def layout(self):
        # Keyed editable build dir (mirrors insight_canon): malf sets MALF_EDITABLE_BUILD_DIR so
        # this harness builds/links against each dep's matching-profile build tree.
        build_dir = (os.environ.get("MALF_EDITABLE_BUILD_DIR")
                     or self.conf.get("user.malf:editable_build_dir", default="build"))
        self.cpp.build.libdirs = [build_dir]
        self.cpp.build.builddirs = [build_dir]

    def requirements(self):
        self.requires("insight_canon/1.9.3")
        self.requires("insight_semantic_github/1.9.3")
        self.requires("insight_semantic_gitlab/1.9.3")
        self.requires("insight_semantic_jenkins/1.9.3")
        self.requires("insight_semantic_test_frameworks/1.9.3")
        self.requires("benchmark/1.9.5")

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

    def package(self):
        cmake = CMake(self)
        cmake.install()

    def package_info(self):
        # Application package: nothing to link downstream; the artifact is the bench executable.
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
