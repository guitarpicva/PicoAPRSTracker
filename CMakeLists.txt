# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/pi/pico/pico-sdk")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(PicoAPRSTracker C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(PicoAPRSTracker PicoAPRSTracker.cpp uikissutils.cpp ez_cpp_utils)

pico_set_program_name(PicoAPRSTracker "PicoAPRSTracker")
pico_set_program_version(PicoAPRSTracker "0.1")

pico_enable_stdio_uart(PicoAPRSTracker 0)
pico_enable_stdio_usb(PicoAPRSTracker 1)

# Add the standard library to the build
target_link_libraries(PicoAPRSTracker pico_stdlib pico_util pico_multicore hardware_flash hardware_sync hardware_rtc)

pico_add_extra_outputs(PicoAPRSTracker)

