cmake_minimum_required(VERSION 3.13)
project(hotfuzz_tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Host-side DSP test harness (no Pico SDK required).
add_executable(test_dsp
    test_dsp.cpp
    ../braids_resources.cc
)
target_include_directories(test_dsp PRIVATE ..)

# Catch accidental float/double promotion.
target_compile_options(test_dsp PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME dsp_tests COMMAND test_dsp)