# RP2350 Metering Station

Ethernetový měřič teploty, vlhkosti a tlaku.

- **Hardware:** Waveshare RP2350-POE-ETH (Ethernet W6300, napájení PoE), senzor BME280, displej 1.3" OLED (SH1106)
- **Jazyk:** C (Pico SDK 2.x), sestavení přes VS Code + rozšíření Raspberry Pi Pico, board `pico2`
- **Výstupy:** webová stránka (port 80), JSON (`/api/data`), Modbus TCP (port 502)

## Použití

1. Zapoj podle `docs/pinout.md`, připoj síťový kabel (PoE nebo USB-C + síť).
2. Měřič si přes DHCP vyžádá IP adresu; zobrazí se dole na displeji a vypíše na sériový výstup.
3. Otevři `http://<IP>/` v prohlížeči, nebo `http://<IP>/api/data` pro JSON.
4. Modbus TCP: port 502, registry (funkce 03 a 04): 0 = teplota (0,1 °C), 1 = vlhkost (0,1 %), 2 = tlak (hPa), 3 = stav (0 OK, 1 chyba senzoru); 4-5 = teplota (float °C), 6-7 = vlhkost (float %), 8-9 = tlak (float hPa). Pořadí slov u floatu se mění v `config.h` (`MODBUS_FLOAT_WORD_SWAP`), při chybě senzoru jsou floaty NaN.

Nastavení (DHCP/statická IP, porty) je v `src/config.h`.

## Struktura

| Soubor | Účel |
|---|---|
| `src/main.c` | hlavní smyčka: měření, displej, obsluha sítě |
| `src/net_w6300.c` | síť: stavový automat (kabel, DHCP), web a Modbus server |
| `src/web.c` | HTTP odpovědi (stránka, JSON) |
| `src/modbus.c` | Modbus TCP (funkce 03/04) |
| `src/bme280*.c` | ovladač senzoru a kompenzační výpočty |
| `src/oled*.c/.h` | ovladač displeje SH1106 a písmo |
| `lib/ethchip/` | knihovna W6300 (WIZnet ioLibrary + SPI vrstva z ukázky Waveshare), viz licence v adresáři |

## Poznámky

- Při uživatelském jménu ve Windows s diakritikou selhává linker; řešení je junction `mklink /J C:\pico-sdk-link "%USERPROFILE%\.pico-sdk"` (viz `CMakeLists.txt`).
- Nahrání: podrž BOOT, stiskni RESET, zkopíruj `build\metering_station.uf2` na disk RP2350.
