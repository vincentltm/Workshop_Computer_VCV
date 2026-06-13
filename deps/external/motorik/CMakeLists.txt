cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico)
include(pico_sdk_import.cmake)
project(motorik C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(motorik main.cpp)

target_include_directories(motorik PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
)

pico_set_binary_type(motorik copy_to_ram)
pico_add_extra_outputs(motorik)

target_compile_definitions(motorik PUBLIC 
    PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
)

pico_enable_stdio_usb(motorik 0)

target_link_libraries(motorik 
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

target_compile_options(motorik PRIVATE -Wall -Wextra -O2 -Wdouble-promotion)
