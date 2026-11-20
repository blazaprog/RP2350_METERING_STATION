// ============================================================================
//  oled.c - ovladač SH1106 přes 4drátové SPI
//
//  Signály displeje: SCK (hodiny), SDA = MOSI (data), CS (výběr čipu, aktivní v 0),
//  DC (0 = bajt je příkaz, 1 = bajt jsou data pro obraz), RES (reset, aktivní v 0).
//
//  Paměť obrazu je rozdělená do 8 "stránek" po 8 řádcích pixelů; jeden bajt je
//  svislý pruh 8 pixelů (nejnižší bit nahoře).
// ============================================================================
#include <string.h>
#include "pico/stdlib.h"
#include "oled.h"
#include "oled_font.h"
#include "config.h"

#define W 128                 // šířka displeje v pixelech
#define H 64                  // výška displeje v pixelech
#define COL_OFFSET OLED_COL_OFFSET   // SH1106 má v paměti 132 sloupců, panel zobrazuje 128 z nich (viz config.h)

static spi_inst_t *s_spi;     // použitá SPI sběrnice
static uint s_dc, s_cs, s_res;
static uint8_t s_fb[W * H / 8];   // framebuffer: s_fb[stránka * W + x], bit0 = horní pixel stránky

// Pošle jeden příkaz (DC = 0).
static void cmd(uint8_t c) {
    gpio_put(s_dc, 0); gpio_put(s_cs, 0);
    spi_write_blocking(s_spi, &c, 1);
    gpio_put(s_cs, 1);
}

// Pošle blok dat pro obraz (DC = 1).
static void data(const uint8_t *d, size_t n) {
    gpio_put(s_dc, 1); gpio_put(s_cs, 0);
    spi_write_blocking(s_spi, d, n);
    gpio_put(s_cs, 1);
}

void oled_init(spi_inst_t *spi, uint dc, uint cs, uint res) {
    s_spi = spi; s_dc = dc; s_cs = cs; s_res = res;
    gpio_init(dc);  gpio_set_dir(dc, GPIO_OUT);
    gpio_init(cs);  gpio_set_dir(cs, GPIO_OUT);  gpio_put(cs, 1);
    gpio_init(res); gpio_set_dir(res, GPIO_OUT);

    // Hardwarový reset: krátký puls do nuly.
    gpio_put(res, 1); sleep_ms(10);
    gpio_put(res, 0); sleep_ms(20);
    gpio_put(res, 1); sleep_ms(20);

    // Inicializační sekvence. Příkazy 0xAD, 0x32, 0x40, 0x81, 0xA0/A1, 0xA4-0xA8 jsou ověřené
    // podle datasheetu SH1106; ostatní (D5, D3, DA, D9, DB, C8) jsou běžná nastavení pro tento řadič.
    cmd(0xAE);              // displej vypnout během nastavování
    cmd(0xD5); cmd(0x80);   // frekvence oscilátoru
    cmd(0xA8); cmd(0x3F);   // multiplex 64 řádků
    cmd(0xD3); cmd(0x00);   // posun obrazu = 0
    cmd(0x40);              // první řádek paměti = první řádek displeje
    cmd(0xAD); cmd(0x8B);   // zapnout vnitřní měnič napětí pro panel
    cmd(0x32);              // napětí měniče 8,0 V
    cmd(OLED_FLIP_X ? 0xA1 : 0xA0);   // směr sloupců (nastavuje se v config.h)
    cmd(OLED_FLIP_Y ? 0xC8 : 0xC0);   // směr řádků (nastavuje se v config.h)
    cmd(0xDA); cmd(0x12);   // zapojení COM vývodů panelu
    cmd(0x81); cmd(OLED_CONTRAST);    // kontrast (jas)
    cmd(0xD9); cmd(0x22);   // doba přednabíjení
    cmd(0xDB); cmd(0x35);   // napětí VCOM
    cmd(0xA4);              // zobrazovat obsah paměti
    cmd(0xA6);              // normální (neinvertovaný) obraz
    oled_clear();
    oled_flush();           // nejdřív pošleme prázdný obraz, ať neblikne šum
    cmd(0xAF);              // displej zapnout
}

void oled_clear(void) { memset(s_fb, 0, sizeof s_fb); }

// Rozsvítí všechny pixely (test displeje: musí svítit celá plocha bez chybějících pruhů).
void oled_fill(void) { memset(s_fb, 0xFF, sizeof s_fb); }


// Rozsvítí jeden pixel v paměti (mimo displej se ignoruje).
static void pixel(int x, int y) {
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    s_fb[(y >> 3) * W + x] |= (uint8_t)(1u << (y & 7));
}

// Nakreslí rámeček po obvodu displeje; podle něj poznáš posun, ořez a zrcadlení obrazu.
void oled_border(void) {
    for (int x = 0; x < W; x++) { pixel(x, 0); pixel(x, H - 1); }
    for (int y = 0; y < H; y++) { pixel(0, y); pixel(W - 1, y); }
}

void oled_text(int x, int y, const char *s, int scale) {
    for (; *s; s++) {
        const uint8_t *g = oled_glyph(*s);               // 5 sloupců bitmapy znaku
        for (int cx = 0; cx < 5; cx++)
            for (int cy = 0; cy < 7; cy++)
                if (g[cx] & (1u << cy))                  // pixel znaku je zapnutý?
                    for (int sx = 0; sx < scale; sx++)   // zvětšení: pixel nahradíme čtvercem
                        for (int sy = 0; sy < scale; sy++)
                            pixel(x + cx * scale + sx, y + cy * scale + sy);
        x += 6 * scale;                                  // 5 sloupců znaku + 1 sloupec mezera
    }
}

void oled_flush(void) {
    for (int page = 0; page < 8; page++) {
        cmd(0xB0 | page);                                // výběr stránky (řádku po 8 pixelech)
        cmd(0x00 | (COL_OFFSET & 0x0F));                 // počáteční sloupec, dolní 4 bity
        cmd(0x10 | (COL_OFFSET >> 4));                   // počáteční sloupec, horní bity
        data(&s_fb[page * W], W);                        // 128 bajtů této stránky
    }
}
