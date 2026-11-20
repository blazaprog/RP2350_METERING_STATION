# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Board: RP2350-POE-ETH is Pico 2 compatible (RP2350A, 16MB flash)
set(PICO_BOARD pico2 CACHE STRING "Board type")

# The VS Code extension sets PICO_SDK_PATH as a CMake variable (pico-vscode.cmake).
# Fall back to the environment variable for command-line builds.
if(NOT PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()
if(NOT PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set (use the Raspberry Pi Pico VS Code extension or set the env variable)")
endif()
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

project(metering_station C CXX ASM)
pico_sdk_init()

add_executable(metering_station
    src/main.c
    src/modbus.c
    src/web.c
    src/net_w6300.c
)

target_include_directories(metering_station PRIVATE src)

target_link_libraries(metering_station
    pico_stdlib
    hardware_i2c
    hardware_spi
)

pico_enable_stdio_usb(metering_station 1)
pico_enable_stdio_uart(metering_station 0)
pico_add_extra_outputs(metering_station)
