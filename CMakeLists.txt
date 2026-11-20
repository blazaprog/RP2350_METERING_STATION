cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Board: RP2350-POE-ETH is Pico 2 compatible (RP2350A, 16MB flash)
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pico SDK / VS Code extension integration
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
if(EXISTS ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
    include(${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
endif()
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

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
