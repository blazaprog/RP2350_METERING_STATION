#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "sensor_data.h"
#include "modbus.h"
#include "web.h"
#include "bme280.h"
#include "oled.h"

// ---- Pin map (verified against Waveshare RP2350-POE-ETH schematic) ----
#define BME_I2C      i2c0
#define BME_SDA      4
#define BME_SCL      5
#define OLED_SPI     spi1
#define OLED_DC      8
#define OLED_CS      9
#define OLED_SCK     10
#define OLED_MOSI    11
#define OLED_RES     12

void w6300_init(void);
void net_poll(void);

sensor_data_t g_data;
uint16_t g_regs[4];   // 0 temp*10, 1 hum*10, 2 pressure hPa, 3 status

static void publish(void) {
    g_regs[0] = (uint16_t)(int16_t)(g_data.temp * 10);
    g_regs[1] = (uint16_t)(g_data.hum * 10);
    g_regs[2] = (uint16_t)g_data.press;
    g_regs[3] = (uint16_t)g_data.status;
}

int main(void) {
    stdio_init_all();
    // Wait up to 8 s for a USB serial terminal so the startup messages are not lost.
    for (int i = 0; i < 80 && !stdio_usb_connected(); i++) sleep_ms(100);
    sleep_ms(200);
    printf("\n=== RP2350 metering station boot ===\n");

    i2c_init(BME_I2C, 400 * 1000);
    gpio_set_function(BME_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BME_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BME_SDA);
    gpio_pull_up(BME_SCL);
    bool bme_ok = bme280_init(BME_I2C);
    printf("BME280: %s\n", bme_ok ? "found" : "NOT found (check wiring: SDA GP4, SCL GP5, 3V3, GND)");

    spi_init(OLED_SPI, 4 * 1000 * 1000);
    gpio_set_function(OLED_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(OLED_MOSI, GPIO_FUNC_SPI);
    oled_init(OLED_SPI, OLED_DC, OLED_CS, OLED_RES);

    w6300_init();

    uint32_t last = 0;
    while (true) {
        if (time_us_32() - last > 1000000) {
            last = time_us_32();
            if (!bme_ok) bme_ok = bme280_init(BME_I2C);   // retry if the sensor appears later
            float t, h, p;
            if (bme_ok && bme280_read(&t, &h, &p)) {
                g_data.temp = t; g_data.hum = h; g_data.press = p; g_data.status = 0;
            } else {
                g_data.status = 1;
                bme_ok = false;
            }
            publish();
            char l1[16], l2[16], l3[24];
            oled_clear();
            if (g_data.status == 0) {
                snprintf(l1, sizeof l1, "T:%.1fC", g_data.temp);
                snprintf(l2, sizeof l2, "H:%.1f%%", g_data.hum);
                snprintf(l3, sizeof l3, "P:%.1fhPa", g_data.press);
            } else {
                snprintf(l1, sizeof l1, "SENSOR");
                snprintf(l2, sizeof l2, "ERROR");
                l3[0] = 0;
            }
            oled_text(0, 2,  l1, 2);
            oled_text(0, 24, l2, 2);
            oled_text(0, 46, l3, 2);
            oled_flush();
            if (g_data.status == 0)
                printf("T=%.2f C  RH=%.1f %%  P=%.1f hPa\n", g_data.temp, g_data.hum, g_data.press);
            else
                printf("sensor error (status=1)\n");
        }
        net_poll();
    }
}
