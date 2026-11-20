# RP2350 Metering Station

Ethernetový měřič teploty, vlhkosti a tlaku.

- **Hardware:** Waveshare RP2350-POE-ETH (Ethernet W6300, napájení PoE), senzor BME280, displej 1.3" OLED (SH1106)
- **Jazyk:** C (Pico SDK 2.x), sestavení přes VS Code + rozšíření Raspberry Pi Pico, board `pico2`
- **Výstupy:** webová stránka (port 80), JSON (`/api/data`), Modbus TCP (port 502)

## Použití

1. Zapoj podle `docs/pinout.md`, připoj síťový kabel (PoE nebo USB-C + síť).
2. Měřič si přes DHCP vyžádá IP adresu; zobrazí se dole na displeji a vypíše na sériový výstup.
3. Otevři `http://<IP>/` v prohlížeči: karty s teplotou (na setiny), vlhkostí a tlakem, graf za posledních 6 hodin, IP, MAC a doba běhu; světlý i tmavý režim podle systému. JSON: `http://<IP>/api/data` (aktuální hodnoty) a `http://<IP>/api/history` (historie pro graf). Stránku upravuješ v `src/web/index.html`, při překladu se sama převede do firmwaru.
4. Modbus TCP: port 502, funkce 03/04/06/16. Měření (teplota, vlhkost, tlak i jako float), jas displeje a LED (zapisovatelný) a diagnostika. Kompletní mapa registrů je v [docs/modbus.md](docs/modbus.md).
5. Nastavení: na webové stránce karta **Nastavení** (DHCP nebo pevná IP, kalibrace senzoru, jas a otočení displeje, název zařízení, Modbus, heslo). Ukládá se do flash a přežije vypnutí i nahrání nového firmwaru. Výchozí hodnoty jsou v `src/config.h`.
6. Servisní kanál: textové příkazy (`help`, `diag`, `show`, `auth`, `defaults`, `reboot`) přes USB sériový port nebo TCP port 2323 (`telnet <IP> 2323`). Při zapomenutém hesle nebo špatné síti pomůže `defaults` po USB.
7. Hlídací obvod (watchdog) restartuje zařízení, když se smyčka zasekne na déle než 8 s.

## Struktura

| Soubor | Účel |
|---|---|
| `src/main.c` | hlavní smyčka: měření, displej, obsluha sítě |
| `src/net_w6300.c` | síť: stavový automat (kabel, DHCP/statická IP), web, Modbus a servisní server |
| `src/web.c` | HTTP odpovědi (stránka, JSON) |
| `src/modbus.c` | Modbus TCP (funkce 03/04/06/16) |
| `src/settings*.c` | nastavení a jeho uložení do flash |
| `src/diag*.c` | diagnostika (čítače, teplota čipu, watchdog) |
| `src/service.c` | servisní příkazy (USB a TCP port 2323) |
| `src/bme280*.c` | ovladač senzoru a kompenzační výpočty |
| `src/oled*.c/.h` | ovladač displeje SH1106 a písmo |
| `lib/ethchip/` | knihovna W6300 (WIZnet ioLibrary + SPI vrstva z ukázky Waveshare), viz licence v adresáři |

## Poznámky

- Při uživatelském jménu ve Windows s diakritikou selhává linker; řešení je junction `mklink /J C:\pico-sdk-link "%USERPROFILE%\.pico-sdk"` (viz `CMakeLists.txt`).
- Nahrání: podrž BOOT, stiskni RESET, zkopíruj `build\metering_station.uf2` na disk RP2350.
