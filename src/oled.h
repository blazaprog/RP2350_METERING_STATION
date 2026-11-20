// ============================================================================
//  oled.h - ovladač displeje 1.3" OLED (řadič SH1106, 128x64, 4drátové SPI)
//  Kreslí se nejdřív do paměti (framebuffer) a až oled_flush() ji pošle na displej.
// ============================================================================
#pragma once
#include <stdint.h>
#include "hardware/spi.h"

// Inicializuje displej. Parametry: SPI sběrnice a čísla GPIO pro DC, CS a RES.
void oled_init(spi_inst_t *spi, uint dc, uint cs, uint res);

// Smaže obsah paměti (celý displej bude černý po příštím oled_flush()).
void oled_clear(void);

// Vykreslí text do paměti na pozici (x, y) v pixelech. scale 1 = znak 5x7 px,
// scale 2 = 10x14 px atd. Zobrazí se až po oled_flush().
void oled_text(int x, int y, const char *s, int scale);

// Pošle obsah paměti na displej.
void oled_flush(void);

// Rozsvítí všechny pixely v paměti (test displeje). Zobrazí se po oled_flush().
void oled_fill(void);

// Nakreslí rámeček po obvodu displeje do paměti. Zobrazí se po oled_flush().
void oled_border(void);
