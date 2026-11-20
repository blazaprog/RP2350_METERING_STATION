// ============================================================================
//  sensor_data.h - společná struktura s naměřenými hodnotami
//  Používá ji main.c (plní), web.c (zobrazuje) a net_w6300.c (předává dál).
// ============================================================================
#pragma once

typedef struct {
    float temp;    // teplota v °C
    float hum;     // relativní vlhkost v %
    float press;   // tlak vzduchu v hPa
    int   status;  // 0 = měření v pořádku, 1 = chyba senzoru
} sensor_data_t;
