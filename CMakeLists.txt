###########################################################
#  insight_canon — core types · tokenization (1.5.1 pure named module)
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
option(INSIGHT_CANON_ENABLE_NUMA  "Enable optional NUMA-aware arena allocation (libnuma)" OFF)

###########################################################
#  External dependencies
###########################################################
find_package(fmt      REQUIRED CONFIG)
find_package(spdlog   REQUIRED CONFIG)
find_package(simdjson REQUIRED CONFIG)

###########################################################
#  Sources (1.5.1 unwrap)
#  - Module interface units (FILE_SET cxx_modules): internal + api + facade (api/) + detail (src/).
#  - Module impl units (regular sources, scanned): src/**/*.cpp attach via `module insight.canon.{api|detail};`.
#  - Textual layer: only src/insight/utils/log_macros.hpp survives (the §11.9 macro layer).
###########################################################
set(CANON_MODULE_UNITS
    ${CMAKE_CURRENT_SOURCE_DIR}/api/insight/canon.internal.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/api/insight/canon.api.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/api/insight/canon.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/src/insight/canon.detail.cppm
)
file(GLOB_RECURSE CANON_IMPL_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

###########################################################
#  Module-bearing static library
###########################################################
add_library(insight_canon STATIC ${CANON_IMPL_SOURCES})
target_sources(insight_canon
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/api ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES    ${CANON_MODULE_UNITS}
)
set_target_properties(insight_canon PROPERTIES EXPORT_NAME canon
    CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
target_compile_features(insight_canon PUBLIC cxx_std_23)
add_library(insight::canon ALIAS insight_canon)
# 1.5.1: the separate _module wrapper is retired (the module lives on insight_canon itself).
add_library(insight::canon_module ALIAS insight_canon)

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
    PRIVATE simdjson::simdjson)

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
if(INSIGHT_CANON_ENABLE_NUMA)
    find_path(NUMA_INCLUDE_DIR numa.h)
    find_library(NUMA_LIBRARY numa)
    if(NUMA_INCLUDE_DIR AND NUMA_LIBRARY)
        target_compile_definitions(insight_canon PRIVATE INSIGHT_HAS_NUMA)
        target_include_directories(insight_canon PRIVATE "${NUMA_INCLUDE_DIR}")
        target_link_libraries(insight_canon PRIVATE "${NUMA_LIBRARY}")
    else()
        message(WARNING "INSIGHT_CANON_ENABLE_NUMA=ON but libnuma was not found; building without NUMA support")
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
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/api/insight/utils/log_macros.hpp
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/insight/utils)
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
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/insight_canon-config.cmake
"include(CMakeFindDependencyMacro)
find_dependency(fmt)
find_dependency(spdlog)
find_dependency(simdjson)
include(\"\${CMAKE_CURRENT_LIST_DIR}/insight_canonTargets.cmake\")
")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/insight_canon-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/insight_canon)

###########################################################
#  Tests (unit + regression)
###########################################################
if(INSIGHT_CANON_BUILD_TESTS)
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/regression/*.cpp"
    )
    if(TEST_SOURCES)
        include(CTest)
        find_package(GTest REQUIRED CONFIG)
        enable_testing()

        # §11.9.6 white-box tests are MODULE MEMBERS: the test target RECOMPILES the whole module
        # (interface units + impl .cpp) + the test TUs, so a test may `import insight.canon.detail`
        # (the sealed internals — drain/format_detector/strategies) WITHOUT the lib exporting them.
        # CXX_MODULE_STD ON: the recompiled module is built against the std module, so the test TUs
        # provide the same one. -ffp-contract=off mirrors the lib (det_math is recompiled here).
        add_executable(insight_canon_tests ${TEST_SOURCES} ${CANON_IMPL_SOURCES})
        target_sources(insight_canon_tests
            PRIVATE
                FILE_SET cxx_modules TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/api ${CMAKE_CURRENT_SOURCE_DIR}/src
                FILES    ${CANON_MODULE_UNITS})
        target_link_libraries(insight_canon_tests PRIVATE
            fmt::fmt spdlog::spdlog simdjson::simdjson
            GTest::gtest GTest::gtest_main)
        target_include_directories(insight_canon_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/api ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_features(insight_canon_tests PRIVATE cxx_std_23)
        set_target_properties(insight_canon_tests PROPERTIES CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
        target_compile_definitions(insight_canon_tests PRIVATE
            $<IF:$<CONFIG:Debug>,SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE,SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>)
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
        add_executable(insight_canon_bench ${BENCH_SOURCES})
        target_link_libraries(insight_canon_bench PRIVATE
            insight_canon benchmark::benchmark spdlog::spdlog)
        target_include_directories(insight_canon_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_features(insight_canon_bench PRIVATE cxx_std_23)
        set_target_properties(insight_canon_bench PROPERTIES CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
    endif()
endif()
