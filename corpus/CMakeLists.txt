# invariant: insight_canon_corpus, a CUT TOOL (DN-142.D33): it builds the three test executables
# that hold canon's corpus gates and installs them, and only them, for the cut to restore and run.
# invariant: each executable is the target its own package's CMakeLists defines, configured here
# with its test switch forced ON, so the ship form builds it too; there is no second definition.
# invariant: none of their tests is registered here, since each package's own verdict judges them;
# this tree's only tests check that every stored binary still lists its declared corpus suites.
cmake_minimum_required(VERSION 3.28)

# note: the gate UUID canon's own root sets before project(): the targets below `import std`.
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "451f2fe2-a8a2-47c3-bc32-94786d8fc91b")

project(insight_canon_corpus LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# note: the repository root, which the recipe's export reproduces around corpus/.
get_filename_component(INSIGHT_CANON_CORPUS_REPO "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)

# invariant: normal variables, which each subproject's option() honours under CMP0077, so the
# ship form's BUILD_TESTING=OFF cannot switch the test targets off.
set(INSIGHT_CANON_BUILD_TESTS ON)
set(INSIGHT_CANON_REGISTER_TESTS OFF)
set(INSIGHT_SEMANTIC_JENKINS_BUILD_TESTS ON)
set(INSIGHT_SEMANTIC_JENKINS_REGISTER_TESTS OFF)
set(INSIGHT_SEMANTIC_GITLAB_BUILD_TESTS ON)
set(INSIGHT_SEMANTIC_GITLAB_REGISTER_TESTS OFF)

# note: core/ first, so the dialects' `if(NOT TARGET insight::canon)` guards take the in-tree canon.
add_subdirectory(${INSIGHT_CANON_CORPUS_REPO}/core core_build)
add_subdirectory(${INSIGHT_CANON_CORPUS_REPO}/semantic/jenkins jenkins_build)
add_subdirectory(${INSIGHT_CANON_CORPUS_REPO}/semantic/gitlab gitlab_build)

# invariant: one row per executable, `<package directory>|<test target>`: the binary is red unless it
# lists every suite that directory's CORPUS_SUITES declares, read from the variable itself.
set(INSIGHT_CANON_CORPUS_TOOLS
    "core|insight_canon_tests"
    "semantic/jenkins|insight_semantic_jenkins_tests"
    "semantic/gitlab|insight_semantic_gitlab_tests")

enable_testing()
set(INSIGHT_CANON_CORPUS_TARGETS)
foreach(tool_row IN LISTS INSIGHT_CANON_CORPUS_TOOLS)
    string(REPLACE "|" ";" tool_fields "${tool_row}")
    list(GET tool_fields 0 tool_dir)
    list(GET tool_fields 1 tool_target)
    get_directory_property(tool_suites DIRECTORY "${INSIGHT_CANON_CORPUS_REPO}/${tool_dir}"
                           DEFINITION CORPUS_SUITES)
    if(NOT TARGET ${tool_target} OR tool_suites STREQUAL "")
        message(FATAL_ERROR "insight_canon_corpus: ${tool_dir} defines no ${tool_target} target or "
                            "declares no CORPUS_SUITES — the cut tool would store nothing to run")
    endif()
    add_test(NAME ${tool_target}_lists_its_corpus_suites
             COMMAND ${CMAKE_COMMAND} -DSUBJECT=$<TARGET_FILE:${tool_target}> "-DSUITES=${tool_suites}"
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/corpus_suites_listed.cmake)
    list(APPEND INSIGHT_CANON_CORPUS_TARGETS ${tool_target})
endforeach()

# post: the package holds bin/insight_canon_tests, bin/insight_semantic_jenkins_tests and
# bin/insight_semantic_gitlab_tests, each classed run-from-cache, and nothing else.
include(GNUInstallDirs)
install(TARGETS ${INSIGHT_CANON_CORPUS_TARGETS}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT corpus_tools)
