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
# OFF by default: libnuma is LGPL-2.1, so NUMA is opt-in (conanfile.py drives this
# from the `with_numa` option — default False — and a raw cmake build stays safe).
option(INSIGHT_CANON_ENABLE_NUMA  "Enable NUMA-aware arena allocation (libnuma, LGPL-2.1, vendored via conan)" OFF)

# Arena reset poison — a DEBUG-BUILD INSTRUMENT (arena_allocator.cpp explains what and why).
# Default ON for Debug, which is also what the ASan/MSan profiles set (malf/profiles/*-asan,
# *-msan carry build_type=Debug), so `malf test --asan` gets it without a second switch.
#
# Keyed on the BUILD TYPE, deliberately not on a bare NDEBUG: NDEBUG is a property of whatever
# flags happened to reach the compiler, so gating on it makes the instrument's presence an
# accident that nobody declares and nobody can read back. An option with a build-type default is a
# DECLARED instrument — it can be forced on in a release build to reproduce a report, and forced
# off in a debug build to isolate a perf question, and either choice is visible in the cache.
# A TRI-STATE re-derived on EVERY configure, deliberately NOT an option().
#
# option() seeds the cache once and never re-defaults — and malf reconfigures a build tree IN
# PLACE when the build type changes, reusing the same directory. So a build-type-defaulted
# option() is STICKY: a directory first configured Debug keeps the poison ON through every later
# Release build of that same directory. That is not hypothetical — it was measured here, and the
# consequence is a memset live on the hot path of a RELEASE canon, which silently pollutes any
# benchmark run against that tree. A perf instrument that turns itself on in release is worse than
# no instrument.
#
# AUTO (the default) is recomputed from CMAKE_BUILD_TYPE at each configure, so an in-place build
# type flip flips the instrument with it. An explicit ON/OFF still wins, so a release build can be
# poisoned deliberately to reproduce a report, and a debug build can be un-poisoned to isolate a
# perf question — both visible in the cache rather than inferred from flags.
unset(INSIGHT_CANON_ARENA_POISON CACHE) # purge the sticky BOOL this replaces (see above)
set(INSIGHT_CANON_ARENA_POISON_MODE AUTO CACHE STRING
    "Arena reset-poison instrument: ON | OFF | AUTO (AUTO = ON for Debug builds only)")
set_property(CACHE INSIGHT_CANON_ARENA_POISON_MODE PROPERTY STRINGS AUTO ON OFF)
if(INSIGHT_CANON_ARENA_POISON_MODE STREQUAL "AUTO")
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(INSIGHT_CANON_ARENA_POISON ON)
    else()
        set(INSIGHT_CANON_ARENA_POISON OFF)
    endif()
else()
    set(INSIGHT_CANON_ARENA_POISON ${INSIGHT_CANON_ARENA_POISON_MODE})
