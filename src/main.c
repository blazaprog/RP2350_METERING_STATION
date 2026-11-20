// ============================================================================
//  main.c - hlavní program měřiče teploty, vlhkosti a tlaku
//
//  Hardware: deska RP2350-POE-ETH (Ethernet W6300, napájení PoE),
//            senzor BME280 (I2C) a displej 1.3" OLED SH1106 (SPI).
//
//  Co program dělá:
//    1. Jednou za sekundu změří teplotu, vlhkost a tlak (BME280).
//    2. Zobrazí hodnoty a stav sítě na displeji.
//    3. Vystaví je přes Ethernet: webová stránka (port 80), JSON (/api/data)
//       a Modbus TCP (port 502).
//    4. Celou dobu průběžně obsluhuje síť (net_poll), takže se nikdy nezasekne.
//    5. Indikační LED na desce pomalu bliká (znamení, že firmware běží).
// ============================================================================
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"

#include "config.h"
#include "sensor_data.h"
#include "bme280.h"
#include "oled.h"
#include "net_w6300.h"
#include "led.h"

// ---- Zapojení periferií (ověřeno podle schématu Waveshare RP2350-POE-ETH) ----
// Poznámka: Ethernet W6300 používá SPI0 a GP16 až GP21 (viz lib/ethchip/Port/DEV_Config.h).
#define BME_I2C      i2c0     // senzor BME280 na sběrnici I2C0
#define BME_SDA      4        //   data
#define BME_SCL      5        //   hodiny
#define OLED_SPI     spi1     // displej na sběrnici SPI1
#define OLED_DC      8        //   příkaz/data
#define OLED_CS      9        //   výběr čipu
#define OLED_SCK     10       //   hodiny
#define OLED_MOSI    11       //   data (na displeji označeno SDA)
#define OLED_RES     12       //   reset

// ---- Sdílená data (používá je i síťová vrstva v net_w6300.c) ----
sensor_data_t g_data;         // poslední naměřené hodnoty
uint16_t g_regs[4];           // Modbus registry: 0 teplota*10, 1 vlhkost*10, 2 tlak [hPa], 3 stav

// Převede naměřené hodnoty do Modbus registrů (celá čísla, teplota a vlhkost v desetinách).
static void publish(void) {
    g_regs[0] = (uint16_t)(int16_t)(g_data.temp * 10);   // teplota se znaménkem (záporné hodnoty!)
    g_regs[1] = (uint16_t)(g_data.hum * 10);
    g_regs[2] = (uint16_t)g_data.press;
    g_regs[3] = (uint16_t)g_data.status;
}

// Vykreslí hodnoty a stav sítě na displej.
static void draw_display(void) {
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
    oled_text(0, 0,  l1, 2);                  // tři řádky velkým písmem (2x)
    oled_text(0, 17, l2, 2);
    oled_text(0, 34, l3, 2);
    oled_text(0, 55, net_status_text(), 1);   // dole malým písmem: IP adresa nebo stav sítě
    oled_flush();
}

int main(void) {
    stdio_init_all();                         // sériový výstup přes USB (pro ladění)
    // Počkáme, až se připojí sériový monitor, aby se neztratily úvodní hlášky.
    for (int i = 0; i < DEBUG_USB_WAIT_MS / 100 && !stdio_usb_connected(); i++) sleep_ms(100);
    sleep_ms(200);
    printf("\n=== RP2350 metering station boot ===\n");

    // --- Senzor BME280 na I2C ---
    i2c_init(BME_I2C, 400 * 1000);            // rychlost sběrnice 400 kHz
    gpio_set_function(BME_SDA, GPIO_FUNC_I2C);
    gpio_set_function(BME_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(BME_SDA);                    // vnitřní pull-up rezistory (modul je má obvykle i vlastní)
    gpio_pull_up(BME_SCL);
    bool bme_ok = bme280_init(BME_I2C);
    printf("BME280: %s\n", bme_ok ? "found" : "NOT found (check wiring: SDA GP4, SCL GP5, 3V3, GND)");

    // --- Displej na SPI ---
    spi_init(OLED_SPI, OLED_SPI_HZ);          // rychlost SPI se nastavuje v config.h (4 MHz je bezpečná)
    gpio_set_function(OLED_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(OLED_MOSI, GPIO_FUNC_SPI);
    oled_init(OLED_SPI, OLED_DC, OLED_CS, OLED_RES);
    printf("OLED: init done (flipX=%d flipY=%d offset=%d)\n", OLED_FLIP_X, OLED_FLIP_Y, OLED_COL_OFFSET);

    // Úvodní test displeje: nejdřív svítí všechny pixely, pak rámeček s nápisem.
    // Podle něj poznáš, jestli displej funguje, je správně otočený a nemá posunuté sloupce.
    if (OLED_SPLASH_MS > 0) {
        oled_fill();   oled_flush();   sleep_ms(OLED_SPLASH_MS / 2);
        oled_clear();  oled_border();
        oled_text(14, 20, "OLED OK", 2);
        oled_flush();  sleep_ms(OLED_SPLASH_MS / 2);
    }

    // --- Ethernet (neblokuje: bez kabelu program normálně běží dál) ---
    w6300_init();

    led_init();                               // indikační LED na desce (WS2812, GP25)

    uint32_t last = 0;
    while (true) {
        led_heartbeat();                      // pomalé blikání LED (neblokující)

        // Jednou za sekundu: měření, aktualizace displeje a výpis na sériový výstup.
        if (time_us_32() - last > 1000000) {
            last = time_us_32();

            if (!bme_ok) bme_ok = bme280_init(BME_I2C);       // senzor mohl být připojen dodatečně
            float t, h, p;
            if (bme_ok && bme280_read(&t, &h, &p)) {
                g_data.temp = t; g_data.hum = h; g_data.press = p; g_data.status = 0;
            } else {
                g_data.status = 1;                            // chyba senzoru
                bme_ok = false;                               // příště zkusíme senzor najít znovu
            }
            publish();
            draw_display();

            if (g_data.status == 0)
                printf("T=%.2f C  RH=%.1f %%  P=%.1f hPa  [%s]\n",
                       g_data.temp, g_data.hum, g_data.press, net_status_text());
            else
                printf("sensor error (status=1)  [%s]\n", net_status_text());
        }

        // Obsluha sítě: musí se volat co nejčastěji (proto je mimo sekundový blok výše).
        net_poll();
    }
}
