#pragma once
#include <stdint.h>

// Calibration data (Bosch BME280 datasheet, section 4.2.2)
typedef struct {
    uint16_t T1; int16_t T2, T3;
    uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9;
    uint8_t  H1, H3; int16_t H2, H4, H5; int8_t H6;
} bme280_calib_t;

// Parse calibration from raw register dumps: a[26] = regs 0x88..0xA1, b[7] = regs 0xE1..0xE7
void bme280_parse_calib(bme280_calib_t *c, const uint8_t *a, const uint8_t *b);

// Integer compensation formulas from the datasheet.
int32_t  bme280_comp_temp(const bme280_calib_t *c, int32_t adc_T, int32_t *t_fine); // 0.01 degC
uint32_t bme280_comp_press(const bme280_calib_t *c, int32_t adc_P, int32_t t_fine); // Q24.8 Pa
uint32_t bme280_comp_hum(const bme280_calib_t *c, int32_t adc_H, int32_t t_fine);   // Q22.10 %RH