endif()

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
    # insight.canon.spi — the provider contract (ADR-17.D8): installed + public, but NOT
    # re-exported by the facade (canon.cppm). A semantic package imports spi + api; consumers of
    # insight.canon never see it.
    ${CMAKE_CURRENT_SOURCE_DIR}/api/canon.spi.cppm
    # insight.canon.transport — the transport vocabulary (ADR-23.D3): the canon-shipped transform
    # catalogue, IngestDeclaration, and the stream-scoped peel. Installed + public; re-exported by
    # the facade. ORTHOGONAL to the semantic packages by construction (ADR-23.D3) — imports api only, and
    # nothing in it may reach the tokenizer's identity path (ADR-23.D4). Listed before compose, which
    # imports it to fold the catalogue version + rows into semantic_identity.
    ${CMAKE_CURRENT_SOURCE_DIR}/api/canon.transport.cppm
    # insight.canon.compose — static composition (ADR-17.D2 / ADR-17.D3): compose()/ComposedSemantics.
    # Installed + public; re-exported by the facade so `import insight.canon;` yields it alongside
    # Tokenizer. Imports spi (plain — the provider surface stays off the consumer default path).
    ${CMAKE_CURRENT_SOURCE_DIR}/api/canon.compose.cppm
    ${CMAKE_CURRENT_SOURCE_DIR}/api/canon.cppm
    # insight.canon.conformance — the package-agnostic CONFORMANCE KIT (ADR-17.D8, SRC-SP-2): the
    # canon-shipped harness every semantic package must pass. Installed + public (an external package
    # author runs the identical gate), NOT re-exported by the facade — a package's test target imports it
    # explicitly. Imports the facade + spi; pure (no gtest, no global operator new in the library).
    ${CMAKE_CURRENT_SOURCE_DIR}/src/conformance/canon.conformance.cppm
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
# PRIVATE, not PUBLIC: SPDLOG_ACTIVE_LEVEL is a spdlog GLOBAL macro and canon's own build-time
# elision level — it must NOT ride canon's api onto consumers' command lines (it collided with
# logcraft's own SPDLOG_ACTIVE_LEVEL, and exposing a config macro through an api is wrong). The only
# api-level consumer of it (the kDebugLogsEnabled constexpr) was moved into the build-only tokenizer/
# parser impl units, so nothing on the public insight.canon.api surface references it anymore.
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(insight_canon PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
else()
    target_compile_definitions(insight_canon PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO)
endif()

# PRIVATE like the level above, and for a stronger reason: this switch changes canon's BEHAVIOUR,
# so it must be decided once, where reset() is compiled. Riding it onto consumers would let a
# consumer's own compile of the api module disagree with the linked implementation. The runtime
# `arena_poisons_on_reset()` exists so downstream can still read the answer truthfully.
if(INSIGHT_CANON_ARENA_POISON)
    target_compile_definitions(insight_canon PRIVATE INSIGHT_CANON_ARENA_POISON)
    message(STATUS "insight_canon: arena reset-poison instrument ENABLED (debug instrument; release builds rewind)")
endif()

###########################################################
#  Float determinism — det_math (header-only inline in the api module) requires cross-machine
#  bit-identity. Disable FMA contraction so `a*b+c` is never fused. PUBLIC so consumers compiling
#  det_math inherit it. (bibles/determinism_model.md "Float Hardening".)
###########################################################
target_compile_options(insight_canon PUBLIC
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-ffp-contract=off>
)

###########################################################
#  Enumerator totality — the mechanism `dual()` (api/canon.spi.cppm) and the conformance
#  diagnostic renderers (src/conformance/canon.conformance.cppm) both DECLARE they stand on.
#  Those switches carry no `default:` label on purpose: a default is what silences the diagnostic
#  and turns a forgotten enumerator into a wrong ANSWER — a writer derived as PayloadEmit::None,
#  a manifest-diff printing "?" where a grammar shape should be named. Until this line existed the
#  promise was prose only: -Wswitch rides -Wall, canon sets neither, so nothing enforced it.
#  PRIVATE, not PUBLIC: it governs canon's own translation units. Pushing it onto consumers is a
#  workspace-wide posture decision and is not this target's to make.
#  MSVC: C4062 is the same diagnostic (unhandled enumerator, no default label). It is off at every
#  warning level, so it has to be named to exist at all. C4061 is the wrong number — it also fires
#  when a default label IS present, which is every deliberate partial switch in the tree.
###########################################################
set(INSIGHT_CANON_SWITCH_TOTALITY
    $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Werror=switch>
    $<$<CXX_COMPILER_ID:MSVC>:/we4062>)
target_compile_options(insight_canon PRIVATE ${INSIGHT_CANON_SWITCH_TOTALITY})

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
#  Measurement instruments (tools/) — built on the package's own compile surface
###########################################################
# The F13 masker-cardinality re-measure (DN-18.D1 § 3.3): a CLI over the PUBLIC
# facade, deliberately NOT a test — its population is operator-mounted, so it reports and declares
# rather than asserts. Built unconditionally so it can never rot unseen (the malf one-compile-surface
# rule), linked exactly as an external consumer would link.
add_executable(f13_cardinality_measure tools/f13_cardinality_measure.cpp)
target_link_libraries(f13_cardinality_measure PRIVATE insight::canon)
target_compile_features(f13_cardinality_measure PRIVATE cxx_std_23)
set_target_properties(f13_cardinality_measure PROPERTIES CXX_SCAN_FOR_MODULES ON CXX_MODULE_STD ON)

###########################################################
#  Tests (unit + regression)
###########################################################
if(INSIGHT_CANON_BUILD_TESTS)
    # Per-domain test mirror (§11.9.11): tests/<domain>/*.cpp 1:1 with the src/ domains
    # (math/arena/utils are api-homed domains).
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
            $<IF:$<CONFIG:Debug>,SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE,SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO>)
        # The test target RECOMPILES arena_allocator.cpp (white-box, above), so it needs the switch
        # in its own right — inheriting the library's PRIVATE define is not a thing. Without this
        # the arena's own instrument test would read `arena_poisons_on_reset() == false` inside a
        # poisoning build and skip itself: the instrument would ship untested.
        if(INSIGHT_CANON_ARENA_POISON)
            target_compile_definitions(insight_canon_tests PRIVATE INSIGHT_CANON_ARENA_POISON)
        endif()
        # The test target COMPILES api/canon.spi.cppm and the conformance module in its own right
        # (white-box, like arena_allocator.cpp above), so the totality option has to be repeated
        # here — a PRIVATE option on insight_canon does not reach this target's copy of those TUs.
        target_compile_options(insight_canon_tests PRIVATE
            $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-ffp-contract=off>
            ${INSIGHT_CANON_SWITCH_TOTALITY})
        include(GoogleTest)
        gtest_discover_tests(insight_canon_tests)
    endif()
endif()

# Benchmarks moved to the sibling `benchmarks/` leaf package (insight_canon_bench, ADR-17.D8 · SRC-SP-5):
# the composed perf gate links the semantic packages, which the core build must
# never do (SRC-SP-1 / R1
# dependency arrow). The bench consumes canon externally (the det_proof shape), so the white-box
# insight.canon.bench aggregate is retired with it.
