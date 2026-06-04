###########################################################
#  insight_canon — core types · tokenization
#
#  Provides insight::canon, a single self-contained static library
#  that bundles:
#    • core         – shared types, logging, time utilities
#    • tokenization – format detection + Drain + arena → CanonicalEvent
###########################################################
cmake_minimum_required(VERSION 3.28)

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
#  Sources
###########################################################
file(GLOB_RECURSE API_HEADERS     CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/api/*.hpp")
file(GLOB_RECURSE PRIVATE_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
file(GLOB_RECURSE LIB_SOURCES     CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

###########################################################
#  Static library target
###########################################################
add_library(insight_canon ${API_HEADERS} ${PRIVATE_HEADERS} ${LIB_SOURCES})
set_target_properties(insight_canon PROPERTIES EXPORT_NAME canon)
add_library(insight::canon ALIAS insight_canon)

target_include_directories(insight_canon
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/api>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# fmt and spdlog appear in the public logging header; consumers must see
# their includes and link against them.
target_link_libraries(insight_canon
    PUBLIC
        fmt::fmt
        spdlog::spdlog
    PRIVATE
        simdjson::simdjson
)

###########################################################
#  Compile-time spdlog level elision
###########################################################
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(insight_canon PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
else()
    target_compile_definitions(insight_canon PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO)
endif()

###########################################################
#  Float determinism
###########################################################
# Deterministic-content / significance-gate paths require cross-machine
# bit-identity (technical_docs/architecture/insight_determinism_model.md, "Float
# Hardening"). Disable FMA contraction so the compiler never fuses `a*b+c` —
# fusion differs across compilers/CPUs and would break bit-identity. PUBLIC so
# consumers that compile det_math's header-only fixed-point math inherit the
# guarantee. SSE (not x87 80-bit) is the x86-64 ABI default and arm has no x87,
# so the register-width hazard needs no extra flag.
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
        message(WARNING
            "INSIGHT_CANON_ENABLE_NUMA=ON but libnuma was not found; "
            "building without NUMA support")
    endif()
endif()

###########################################################
#  Named-module wrapper (insight.canon) — §8.1 (1.5.0)
###########################################################
# Re-exports insight_canon's global-module public surface (5 namespaces) so
# downstream repos `import insight.canon` instead of #include "insight/…". The
# defs stay in src/*.cpp → the installed insight_canon.a is byte-identical; the
# .cppm.o is the only added artifact. Links the lib PUBLIC (api/ includes,
# fmt/spdlog, -ffp-contract=off); cxx_std_23 PUBLIC is the §10.4 producer gotcha.
add_library(insight_canon_module STATIC)
target_sources(insight_canon_module
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/modules
        FILES    ${CMAKE_CURRENT_SOURCE_DIR}/modules/insight_canon.cppm
)
target_link_libraries(insight_canon_module PUBLIC insight_canon)
target_compile_features(insight_canon_module PUBLIC cxx_std_23)
add_library(insight::canon_module ALIAS insight_canon_module)

###########################################################
#  Install rules  (used by `conan create`)
###########################################################
include(GNUInstallDirs)
# Library + module wrapper in ONE export set → single file-set owner (§10.5/§10.8).
install(TARGETS insight_canon insight_canon_module
    EXPORT insight_canonTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILE_SET cxx_modules DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/insight_canon_modules
)
install(DIRECTORY api/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT insight_canonTargets
    NAMESPACE insight::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/insight_canon
)

###########################################################
#  Cross-package module shipping (Argos — §10.7/§10.9)
###########################################################
# Ship the package's OWN cmake config (carries FILE_SET CXX_MODULES); conan
# defers via cmake_find_mode=none. create → install(EXPORT); editable →
# BUILD-TREE export(). The config find_dependency()s canon's link deps.
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

        add_executable(insight_canon_tests ${TEST_SOURCES})
        target_link_libraries(insight_canon_tests PRIVATE
            insight_canon
            GTest::gtest
            GTest::gtest_main
        )
        target_include_directories(insight_canon_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
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
            insight_canon
            benchmark::benchmark
            spdlog::spdlog
        )
        target_include_directories(insight_canon_bench PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
        )
    endif()
endif()
