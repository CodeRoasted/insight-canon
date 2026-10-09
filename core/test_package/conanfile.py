import runpy

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain, cmake_layout


class InsightCanonTestPackageConan(ConanFile):
    settings = "os", "arch", "compiler", "build_type"
    test_type = "explicit"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def build_requirements(self):
        self.test_requires("gtest/1.17.0")

    def configure(self):
        self.options["gtest"].shared = False

    def layout(self):
        cmake_layout(self)

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

    def test(self):
        # malf's helper runs the package's one test selection here, in the writer's create, writes
        # the step's second result file and fails the create on any red (DN-142.D13 (2)); under
        # tools.build:skip_test it runs nothing.
        runpy.run_path(self.conf.get("user.malf:recipe_tests"))["run_test_package"](self)
