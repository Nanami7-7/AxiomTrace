# ---------------------------------------------------------------------------
# STM32F4 Port - CMake Configuration
# ---------------------------------------------------------------------------
#
# This file provides CMake configuration for AxiomTrace STM32F4 port.
# Include from your project's CMakeLists.txt after axiomtrace is built.
#
# Usage:
#   add_subdirectory(path/to/axiomtrace/baremetal/ports/stm32 axiom_trace_stm32)
#
# Options:
#   AXIOM_UART_TX_BUFFER_SIZE - UART TX buffer size (default: 256)
#
# Example:
#   set(AXIOM_UART_TX_BUFFER_SIZE 512)
#   add_subdirectory(vendor/axiomtrace/baremetal/ports/stm32 axiom_trace_stm32)
#
# ---------------------------------------------------------------------------

cmake_minimum_required(VERSION 3.16)

# This package expects the core target to be created by the parent project.
if(NOT TARGET AxiomTrace::axiomtrace)
    message(FATAL_ERROR
        "AxiomTrace STM32 port requires AxiomTrace::axiomtrace; "
        "add the core project before this directory")
endif()

if(NOT DEFINED AXIOM_UART_TX_BUFFER_SIZE)
    set(AXIOM_UART_TX_BUFFER_SIZE 256)
endif()

# ---------------------------------------------------------------------------
# Source files for STM32 port
# ---------------------------------------------------------------------------
set(AXIOM_STM32_PORT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/axiom_port_stm32.c
    ${CMAKE_CURRENT_SOURCE_DIR}/axiom_backend_uart.c
)

# ---------------------------------------------------------------------------
# Create library
# ---------------------------------------------------------------------------
add_library(axiom_trace_stm32 STATIC ${AXIOM_STM32_PORT_SOURCES})

target_link_libraries(axiom_trace_stm32 PUBLIC AxiomTrace::axiomtrace)

if(CMAKE_C_COMPILER_ID MATCHES "GNU")
    target_compile_options(axiom_trace_stm32 PRIVATE
        -mcpu=cortex-m4
        -mthumb
        -mfloat-abi=hard
        -mfpu=fpv4-sp-d16
        -Wall
        -Wextra
    )
endif()

target_include_directories(axiom_trace_stm32 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(axiom_trace_stm32 PUBLIC
    AXIOM_UART_TX_BUFFER_SIZE=${AXIOM_UART_TX_BUFFER_SIZE}
)

# ---------------------------------------------------------------------------
# Linker script (optional - provide your own for final linking)
# ---------------------------------------------------------------------------
# target_link_libraries(axiom_trace_stm32
#     your_chip_startup
#     your_linker_script
# )

# ---------------------------------------------------------------------------
# Integration Notes
# ---------------------------------------------------------------------------
# 1. Add this directory to your CMake project BEFORE axiomtrace is linked
# 2. Link axiom_trace_stm32 library to your application
# 3. Call axiom_port_init() in your main() before axiom_init()
# 4. Call axiom_backend_uart_init() with your UART/DMA peripheral addresses
# 5. Add DMA IRQ handler for your DMA stream
# 6. Connect UART TX to your host (e.g., via ST-Link virtual COM port)
