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
# ============================================================================
#  CMakeLists.txt - sestavení firmwaru (Pico SDK, deska RP2350-POE-ETH)
#  Sestavení se spouští přes rozšíření Raspberry Pi Pico ve VS Code (tlačítko Compile).
# ============================================================================
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Board: RP2350-POE-ETH is Pico 2 compatible (RP2350A, 16MB flash)
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Windows user names with non-ASCII characters break GNU ld (cannot open linker script).
# Workaround: create an ASCII junction in cmd:  mklink /J C:\pico-sdk-link "%USERPROFILE%\.pico-sdk"
# If it exists, use the SDK through it.
if(WIN32 AND EXISTS C:/pico-sdk-link/sdk/${sdkVersion})
    set(PICO_SDK_PATH C:/pico-sdk-link/sdk/${sdkVersion})
endif()

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

# Knihovna pro Ethernet čip W6300 (převzatá z WIZnet ioLibrary a ukázky Waveshare)
add_subdirectory(lib/ethchip)

# Zdrojové soubory firmwaru
add_executable(metering_station
    src/main.c
    src/modbus.c
    src/web.c
    src/history.c
    src/settings.c
    src/settings_flash.c
    src/diag.c
    src/diag_hw.c
    src/service.c
    src/net_w6300.c
    src/bme280.c
    src/bme280_calc.c
    src/oled.c
    src/oled_fonts.c
    src/led.c
)

# Webová stránka src/web/index.html se při konfiguraci převede na pole bajtů v jazyce C
# (web_page.h v adresáři sestavení). Stačí upravit HTML a znovu přeložit.
set(WEB_HTML ${CMAKE_CURRENT_LIST_DIR}/src/web/index.html)
file(READ ${WEB_HTML} WEB_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," WEB_ARRAY "${WEB_HEX}")
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/web_page.h
     "// Vygenerováno z src/web/index.html, needituj.\nstatic const unsigned char web_page[] = {${WEB_ARRAY}0x00};\n")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${WEB_HTML})

target_include_directories(metering_station PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})

# Použité knihovny: základ SDK, I2C (senzor), SPI (displej), PIO (LED), unikátní ID čipu (MAC adresa) a Ethernet
target_link_libraries(metering_station
    pico_stdlib
    hardware_i2c
    hardware_spi
    hardware_pio
    hardware_flash
    hardware_adc
    hardware_watchdog
    pico_flash
    pico_unique_id
    ethchip
)

# Z programu pro PIO (led.pio) se při sestavení vygeneruje hlavička led.pio.h
pico_generate_pio_header(metering_station ${CMAKE_CURRENT_LIST_DIR}/src/led.pio)

# Výpisy (printf) jdou přes USB (sériový monitor), UART je vypnutý
pico_enable_stdio_usb(metering_station 1)
pico_enable_stdio_uart(metering_station 0)
# Vytvoří i soubor .uf2 pro nahrání do desky přes USB
pico_add_extra_outputs(metering_station)
