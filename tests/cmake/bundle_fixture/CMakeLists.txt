cmake_minimum_required(VERSION 3.16)
project(axiomtrace_bundle_fixture LANGUAGES C)

if(NOT AXIOMTRACE_ROOT)
    message(FATAL_ERROR "AXIOMTRACE_ROOT is required")
endif()

set(AXIOM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(AXIOM_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory("${AXIOMTRACE_ROOT}" axiomtrace-source)

add_executable(fixture main.c emitter_a.c emitter_b.c)
target_link_libraries(fixture PRIVATE axiomtrace)

set(AXIOM_FIXTURE_LOCATION_MODE "file_id" CACHE STRING "Location mode for the bundle integration fixture")
set_property(CACHE AXIOM_FIXTURE_LOCATION_MODE PROPERTY STRINGS file_id hash)

include("${AXIOMTRACE_ROOT}/cmake/AxiomTraceTools.cmake")
axiomtrace_add_bundle(
    TARGET fixture
    EVENTS "${CMAKE_CURRENT_SOURCE_DIR}/events.json"
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/axiomtrace-bundle"
    LOCATION_MODE "${AXIOM_FIXTURE_LOCATION_MODE}"
    FIRMWARE_NAME fixture
    FIRMWARE_VERSION 0.7.0
)
