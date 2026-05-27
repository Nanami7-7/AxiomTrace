cmake_minimum_required(VERSION 3.16)
project(AxiomTrace LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(AXIOM_BUILD_TESTS "Build host unit tests" ON)
option(AXIOM_BUILD_BENCHMARKS "Build benchmarks" OFF)
option(AXIOM_BUILD_EXAMPLES "Build examples" ON)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(AxiomTraceTools OPTIONAL)

# ---------------------------------------------------------------------------
# Platform/Port options (three-layer architecture)
# ---------------------------------------------------------------------------
set(AXIOM_PLATFORM_OPTIONS
    cortex-m
    riscv
    host
    CACHE STRING "Target architecture platform (arch layer)"
)

set(AXIOM_SOC_OPTIONS
    generic
    stm32f4
    nrf52
    esp32
    CACHE STRING "Target SoC/family (soc layer)"
)

set(AXIOM_BOARD_OPTIONS
    generic
    CACHE STRING "Target board (board layer, overrides soc defaults)"
)

# Platform auto-detection (if not explicitly set)
if(NOT DEFINED AXIOM_PLATFORM)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm|cortex")
        set(AXIOM_PLATFORM "cortex-m" CACHE STRING "Target architecture platform" FORCE)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv|rv")
        set(AXIOM_PLATFORM "riscv" CACHE STRING "Target architecture platform" FORCE)
    else()
        set(AXIOM_PLATFORM "host" CACHE STRING "Target architecture platform" FORCE)
    endif()
    message(STATUS "AXIOM_PLATFORM auto-detected: ${AXIOM_PLATFORM}")
endif()

# SoC (default: generic)
if(NOT DEFINED AXIOM_SOC)
    set(AXIOM_SOC "generic" CACHE STRING "Target SoC/family" FORCE)
    message(STATUS "AXIOM_SOC defaulting to: ${AXIOM_SOC}")
endif()

# Board (default: generic)
if(NOT DEFINED AXIOM_BOARD)
    set(AXIOM_BOARD "generic" CACHE STRING "Target board" FORCE)
    message(STATUS "AXIOM_BOARD defaulting to: ${AXIOM_BOARD}")
endif()

# Validate platform selection
if(NOT AXIOM_PLATFORM IN_LIST AXIOM_PLATFORM_OPTIONS)
    message(FATAL_ERROR "Invalid AXIOM_PLATFORM: '${AXIOM_PLATFORM}'. Valid: ${AXIOM_PLATFORM_OPTIONS}")
endif()

# Include unified ports management
include(baremetal/ports/CMakeLists.txt)

# ---------------------------------------------------------------------------
# Compiler warnings (gcc/clang)
# ---------------------------------------------------------------------------
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wshadow
        -Wcast-align
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wmissing-declarations
        -Wredundant-decls
        -Wbad-function-cast
        -Wconversion
        -Wsign-conversion
        -Wdouble-promotion
        -Wfloat-equal
        -Wundef
        -Wunused
        -Werror=implicit-function-declaration
        -Werror=return-type
        -Werror=pointer-arith
        -pedantic
    )
endif()

# ---------------------------------------------------------------------------
# Static analysis integration (disabled during v1.0 refactor)
# ---------------------------------------------------------------------------
# find_program(CLANG_TIDY_EXE clang-tidy)
# if(CLANG_TIDY_EXE)
#     set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE};--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy")
# endif()

# ---------------------------------------------------------------------------
# AxiomTrace core library
# ---------------------------------------------------------------------------
set(AXIOM_CORE_HEADERS
    ${PROJECT_SOURCE_DIR}/baremetal/include/axiom_static_assert.h
    ${PROJECT_SOURCE_DIR}/baremetal/port/axiom_port.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_event.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_encode.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_crc.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_ring.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_timestamp.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_filter.h
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_selftest.h
    ${PROJECT_SOURCE_DIR}/baremetal/frontend/axiom_frontend.h
    ${PROJECT_SOURCE_DIR}/baremetal/backend/axiom_backend.h
)

set(AXIOM_CORE_SOURCES
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_event.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_crc.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_ring.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_timestamp.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_filter.c
    ${PROJECT_SOURCE_DIR}/baremetal/core/axiom_selftest.c
    ${PROJECT_SOURCE_DIR}/baremetal/backend/axiom_backend.c
)

add_library(axiomtrace STATIC ${AXIOM_CORE_SOURCES} ${AXIOM_CORE_HEADERS})
target_include_directories(axiomtrace PUBLIC
    ${PROJECT_SOURCE_DIR}/baremetal
    ${PROJECT_SOURCE_DIR}/baremetal/include
    ${PROJECT_SOURCE_DIR}/baremetal/port
    ${PROJECT_SOURCE_DIR}/baremetal/core
    ${PROJECT_SOURCE_DIR}/baremetal/frontend
    ${PROJECT_SOURCE_DIR}/baremetal/backend
)

# Port selection (using new three-layer architecture)
# Priority: board > soc > arch > generic
# The AXIOM_PORT_SOURCES is set by baremetal/ports/CMakeLists.txt
target_sources(axiomtrace PRIVATE ${AXIOM_PORT_SOURCES})

# Legacy port path for backward compatibility with existing includes
target_include_directories(axiomtrace PRIVATE ${PROJECT_SOURCE_DIR}/baremetal/ports)

# ---------------------------------------------------------------------------
# Optional feature toggles (forwarded as -D preprocessor macros)
# Must come after add_library(axiomtrace ...)
# ---------------------------------------------------------------------------
option(AXIOM_SELFTEST "Enable runtime self-test at boot" OFF)
option(AXIOM_DEBUG "Enable debug assertions and diagnostics" OFF)

if(AXIOM_SELFTEST)
    target_compile_definitions(axiomtrace PUBLIC AXIOM_SELFTEST=1)
endif()
if(AXIOM_DEBUG)
    target_compile_definitions(axiomtrace PUBLIC AXIOM_DEBUG=1)
endif()
if(AXIOM_BUILD_TESTS AND AXIOM_PLATFORM STREQUAL "host")
    # Expose a controllable generic-port timestamp only to host test builds.
    target_compile_definitions(axiomtrace PUBLIC AXIOM_HOST_TESTING=1)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(AXIOM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/host)
endif()

# ---------------------------------------------------------------------------
# Examples
# ---------------------------------------------------------------------------
if(AXIOM_BUILD_EXAMPLES)
    add_subdirectory(baremetal/examples)
endif()
