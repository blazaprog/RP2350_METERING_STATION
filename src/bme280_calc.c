// ============================================================================
//  bme280_calc.c - kompenzační vzorce senzoru BME280
//  Surové hodnoty z ADC senzoru jsou nepřesné; každý kus má ve výrobě změřené
//  kalibrační konstanty a vzorcem z datasheetu se z nich spočítá správná hodnota.
//  Vzorce jsou beze změny převzaté z datasheetu Bosch (celočíselná verze).
// ============================================================================
#include "bme280_calc.h"

// Složí 16bitové hodnoty z dvojic bajtů (nižší bajt je první - "little endian").
// Pozor na registry vlhkosti H4 a H5: sdílejí jeden bajt (0xE5), každá používá 4 bity.
void bme280_parse_calib(bme280_calib_t *c, const uint8_t *a, const uint8_t *b) {
    c->T1 = (uint16_t)(a[0] | (a[1] << 8));
    c->T2 = (int16_t)(a[2] | (a[3] << 8));
    c->T3 = (int16_t)(a[4] | (a[5] << 8));
    c->P1 = (uint16_t)(a[6] | (a[7] << 8));
    c->P2 = (int16_t)(a[8] | (a[9] << 8));
    c->P3 = (int16_t)(a[10] | (a[11] << 8));
    c->P4 = (int16_t)(a[12] | (a[13] << 8));
    c->P5 = (int16_t)(a[14] | (a[15] << 8));
    c->P6 = (int16_t)(a[16] | (a[17] << 8));
    c->P7 = (int16_t)(a[18] | (a[19] << 8));
    c->P8 = (int16_t)(a[20] | (a[21] << 8));
    c->P9 = (int16_t)(a[22] | (a[23] << 8));
    c->H1 = a[25];
    c->H2 = (int16_t)(b[0] | (b[1] << 8));
    c->H3 = b[2];
    c->H4 = (int16_t)(((int16_t)(int8_t)b[3] * 16) | (b[4] & 0x0F));
    c->H5 = (int16_t)(((int16_t)(int8_t)b[5] * 16) | (b[4] >> 4));
    c->H6 = (int8_t)b[6];
}

// Teplota. Vedlejším výsledkem je t_fine, které potřebují i výpočty tlaku a vlhkosti.
int32_t bme280_comp_temp(const bme280_calib_t *c, int32_t adc_T, int32_t *t_fine) {
    int32_t var1 = ((((adc_T >> 3) - ((int32_t)c->T1 << 1))) * ((int32_t)c->T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - (int32_t)c->T1) * ((adc_T >> 4) - (int32_t)c->T1)) >> 12)
                    * (int32_t)c->T3) >> 14;
    *t_fine = var1 + var2;
    return (*t_fine * 5 + 128) >> 8;
}

// Tlak (64bitový výpočet kvůli rozsahu mezivýsledků).
uint32_t bme280_comp_press(const bme280_calib_t *c, int32_t adc_P, int32_t t_fine) {
    int64_t var1 = (int64_t)t_fine - 128000;
    int64_t var2 = var1 * var1 * (int64_t)c->P6;
    var2 = var2 + ((var1 * (int64_t)c->P5) << 17);
    var2 = var2 + (((int64_t)c->P4) << 35);
    var1 = ((var1 * var1 * (int64_t)c->P3) >> 8) + ((var1 * (int64_t)c->P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)c->P1) >> 33;
    if (var1 == 0) return 0;
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)c->P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)c->P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)c->P7) << 4);
    return (uint32_t)p;
}

// Vlhkost (výsledek omezíme na platný rozsah 0 až 100 %).
uint32_t bme280_comp_hum(const bme280_calib_t *c, int32_t adc_H, int32_t t_fine) {
    int32_t v = t_fine - 76800;
    v = (((((adc_H << 14) - (((int32_t)c->H4) << 20) - (((int32_t)c->H5) * v)) + 16384) >> 15)
         * (((((((v * ((int32_t)c->H6)) >> 10) * (((v * ((int32_t)c->H3)) >> 11) + 32768)) >> 10)
               + 2097152) * ((int32_t)c->H2) + 8192) >> 14));
    v = v - (((((v >> 15) * (v >> 15)) >> 7) * ((int32_t)c->H1)) >> 4);
    v = v < 0 ? 0 : v;
    v = v > 419430400 ? 419430400 : v;
    return (uint32_t)(v >> 12);
}
