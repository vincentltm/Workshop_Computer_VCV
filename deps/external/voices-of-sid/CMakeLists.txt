cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico)
include(pico_sdk_import.cmake)
project(voices_of_sid C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(voices_of_sid
    main.cpp
    reSID/sid.cc
    reSID/wave.cc
    reSID/envelope.cc
    reSID/filter.cc
    reSID/extfilt.cc
    reSID/pot.cc
    reSID/voice.cc
    reSID/version.cc
)

target_include_directories(voices_of_sid PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/reSID
    ${CMAKE_CURRENT_SOURCE_DIR}/ComputerCard
)

pico_set_binary_type(voices_of_sid copy_to_ram)
pico_add_extra_outputs(voices_of_sid)

target_compile_definitions(voices_of_sid PUBLIC
    PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
)

pico_enable_stdio_usb(voices_of_sid 0)

target_link_libraries(voices_of_sid
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

target_compile_options(voices_of_sid PRIVATE -Wall -Wextra -O2 -Wdouble-promotion)