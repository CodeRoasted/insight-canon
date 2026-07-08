###########################################################
#  insight_semantic_github — the GitHub Actions / Azure Pipelines dialect semantic package
#  (ADR 0024). One module interface unit (insight.semantic.github) + one impl unit — NO partitions
#  (the cross-package BMI closure must not deepen — ADR 0024 §7 / the gcc-15 hazard).
###########################################################
cmake_minimum_required(VERSION 3.28)

set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(insight_semantic_github LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(INSIGHT_SEMANTIC_GITHUB_BUILD_TESTS "Build unit tests" ${PROJECT_IS_TOP_LEVEL})

# Cross-package dep: guarded so an umbrella build's single resolution stays authoritative.
if(NOT TARGET insight::canon)
    find_package(insight_canon REQUIRED CONFIG)
endif()

set(GITHUB_MODULE_UNITS  ${CMAKE_CURRENT_SOURCE_DIR}/src/github.cppm)
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

# Determinism: the dialect recognizers feed deterministic content → no FMA contraction (mirrors canon).
target_compile_options(insight_semantic_github PUBLIC
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-ffp-contract=off>
)

###########################################################
#  Install + cross-package module shipping (mirrors insight_canon §10.7/§10.9)
###########################################################
include(GNUInstallDirs)
install(TARGETS insight_semantic_github
    EXPORT insight_semantic_githubTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILE_SET cxx_modules DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/insight_semantic_github_modules
)
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
        target_compile_features(insight_semantic_github_tests PRIVATE cxx_std_23)
        set_target_properties(insight_semantic_github_tests PROPERTIES
            CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)
        target_compile_options(insight_semantic_github_tests PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-ffp-contract=off>)
        include(GoogleTest)
        gtest_discover_tests(insight_semantic_github_tests)
    endif()
endif()
