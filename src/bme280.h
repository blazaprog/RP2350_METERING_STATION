#pragma once
#include <stdbool.h>
#include "hardware/i2c.h"

// Detects a BME280 at 0x76 or 0x77 (chip id 0x60) and loads calibration.
bool bme280_init(i2c_inst_t *i2c);
// Forced-mode measurement. Returns false on I2C error.
// temp in degC, hum in %RH, press in hPa.
bool bme280_read(float *temp, float *hum, float *press);
