# ============================================================================
# AxiomTrace architecture-level port selection
# ============================================================================
#
# This file is included by the top-level CMake project. SDK-dependent vendor
# packages stay under baremetal/ports/{esp32,nrf52,stm32} and are consumed by
# firmware projects separately.
# ============================================================================

set(AXIOM_PORTS_DIR "${CMAKE_CURRENT_LIST_DIR}")

if(AXIOM_PLATFORM STREQUAL "cortex-m")
    set(AXIOM_PORT_SOURCE
        "${AXIOM_PORTS_DIR}/arch/cortex-m/axiom_port_cortex_m.c")
elseif(AXIOM_PLATFORM STREQUAL "riscv")
    set(AXIOM_PORT_SOURCE
        "${AXIOM_PORTS_DIR}/arch/riscv/axiom_port_riscv.c")
else()
    set(AXIOM_PORT_SOURCE
        "${AXIOM_PORTS_DIR}/generic/axiom_port_generic.c")
endif()

if(NOT EXISTS "${AXIOM_PORT_SOURCE}")
    message(FATAL_ERROR "Selected port source is missing: ${AXIOM_PORT_SOURCE}")
endif()

# Variables are intentionally normal-scope values, not cache entries. They
# are consumed by the parent target and must not leak into downstream users.
set(AXIOM_PORT_SOURCES "${AXIOM_PORT_SOURCE}")

message(STATUS "=== AxiomTrace Port Configuration ===")
message(STATUS "  AXIOM_PLATFORM: ${AXIOM_PLATFORM}")
message(STATUS "  Port source:    ${AXIOM_PORT_SOURCE}")
message(STATUS "===================================")
