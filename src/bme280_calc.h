// ============================================================================
//  bme280_calc.h - výpočty (kompenzace) surových hodnot senzoru BME280
//  Čistá matematika bez závislosti na hardwaru. Vzorce jsou z datasheetu Bosch.
// ============================================================================
#pragma once
#include <stdint.h>

// Kalibrační konstanty uložené ve výrobě v paměti senzoru (datasheet BME280, kap. 4.2.2).
typedef struct {
    uint16_t T1; int16_t T2, T3;                                        // teplota
    uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9;                // tlak
    uint8_t  H1, H3; int16_t H2, H4, H5; int8_t H6;                     // vlhkost
} bme280_calib_t;

// Převede surový výpis kalibračních registrů do struktury.
//   a = 26 bajtů z registrů 0x88 až 0xA1, b = 7 bajtů z registrů 0xE1 až 0xE7
void bme280_parse_calib(bme280_calib_t *c, const uint8_t *a, const uint8_t *b);

// Kompenzace z surové hodnoty ADC na fyzikální veličinu (celočíselné vzorce z datasheetu).
int32_t  bme280_comp_temp(const bme280_calib_t *c, int32_t adc_T, int32_t *t_fine); // v setinách °C
uint32_t bme280_comp_press(const bme280_calib_t *c, int32_t adc_P, int32_t t_fine); // Pa ve formátu Q24.8 (dělit 256)
uint32_t bme280_comp_hum(const bme280_calib_t *c, int32_t adc_H, int32_t t_fine);   // %RH ve formátu Q22.10 (dělit 1024)
