###########################################################
#  insight_semantic_github — the GitHub Actions / Azure Pipelines dialect semantic package
#  (ADR 0024). ONE module interface unit (insight.semantic.github, a thin wrapper around the
#  projection of github.dialect.yaml) plus one impl unit (its code tier), and NO partitions (the
#  cross-package BMI closure must not deepen — ADR 0024 §7 / the named-modules hazard family).
###########################################################
cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(insight_semantic_github LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(INSIGHT_SEMANTIC_GITHUB_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})

# Pulled in here rather than beside the install rules: CMAKE_INSTALL_INCLUDEDIR is read by the
# module target's install interface (target_include_directories below), which is declared with the
# target and not with the install block.
include(GNUInstallDirs)

# Cross-package dep: guarded so an umbrella build's single resolution stays authoritative.
if(NOT TARGET insight::canon)
    find_package(insight_canon REQUIRED CONFIG)
endif()

###########################################################
#  github.dialect.yaml -> generated package content (ADR-17 / DN-17.D31)
#  The declaration IS the ruleset: `dialect_package_codegen.py` (the PUBLIC malf-toolchain
#  half) projects it into C++ at build time. The projection is BUILT, never committed —
#  a committed projection is a second artifact that can disagree with its source.
#  Consequence, stated because it is a real cost: this public Apache-2.0 package therefore
#  has a build-time dependency on Python 3 and on a malf-toolchain checkout at a DECLARED
#  path. `--selftest` runs before every regeneration, so the tool's own falsifiers execute
#  on every host that builds this package, including the raw determinism legs.
###########################################################
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# MALF_TOOLCHAIN_DIR is the RESOLUTION MECHANISM (cache var, then environment). The relative
# branch below is a DESK CONVENIENCE and anchors on exactly ONE documented layout — the
# workspace map in CLAUDE.md, where every repo including malf is a sibling. There is no second
# candidate and there never may be: a two-candidate `if(EXISTS a) else(b)` probe SUCCEEDS in
# both environments while being right in only one, and nothing downstream can tell which it
# picked. In CI the variable is exported (`conan_module.sh`, `setup-build-env`, and each raw
# golden leg), so the two never compete; under `conan create` no relative path can reach the
# toolchain at all, because the build runs from sources exported into the conan cache.
if(NOT DEFINED MALF_TOOLCHAIN_DIR OR MALF_TOOLCHAIN_DIR STREQUAL "")
    if(DEFINED ENV{MALF_TOOLCHAIN_DIR} AND NOT "$ENV{MALF_TOOLCHAIN_DIR}" STREQUAL "")
        set(MALF_TOOLCHAIN_DIR "$ENV{MALF_TOOLCHAIN_DIR}")
    else()
        # <workspace>/insight-canon/semantic/github -> <workspace>/malf. The depth literal is one
        # arithmetic fact that repeats once per declaration-bearing package, so the FATAL below
        # names the layout it assumed: a wrong copy must fail legibly rather than resolve
        # somewhere plausible.
        get_filename_component(MALF_TOOLCHAIN_DIR
            "${CMAKE_CURRENT_SOURCE_DIR}/../../../malf" ABSOLUTE)
    endif()
endif()
set(DIALECT_CODEGEN_TOOL "${MALF_TOOLCHAIN_DIR}/dialect_package_codegen.py")
if(NOT EXISTS "${DIALECT_CODEGEN_TOOL}")
    message(FATAL_ERROR
        "insight_semantic_github: dialect_package_codegen.py NOT FOUND.\n"
        "  looked for: ${DIALECT_CODEGEN_TOOL}\n"
        "  MALF_TOOLCHAIN_DIR resolved to: '${MALF_TOOLCHAIN_DIR}'\n"
        "  package source dir: ${CMAKE_CURRENT_SOURCE_DIR}\n"
        "This package's ruleset is generated at build time from github.dialect.yaml by the "
        "public malf-toolchain tool, so the build CANNOT proceed without it and never "
        "degrades to a smaller package. MALF_TOOLCHAIN_DIR is the resolution mechanism: pass "
        "-DMALF_TOOLCHAIN_DIR=<path-to-malf-toolchain-checkout> or export it in the "
        "environment. The relative fallback that was tried assumes the ONE documented desk "
        "layout — malf checked out as a SIBLING of insight-canon under the workspace root, "
        "i.e. <workspace>/malf next to <workspace>/insight-canon — and it is a convenience, "
        "not a mechanism. It cannot work under `conan create`, which builds from sources "
        "exported into the conan cache.")
