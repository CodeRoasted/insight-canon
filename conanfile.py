import os
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain


required_conan_version = ">=2.28"


class InsightCanonConan(ConanFile):
    name = "insight_canon"
    version = "1.5.4"
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
        # NUMA-aware arena allocation links libnuma (LGPL-2.1-or-later). It is
        # OPT-IN — off by default — so no distributed artifact ships copyleft by
        # accident (it rode silently into the proprietary `sift` binary at 1.5.1).
        # NUMA-off is BIT-IDENTICAL to NUMA-on (det_public_proof golden c88e8e9a)
        # and a no-op on single-socket hosts (numa_available() short-circuits to
        # the portable allocator). A consumer that genuinely runs multi-socket iron
        # — and accepts the LGPL-2.1 §6 static-link obligations for ITS artifact —
        # opts in with `insight_canon/*:with_numa=True`. Linux-only effect.
        "with_numa": [True, False],
    }

    default_options = {
        "shared": False,
        "fPIC": True,
        "with_numa": False,
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
        # Keyed editable build dir: malf sets the env (all profiles incl. sanitizer); a RAW
        # `conan create --profile X` instead reads it from the profile [conf] → a consumer under
        # ANY profile links THIS dep's matching-profile build, not the libc++-default build/
        # ([[malf-build-type-isolation]] keying gap).
        build_dir = (os.environ.get("MALF_EDITABLE_BUILD_DIR")
                     or self.conf.get("user.malf:editable_build_dir", default="build"))
        self.cpp.build.libdirs = [build_dir]
        # Editable: the build-tree export()'d insight_canon-config.cmake (carrying the
        # FILE_SET CXX_MODULES) lives in the build dir → consumers find it there (§10.9).
        self.cpp.build.builddirs = [build_dir]

    def requirements(self):
        # spdlog/fmt are actual libraries that appear in public API headers
        self.requires("spdlog/1.17.0", transitive_headers=True, transitive_libs=True)
        self.requires("fmt/12.1.0",    transitive_headers=True, transitive_libs=True)
        self.requires("simdjson/4.6.3")
        # NUMA-aware arena allocation (hot path). OPT-IN (see the `with_numa` option):
        # libnuma is LGPL-2.1, so it enters the graph ONLY when explicitly enabled.
        # When on it is **dynamically** linked (shared=True), NEVER statically: LGPL-2.1
        # §6(b) permits a proprietary work to use the library via a replaceable shared
        # object, whereas a STATIC link triggers the §6 relink obligation (ship object
        # files / written offer) we will not meet. So NUMA-on is compliant by
        # construction — the distributing artifact (the server) still owes the LGPL-2.1
        # text + a "uses libnuma" notice + a source pointer (see SBOM.md § NUMA). Off by
        # default ⇒ zero copyleft in the proprietary `sift` binary.
        if self.settings.os == "Linux" and self.options.with_numa:
            self.requires("libnuma/2.0.19", transitive_headers=True, transitive_libs=True,
                          options={"shared": True})

    def build_requirements(self):
        self.test_requires("gtest/1.17.0")
        self.test_requires("benchmark/1.9.5")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generator = "Ninja"
        # Drive the CMake switch from the conan option so the two never drift: the
        # libnuma require above and the INSIGHT_HAS_NUMA compile path are armed
        # together or not at all.
        tc.cache_variables["INSIGHT_CANON_ENABLE_NUMA"] = bool(self.options.with_numa)
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
        if self.settings.os == "Linux" and self.options.with_numa:
            self.cpp_info.requires.append("libnuma::libnuma")
        # Cross-package C++ modules (§10.7): defer to the package's OWN cmake config
        # (it carries FILE_SET CXX_MODULES; conan's generator does not emit it).
        # Editable build-tree config dir + create install path both listed.
        self.cpp_info.set_property("cmake_find_mode", "none")
        malf_editable_build_dir = os.environ.get("MALF_EDITABLE_BUILD_DIR")
        if malf_editable_build_dir:
            self.cpp_info.builddirs = [malf_editable_build_dir, "lib/cmake/insight_canon"]
        else:
            self.cpp_info.builddirs = ["lib/cmake/insight_canon"]
