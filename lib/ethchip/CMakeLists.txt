# Knihovna pro Ethernet čip W6300 (WIZnet ioLibrary + SPI vrstva z ukázky Waveshare).
# Zdroje: lib/ethchip/LICENSE-ioLibrary.txt (WIZnet, MIT) a LICENSE-waveshare-apache2.txt (Waveshare, Apache 2.0).

add_library(ethchip STATIC
    Ethernet/socket.c            # BSD-like socket API (socket, listen, recv, send, ...)
    Ethernet/ethchip_conf.c      # obecné funkce čipu (inicializace, síťové informace)
    Ethernet/W6300/w6300.c       # přístup k registrům W6300
    Internet/DHCP/dhcp.c         # DHCP klient
    Port/ethchip_spi.c           # propojení knihovny s SPI0 a GPIO na RP2350
)

target_include_directories(ethchip PUBLIC
    Ethernet
    Ethernet/W6300
    Internet/DHCP
    Port
)

target_link_libraries(ethchip PUBLIC
    pico_stdlib
    hardware_spi
    hardware_dma
    hardware_clocks
)

# Převzatý kód generuje spoustu varování, která nejsou naše chyba. Potlačíme je jen pro tuto knihovnu.
target_compile_options(ethchip PRIVATE -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable)
