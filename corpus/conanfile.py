import runpy
from conan import ConanFile
from conan.tools.cmake import CMake, CMakeDeps, CMakeToolchain


required_conan_version = ">=2.28"


class InsightCanonCorpusConan(ConanFile):
    name = "insight_canon_corpus"
    version = "1.10.9"
    package_type = "application"
    license = "Apache-2.0"
    url = "https://github.com/CodeRoasted/insight-canon"
    description = (
        "InSight Canon corpus cut tool (DN-142.D33): the three test executables that hold canon's "
        "corpus gates, insight_canon_tests, insight_semantic_jenkins_tests and "
        "insight_semantic_gitlab_tests, built from the targets core/ and semantic/{jenkins,gitlab} "
        "define. A candidate stores the ship form; the cut restores them by key and runs their "
        "corpus suites over the private corpora. Never released."
    )
    settings = "os", "arch", "compiler", "build_type"

    # THE EXPORT IS SHAPED AS THE REPOSITORY, as insight_canon_proof's is. The executables are the
    # SAME targets the released packages' CMakeLists define, so the recipe's root is the repository
    # (layout() below) and core/ and the two dialects are exported at their repository paths, beside
    # this directory's CMakeLists and its suite check. Each through malf's tracked-files helper
    # (DN-142.D4 (b)): a file git does not track is never exported.
    _EXPORTS = (
        ("CMakeLists.txt", "corpus/CMakeLists.txt"),
        ("corpus_suites_listed.cmake", "corpus/corpus_suites_listed.cmake"),
        ("../core", "core"),
        ("../semantic/jenkins", "semantic/jenkins"),
        ("../semantic/gitlab", "semantic/gitlab"),
    )
    # The install component that holds the three executables and nothing else: the libraries,
    # modules and cmake configs core/ and the dialects install for their own packages stay out.
    _COMPONENT = "corpus_tools"

    def export_sources(self):
        export_tracked = runpy.run_path(self.conf.get("user.malf:recipe_exports"))["export_tracked"]
        for source, destination in self._EXPORTS:
            export_tracked(self, source, destination)

    def layout(self):
        # The recipe sits in corpus/ and its sources span the repository (above): conan's documented
        # subfolder layout, identical on the desk and in the create.
        self.folders.root = ".."
        self.folders.source = "corpus"

    def requirements(self):
        # canon is compiled in this tree (the test target recompiles it white-box, and the dialects'
        # test targets link that in-tree insight::canon), so its own requirements are this recipe's.
        self.requires("spdlog/1.17.0")
        self.requires("fmt/12.1.0")
        self.requires("simdjson/4.6.3")
        self.requires("picosha2/1.0.0")
        # A plain requirement, never a test requirement: googletest is linked INTO the artefact this
        # package ships to the cut, in every form, the ship form included.
        self.requires("gtest/1.17.0")

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
        cmake.install(component=self._COMPONENT)

    def package_info(self):
        # Application package: nothing to link downstream; the artefacts are the three executables.
        self.cpp_info.libdirs = []
        self.cpp_info.includedirs = []
