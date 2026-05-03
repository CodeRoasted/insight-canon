cmake_minimum_required(VERSION 3.28)

project(insight_canon_test_package LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(insight_canon REQUIRED CONFIG)
find_package(GTest REQUIRED CONFIG)

include(CTest)

add_executable(insight_canon_test_package test_package.cpp)
target_link_libraries(insight_canon_test_package PRIVATE
    insight::canon
    GTest::gtest
    GTest::gtest_main
)

enable_testing()
include(GoogleTest)
gtest_discover_tests(insight_canon_test_package)
