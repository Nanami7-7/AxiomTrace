# ============================================================================
# nRF52 Platform Port - CMake Configuration
# ============================================================================
#
# This CMake file provides nRF52-specific build configuration for AxiomTrace.
#
# Usage:
#   In your nRF52 project CMakeLists.txt, add:
#     add_subdirectory(path/to/axiomtrace/baremetal/ports/nrf52 axiom_build)
#
# Requirements:
#   - SEGGER J-Link RTT support (RTT.h included via SEGGER_JLink.h)
#   - nRF52 SDK or standalone CMSIS headers
#
# ============================================================================

cmake_minimum_required(VERSION 3.16)

if(NOT TARGET AxiomTrace::axiomtrace)
    message(FATAL_ERROR
        "AxiomTrace nRF52 port requires AxiomTrace::axiomtrace; "
        "add the core project before this directory")
endif()

# ---------------------------------------------------------------------------
# Port Name and Sources
# ---------------------------------------------------------------------------
set(AXIOM_PORT_DIR ${CMAKE_CURRENT_LIST_DIR})

# nRF52 port source files
set(AXIOM_PORT_SOURCES
    ${AXIOM_PORT_DIR}/axiom_port_nrf52.c
    ${AXIOM_PORT_DIR}/axiom_backend_rtt.c
)

# nRF52 port headers
set(AXIOM_PORT_HEADERS
    ${AXIOM_PORT_DIR}/axiom_port_nrf52.h
    ${AXIOM_PORT_DIR}/axiom_backend_rtt.h
)

# ---------------------------------------------------------------------------
# Interface Library for nRF52 Port
# ---------------------------------------------------------------------------
add_library(axiom_port_nrf52 INTERFACE)
target_sources(axiom_port_nrf52 INTERFACE
    ${AXIOM_PORT_SOURCES}
    ${AXIOM_PORT_HEADERS}
)
target_include_directories(axiom_port_nrf52 INTERFACE
    ${AXIOM_PORT_DIR}
)
target_link_libraries(axiom_port_nrf52 INTERFACE AxiomTrace::axiomtrace)
target_compile_definitions(axiom_port_nrf52 INTERFACE
    AXIOM_PORT_NRF52
    NRF52_SERIES
)

# ---------------------------------------------------------------------------
# Optional: SEGGER RTT Dependency Check
# ---------------------------------------------------------------------------
# The RTT backend requires SEGGER_RTT_Write() from SEGGER's RTT API.
# If SEGGER_RTT.h is not available, axiom_backend_rtt.c will fail to compile.
# You can provide SEGGER_RTT.h via:
#   1. J-Link SDK (Recommended: https://www.segger.com/downloads/jlink/)
#   2. nRF5 SDK (nrfx/drivers/src/SEGGER_RTT.c)
#   3. Standalone: https://www.segger.com/downloads/jlink/UM08001_JLink.pdf

# Option to enable SEGGER RTT support (default ON if headers found)
option(AXIOM_NRF52_RTT "Enable SEGGER RTT backend" ON)

if(AXIOM_NRF52_RTT)
    # Check for SEGGER RTT header
    find_path(SEGGER_RTT_H SEGGER_RTT.h
        PATHS
            $ENV{JLINK_BASE}/SDK/include
            $ENV{NRF_SDK_ROOT}/components/softdevice/S132/headers
            ${CMAKE_CURRENT_SOURCE_DIR}/../..
        NO_DEFAULT_PATH
    )

    if(SEGGER_RTT_H)
        message(STATUS "nRF52 RTT: SEGGER_RTT.h found at ${SEGGER_RTT_H}")
        target_include_directories(axiom_port_nrf52 INTERFACE ${SEGGER_RTT_H})
        target_compile_definitions(axiom_port_nrf52 INTERFACE AXIOM_RTT_ENABLED)
    else()
        message(WARNING "nRF52 RTT: SEGGER_RTT.h not found. RTT backend disabled.")
        target_compile_definitions(axiom_port_nrf52 INTERFACE AXIOM_RTT_DISABLED)
    endif()
endif()

# ---------------------------------------------------------------------------
# Optional: nRF5 SDK Integration
# ---------------------------------------------------------------------------
option(AXIOM_NRF52_SDK "Enable nRF5 SDK integration" OFF)

if(AXIOM_NRF52_SDK)
    # nRF5 SDK provides nrf_log backend as well
    find_path(NRF_HAL nrf.h
        PATHS
            $ENV{NRF_SDK_ROOT}/modules/hal/nrf
            ${CMAKE_CURRENT_SOURCE_DIR}/../../../modules/hal/nrf
        NO_DEFAULT_PATH
    )

    if(NRF_HAL)
        message(STATUS "nRF52 SDK: nrf.h found at ${NRF_HAL}")
        target_include_directories(axiom_port_nrf52 INTERFACE ${NRF_HAL})
    endif()
endif()

# ---------------------------------------------------------------------------
# Link-Time Optimization (LTO) for Production Builds
# ---------------------------------------------------------------------------
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(axiom_port_nrf52 INTERFACE
        -flto
    )
endif()
