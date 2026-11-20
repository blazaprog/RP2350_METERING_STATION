// ============================================================================
//  diag_hw.c - diagnostika, která potřebuje hardware RP2350
// ============================================================================
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include "diag.h"

diag_t g_diag;
static bool s_wd_reboot;

void diag_init(void) {
    s_wd_reboot = watchdog_enable_caused_reboot();   // musí se zjistit dřív, než se watchdog znovu zapne
    adc_init();
    adc_set_temp_sensor_enabled(true);               // vnitřní teploměr čipu
}

uint32_t diag_uptime_s(void) { return to_ms_since_boot(get_absolute_time()) / 1000; }

// Teplota čipu z vnitřního snímače. Vzorec je z datasheetu RP2350 (orientační, ±několik °C).
float diag_chip_temp(void) {
    adc_select_input(ADC_TEMPERATURE_CHANNEL_NUM);
    float v = adc_read() * 3.3f / 4096.0f;
    return 27.0f - (v - 0.706f) / 0.001721f;
}

bool diag_watchdog_reboot(void) { return s_wd_reboot; }

void diag_reboot(void) {
    watchdog_reboot(0, 0, 50);                       // restart za 50 ms
    while (true) tight_loop_contents();
}
