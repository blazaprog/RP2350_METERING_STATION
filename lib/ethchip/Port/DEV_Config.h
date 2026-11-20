// Minimal replacement of Waveshare's DEV_Config.h: only what the W6300 port layer needs.
#pragma once
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

/* W6300 on RP2350-POE-ETH (verified against the Waveshare schematic) */
#define ETHCHIP_SPI_PORT    spi0
#define ETHCHIP_SPI_CLK     70  /* MHz requested, the real clock is limited by clk_peri */

#define ETHCHIP_PIN_SCK     18
#define ETHCHIP_PIN_MOSI    19
#define ETHCHIP_PIN_MISO    16
#define ETHCHIP_PIN_CS      17
#define ETHCHIP_PIN_RST     20
#define ETHCHIP_PIN_INT     21
