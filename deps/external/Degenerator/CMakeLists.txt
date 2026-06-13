cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD pico)
include(pico_sdk_import.cmake)

project(degenerator C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(degenerator
    main.cpp
)

target_link_libraries(degenerator
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

pico_add_extra_outputs(degenerator)

target_compile_definitions(degenerator PUBLIC PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)
pico_enable_stdio_usb(degenerator 1)
pico_enable_stdio_uart(degenerator 0)

target_compile_options(degenerator PRIVATE
    -Wall
    -Wextra
    -O2
    -ffast-math
    -funroll-loops
)

target_include_directories(degenerator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)