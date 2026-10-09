# Canon public determinism proof — per-cell harness (Approach B, Daidalos ruling 2026-06-06).
#
# The "N ways" of the proof methodology (build N ways → canonical digest → assert identical
# + golden) moved from a raw single-shot textual recompile (the 1.5.1 unwrap deleted canon's
# textual public headers) to: build canon as a MODULE STATIC-LIB per cell via its real
# CXX_MODULE_STD build, and link the constant det_proof fixture against it. Building the lib
# IS recompiling canon's source under the cell's codegen — only the trivial fixture moves from
# recompile to link, so matrix coverage is unchanged. det_public_proof.sh drives one configure
# per cell (compiler×stdlib via the conan toolchain × -O × -ffp-contract via CELL_FLAGS); each
# cell is one legible cmake build whose compile_commands.json proves the flags it applied.
cmake_minimum_required(VERSION 3.28)

# Must be set before project()/CXX-enable — matches canon's own root (451f2fe2-… = CMake 4.3.1's
# CMAKE_EXPERIMENTAL_CXX_IMPORT_STD); det_proof imports insight.canon, built against the std module.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(canon_det_proof LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# TWO HARNESSES, ONE FIXTURE. With CANON_ROOT defined (the N-ways cells of det_public_proof.sh and
# the `canon_det_proof` inventory cell both pass it) this project recompiles canon and the semantic
# packages in its own tree under the cell's codegen. Without it this is the `insight_canon_proof`
# package step (proof/conanfile.py, DN-142.D5 (4)): det_proof links the insight_canon and semantic
# packages its graph resolves, the STORED ones in the writer's create, and the proof's gates run
# against that binary.
if(DEFINED CANON_ROOT)
    set(CANON_PROOF_IN_TREE ON)
else()
    set(CANON_PROOF_IN_TREE OFF)
endif()

# The cell's -O / -ffp-contract (+ SPDLOG off, so engine logs never reach the digest). Appended
# AFTER the conan toolchain's profile flags (-O3 -march -stdlib), so the cell's -O/-ffp win (last
# flag wins in gcc/clang). Applied to canon AND the fixture (declared before add_subdirectory).
if(DEFINED CELL_FLAGS)
    separate_arguments(_cell_flags NATIVE_COMMAND "${CELL_FLAGS}")
    add_compile_options(${_cell_flags})
endif()

if(CANON_PROOF_IN_TREE)
    # Build canon as a module static-lib (tests default OFF — not top-level here).
    add_subdirectory(${CANON_ROOT}/core canon_build)
    # The semantic packages (ADR-17.D8) — added AFTER canon so their `if(NOT TARGET insight::canon)`
    # guards see the in-tree target (no find_package). det_proof composes the product's exact
    # manifest set {github, gitlab, jenkins, package_managers, test_frameworks}, so the proof
    # exercises the composed pipeline (the byte-identity G-SP-1 gate; the grammar-2 composed
    # identity = G-OUT-6).
    add_subdirectory(${CANON_ROOT}/semantic/github github_build)
    add_subdirectory(${CANON_ROOT}/semantic/gitlab gitlab_build)
    add_subdirectory(${CANON_ROOT}/semantic/jenkins jenkins_build)
    add_subdirectory(${CANON_ROOT}/semantic/package_managers pkgmgr_build)
    add_subdirectory(${CANON_ROOT}/semantic/test_frameworks testfw_build)
else()
    find_package(insight_canon REQUIRED CONFIG)
    find_package(insight_semantic_github REQUIRED CONFIG)
    find_package(insight_semantic_gitlab REQUIRED CONFIG)
    find_package(insight_semantic_jenkins REQUIRED CONFIG)
    find_package(insight_semantic_package_managers REQUIRED CONFIG)
    find_package(insight_semantic_test_frameworks REQUIRED CONFIG)
endif()

# The constant fixture, consuming canon + the packages as external modules. picosha2 is canon's own
# header-only SHA-256 dependency: `--digest` hashes each member column of canon's projection with it
# (DN-108.D24, the cut's generation gate), and canon links it PRIVATE, so the fixture names it.
find_package(picosha2 REQUIRED CONFIG)
add_executable(det_proof det_proof.cpp)
target_link_libraries(det_proof PRIVATE
    insight::canon insight::semantic_github insight::semantic_gitlab
    insight::semantic_jenkins insight::semantic_package_managers insight::semantic_test_frameworks
    picosha2::picosha2)
target_compile_features(det_proof PRIVATE cxx_std_23)
set_target_properties(det_proof PROPERTIES CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)

# The showcase view gate, registered so `malf test` runs it every day: in the inventory cell
# (ADR-3.D9) and in the package step, each over its own det_proof; `malf build` never runs it.
# det_public_proof.sh runs the same gate on every cell of the determinism tower at the cut. Nested,
# canon and the semantic packages register no test. The scripts are found where the repository
# puts them, beside proof/, which the package's export reproduces.
get_filename_component(CANON_PROOF_REPO "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
enable_testing()
add_test(NAME showcase_view
         COMMAND bash "${CANON_PROOF_REPO}/scripts/tests/showcase_view_test.sh" "$<TARGET_FILE:det_proof>")

if(NOT CANON_PROOF_IN_TREE)
    # The determinism proof's REPORTING gate — the desk-runnable proof that a failing fixture
    # reds scripts/det_public_proof.sh, and that its -ffp-contract premise assertion refuses a
    # vacuous pass. Those paths were demonstrated once, by hand, by building the four-cell module
    # tower with a corpus file made unreadable; at minutes of compile per assertion nothing
    # re-checked them, so the gate's RED was unproven and its green read for more than it was
    # worth. The script sources the proof (which returns early when sourced) and drives the
    # reporting functions against hand-made files: no build, no compiler, well under a second.
    #
    # IT LIVES IN THE PACKAGE STEP, whose export carries the driver and scripts/tests/
    # (DN-142.D13 (3)): canon core's export does not, and an edit of the driver would otherwise
    # move canon's recipe revision for a file the library never reads.
    #
    # NOT ON WINDOWS, and the absence is declared rather than incidental: the subject is a bash
    # script, and the MSVC determinism leg does not run it (.github/workflows/golden.yaml drives
    # that leg through its own PowerShell path). A `find_program(bash)`-conditional registration
    # would make the same absence silent, which is the failure this comment exists to avoid.
    if(NOT WIN32)
        add_test(NAME det_public_proof_reporting
                 COMMAND bash "${CANON_PROOF_REPO}/scripts/tests/det_public_proof_reporting_test.sh")
    endif()
    include(GNUInstallDirs)
    install(TARGETS det_proof RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
