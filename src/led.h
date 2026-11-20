// ============================================================================
//  led.h - ovládání RGB LED WS2812 na desce (pin GP25)
// ============================================================================
#pragma once
#include <stdint.h>

void led_init(void);                          // nastaví PIO a zhasne LED
void led_set(uint8_t r, uint8_t g, uint8_t b);// nastaví barvu LED (0 až 255 na složku)
void led_heartbeat(void);                     // volat v hlavní smyčce: pomalu bliká