endif()
# Fence: the declaration is NAMED, never globbed. An empty glob yielding an empty Intent
# library is a legitimate degenerate state in logcraft/core; here it is not — ZERO ROWS IS NOT
# A DEGENERATE DIALECT, IT IS A BROKEN ONE. A row-less manifest still compiles, still composes
# and still publishes a `semantic_identity`, so a glob that silently matched nothing would ship
# a valid-looking empty dialect. Absence is a configure-time FATAL instead.
set(GITHUB_DIALECT_DECLARATION "${CMAKE_CURRENT_SOURCE_DIR}/github.dialect.yaml")
if(NOT EXISTS "${GITHUB_DIALECT_DECLARATION}")
    message(FATAL_ERROR
        "insight_semantic_github: github.dialect.yaml NOT FOUND.\n"
        "  looked for: ${GITHUB_DIALECT_DECLARATION}\n"
        "This file IS the package's ruleset — every structural role, intent marker and "
        "level-lift row is a projection of it. It is named explicitly rather than globbed "
        "because a dialect with no rows still builds and still publishes an identity, so a "
        "missing declaration must stop the build here instead of producing an empty package. "
        "Under `conan create` its presence is carried by `exports_sources` in conanfile.py.")
endif()
set(GITHUB_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(GITHUB_GENERATED_INC "${GITHUB_GENERATED_DIR}/github.generated.inc")
set(GITHUB_GENERATED_STAMP "${GITHUB_GENERATED_DIR}/github.dialect.stamp")
# Stamp pattern (mirrors logcraft/core): the tool write-if-changes the .inc, so unchanged
# content never recompiles a dependent TU, while the always-touched stamp stops ninja
# re-running the command on every build.
# No `--module-suffix`: the projection lands in `insight::semantic::github`, the canonical
# namespace, because it IS this package now. The suffix was the coexistence parameter that kept a
# second, projected package disjoint from a hand-written one while the two were compared; there is
# one package again and the suffix would only re-introduce a name nothing consumes.
add_custom_command(
    OUTPUT "${GITHUB_GENERATED_STAMP}"
    BYPRODUCTS "${GITHUB_GENERATED_INC}"
    COMMAND Python3::Interpreter "${DIALECT_CODEGEN_TOOL}" --selftest
    COMMAND Python3::Interpreter "${DIALECT_CODEGEN_TOOL}"
            --declaration "${GITHUB_DIALECT_DECLARATION}"
            --out "${GITHUB_GENERATED_INC}"
    COMMAND ${CMAKE_COMMAND} -E touch "${GITHUB_GENERATED_STAMP}"
    DEPENDS "${DIALECT_CODEGEN_TOOL}" "${GITHUB_DIALECT_DECLARATION}"
    COMMENT "dialect_package_codegen: --selftest, then project github.dialect.yaml"
    VERBATIM)
add_custom_target(insight_semantic_github_dialect_gen DEPENDS "${GITHUB_GENERATED_STAMP}")

# One module interface unit: `github.cppm`, the thin wrapper that textually includes the projection
# generated above. It is PUBLIC and it is EXPORTED — cross-package module shipping installs the
# interface unit itself and the CONSUMER recompiles it from the conan package (§10.7/§10.9) — which
# is why the projection travels with it (the install(FILES) below) and why the include directory is
# declared with both interfaces (target_include_directories below).
set(GITHUB_MODULE_UNITS ${CMAKE_CURRENT_SOURCE_DIR}/src/github.cppm)
file(GLOB_RECURSE GITHUB_IMPL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(insight_semantic_github STATIC ${GITHUB_IMPL_SOURCES})
target_sources(insight_semantic_github
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES    ${GITHUB_MODULE_UNITS}
)
set_target_properties(insight_semantic_github PROPERTIES EXPORT_NAME semantic_github
    CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
target_compile_features(insight_semantic_github PUBLIC cxx_std_23)
add_library(insight::semantic_github ALIAS insight_semantic_github)

target_link_libraries(insight_semantic_github PUBLIC insight::canon)

# THE PROJECTION'S TWO ADDRESSES, and they are different questions rather than one path written
# twice. In the BUILD TREE the projection sits in `<build>/generated/`, an absolute path that
# resolves only on the machine that built it — correct there, and CMake carries it to a consumer
# that recompiles this interface unit through `IMPORTED_CXX_MODULES_INCLUDE_DIRECTORIES`, which is
# what makes the editable workspace work. In the INSTALLED package that path does not exist, so the
# projection is installed NEXT TO `github.cppm` in the module directory (install(FILES) below) and
# the include directory is declared against the install prefix as well. Belt and braces on purpose:
# the quoted `#include "github.generated.inc"` resolves by the includer's own directory before any
# search path is consulted, so the installed layout works even where CMake declines to carry a
# module include directory across the export.
target_include_directories(insight_semantic_github PUBLIC
    $<BUILD_INTERFACE:${GITHUB_GENERATED_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/insight_semantic_github_modules>)

# The declaration is projected as part of BUILDING this package, not as an opt-in target:
# a generation step someone must remember to invoke is a step that does not run. Ordering is also
# a correctness requirement rather than a convenience: the module SCAN preprocesses
# github.cppm, so the projection must exist before the target's first build step, not merely
# before its compile step.
add_dependencies(insight_semantic_github insight_semantic_github_dialect_gen)

# Determinism: the dialect recognizers feed deterministic content → no FMA contraction (mirrors canon).
target_compile_options(insight_semantic_github PUBLIC
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-ffp-contract=off>
)

###########################################################
#  Install + cross-package module shipping (mirrors insight_canon §10.7/§10.9)
###########################################################
install(TARGETS insight_semantic_github
    EXPORT insight_semantic_githubTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILE_SET cxx_modules DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/insight_semantic_github_modules
)
# The projection travels WITH the interface unit that includes it, into the same directory. This is
# not a convenience copy: the consumer recompiles `github.cppm` from the package, and the file it
# textually includes has to be reachable from wherever the consumer unpacked us. Installed as a
# sibling, `#include "github.generated.inc"` resolves by the includer's own directory. The file is a
# BYPRODUCT of the generation step above, so a missing one is an install-time error naming it —
# fail-closed, in the direction that matters: a package that shipped the interface unit without its
# content would fail at every consumer's scan step instead of here.
install(FILES ${GITHUB_GENERATED_INC}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/insight_semantic_github_modules)
install(EXPORT insight_semantic_githubTargets
    NAMESPACE insight::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/insight_semantic_github
)
# Ship the package's OWN cmake config (carries FILE_SET CXX_MODULES); conan defers via
# cmake_find_mode=none. create → install(EXPORT); editable → BUILD-TREE export().
export(EXPORT insight_semantic_githubTargets
    NAMESPACE insight::
    FILE ${CMAKE_CURRENT_BINARY_DIR}/insight_semantic_githubTargets.cmake)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/insight_semantic_github-config.cmake
"include(CMakeFindDependencyMacro)
find_dependency(insight_canon)
include(\"\${CMAKE_CURRENT_LIST_DIR}/insight_semantic_githubTargets.cmake\")
")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/insight_semantic_github-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/insight_semantic_github)

###########################################################
#  Tests (Kleio homes the package test suites; this wires them when present)
###########################################################
if(INSIGHT_SEMANTIC_GITHUB_BUILD_TESTS)
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    if(TEST_SOURCES)
        include(CTest)
        find_package(GTest REQUIRED CONFIG)
        enable_testing()
        add_executable(insight_semantic_github_tests ${TEST_SOURCES} ${GITHUB_IMPL_SOURCES})
        target_sources(insight_semantic_github_tests
            PRIVATE
                FILE_SET cxx_modules TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
                FILES    ${GITHUB_MODULE_UNITS})
        target_link_libraries(insight_semantic_github_tests PRIVATE
            insight::canon GTest::gtest GTest::gtest_main)
        # This executable recompiles the module unit rather than linking the library, so it needs
        # the projection on its own include path and before its own scan (see the library above).
        target_include_directories(insight_semantic_github_tests PRIVATE ${GITHUB_GENERATED_DIR})
        add_dependencies(insight_semantic_github_tests insight_semantic_github_dialect_gen)
        target_compile_features(insight_semantic_github_tests PRIVATE cxx_std_23)
        set_target_properties(insight_semantic_github_tests PROPERTIES
            CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
        target_compile_options(insight_semantic_github_tests PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-ffp-contract=off>)
        include(GoogleTest)
        gtest_discover_tests(insight_semantic_github_tests)
    endif()
endif()
