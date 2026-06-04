import os
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain


required_conan_version = ">=2.28"


class InsightCanonConan(ConanFile):
    name = "insight_canon"
    version = "1.4.4"
    package_type = "library"
    license = "Apache-2.0"
    url = "https://github.com/CodeRoasted/insight-canon"
    description = (
        "InSight Canon: shared types, log tokenization, and streaming sequence model. "
        "Bundles core types, tokenization, and sequence into a single "
        "self-contained Conan package exposed as insight::canon."
    )
    settings = "os", "arch", "compiler", "build_type"

    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": True,
    }

    exports_sources = "CMakeLists.txt", "src/*", "api/*"

    def config_options(self):
        if self.settings.os == "Windows":
            self.options.rm_safe("fPIC")

    def configure(self):
        if self.options.get_safe("shared"):
            self.options.rm_safe("fPIC")

    def layout(self):
        self.cpp.source.includedirs = ["api"]
        self.cpp.build.libdirs = [os.environ.get("MALF_EDITABLE_BUILD_DIR", "build")]

    def requirements(self):
        # spdlog/fmt are actual libraries that appear in public API headers
        self.requires("spdlog/1.17.0", transitive_headers=True, transitive_libs=True)
        self.requires("fmt/12.1.0",    transitive_headers=True, transitive_libs=True)
        self.requires("simdjson/4.6.3")

    def build_requirements(self):
        self.test_requires("gtest/1.17.0")
        self.test_requires("benchmark/1.9.5")

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
        self.cpp_info.libs = ["insight_canon"]
        self.cpp_info.set_property("cmake_file_name", "insight_canon")
        self.cpp_info.set_property("cmake_target_name", "insight::canon")
        # Explicitly declare all consumed dependencies; spdlog/fmt are header-only
        self.cpp_info.requires = [
            "spdlog::spdlog",
            "fmt::fmt",  
            "simdjson::simdjson"
        ]
