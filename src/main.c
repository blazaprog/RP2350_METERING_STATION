#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "sensor_data.h"
#include "modbus.h"
#include "web.h"

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
    // TODO: BME280 init (Bosch driver)

    spi_init(OLED_SPI, 8 * 1000 * 1000);
    gpio_set_function(OLED_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(OLED_MOSI, GPIO_FUNC_SPI);
    // TODO: SH1106 init (u8g2, 4-wire SPI; DC/CS/RES as GPIO). Note 2-column RAM offset.

    w6300_init();

    uint32_t last = 0;
    while (true) {
        if (time_us_32() - last > 1000000) {
            last = time_us_32();
            // TODO: bme280 read into g_data; draw on OLED
            g_data.status = 1;   // until the sensor is integrated
            publish();
            printf("alive %lu s, sensor status=%d (1 = not integrated yet)\n",
                   (unsigned long)(to_ms_since_boot(get_absolute_time()) / 1000), g_data.status);
        }
        net_poll();
    }
}
