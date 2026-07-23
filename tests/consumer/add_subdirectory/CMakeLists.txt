cmake_minimum_required(VERSION 3.16)
project(AxiomTraceAddSubdirectoryConsumer LANGUAGES C)

if(NOT AXIOMTRACE_SOURCE)
    message(FATAL_ERROR "AXIOMTRACE_SOURCE is required")
endif()

add_subdirectory(${AXIOMTRACE_SOURCE} axiomtrace-build)

if(AXIOM_BUILD_TESTS OR AXIOM_BUILD_EXAMPLES)
    message(FATAL_ERROR "AxiomTrace subproject enabled tests or examples")
endif()

foreach(INTERNAL_CACHE_ENTRY AXIOM_PLATFORM_OPTIONS AXIOM_PORT_SOURCES)
    if(DEFINED CACHE{${INTERNAL_CACHE_ENTRY}})
        message(FATAL_ERROR
            "AxiomTrace leaked internal helper into parent cache: ${INTERNAL_CACHE_ENTRY}")
    endif()
endforeach()

add_executable(consumer ../main.c)
target_link_libraries(consumer PRIVATE AxiomTrace::axiomtrace)

get_target_property(CONSUMER_OPTIONS consumer COMPILE_OPTIONS)
if(CONSUMER_OPTIONS)
    message(FATAL_ERROR "AxiomTrace polluted consumer compile options: ${CONSUMER_OPTIONS}")
endif()
