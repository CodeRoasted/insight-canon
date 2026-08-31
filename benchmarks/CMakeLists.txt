###########################################################
#  insight_canon_bench — the composed benchmark harness (ADR-17.D8 · SRC-SP-5)
#
#  Consumes canon + the two semantic packages as EXTERNAL module libraries (the det_proof /
#  test_package consumption shape — not a module-member rebuild), so the measured object is the
#  shipped library build. Two arms in one binary: BM_TokenizationThroughput composes the real
#  vocabulary set; the Degenerate arm composes {}. Their delta on a non-dialect corpus is what
#  measures SRC-SP-5 — see canon.compose.cppm for the contract.
###########################################################
cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(insight_canon_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Cross-package deps: guarded so an umbrella build's single resolution stays authoritative.
if(NOT TARGET insight::canon)
    find_package(insight_canon REQUIRED CONFIG)
endif()
if(NOT TARGET insight::semantic_github)
    find_package(insight_semantic_github REQUIRED CONFIG)
endif()
if(NOT TARGET insight::semantic_gitlab)
    find_package(insight_semantic_gitlab REQUIRED CONFIG)
endif()
if(NOT TARGET insight::semantic_jenkins)
    find_package(insight_semantic_jenkins REQUIRED CONFIG)
endif()
if(NOT TARGET insight::semantic_test_frameworks)
    find_package(insight_semantic_test_frameworks REQUIRED CONFIG)
endif()
find_package(benchmark REQUIRED CONFIG)

add_executable(insight_canon_bench
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bench_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bench_tokenization.cpp)
target_link_libraries(insight_canon_bench PRIVATE
    insight::canon insight::semantic_github insight::semantic_gitlab insight::semantic_jenkins
    insight::semantic_test_frameworks
    benchmark::benchmark)
target_compile_features(insight_canon_bench PRIVATE cxx_std_23)
set_target_properties(insight_canon_bench PROPERTIES CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)

include(GNUInstallDirs)
install(TARGETS insight_canon_bench RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
