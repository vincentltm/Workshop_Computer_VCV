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

cmake_minimum_required (VERSION 3.13)
include(pico_sdk_import.cmake)
project(two_tracks C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(two_tracks)
target_include_directories(two_tracks PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(two_tracks
    pico_unique_id pico_stdlib hardware_dma hardware_i2c hardware_pwm
    hardware_adc hardware_spi hardware_flash pico_multicore)
pico_add_extra_outputs(two_tracks)

target_sources(two_tracks PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}/main.cpp
    ${CMAKE_CURRENT_LIST_DIR}/two_tracks_stream.c)

target_compile_options(two_tracks PRIVATE
    -Wdouble-promotion -Wfloat-conversion -Wall -Wextra)
target_link_options(two_tracks PRIVATE -Wl,--print-memory-usage)
target_compile_definitions(two_tracks PRIVATE PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)
pico_enable_stdio_usb(two_tracks 0)

# Flash size in bytes. Default 2 MB (standard pico board); set to 16777216 for
# 16 MB cards. Used at runtime by tt_stream_init() to size the audio region.
set(PICO_FLASH_SIZE_BYTES "2097152" CACHE STRING "Flash size in bytes (2MB default; 16777216 for 16MB cards)")
target_compile_definitions(two_tracks PRIVATE PICO_FLASH_SIZE_BYTES=${PICO_FLASH_SIZE_BYTES})

# Run the whole image from RAM: two_tracks writes flash during operation, so no
# code (incl. SDK divider/float routines) may be fetched from flash while core 1
# is mid-erase. Matches Goldfish.
pico_set_binary_type(two_tracks copy_to_ram)

# Single source of truth for the firmware version (keep in sync with info.yaml).
set(TWO_TRACKS_VERSION "1.0")

# Embed program metadata into the built artifact (readable via `picotool info`).
pico_set_program_name(two_tracks "Two Tracks")
pico_set_program_version(two_tracks "${TWO_TRACKS_VERSION}")

# Rename the default output to a version-stamped UF2 (e.g. two_tracks.1.0.uf2) and
# copy that single, versioned artifact into the UF2/ directory.
add_custom_command(TARGET two_tracks POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E rename
        "$<TARGET_FILE_DIR:two_tracks>/two_tracks.uf2"
        "$<TARGET_FILE_DIR:two_tracks>/two_tracks.${TWO_TRACKS_VERSION}.uf2"
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/UF2
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE_DIR:two_tracks>/two_tracks.${TWO_TRACKS_VERSION}.uf2"
        ${CMAKE_CURRENT_SOURCE_DIR}/UF2/
    COMMENT "Creating version-stamped UF2: two_tracks.${TWO_TRACKS_VERSION}.uf2"
    VERBATIM)