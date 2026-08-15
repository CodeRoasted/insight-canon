import os
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain


required_conan_version = ">=2.28"


class InsightSemanticGithubConan(ConanFile):
    name = "insight_semantic_github"
    version = "1.9.4"
    package_type = "library"
    license = "Apache-2.0"
    url = "https://github.com/CodeRoasted/insight-canon"
    description = (
        "InSight Canon semantic package: the GitHub Actions / Azure Pipelines dialect "
        "(ADR 0024). Structural-role / intent-marker / level-lift rule rows in the closed "
        "canon rule grammar, plus the dialect format strategy + echoed-source provenance hook "
        "(the code tier). Statically composed into a binary via insight::semantic::compose()."
    )
    settings = "os", "arch", "compiler", "build_type"

    options = {"shared": [True, False], "fPIC": [True, False]}
    default_options = {"shared": False, "fPIC": True}

    exports_sources = "CMakeLists.txt", "src/*"

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        if self.options.get_safe("shared"):
            self.options.rm_safe("fPIC")

    def layout(self):
        # Keyed editable build dir (mirrors insight_canon): malf sets MALF_EDITABLE_BUILD_DIR so a
        # consumer under any profile links THIS dep's matching-profile build.
        build_dir = (os.environ.get("MALF_EDITABLE_BUILD_DIR")
                     or self.conf.get("user.malf:editable_build_dir", default="build"))
        self.cpp.build.libdirs = [build_dir]
        self.cpp.build.builddirs = [build_dir]

    def requirements(self):
        # The provider contract (insight.canon.spi) + api types live in insight_canon; the package
        # imports its modules. transitive_headers/libs so a downstream composing this package resolves
        # canon's public module surface (fmt/spdlog GMF) through the same graph.
        self.requires("insight_canon/1.9.4", transitive_headers=True, transitive_libs=True)

    def build_requirements(self):
        self.test_requires("gtest/1.17.0")

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
        self.cpp_info.libs = ["insight_semantic_github"]
        self.cpp_info.set_property("cmake_file_name", "insight_semantic_github")
        self.cpp_info.set_property("cmake_target_name", "insight::semantic_github")
        self.cpp_info.requires = ["insight_canon::insight_canon"]
        # Cross-package C++ modules (mirrors insight_canon §10.7): defer to the package's OWN cmake
        # config (it carries FILE_SET CXX_MODULES; conan's generator does not emit it).
        self.cpp_info.set_property("cmake_find_mode", "none")
        malf_editable_build_dir = os.environ.get("MALF_EDITABLE_BUILD_DIR")
        if malf_editable_build_dir:
            self.cpp_info.builddirs = [malf_editable_build_dir,
                                       "lib/cmake/insight_semantic_github"]
        else:
            self.cpp_info.builddirs = ["lib/cmake/insight_semantic_github"]
