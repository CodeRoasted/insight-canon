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

# CANON_ROOT defaults to the parent (canon repo root); the driver may override it.
if(NOT DEFINED CANON_ROOT)
    get_filename_component(CANON_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
endif()

# The cell's -O / -ffp-contract (+ SPDLOG off, so engine logs never reach the digest). Appended
# AFTER the conan toolchain's profile flags (-O3 -march -stdlib), so the cell's -O/-ffp win (last
# flag wins in gcc/clang). Applied to canon AND the fixture (declared before add_subdirectory).
if(DEFINED CELL_FLAGS)
    separate_arguments(_cell_flags NATIVE_COMMAND "${CELL_FLAGS}")
    add_compile_options(${_cell_flags})
endif()

# Build canon as a module static-lib (tests/benchmarks default OFF — not top-level here).
add_subdirectory(${CANON_ROOT} canon_build)
# The two semantic packages (ADR 0024) — added AFTER canon so their `if(NOT TARGET insight::canon)`
# guards see the in-tree target (no find_package). det_proof composes their manifests, so the proof
# exercises the composed pipeline (the byte-identity G-SP-1 gate).
add_subdirectory(${CANON_ROOT}/semantic/github github_build)
add_subdirectory(${CANON_ROOT}/semantic/test_frameworks testfw_build)

# The constant fixture, consuming canon + the two packages as external modules.
add_executable(det_proof det_proof.cpp)
target_link_libraries(det_proof PRIVATE
    insight::canon insight::semantic_github insight::semantic_test_frameworks)
target_compile_features(det_proof PRIVATE cxx_std_23)
set_target_properties(det_proof PROPERTIES CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
