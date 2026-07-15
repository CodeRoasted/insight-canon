import os
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain


required_conan_version = ">=2.28"


class InsightSemanticTestFrameworksConan(ConanFile):
    name = "insight_semantic_test_frameworks"
    version = "1.8.0"
    package_type = "library"
    license = "Apache-2.0"
    url = "https://github.com/CodeRoasted/insight-canon"
    description = (
        "InSight Canon semantic package: test-framework file-location vocabulary (ADR 0024). "
        "Location rule rows (jest/vitest/playwright/pytest/go/ruby test-file families) in the closed "
        "semantic-grammar-1 — DATA only, no code tier. The core recognizer walks these composed rows; "
        "framework file-naming is CI-dialect-independent. Statically composed via compose()."
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
        build_dir = (os.environ.get("MALF_EDITABLE_BUILD_DIR")
                     or self.conf.get("user.malf:editable_build_dir", default="build"))
        self.cpp.build.libdirs = [build_dir]
        self.cpp.build.builddirs = [build_dir]

    def requirements(self):
        self.requires("insight_canon/1.8.0", transitive_headers=True, transitive_libs=True)

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
        self.cpp_info.libs = ["insight_semantic_test_frameworks"]
        self.cpp_info.set_property("cmake_file_name", "insight_semantic_test_frameworks")
        self.cpp_info.set_property("cmake_target_name", "insight::semantic_test_frameworks")
        self.cpp_info.requires = ["insight_canon::insight_canon"]
        self.cpp_info.set_property("cmake_find_mode", "none")
        malf_editable_build_dir = os.environ.get("MALF_EDITABLE_BUILD_DIR")
        if malf_editable_build_dir:
            self.cpp_info.builddirs = [malf_editable_build_dir,
                                       "lib/cmake/insight_semantic_test_frameworks"]
        else:
            self.cpp_info.builddirs = ["lib/cmake/insight_semantic_test_frameworks"]
