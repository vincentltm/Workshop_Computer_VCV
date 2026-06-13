# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.0)
set(toolchainVersion 13_3_Rel1)
set(picotoolVersion 2.1.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

# What CMake to start at
cmake_minimum_required(VERSION 3.12)

# Include the subsidiary .cmake file to get the SDK
include(pico_sdk_import.cmake)

# Set the name and version of the project
project(toolbox VERSION 1.0.0)

# Link the Project to a source file (step 4.6)
add_executable(toolbox toolbox.cpp)

# Link the Project to an extra library (pico_stdlib)
target_link_libraries(toolbox pico_stdlib)

# Initalise the SDK
pico_sdk_init()


target_link_libraries(toolbox pico_unique_id pico_stdlib hardware_dma hardware_i2c hardware_pwm hardware_adc hardware_spi)
pico_add_extra_outputs(toolbox)
target_compile_definitions(toolbox PRIVATE PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)

# Enable USB, UART output
pico_enable_stdio_usb(toolbox 1)
pico_enable_stdio_uart(toolbox 1)

# Enable extra outputs (SWD?)
pico_add_extra_outputs(toolbox)

pico_set_binary_type(${PROJECT_NAME} copy_to_ram)
