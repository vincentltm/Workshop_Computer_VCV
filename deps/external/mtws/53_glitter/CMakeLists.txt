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
set(PROJECT_NAME glitter)
project(${PROJECT_NAME} C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()



add_executable(${PROJECT_NAME})
target_include_directories(${PROJECT_NAME} PUBLIC 
${CMAKE_CURRENT_LIST_DIR}
${CMAKE_CURRENT_LIST_DIR}/inc
)

target_link_libraries(${PROJECT_NAME} pico_unique_id pico_stdlib hardware_dma hardware_i2c hardware_pwm hardware_adc hardware_spi)
pico_add_extra_outputs(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PUBLIC
${CMAKE_CURRENT_LIST_DIR}/main.cpp
${CMAKE_CURRENT_LIST_DIR}/src/Glitter.cpp
${CMAKE_CURRENT_LIST_DIR}/src/Utils.cpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)
target_compile_definitions(${PROJECT_NAME} PRIVATE PICO_USE_FASTEST_SUPPORTED_CLOCK=1)

pico_enable_stdio_usb(${PROJECT_NAME} 0)
