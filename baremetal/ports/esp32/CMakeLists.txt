# ---------------------------------------------------------------------------
# ESP32 Port - ESP-IDF Component Configuration
# ---------------------------------------------------------------------------
#
# This file provides ESP-IDF component configuration for AxiomTrace ESP32 port.
#
# Usage in your ESP-IDF project:
#   1. Add this component to your main CMakeLists.txt or via idf.py
#   2. Use menuconfig (idf.py menuconfig) to configure UART pins
#   3. Initialize axiom_port_esp32 and axiom_backend_uart in your code
#
# Example in your project's CMakeLists.txt:
#   set(AXIOM_ESP32_UART_NUM 1)
#   set(AXIOM_ESP32_UART_TX_GPIO 10)
#   set(AXIOM_ESP32_UART_RX_GPIO 9)
#   set(AXIOM_ESP32_UART_BAUD 921600)
#   add_subdirectory(vendor/axiomtrace/baremetal/ports/esp32 axiom_trace_esp32)
#   target_link_libraries(${COMPONENT_TARGET} PRIVATE axiom_trace_esp32)
#
# Or via Kconfig (idf.py menuconfig -> Component config -> AxiomTrace ESP32):
#   CONFIG_AXIOM_ESP32_UART_NUM=1
#   CONFIG_AXIOM_ESP32_UART_TX_GPIO=10
#   CONFIG_AXIOM_ESP32_UART_RX_GPIO=9
#   CONFIG_AXIOM_ESP32_UART_BAUD=921600
#
# ---------------------------------------------------------------------------

if(CONFIG_IDF_TARGET_ESP32 OR CONFIG_IDF_TARGET_ESP32C3 OR CONFIG_IDF_TARGET_ESP32S2 OR CONFIG_IDF_TARGET_ESP32S3)
    idf_component_register(
        SRCS
            axiom_port_esp32.c
            axiom_backend_uart.c
        INCLUDE_DIRS
            .
        PRIV_REQUIRES
            esp_timer
            freertos
        REQUIRES
            driver
    )

    target_compile_options(${COMPONENT_LIB} PRIVATE
        -Wno-unused-function
        -Wno-unused-but-set-variable)

else()
    message(FATAL_ERROR "axiom_trace_esp32: This component only supports ESP32 series chips")
endif()

# ---------------------------------------------------------------------------
# Kconfig - ESP-IDF Configuration Options
# ---------------------------------------------------------------------------
# This file provides Kconfig entries for menuconfig integration.
# ESP-IDF will automatically merge Kconfig files from components.
#
# To customize via menuconfig:
#   idf.py menuconfig -> Component config -> AxiomTrace ESP32
# ---------------------------------------------------------------------------

if(CONFIG_IDF_TARGET_ESP32 OR CONFIG_IDF_TARGET_ESP32C3 OR CONFIG_IDF_TARGET_ESP32S2 OR CONFIG_IDF_TARGET_ESP32S3)

    menu "AxiomTrace ESP32 Configuration"

        config AXIOM_ESP32_ENABLE
            bool "Enable AxiomTrace ESP32 Port"
            default y
            help
                Enable AxiomTrace platform support for ESP32.

        config AXIOM_ESP32_UART_NUM
            int "UART port number"
            default 1
            range 0 2
            help
                UART peripheral to use for AxiomTrace output.
                0 = UART0 (typically used for console)
                1 = UART1 (available for custom use)
                2 = UART2 (available for custom use)

        config AXIOM_ESP32_UART_TX_GPIO
            int "UART TX GPIO pin"
            default 10
            help
                GPIO pin number for UART TX.
                Make sure this pin is available on your board and not used by SPI Flash.
                For UART1 default pins (GPIO10/9), they may conflict with SPI flash.
                Consider using: 17/16 (UART2) or configure via Matrix.

        config AXIOM_ESP32_UART_RX_GPIO
            int "UART RX GPIO pin"
            default 9
            help
                GPIO pin number for UART RX.
                Make sure this pin is available on your board and not used by SPI Flash.

        config AXIOM_ESP32_UART_BAUD
            int "UART baud rate"
            default 921600
            help
                UART baud rate for AxiomTrace output.
                Higher rates (921600) recommended for production.
                115200 can be used for debugging.

        config AXIOM_ESP32_UART_TX_BUF_SIZE
            int "UART TX buffer size"
            default 256
            range 64 1024
            help
                UART TX ring buffer size in bytes.
                Larger buffers can handle burst traffic better.

        config AXIOM_ESP32_UART_COBS_ENABLED
            bool "Enable COBS encoding"
            default y
            help
                Enable Consistent Overhead Byte Stuffing (COBS) encoding.
                COBS ensures 0x00 can be used as frame delimiter.

    endmenu

endif()
