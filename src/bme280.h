// ============================================================================
//  bme280.h - ovladač senzoru teploty, vlhkosti a tlaku BME280 (sběrnice I2C)
// ============================================================================
#pragma once
#include <stdbool.h>
#include "hardware/i2c.h"

// Najde senzor na adrese 0x76 nebo 0x77 (ověří ID čipu 0x60) a načte kalibraci.
// Vrací true, pokud byl senzor nalezen.
bool bme280_init(i2c_inst_t *i2c);

// Provede jedno měření (režim "forced": senzor změří a znovu usne, šetří ohřev).
// Výstup: temp v °C, hum v % RH, press v hPa. Vrací false při chybě I2C.
bool bme280_read(float *temp, float *hum, float *press);
