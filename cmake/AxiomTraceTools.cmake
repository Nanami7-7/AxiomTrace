# AxiomTrace host toolchain helpers.
#
# Usage:
#   include(cmake/AxiomTraceTools.cmake)
#   axiomtrace_add_bundle(
#       TARGET firmware
#       EVENTS ${CMAKE_SOURCE_DIR}/events.yaml
#       OUTPUT_DIR ${CMAKE_BINARY_DIR}/axiomtrace-bundle
#       LOCATION_MODE file_id
#   )

include(CMakeParseArguments)
set(AXIOMTRACE_TOOLS_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(axiomtrace_add_bundle)
    set(options LOCATION_FUNCTION)
    set(one_value_args TARGET EVENTS OUTPUT_DIR FIRMWARE_NAME FIRMWARE_VERSION LOCATION_MODE)
    cmake_parse_arguments(AXIOM_BUNDLE "${options}" "${one_value_args}" "" ${ARGN})

    if(NOT AXIOM_BUNDLE_TARGET)
        message(FATAL_ERROR "axiomtrace_add_bundle: TARGET is required")
    endif()
    if(NOT TARGET ${AXIOM_BUNDLE_TARGET})
        message(FATAL_ERROR "axiomtrace_add_bundle: target '${AXIOM_BUNDLE_TARGET}' does not exist")
    endif()
    if(NOT AXIOM_BUNDLE_EVENTS)
        message(FATAL_ERROR "axiomtrace_add_bundle: EVENTS is required")
    endif()
    if(NOT AXIOM_BUNDLE_OUTPUT_DIR)
        set(AXIOM_BUNDLE_OUTPUT_DIR "${CMAKE_BINARY_DIR}/axiomtrace-bundle")
    endif()
    if(NOT AXIOM_BUNDLE_LOCATION_MODE)
        set(AXIOM_BUNDLE_LOCATION_MODE "file_id")
    endif()
    if(NOT AXIOM_BUNDLE_LOCATION_MODE MATCHES "^(none|hash|file_id)$")
        message(FATAL_ERROR "axiomtrace_add_bundle: LOCATION_MODE must be none, hash, or file_id")
    endif()

    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    set(AXIOM_TOOLS_PYTHONPATH "${AXIOMTRACE_TOOLS_MODULE_DIR}/../tool/src")
    set(AXIOM_GENERATED_DIR "${AXIOM_BUNDLE_OUTPUT_DIR}/generated")
    set(AXIOM_SOURCE_ID_MAP "${AXIOM_GENERATED_DIR}/source_ids.json")
    set(AXIOM_GENERATED_SOURCE_MAP "${AXIOM_GENERATED_DIR}/source_map.json")
    set(AXIOM_COMPILE_DB "${CMAKE_BINARY_DIR}/compile_commands.json")
    set(AXIOM_BUNDLE_MANIFEST "${AXIOM_BUNDLE_OUTPUT_DIR}/manifest.json")
    set(AXIOM_BUNDLE_DICTIONARY "${AXIOM_BUNDLE_OUTPUT_DIR}/dictionary.json")
    set(AXIOM_GENERATED_EVENTS "${AXIOM_GENERATED_DIR}/axiom_events_generated.h")
    set(AXIOM_GENERATED_MODULES "${AXIOM_GENERATED_DIR}/axiom_modules_generated.h")
    set(AXIOM_GENERATED_METADATA_ID "${AXIOM_GENERATED_DIR}/axiom_metadata_id_generated.h")

    get_target_property(AXIOM_TARGET_SOURCES ${AXIOM_BUNDLE_TARGET} SOURCES)
    set(AXIOM_EVENT_SOURCES)
    foreach(AXIOM_SOURCE IN LISTS AXIOM_TARGET_SOURCES)
        if(NOT AXIOM_SOURCE MATCHES "^\\$<" AND AXIOM_SOURCE MATCHES "\\.[cC]$")
            get_filename_component(AXIOM_ABSOLUTE_SOURCE "${AXIOM_SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
            list(APPEND AXIOM_EVENT_SOURCES "${AXIOM_ABSOLUTE_SOURCE}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES AXIOM_EVENT_SOURCES)
    list(SORT AXIOM_EVENT_SOURCES)

    set(AXIOM_SOURCE_ID_CONTENT "{\n  \"files\": [")
    set(AXIOM_SOURCE_ID 1)
    foreach(AXIOM_SOURCE IN LISTS AXIOM_EVENT_SOURCES)
        file(TO_CMAKE_PATH "${AXIOM_SOURCE}" AXIOM_SOURCE_PATH)
        if(AXIOM_SOURCE_ID GREATER 1)
            string(APPEND AXIOM_SOURCE_ID_CONTENT ",")
        endif()
        string(APPEND AXIOM_SOURCE_ID_CONTENT "\n    {\"id\": ${AXIOM_SOURCE_ID}, \"path\": \"${AXIOM_SOURCE_PATH}\"}")
        if(AXIOM_BUNDLE_LOCATION_MODE STREQUAL "file_id")
            set_property(SOURCE "${AXIOM_SOURCE}" APPEND PROPERTY COMPILE_DEFINITIONS "AXIOM_SOURCE_FILE_ID=${AXIOM_SOURCE_ID}u")
        endif()
        math(EXPR AXIOM_SOURCE_ID "${AXIOM_SOURCE_ID} + 1")
    endforeach()
    string(APPEND AXIOM_SOURCE_ID_CONTENT "\n  ]\n}\n")
    file(MAKE_DIRECTORY "${AXIOM_GENERATED_DIR}")
    file(GENERATE OUTPUT "${AXIOM_SOURCE_ID_MAP}" CONTENT "${AXIOM_SOURCE_ID_CONTENT}")

    if(AXIOM_BUNDLE_LOCATION_MODE STREQUAL "file_id")
        target_compile_definitions(${AXIOM_BUNDLE_TARGET} PRIVATE AXIOM_CFG_LOCATION_MODE=2)
    elseif(AXIOM_BUNDLE_LOCATION_MODE STREQUAL "hash")
        target_compile_definitions(${AXIOM_BUNDLE_TARGET} PRIVATE AXIOM_CFG_LOCATION_MODE=1)
        if(AXIOM_BUNDLE_LOCATION_FUNCTION)
            target_compile_definitions(${AXIOM_BUNDLE_TARGET} PRIVATE AXIOM_CFG_LOCATION_FUNCTION=1)
        endif()
    else()
        target_compile_definitions(${AXIOM_BUNDLE_TARGET} PRIVATE AXIOM_CFG_LOCATION_MODE=0)
    endif()

    set(AXIOM_LOCATION_ARGS --location-mode "${AXIOM_BUNDLE_LOCATION_MODE}")
    if(AXIOM_BUNDLE_LOCATION_FUNCTION)
        list(APPEND AXIOM_LOCATION_ARGS --location-function)
    endif()

    add_custom_command(
        OUTPUT
            "${AXIOM_GENERATED_EVENTS}"
            "${AXIOM_GENERATED_MODULES}"
            "${AXIOM_GENERATED_METADATA_ID}"
            "${AXIOM_GENERATED_SOURCE_MAP}"
            "${AXIOM_GENERATED_DIR}/dictionary.json"
        COMMAND
            ${CMAKE_COMMAND} -E env "PYTHONPATH=${AXIOM_TOOLS_PYTHONPATH}"
            ${Python3_EXECUTABLE} -m axiomtrace_tools.cli codegen
            --events "${AXIOM_BUNDLE_EVENTS}"
            --source-id-map "${AXIOM_SOURCE_ID_MAP}"
            ${AXIOM_LOCATION_ARGS}
            --out "${AXIOM_GENERATED_DIR}"
        DEPENDS "${AXIOM_BUNDLE_EVENTS}" "${AXIOM_SOURCE_ID_MAP}"
        COMMENT "Generating AxiomTrace event headers and metadata identity"
        VERBATIM
    )

    add_custom_target(${AXIOM_BUNDLE_TARGET}_axiom_codegen
        DEPENDS
            "${AXIOM_GENERATED_EVENTS}"
            "${AXIOM_GENERATED_MODULES}"
            "${AXIOM_GENERATED_METADATA_ID}"
            "${AXIOM_GENERATED_SOURCE_MAP}"
            "${AXIOM_GENERATED_DIR}/dictionary.json"
    )

    target_include_directories(${AXIOM_BUNDLE_TARGET} PRIVATE "${AXIOM_GENERATED_DIR}")
    add_dependencies(${AXIOM_BUNDLE_TARGET} ${AXIOM_BUNDLE_TARGET}_axiom_codegen)

    set(AXIOM_BUNDLE_ARGS
        --events "${AXIOM_BUNDLE_EVENTS}"
        --source-id-map "${AXIOM_SOURCE_ID_MAP}"
        ${AXIOM_LOCATION_ARGS}
        --out "${AXIOM_BUNDLE_OUTPUT_DIR}"
        --elf "$<TARGET_FILE:${AXIOM_BUNDLE_TARGET}>"
    )
    if(CMAKE_EXPORT_COMPILE_COMMANDS)
        list(APPEND AXIOM_BUNDLE_ARGS --compile-db "${AXIOM_COMPILE_DB}")
    endif()
    if(AXIOM_BUNDLE_FIRMWARE_NAME)
        list(APPEND AXIOM_BUNDLE_ARGS --firmware-name "${AXIOM_BUNDLE_FIRMWARE_NAME}")
    endif()
    if(AXIOM_BUNDLE_FIRMWARE_VERSION)
        list(APPEND AXIOM_BUNDLE_ARGS --firmware-version "${AXIOM_BUNDLE_FIRMWARE_VERSION}")
    endif()

    add_custom_command(
        OUTPUT
            "${AXIOM_BUNDLE_MANIFEST}"
            "${AXIOM_BUNDLE_DICTIONARY}"
            "${AXIOM_BUNDLE_OUTPUT_DIR}/source_map.json"
            "${AXIOM_BUNDLE_OUTPUT_DIR}/build_info.json"
        COMMAND
            ${CMAKE_COMMAND} -E env "PYTHONPATH=${AXIOM_TOOLS_PYTHONPATH}"
            ${Python3_EXECUTABLE} -m axiomtrace_tools.cli bundle generate ${AXIOM_BUNDLE_ARGS}
        DEPENDS
            ${AXIOM_BUNDLE_TARGET}
            "${AXIOM_BUNDLE_EVENTS}"
            "${AXIOM_GENERATED_EVENTS}"
            "${AXIOM_GENERATED_MODULES}"
            "${AXIOM_GENERATED_METADATA_ID}"
        COMMENT "Generating AxiomTrace metadata bundle"
        VERBATIM
    )

    add_custom_target(${AXIOM_BUNDLE_TARGET}_axiom_bundle ALL
        DEPENDS
            "${AXIOM_BUNDLE_MANIFEST}"
            "${AXIOM_BUNDLE_DICTIONARY}"
            "${AXIOM_BUNDLE_OUTPUT_DIR}/source_map.json"
            "${AXIOM_BUNDLE_OUTPUT_DIR}/build_info.json"
    )
endfunction()
