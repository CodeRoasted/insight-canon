# The arena loads libnuma at run time and never links it (DN-142.D16). The subject is an ELF
# executable carrying canon's arena; it fails on a `libnuma` NEEDED entry or on any libnuma symbol,
# a static `T numa_…` or a dynamic `U numa_…`. The pattern is the release licence gate's. The
# loader's dlsym names are string literals, never symbols, and canon's own names are C++-mangled,
# so neither can match. A probe that cannot run fails: an unread binary is never a pass.
if(NOT DEFINED SUBJECT OR NOT EXISTS "${SUBJECT}")
    message(FATAL_ERROR "never_links_libnuma: no SUBJECT executable (got '${SUBJECT}')")
endif()

find_program(READELF_EXECUTABLE readelf)
find_program(NM_EXECUTABLE nm)
if(NOT READELF_EXECUTABLE OR NOT NM_EXECUTABLE)
    message(FATAL_ERROR "never_links_libnuma: readelf or nm not found, so ${SUBJECT} was not read")
endif()

execute_process(COMMAND "${READELF_EXECUTABLE}" -d "${SUBJECT}"
    OUTPUT_VARIABLE dynamic_section RESULT_VARIABLE readelf_status ERROR_VARIABLE readelf_error)
if(NOT readelf_status EQUAL 0)
    message(FATAL_ERROR "never_links_libnuma: readelf -d exited ${readelf_status}: ${readelf_error}")
endif()
string(REGEX MATCHALL "\\(NEEDED\\)[^\n]*" needed_entries "${dynamic_section}")
list(LENGTH needed_entries needed_count)
if(needed_count EQUAL 0)
    message(FATAL_ERROR "never_links_libnuma: readelf -d listed no NEEDED entry for ${SUBJECT}, "
                        "so the parse broke rather than the binary being clean")
endif()
foreach(entry IN LISTS needed_entries)
    if(entry MATCHES "libnuma")
        message(FATAL_ERROR "never_links_libnuma: ${SUBJECT} links libnuma: ${entry}")
    endif()
endforeach()

execute_process(COMMAND "${NM_EXECUTABLE}" "${SUBJECT}"
    OUTPUT_VARIABLE static_symbols RESULT_VARIABLE nm_status ERROR_VARIABLE nm_error)
if(NOT nm_status EQUAL 0)
    message(FATAL_ERROR "never_links_libnuma: nm exited ${nm_status}: ${nm_error}")
endif()
execute_process(COMMAND "${NM_EXECUTABLE}" -D "${SUBJECT}"
    OUTPUT_VARIABLE dynamic_symbols RESULT_VARIABLE nm_dynamic_status ERROR_VARIABLE nm_dynamic_error)
if(NOT nm_dynamic_status EQUAL 0)
    message(FATAL_ERROR "never_links_libnuma: nm -D exited ${nm_dynamic_status}: ${nm_dynamic_error}")
endif()
string(REGEX MATCHALL "(^|[^A-Za-z0-9_])numa_(alloc|available|free|num_configured|preferred|set)[^\n]*"
    libnuma_symbols "${static_symbols}\n${dynamic_symbols}")
list(LENGTH libnuma_symbols libnuma_symbol_count)
if(libnuma_symbol_count GREATER 0)
    message(FATAL_ERROR "never_links_libnuma: ${SUBJECT} carries ${libnuma_symbol_count} libnuma "
                        "symbol(s): ${libnuma_symbols}")
endif()
message(STATUS "never_links_libnuma: ${SUBJECT}: ${needed_count} NEEDED entries, none libnuma; "
               "no libnuma symbol")
