cmake_minimum_required(VERSION 3.13)

include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(flux C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# define the ComputerCard library
add_library(ComputerCard INTERFACE)
target_include_directories(ComputerCard INTERFACE ${CMAKE_CURRENT_LIST_DIR}/lib/ComputerCard)
target_link_libraries(ComputerCard INTERFACE pico_stdlib hardware_dma hardware_i2c hardware_pwm hardware_adc hardware_spi pico_unique_id)

# define the pipicofx library
add_library(pipicofx STATIC)
target_include_directories(pipicofx PUBLIC ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/inc)
target_sources(pipicofx PUBLIC 
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxPrograms.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/audiotools.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramAmpModel.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramVibChorus.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramOff.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramAmpModel2.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramMonsterCrusher.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramDelay.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramCompressor.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramEchoVerb.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramReverb.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramSineChorus.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramReverb2.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramReverb3.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramEq.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramPitchShifter.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramShimmerVerb.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramTremolo.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramFreeVerb.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramSpringReverb.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramPlateReverb.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramCwo.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramGranular.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramCathedral.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramMicroLoop.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramGenLoss.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramLossy.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramSpectral.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramOilCan.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramStrings.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramWind.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fxProgramSpeech.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/pitchshifter.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/tremolo.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/phaseDistortedSineSquare.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/stringFunctions.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/intFunctions.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/romfunc.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/exptable.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/logtable.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/fastExpLog.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/firstOrderIirFilter.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/secondOrderIirFilter.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/firFilter.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/waveShaper.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/delay.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/compressor.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/compressor2.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/compressor3.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/reverb.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/reverb2.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/reverb3.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/reverbUtils.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/threebandeq.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/sineChorus.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/simpleChorus.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/bitcrusher.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/gainstage.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/pipicofx/src/oversamplingWaveshaper.c
)

add_executable(flux 
    src/main.cpp 
    src/SynthCore.cpp 
    src/MathTables.cpp
    src/usb_descriptors.c
    src/synths/synth_noise.cpp
    src/synths/synth_wavetable.cpp
    src/synths/synth_vabass.cpp
    src/synths/synth_strings.cpp
    src/synths/synth_modal.cpp
    src/synths/synth_granular.cpp
    src/synths/synth_piano.cpp
    src/synths/synth_sampler_oneshot.cpp
    src/synths/synth_sampler_loop.cpp
    src/synths/synth_sampler_player.cpp
    src/synths/synth_drums.cpp
    src/synths/synth_fm.cpp
    src/synths/synth_drumsynth.cpp
)

# Give oscillator more time to start
target_compile_definitions(flux PRIVATE
    PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
    # Sample rate divisor: 1=48kHz, 2=24kHz. Keep in sync with
    # AUDIO_SAMPLE_RATE_DIV in include/SynthCore.h.
    COMPUTERCARD_SAMPLE_RATE_DIV=2
)

# Include current directory (for tusb_config.h etc if they were here, but now in include/)
# And include the new include directory
target_include_directories(flux PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(flux PRIVATE ComputerCard pipicofx pico_stdlib pico_multicore tinyusb_device tinyusb_board)

pico_add_extra_outputs(flux)

# Integrated UF2 Generation
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_custom_command(TARGET flux POST_BUILD
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/tools/create_merged_uf2.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Creating integrated.uf2 with merged audio samples"
    VERBATIM
)

add_custom_command(TARGET flux POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_CURRENT_BINARY_DIR}/flux.uf2
        ${CMAKE_CURRENT_LIST_DIR}/web/flux.uf2
    COMMENT "Copying flux.uf2 to the web folder"
    VERBATIM
)

# pico_enable_stdio_usb(multifx 1) # Disabled to use custom MIDI descriptors

# Performance: We rely on __not_in_flash_func for hot paths since the binary (1MB+) 
# is too large to fit entirely in the 256KB RAM.
# pico_set_binary_type(flux copy_to_ram)

# Catch accidental double-promotion (software float on M0+ is very expensive)
target_compile_options(flux PRIVATE -Wdouble-promotion -Wfloat-conversion -Wall)

# Show RAM/Flash usage at link time
target_link_options(flux PRIVATE -Wl,--print-memory-usage)
