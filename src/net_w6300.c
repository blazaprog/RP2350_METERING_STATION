// W6300 Ethernet layer - STUB.
// The W6300 driver (WIZnet ioLibrary / Waveshare example) is not integrated yet.
// TODO: take the W6300 init + socket API from Waveshare's RP2350-POE-ETH examples,
//       then implement: w6300_init(), net_poll() (HTTP :80, Modbus TCP :502).
#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"

#define W6300_SPI   spi0
#define W6300_MISO  16
#define W6300_CS    17
#define W6300_SCK   18
#define W6300_MOSI  19
#define W6300_RST   20
#define W6300_INT   21

void w6300_init(void) {
    spi_init(W6300_SPI, 10 * 1000 * 1000);
    gpio_set_function(W6300_MISO, GPIO_FUNC_SPI);
    gpio_set_function(W6300_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(W6300_MOSI, GPIO_FUNC_SPI);
    gpio_init(W6300_CS);  gpio_set_dir(W6300_CS, GPIO_OUT);  gpio_put(W6300_CS, 1);
    gpio_init(W6300_RST); gpio_set_dir(W6300_RST, GPIO_OUT);
    gpio_put(W6300_RST, 0); sleep_ms(10); gpio_put(W6300_RST, 1); sleep_ms(100);
    gpio_init(W6300_INT); gpio_set_dir(W6300_INT, GPIO_IN);
    printf("W6300: SPI pins and reset done, driver not integrated yet\n");
}

void net_poll(void) {
    // TODO: socket handling
}
