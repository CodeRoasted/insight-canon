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
#  Install rules  (used by `conan create`)
###########################################################
include(GNUInstallDirs)
install(TARGETS insight_canon
    EXPORT insight_canonTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY api/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT insight_canonTargets
    NAMESPACE insight::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/insight_canon
)

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
        find_package(GTest         REQUIRED CONFIG)
        find_package(nlohmann_json REQUIRED CONFIG)
        enable_testing()

        add_executable(insight_canon_tests ${TEST_SOURCES})
        target_link_libraries(insight_canon_tests PRIVATE
            insight_canon
            nlohmann_json::nlohmann_json
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
