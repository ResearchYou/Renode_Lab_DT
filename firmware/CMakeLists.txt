cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(digital_twin_poc C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(firmware main.c)

target_link_libraries(firmware
    pico_stdlib
    hardware_i2c
    hardware_timer
)

# Route stdio to UART (not USB) for Renode compatibility
pico_enable_stdio_uart(firmware 1)
pico_enable_stdio_usb(firmware 0)

pico_add_extra_outputs(firmware)
