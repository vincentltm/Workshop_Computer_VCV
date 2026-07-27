# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required (VERSION 3.13)
include(pico_sdk_import.cmake)
project(hotfuzz C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(hot_fuzz
    main.cpp
    braids_resources.cc
)

# Warnings: catch accidental float/double promotion (slow on M0+)
target_compile_options(hot_fuzz PRIVATE -Wdouble-promotion -Wfloat-conversion -Wall -Wextra)
# Optimisation flags, matching the Workshop Computer convention
target_compile_options(hot_fuzz PRIVATE -O2 -ffast-math -funroll-loops)

# Give oscillator more time to start - some boards won't run if this isn't included
target_compile_definitions(hot_fuzz PRIVATE PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)

# Copy binary fully into RAM to avoid flash-cache jitter in the audio ISR
pico_set_binary_type(hot_fuzz copy_to_ram)

target_include_directories(hot_fuzz PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(hot_fuzz
    pico_unique_id
    pico_stdlib
    hardware_dma
    hardware_i2c
    hardware_pwm
    hardware_adc
    hardware_spi
    pico_multicore
)

pico_add_extra_outputs(hot_fuzz)
pico_enable_stdio_usb(hot_fuzz 0)