// ============================================================================
//  led.c - ovládání RGB LED WS2812 na desce RP2350-POE-ETH (pin GP25)
//
//  LED slouží jako "tep" zařízení: pomalu bliká, dokud hlavní smyčka běží.
//  Když by firmware zamrzl, blikání se zastaví - je to rychlá kontrola, že žije.
//  Blikání je neblokující (nepoužívá sleep), takže nezdržuje síť ani měření.
// ============================================================================
#include "led.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "led.pio.h"          // vygeneruje se při sestavení z led.pio
#include "config.h"

static PIO  s_pio = pio0;     // použitý blok PIO
static uint s_sm;             // číslo stavového automatu

// Nastaví PIO a zhasne LED.
void led_init(void) {
    s_sm = pio_claim_unused_sm(s_pio, true);
    uint offset = pio_add_program(s_pio, &led_program);
    led_program_init(s_pio, s_sm, offset, LED_PIN, 800000);
    led_set(0, 0, 0);
}

// Odešle barvu do LED. Pořadí složek odpovídá ukázce Waveshare pro tuto desku.
void led_set(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t px = ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8);
    pio_sm_put_blocking(s_pio, s_sm, px);
}

// Přepíná LED zapnuto/vypnuto po LED_BLINK_MS milisekundách (neblokující).
void led_heartbeat(void) {
    static uint32_t last_ms = 0;
    static bool on = false;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_ms >= LED_BLINK_MS) {
        last_ms = now;
        on = !on;
        if (on) led_set(LED_ON_R, LED_ON_G, LED_ON_B);   // svítí
        else    led_set(0, 0, 0);                        // zhasnuto
    }
}
