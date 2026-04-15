cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(node_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(node main.c)

target_link_libraries(node
    pico_stdlib
    hardware_i2c
    hardware_spi
    hardware_timer
)

# Route stdio to UART0 for Renode compatibility
pico_enable_stdio_uart(node 1)
pico_enable_stdio_usb(node 0)

pico_add_extra_outputs(node)
