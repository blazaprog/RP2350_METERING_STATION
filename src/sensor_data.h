#pragma once

typedef struct {
    float temp;   // deg C
    float hum;    // % RH
    float press;  // hPa
    int   status; // 0 = OK, 1 = sensor error
} sensor_data_t;
