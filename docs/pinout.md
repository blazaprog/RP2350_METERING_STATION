# Pinout (RP2350-POE-ETH)

| Component | Signal | GPIO |
|---|---|---|
| W6300 (SPI0) | MISO / CSn / SCK / MOSI | GP16 / GP17 / GP18 / GP19 |
| W6300 | RSTn / INTn | GP20 / GP21 |
| OLED 1.3" SH1106 (SPI1, 4-wire) | SCK / MOSI(SDA) / CS / DC / RES | GP10 / GP11 / GP9 / GP8 / GP12 |
| BME280 (I2C0) | SDA / SCL | GP4 / GP5 |

Reserved by the board: GP23 POE_DET, GP24 USB_DET, GP25 WS2812, GP29 ADC3.

## Modbus TCP (port 502)
Full register map (measurements 0-9, writable brightness 10-11, diagnostics 16-30): see [modbus.md](modbus.md).

## Service channel
TCP port 2323 (socket 6) and USB serial: `help`, `diag`, `show`, `auth <pw>`, `defaults`, `reboot`, `quit`.

## HTTP
- `GET /` HTML page (cards + 6 h charts, source in `src/web/index.html`).
- `GET /api/data` JSON: temperature, humidity, pressure (2 decimals, null on sensor error), status, uptime_s, ip, mac, fw.
- `GET /api/settings`, `POST /api/settings` (form-urlencoded; password in `pw` when set), `GET /api/diag`.
- `GET /api/history` JSON: `{"step":60,"n":N,"t":[...],"h":[...],"p":[...]}`, oldest first; t = 0.01 C, h = 0.01 %, p = 0.1 hPa. One point per minute, up to 360 (6 h), kept in RAM.
