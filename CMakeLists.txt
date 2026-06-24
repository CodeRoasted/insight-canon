###########################################################
#  insight_canon — core types · tokenization (1.5.2 domain-decomposed named modules)
###########################################################
cmake_minimum_required(VERSION 3.28)

# 1.5.1 unwrap: the pure module does `import std` → the experimental import-std gate UUID MUST be set
# before project()/CXX-enable (451f2fe2-… = CMake 4.3.1's CMAKE_EXPERIMENTAL_CXX_IMPORT_STD).
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(insight_canon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(INSIGHT_CANON_BUILD_TESTS  "Build unit and regression tests" ${PROJECT_IS_TOP_LEVEL})
option(INSIGHT_CANON_BUILD_BENCHMARKS "Build micro-benchmarks" ${PROJECT_IS_TOP_LEVEL})
# OFF by default: libnuma is LGPL-2.1, so NUMA is opt-in (conanfile.py drives this
# from the `with_numa` option — default False — and a raw cmake build stays safe).
option(INSIGHT_CANON_ENABLE_NUMA  "Enable NUMA-aware arena allocation (libnuma, LGPL-2.1, vendored via conan)" OFF)

###########################################################
#  External dependencies
###########################################################
find_package(fmt      REQUIRED CONFIG)
find_package(spdlog   REQUIRED CONFIG)
find_package(simdjson REQUIRED CONFIG)
find_package(picosha2 REQUIRED CONFIG)

###########################################################
#  Sources (1.5.2 domain decomposition, §11.9.11)
#  - PUBLIC module interface units (installed): internal (src/) + api + facade (api/).
#  - PRIVATE (sealed, build-only) detail shards (src/<domain>/): scan ◀ strategy ◀ parse; mask a
#    leaf over api. Never installed — external consumers cannot import insight.canon.detail.*.
#  - Module impl units (regular sources, scanned): src/**/*.cpp attach via
#    `module insight.canon{.api|.detail.<domain>};` (+ the facade impl unit tokenizer_engine.cpp).
#  - Textual layer: api/utils/log_macros.hpp (the §11.9 macro layer) and
#    src/strategy/simdjson_scratch.hpp (TU-local simdjson entities).
###########################################################
set(CANON_PUBLIC_MODULE_UNITS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/canon.internal.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/api/canon.api.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/api/canon.cppm
)
set(CANON_DETAIL_MODULE_UNITS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/scan/canon.detail.scan.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strategy/canon.detail.strategy.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/mask/canon.detail.mask.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/parse/canon.detail.parse.cppm
)
file(GLOB_RECURSE CANON_IMPL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

###########################################################
#  Module-bearing static library
###########################################################
# §11.9.7/§11.9.11 install seal: the PUBLIC file set (internal/api/facade) is installed; the
# PRIVATE cxx_modules_detail set (the detail.<domain> shards) is build-only — legal because no
# external target imports the lib's private modules (tests/benchmarks are module MEMBERS below).
add_library(insight_canon STATIC ${CANON_IMPL_SOURCES})
target_sources(insight_canon
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/api ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES    ${CANON_PUBLIC_MODULE_UNITS}
    PRIVATE
        FILE_SET cxx_modules_detail TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES    ${CANON_DETAIL_MODULE_UNITS}
)
set_target_properties(insight_canon PROPERTIES EXPORT_NAME canon
    CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
target_compile_features(insight_canon PUBLIC cxx_std_23)
add_library(insight::canon ALIAS insight_canon)

# src/ on the private include path so impl units resolve the textual macro layer
# (#include "insight/utils/log_macros.hpp") and log_macros' fmt/spdlog includes.
target_include_directories(insight_canon
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api> $<INSTALL_INTERFACE:include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
# Public textual macro layer: INSIGHT_LOG_* live in a header (macros can't be exported from a module);
# downstream consumers `#include <insight/utils/log_macros.hpp>` + `import insight.canon` (accessors).

# fmt + spdlog appear in the public logger accessor signatures (api module GMF); consumers
# must see their includes + link them. simdjson is impl-only (JSON strategies in src/).
target_link_libraries(insight_canon
    PUBLIC  fmt::fmt spdlog::spdlog
    PRIVATE simdjson::simdjson picosha2::picosha2)

###########################################################
#  Compile-time spdlog level elision
###########################################################
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(insight_canon PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
else()
    target_compile_definitions(insight_canon PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO)
endif()

###########################################################
#  Float determinism — det_math (header-only inline in the api module) requires cross-machine
#  bit-identity. Disable FMA contraction so `a*b+c` is never fused. PUBLIC so consumers compiling
#  det_math inherit it. (insight_determinism_model.md "Float Hardening".)
###########################################################
target_compile_options(insight_canon PUBLIC
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-ffp-contract=off>
)

###########################################################
#  Optional NUMA support (arena allocator)
###########################################################
# libnuma is vendored via conan (libnuma/2.0.19 on Linux, mirrors logcraft) — hermetic, so
# CI/release builds get NUMA without a system libnuma-dev. numa.h is included only under
# INSIGHT_HAS_NUMA (arena_allocator.cpp), so a non-Linux / NUMA-off build needs no libnuma.
if(INSIGHT_CANON_ENABLE_NUMA)
    find_package(libnuma CONFIG QUIET)
    if(libnuma_FOUND)
        target_compile_definitions(insight_canon PRIVATE INSIGHT_HAS_NUMA)
        target_link_libraries(insight_canon PRIVATE libnuma::libnuma)
        message(STATUS "libnuma found via conan (libnuma::libnuma) — NUMA-aware arena enabled")
    else()
        message(STATUS "INSIGHT_CANON_ENABLE_NUMA=ON but libnuma not found — arena uses the portable allocator")
    endif()
endif()

###########################################################
#  Install rules  (used by `conan create`)
###########################################################
include(GNUInstallDirs)
# Ships the module file set only — no install(DIRECTORY api/) (textual headers retired in the unwrap).
install(TARGETS insight_canon
    EXPORT insight_canonTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILE_SET cxx_modules DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/insight_canon_modules
)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/api/utils/log_macros.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/utils)
# det_int128.hpp is the OTHER textual GMF dependency: canon.api.cppm does `#include
# "det/det_int128.hpp"`, so the installed module copy needs it alongside, at the same relative
# path (consumer include root = INCLUDEDIR → `det/…` resolves). Same shipping shape as log_macros;
# without it the test_package / any downstream module rebuild fails "Cannot open det/det_int128.hpp".
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/api/det/det_int128.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/det)
install(EXPORT insight_canonTargets
    NAMESPACE insight::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/insight_canon
)

###########################################################
#  Cross-package module shipping (Argos — §10.7/§10.9)
###########################################################
# Ship the package's OWN cmake config (carries FILE_SET CXX_MODULES); conan defers via
# cmake_find_mode=none. create → install(EXPORT); editable → BUILD-TREE export().
export(EXPORT insight_canonTargets
    NAMESPACE insight::
    FILE ${CMAKE_CURRENT_BINARY_DIR}/insight_canonTargets.cmake)
# canon is a STATIC lib, so even PRIVATE link deps (libnuma) appear in its Targets.cmake and
# must resolve in the consumer — add find_dependency(libnuma) ONLY when it was actually linked.
set(_canon_find_deps "find_dependency(fmt)
find_dependency(spdlog)
find_dependency(simdjson)
find_dependency(picosha2)
")
if(INSIGHT_CANON_ENABLE_NUMA AND libnuma_FOUND)
    string(APPEND _canon_find_deps "find_dependency(libnuma)\n")
endif()
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/insight_canon-config.cmake
"include(CMakeFindDependencyMacro)
${_canon_find_deps}include(\"\${CMAKE_CURRENT_LIST_DIR}/insight_canonTargets.cmake\")
")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/insight_canon-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/insight_canon)

###########################################################
#  Tests (unit + regression)
###########################################################
if(INSIGHT_CANON_BUILD_TESTS)
    # Per-domain test mirror (§11.9.11): tests/<domain>/*.cpp 1:1 with the src/ domains
    # (math/arena/utils are api-homed domains; regression holds the loghub corpus suite).
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
    )
    if(TEST_SOURCES)
        include(CTest)
        find_package(GTest REQUIRED CONFIG)
        enable_testing()

        # §11.9.7 white-box tests are MODULE MEMBERS: the test target RECOMPILES the whole module
        # (interface units + impl .cpp) + the test TUs, so a test may import the sealed
        # insight.canon.detail.* shards (via the insight.canon.test aggregate) WITHOUT the lib
        # exporting them. CXX_MODULE_STD ON: the recompiled module is built against the std module,
        # so the test TUs provide the same one. -ffp-contract=off mirrors the lib (det_math is
        # recompiled here).
        add_executable(insight_canon_tests ${TEST_SOURCES} ${CANON_IMPL_SOURCES})
        target_sources(insight_canon_tests
            PRIVATE
                FILE_SET cxx_modules TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/api ${CMAKE_CURRENT_SOURCE_DIR}/src
                         ${CMAKE_CURRENT_SOURCE_DIR}/tests
                FILES    ${CANON_PUBLIC_MODULE_UNITS} ${CANON_DETAIL_MODULE_UNITS}
                         tests/canon.test.cppm)
        target_link_libraries(insight_canon_tests PRIVATE
            fmt::fmt spdlog::spdlog simdjson::simdjson picosha2::picosha2
            GTest::gtest GTest::gtest_main)
        target_include_directories(insight_canon_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/api ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_features(insight_canon_tests PRIVATE cxx_std_23)
        set_target_properties(insight_canon_tests PROPERTIES CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
        target_compile_definitions(insight_canon_tests PRIVATE
            $<IF:$<CONFIG:Debug>,SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE,SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>
            # The committed LogHub smoke slice (ADR 0016 §5) — absolute source path so the
            # regression suite resolves it independent of the CTest CWD (never data/logs/).
            LOGHUB_SLICE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/corpora/loghub/slice")
        target_compile_options(insight_canon_tests PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-ffp-contract=off>)
        include(GoogleTest)
        gtest_discover_tests(insight_canon_tests)
    endif()
endif()

###########################################################
#  Benchmarks
###########################################################
if(INSIGHT_CANON_BUILD_BENCHMARKS)
    file(GLOB_RECURSE BENCH_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/benchmarks/*.cpp"
    )
    if(BENCH_SOURCES)
        find_package(benchmark REQUIRED CONFIG)
        # §11.9.7/§11.9.11: like the white-box tests, benchmarks are MODULE MEMBERS, not consumers —
        # the bench target RECOMPILES the whole module (interface + detail shards + impl .cpp) + the
        # bench TUs, so a bench may import the sealed insight.canon.detail.* shards (via the
        # insight.canon.bench aggregate) WITHOUT the lib exporting them. Deliberately does NOT link
        # insight_canon (the module is rebuilt here); pulls the lib's deps directly. NUMA mirrors
        # the lib's configuration so the benched arena is the shipped arena.
        add_executable(insight_canon_bench ${BENCH_SOURCES} ${CANON_IMPL_SOURCES})
        target_sources(insight_canon_bench
            PRIVATE
                FILE_SET cxx_modules TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/api ${CMAKE_CURRENT_SOURCE_DIR}/src
                         ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks
                FILES    ${CANON_PUBLIC_MODULE_UNITS} ${CANON_DETAIL_MODULE_UNITS}
                         benchmarks/canon.bench.cppm)
        target_link_libraries(insight_canon_bench PRIVATE
            fmt::fmt spdlog::spdlog simdjson::simdjson picosha2::picosha2 benchmark::benchmark)
        if(INSIGHT_CANON_ENABLE_NUMA AND libnuma_FOUND)
            target_compile_definitions(insight_canon_bench PRIVATE INSIGHT_HAS_NUMA)
            target_link_libraries(insight_canon_bench PRIVATE libnuma::libnuma)
        endif()
        target_include_directories(insight_canon_bench PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/api ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_features(insight_canon_bench PRIVATE cxx_std_23)
        set_target_properties(insight_canon_bench PROPERTIES CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
        target_compile_definitions(insight_canon_bench PRIVATE
            $<IF:$<CONFIG:Debug>,SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE,SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>)
        target_compile_options(insight_canon_bench PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-ffp-contract=off>)
    endif()
endif()
