###########################################################
#  insight_semantic_test_frameworks — test-framework file-location semantic package (ADR-17).
#  DATA only: one module interface unit (insight.semantic.test_frameworks) carrying location rule
#  rows. No code tier (the matching algorithm lives in canon core). No partitions.
###########################################################
cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(insight_semantic_test_frameworks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(INSIGHT_SEMANTIC_TESTFW_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})

if(NOT TARGET insight::canon)
    find_package(insight_canon REQUIRED CONFIG)
endif()

set(TESTFW_MODULE_UNITS ${CMAKE_CURRENT_SOURCE_DIR}/src/test_frameworks.cppm)

# The module interface unit is the sole translation unit (data-only package — no impl .cpp).
add_library(insight_semantic_test_frameworks STATIC)
target_sources(insight_semantic_test_frameworks
    PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
        FILES    ${TESTFW_MODULE_UNITS}
)
set_target_properties(insight_semantic_test_frameworks PROPERTIES EXPORT_NAME semantic_test_frameworks
    CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
target_compile_features(insight_semantic_test_frameworks PUBLIC cxx_std_23)
add_library(insight::semantic_test_frameworks ALIAS insight_semantic_test_frameworks)

target_link_libraries(insight_semantic_test_frameworks PUBLIC insight::canon)

target_compile_options(insight_semantic_test_frameworks PUBLIC
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-ffp-contract=off>
)

###########################################################
#  Install + cross-package module shipping (mirrors insight_semantic_github)
###########################################################
include(GNUInstallDirs)
install(TARGETS insight_semantic_test_frameworks
    EXPORT insight_semantic_test_frameworksTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILE_SET cxx_modules DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/insight_semantic_test_frameworks_modules
)
install(EXPORT insight_semantic_test_frameworksTargets
    NAMESPACE insight::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/insight_semantic_test_frameworks
)
export(EXPORT insight_semantic_test_frameworksTargets
    NAMESPACE insight::
    FILE ${CMAKE_CURRENT_BINARY_DIR}/insight_semantic_test_frameworksTargets.cmake)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/insight_semantic_test_frameworks-config.cmake
"include(CMakeFindDependencyMacro)
find_dependency(insight_canon)
include(\"\${CMAKE_CURRENT_LIST_DIR}/insight_semantic_test_frameworksTargets.cmake\")
")
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/insight_semantic_test_frameworks-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/insight_semantic_test_frameworks)

###########################################################
#  Tests (Kleio homes the package test suites)
###########################################################
if(INSIGHT_SEMANTIC_TESTFW_BUILD_TESTS)
    file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    if(TEST_SOURCES)
        include(CTest)
        find_package(GTest REQUIRED CONFIG)
        enable_testing()
        add_executable(insight_semantic_test_frameworks_tests ${TEST_SOURCES})
        target_sources(insight_semantic_test_frameworks_tests
            PRIVATE
                FILE_SET cxx_modules TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/src
                FILES    ${TESTFW_MODULE_UNITS})
        target_link_libraries(insight_semantic_test_frameworks_tests PRIVATE
            insight::canon GTest::gtest GTest::gtest_main)
        target_compile_features(insight_semantic_test_frameworks_tests PRIVATE cxx_std_23)
        set_target_properties(insight_semantic_test_frameworks_tests PROPERTIES
            CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
        target_compile_options(insight_semantic_test_frameworks_tests PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-ffp-contract=off>)
        include(GoogleTest)
        gtest_discover_tests(insight_semantic_test_frameworks_tests)
    endif()
endif()
