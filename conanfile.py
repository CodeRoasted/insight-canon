from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain


class InsightCanonConan(ConanFile):
    name = "insight_canon"
    version = "1.2.8"
    package_type = "library"
    license = "Apache-2.0"
    url = "https://github.com/coderoast-dev/insight-canon"
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

    def requirements(self):
        # spdlog/fmt appear in the public logging API header; consumers must
        # see their includes and link against them.
        self.requires("spdlog/1.13.0", transitive_headers=True, transitive_libs=True)
        self.requires("fmt/10.2.1",    transitive_headers=True, transitive_libs=True)
        self.requires("simdjson/3.13.0")

    def build_requirements(self):
        self.test_requires("gtest/1.17.0")
        self.test_requires("nlohmann_json/3.11.3")

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
