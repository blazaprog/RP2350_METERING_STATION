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

// Bitmapové písmo: šířka a výška znaku v pixelech a tabulka znaků (ASCII 32 až 126 a stupeň).
// Jeden řádek znaku zabírá (w+7)/8 bajtů, nejlevější pixel je nejvyšší bit.
typedef struct { uint8_t w, h; const uint8_t *data; } oled_font_t;

// Dostupná písma (Spleen, viz src/oled_fonts.c): číslo = šířka x výška znaku v pixelech.
extern const oled_font_t font_5x8, font_6x12, font_8x16, font_12x24;

// Vykreslí text zvoleným písmem na pozici (x, y). Text je v UTF-8, podporuje znak ° (zapisuje se "\xC2\xB0").
void oled_text_font(int x, int y, const char *s, const oled_font_t *f);

// Šířka textu v pixelech pro dané písmo (např. pro zarovnání doprava).
int oled_text_width(const char *s, const oled_font_t *f);

// Nastaví jas displeje v procentech: 0 displej vypne, 1 až 100 ho zapne a nastaví jas.
void oled_set_brightness(uint8_t percent);

// Nastaví směr zobrazení: x = 1 výchozí směr sloupců, 0 zrcadlově; y = 1 výchozí směr řádků, 0 vzhůru nohama.
void oled_set_flip(uint8_t x, uint8_t y);
