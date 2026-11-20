#pragma once
#include <stdint.h>
#include "hardware/spi.h"

// SH1106 128x64, 4-wire SPI (D0=SCK, D1=MOSI, A0=DC, CS, RES)
void oled_init(spi_inst_t *spi, uint dc, uint cs, uint res);
void oled_clear(void);
// Draws text into the framebuffer at pixel (x, y); scale 1 = 5x7 px glyphs, 2 = 10x14 ...
void oled_text(int x, int y, const char *s, int scale);
void oled_flush(void);
