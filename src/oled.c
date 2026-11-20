#include <string.h>
#include "pico/stdlib.h"
#include "oled.h"
#include "oled_font.h"

#define W 128
#define H 64
#define COL_OFFSET 2   // SH1106 RAM is 132 wide, the panel shows columns 2..129

static spi_inst_t *s_spi;
static uint s_dc, s_cs, s_res;
static uint8_t s_fb[W * H / 8];   // page-organised: s_fb[page * W + x], bit0 = top pixel of the page

static void cmd(uint8_t c) {
    gpio_put(s_dc, 0); gpio_put(s_cs, 0);
    spi_write_blocking(s_spi, &c, 1);
    gpio_put(s_cs, 1);
}

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
    gpio_put(res, 1); sleep_ms(10);
    gpio_put(res, 0); sleep_ms(20);
    gpio_put(res, 1); sleep_ms(20);

    cmd(0xAE);              // display off
    cmd(0xD5); cmd(0x80);   // clock divide / oscillator
    cmd(0xA8); cmd(0x3F);   // multiplex ratio 64
    cmd(0xD3); cmd(0x00);   // display offset
    cmd(0x40);              // start line 0
    cmd(0xAD); cmd(0x8B);   // internal DC-DC on
    cmd(0x32);              // pump voltage 8.0 V
    cmd(0xA1);              // segment remap (flip with 0xA0 if the image is mirrored)
    cmd(0xC8);              // COM scan direction (flip with 0xC0 if the image is upside down)
    cmd(0xDA); cmd(0x12);   // COM pins configuration
    cmd(0x81); cmd(0xCF);   // contrast
    cmd(0xD9); cmd(0x22);   // pre-charge
    cmd(0xDB); cmd(0x35);   // VCOM deselect
    cmd(0xA4);              // follow RAM
    cmd(0xA6);              // normal (not inverted)
    oled_clear();
    oled_flush();
    cmd(0xAF);              // display on
}

void oled_clear(void) { memset(s_fb, 0, sizeof s_fb); }

static void pixel(int x, int y) {
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    s_fb[(y >> 3) * W + x] |= (uint8_t)(1u << (y & 7));
}

void oled_text(int x, int y, const char *s, int scale) {
    for (; *s; s++) {
        const uint8_t *g = oled_glyph(*s);
        for (int cx = 0; cx < 5; cx++)
            for (int cy = 0; cy < 7; cy++)
                if (g[cx] & (1u << cy))
                    for (int sx = 0; sx < scale; sx++)
                        for (int sy = 0; sy < scale; sy++)
                            pixel(x + cx * scale + sx, y + cy * scale + sy);
        x += 6 * scale;
    }
}

void oled_flush(void) {
    for (int page = 0; page < 8; page++) {
        cmd(0xB0 | page);
        cmd(0x00 | (COL_OFFSET & 0x0F));
        cmd(0x10 | (COL_OFFSET >> 4));
        data(&s_fb[page * W], W);
    }
}
