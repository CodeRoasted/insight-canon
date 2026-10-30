# pre: SUBJECT names a googletest executable; SUITES is a gtest filter of `<Suite>.*` entries
# joined by `:`, the CORPUS_SUITES the executable's own CMakeLists declares.
# post: fails unless SUITES names at least one suite and `SUBJECT --gtest_list_tests` lists every
# one of them, printing each missing suite and the suites the binary does list.
if(NOT DEFINED SUBJECT OR NOT DEFINED SUITES OR SUITES STREQUAL "")
    message(FATAL_ERROR "corpus_suites_listed: SUBJECT and a non-empty SUITES are required "
                        "(SUBJECT='${SUBJECT}', SUITES='${SUITES}')")
endif()

execute_process(COMMAND "${SUBJECT}" --gtest_list_tests
                RESULT_VARIABLE listing_status
                OUTPUT_VARIABLE listing
                ERROR_VARIABLE listing_errors)
if(NOT listing_status EQUAL 0)
    message(FATAL_ERROR "corpus_suites_listed: `${SUBJECT} --gtest_list_tests` exited "
                        "${listing_status}: ${listing_errors}")
endif()

# note: a suite heads its block on a line of its own, `<Suite>.`, a typed suite adding a comment.
string(REGEX MATCHALL "\n[A-Za-z_][A-Za-z0-9_/]*\\." listed_heads "\n${listing}")
list(LENGTH listed_heads listed_count)

string(REPLACE ":" ";" declared_entries "${SUITES}")
set(declared_count 0)
set(missing_suites)
foreach(entry IN LISTS declared_entries)
    if(NOT entry MATCHES "^([A-Za-z_][A-Za-z0-9_]*)\\.\\*$")
        message(FATAL_ERROR "corpus_suites_listed: `${entry}` in SUITES is not a whole-suite "
                            "`<Suite>.*` entry, so this check cannot judge it")
    endif()
    set(suite "${CMAKE_MATCH_1}")
    math(EXPR declared_count "${declared_count} + 1")
    if(NOT "\n${listing}" MATCHES "\n${suite}\\.[\n ]")
        list(APPEND missing_suites "${suite}")
    endif()
endforeach()

if(missing_suites)
    string(REPLACE "\n" " " listed_text "${listed_heads}")
    message(FATAL_ERROR "corpus_suites_listed: ${SUBJECT} lists ${listed_count} suites and is "
                        "missing declared corpus suite(s): ${missing_suites}\n"
                        "listed suites:${listed_text}")
endif()
message(STATUS "corpus_suites_listed: ${SUBJECT} lists all ${declared_count} declared corpus "
               "suites (of ${listed_count} suites listed)")
