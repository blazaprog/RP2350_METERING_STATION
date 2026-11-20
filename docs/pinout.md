# Pinout (RP2350-POE-ETH)

| Component | Signal | GPIO |
|---|---|---|
| W6300 (SPI0) | MISO / CSn / SCK / MOSI | GP16 / GP17 / GP18 / GP19 |
| W6300 | RSTn / INTn | GP20 / GP21 |
| OLED 1.3" SH1106 (SPI1, 4-wire) | SCK / MOSI(SDA) / CS / DC / RES | GP10 / GP11 / GP9 / GP8 / GP12 |
| BME280 (I2C0) | SDA / SCL | GP4 / GP5 |

Reserved by the board: GP23 POE_DET, GP24 USB_DET, GP25 WS2812, GP29 ADC3.

## Modbus TCP (port 502), FC 03/04
| Addr | Value | Unit |
|---|---|---|
| 0 | Temperature (int16) | 0.1 C |
| 1 | Humidity (uint16) | 0.1 %RH |
| 2 | Pressure (uint16) | hPa |
| 3 | Status (0 OK, 1 sensor error) | - |
| 4-5 | Temperature (float32) | C |
| 6-7 | Humidity (float32) | %RH |
| 8-9 | Pressure (float32) | hPa |

Floats are IEEE 754, high word first (ABCD); set `MODBUS_FLOAT_WORD_SWAP 1` in `src/config.h` for CDAB. Floats are NaN when the sensor fails.

## HTTP
- `GET /` HTML page (cards + 6 h charts, source in `src/web/index.html`).
- `GET /api/data` JSON: temperature, humidity, pressure (2 decimals, null on sensor error), status, uptime_s, ip, mac, fw.
- `GET /api/history` JSON: `{"step":60,"n":N,"t":[...],"h":[...],"p":[...]}`, oldest first; t = 0.01 C, h = 0.01 %, p = 0.1 hPa. One point per minute, up to 360 (6 h), kept in RAM.
