// ============================================================================
//  bme280.c - ovladač BME280 přes I2C (komunikace se senzorem)
//  Matematika (kompenzace) je odděleně v bme280_calc.c.
// ============================================================================
#include "pico/stdlib.h"
#include "bme280.h"
#include "bme280_calc.h"

// Adresy registrů senzoru (z datasheetu)
#define REG_ID        0xD0   // ID čipu: BME280 = 0x60 (BMP280 bez vlhkosti má 0x58)
#define REG_RESET     0xE0   // zápis 0xB6 = softwarový reset
#define REG_CTRL_HUM  0xF2   // nastavení měření vlhkosti
#define REG_CTRL_MEAS 0xF4   // nastavení měření teploty a tlaku + režim
#define REG_CONFIG    0xF5   // filtr a pauzy (nepoužíváme)
#define REG_DATA      0xF7   // začátek 8 bajtů naměřených dat (tlak, teplota, vlhkost)

static i2c_inst_t *s_i2c;    // použitá I2C sběrnice
static uint8_t s_addr;       // nalezená adresa senzoru (0x76 nebo 0x77)
static bme280_calib_t s_cal; // kalibrační konstanty načtené ze senzoru

// Přečte n bajtů od registru reg. Časový limit 20 ms ochrání před zaseknutím sběrnice.
static bool rd(uint8_t reg, uint8_t *buf, size_t n) {
    if (i2c_write_blocking_until(s_i2c, s_addr, &reg, 1, true, make_timeout_time_ms(20)) != 1) return false;
    return i2c_read_blocking_until(s_i2c, s_addr, buf, n, false, make_timeout_time_ms(20)) == (int)n;
}

// Zapíše jednu hodnotu do registru.
static bool wr(uint8_t reg, uint8_t val) {
    uint8_t b[2] = { reg, val };
    return i2c_write_blocking_until(s_i2c, s_addr, b, 2, false, make_timeout_time_ms(20)) == 2;
}

bool bme280_init(i2c_inst_t *i2c) {
    s_i2c = i2c;
    const uint8_t addrs[2] = { 0x76, 0x77 };            // senzor může být na jedné z obou adres
    for (int i = 0; i < 2; i++) {
        s_addr = addrs[i];
        uint8_t id = 0;
        if (rd(REG_ID, &id, 1) && id == 0x60) {          // našli jsme BME280
            wr(REG_RESET, 0xB6);                         // reset do výchozího stavu
            sleep_ms(10);
            uint8_t a[26], b[7];                         // načtení kalibrace (dvě oddělené oblasti registrů)
            if (!rd(0x88, a, 26) || !rd(0xE1, b, 7)) return false;
            bme280_parse_calib(&s_cal, a, b);
            wr(REG_CONFIG, 0x00);                        // bez filtru
            return true;
        }
    }
    return false;                                        // na žádné adrese neodpovídá
}

bool bme280_read(float *temp, float *hum, float *press) {
    if (!wr(REG_CTRL_HUM, 0x01)) return false;           // vlhkost: 1x oversampling (musí se zapsat před ctrl_meas)
    if (!wr(REG_CTRL_MEAS, 0x25)) return false;          // teplota 1x, tlak 1x, režim forced (jedno měření)
    sleep_ms(15);                                        // měření trvá nejvýše asi 10 ms
    uint8_t d[8];
    if (!rd(REG_DATA, d, 8)) return false;               // přečteme tlak, teplotu a vlhkost najednou

    // Složení 20bitových (tlak, teplota) a 16bitové (vlhkost) surové hodnoty z bajtů.
    int32_t adc_P = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
    int32_t adc_T = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
    int32_t adc_H = ((int32_t)d[6] << 8) | d[7];

    // Kompenzace: nejdřív teplota (dává t_fine), z ní pak tlak a vlhkost.
    int32_t t_fine;
    int32_t t = bme280_comp_temp(&s_cal, adc_T, &t_fine);
    uint32_t p = bme280_comp_press(&s_cal, adc_P, t_fine);
    uint32_t h = bme280_comp_hum(&s_cal, adc_H, t_fine);

    *temp  = t / 100.0f;             // setiny °C -> °C
    *press = p / 256.0f / 100.0f;    // Q24.8 Pa -> Pa -> hPa
    *hum   = h / 1024.0f;            // Q22.10 -> % RH
    return true;
}
