cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)
include(pico_sdk_import.cmake)

project(tesserae C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(tesserae
    main.cpp
)

target_link_libraries(tesserae
    pico_stdlib
    hardware_gpio
    hardware_pwm
    hardware_adc
    hardware_dma
    hardware_spi
    hardware_i2c
    hardware_flash
    hardware_irq
    hardware_clocks
    pico_multicore
    pico_unique_id
)

pico_set_binary_type(tesserae copy_to_ram)
pico_add_extra_outputs(tesserae)

target_compile_definitions(tesserae PUBLIC PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)
pico_enable_stdio_usb(tesserae 1)
pico_enable_stdio_uart(tesserae 0)

target_compile_options(tesserae PRIVATE
    -Wall
    -Wextra
    -O2
    -ffast-math
    -funroll-loops
)

target_include_directories(tesserae PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)