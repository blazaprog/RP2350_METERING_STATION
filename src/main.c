// ============================================================================
//  main.c - hlavní program měřiče teploty, vlhkosti a tlaku
//
//  Hardware: deska RP2350-POE-ETH (Ethernet W6300, napájení PoE),
//            senzor BME280 (I2C) a displej 1.3" OLED SH1106 (SPI).
//
//  Co program dělá:
//    1. Několikrát za sekundu (MEASURE_PERIOD_MS) změří teplotu, vlhkost a tlak (BME280)
//       a přičte kalibrační posun z nastavení.
//    2. Zobrazí hodnoty a stav sítě na displeji.
//    3. Vystaví je přes Ethernet: webová stránka (port 80), JSON (/api/data),
//       Modbus TCP (port 502) a servisní kanál (port 2323).
//    4. Celou dobu průběžně obsluhuje síť (net_poll), takže se nikdy nezasekne;
//       kdyby se přesto zasekl, hlídací obvod (watchdog) ho po WATCHDOG_MS restartuje.
//    5. Indikační LED na desce pomalu bliká (znamení, že firmware běží).
//    6. Nastavení (síť, kalibrace, displej, ...) je uložené ve flash (settings.h).
// ============================================================================
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"

#include "config.h"
#include "sensor_data.h"
#include "bme280.h"
#include "oled.h"
#include "net_w6300.h"
#include "led.h"
#include "history.h"
#include "settings.h"
#include "diag.h"
#include "service.h"

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
sensor_data_t g_data;           // poslední naměřené hodnoty (po kalibraci)
uint16_t g_regs[MODBUS_NREGS];  // Modbus registry (úplná mapa je v docs/modbus.md)

// Mapa Modbus registrů (funkce 03 i 04, každý registr má 16 bitů):
//   0      teplota v desetinách °C (int16, se znaménkem)
//   1      vlhkost v desetinách % (uint16)
//   2      tlak v hPa (uint16)
//   3      stav: 0 = OK, 1 = chyba senzoru
//   4-5    teplota [°C]   jako float (IEEE 754, 32 bitů ve dvou registrech)
//   6-7    vlhkost [%]    jako float
//   8-9    tlak [hPa]     jako float
//   10     jas displeje v % (ČTENÍ I ZÁPIS, 0 = vypnuto)
//   11     jas LED v %      (ČTENÍ I ZÁPIS, 0 = zhasnuto)
//   12-15  rezerva (0)
//   16-30  diagnostika (jen čtení), viz diag.c
// Při chybě senzoru jsou floaty NaN (0x7FC00000), aby si klient nevzal starou hodnotu za platnou.

// Uloží číslo typu float do dvou po sobě jdoucích registrů od indexu idx.
static void put_float(int idx, float v) {
    uint32_t u;
    memcpy(&u, &v, sizeof u);                 // bitová podoba floatu (IEEE 754)
    uint16_t hi = (uint16_t)(u >> 16);        // horní slovo (znaménko, exponent, začátek mantisy)
    uint16_t lo = (uint16_t)(u & 0xFFFF);     // dolní slovo
    if (g_set.mb_swap) { g_regs[idx] = lo; g_regs[idx + 1] = hi; }   // prohozená slova (CDAB), z nastavení
    else               { g_regs[idx] = hi; g_regs[idx + 1] = lo; }   // standardní pořadí (ABCD)
}

// Převede naměřené hodnoty a stav do Modbus registrů.
static void publish(void) {
    g_regs[0] = (uint16_t)(int16_t)(g_data.temp * 10);   // teplota se znaménkem (záporné hodnoty!)
    g_regs[1] = (uint16_t)(g_data.hum * 10);
    g_regs[2] = (uint16_t)g_data.press;
    g_regs[3] = (uint16_t)g_data.status;
    if (g_data.status == 0) {
        put_float(4, g_data.temp);
        put_float(6, g_data.hum);
        put_float(8, g_data.press);
    } else {
        put_float(4, NAN);                    // chyba senzoru: "není číslo"
        put_float(6, NAN);
        put_float(8, NAN);
    }
    g_regs[MB_REG_DISP_BRIGHT] = g_set.disp_bright;      // zapisovatelné registry ukazují aktuální nastavení
    g_regs[MB_REG_LED_LEVEL]   = g_set.led_level;
    diag_fill_regs(g_regs);                              // diagnostika (registry 16 až 30)
}

// Zápis do registru z Modbusu (funkce 06 a 16). Zapisovat jde jen jas displeje a LED.
// Změna se projeví hned a do flash se uloží až po chvíli klidu (šetří flash při častém zápisu).
// commit = false jen zkontroluje, commit = true provede. Vrací 0 = v pořádku, 2 = neplatná adresa, 3 = neplatná hodnota.
int g_regs_write(uint16_t addr, uint16_t value, bool commit) {
    uint8_t *target;
    switch (addr) {
    case MB_REG_DISP_BRIGHT: target = &g_set.disp_bright; break;
    case MB_REG_LED_LEVEL:   target = &g_set.led_level;   break;
    default: return 2;                                   // ostatní registry jsou jen pro čtení
    }
    if (value > 100) return 3;                           // jas je v procentech
    if (commit && *target != value) {
        *target = (uint8_t)value;
        settings_save_later();                           // zapíše se do flash po SETTINGS_SAVE_DELAY_MS
    }
    return 0;
}

// Zbývající sekundy do hardwarového resetu nastavení (0 = odpočet neběží). Nastavuje factory_reset_pin_task().
static int s_reset_left_s = 0;

// Vykreslí hodnoty a stav sítě na displej.
// Rozložení (128x64 px): nahoře velká teplota, pod ní vlhkost a tlak, dole malým písmem stav sítě/IP.
static void draw_display(void) {
    char l1[20], l2[20], l3[20];
    oled_clear();
    if (s_reset_left_s > 0) {                             // odpočet do resetu nastavení přebije běžné zobrazení
        snprintf(l1, sizeof l1, "RESET %d", s_reset_left_s);
        oled_text_font(0, 4, l1, &font_12x24);
        oled_text_font(0, 34, "Pust pin pro", &font_8x16);
        oled_text_font(0, 48, "zruseni", &font_8x16);
        oled_flush();
        return;
    }
    if (g_data.status == 0) {
        // "\xC2\xB0" je znak ° v UTF-8 (zapsaný zvlášť, aby se za ním "C" nezměnilo na číslo znaku)
        snprintf(l1, sizeof l1, "%.*f" "\xC2\xB0" "C", g_set.disp_temp_dec, g_data.temp);   // počet desetinných míst z nastavení
        snprintf(l2, sizeof l2, "RH %.1f %%", g_data.hum);
        snprintf(l3, sizeof l3, "%.1f hPa", g_data.press);
    } else {
        snprintf(l1, sizeof l1, "---");
        snprintf(l2, sizeof l2, "SENSOR ERROR");
        l3[0] = 0;
    }
    oled_text_font(0, 0,  l1, &font_12x24);               // teplota největším písmem
    oled_text_font(0, 26, l2, &font_8x16);                // vlhkost
    oled_text_font(0, 42, l3, &font_8x16);                // tlak
    oled_text_font(0, 56, net_status_text(), &font_5x8);  // dole: IP adresa nebo stav sítě
    oled_flush();
}

// Přenese změny nastavení do displeje (jas, otočení). Posílá příkazy jen když se hodnota změnila.
static void apply_display_settings(void) {
    static int last_bright = -1, last_fx = -1, last_fy = -1;
    if (g_set.disp_flip_x != last_fx || g_set.disp_flip_y != last_fy) {
        oled_set_flip(g_set.disp_flip_x, g_set.disp_flip_y);
        last_fx = g_set.disp_flip_x; last_fy = g_set.disp_flip_y;
    }
    if (g_set.disp_bright != last_bright) {
        oled_set_brightness(g_set.disp_bright);
        last_bright = g_set.disp_bright;
    }
}

// Konzole na USB sériovém portu: stejné příkazy jako servisní kanál (napiš "help").
// Fyzický přístup ke kabelu USB se bere jako oprávněný, heslo se tady nevyžaduje.
static void serial_console(void) {
    static char line[96];
    static size_t len = 0;
    static svc_session_t session = { .authed = true };
    static char out[2048];
    int ch;
    while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (ch == '\r' || ch == '\n') {
            if (len == 0) continue;
            line[len] = 0;
            len = 0;
            svc_action_t act = service_exec(&session, line, out, sizeof out);
            printf("%s", out);
            if (act == SVC_REBOOT) diag_reboot();
        } else if (len < sizeof line - 1 && ch >= 0x20 && ch < 0x7F) {
            line[len++] = (char)ch;
        }
    }
}

// Hardwarový reset nastavení: pin držený proti zemi FACTORY_RESET_HOLD_MS vrátí tovární nastavení a restartuje.
// Volá se z hlavní smyčky; měří dobu, po kterou je pin nepřetržitě sepnutý.
static void factory_reset_pin_task(void) {
    static uint32_t pressed_since = 0;     // čas, kdy se pin sepnul (0 = nesepnutý)
    static bool warned = false;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (gpio_get(FACTORY_RESET_PIN)) {                    // pull-up: ve vysoké úrovni = nesepnuto
        pressed_since = 0; warned = false;
        if (s_reset_left_s) { s_reset_left_s = 0; draw_display(); }   // pin puštěn: odpočet zrušen, hned překreslíme
        return;
    }
    if (pressed_since == 0) pressed_since = now ? now : 1;
    if (!warned && now - pressed_since > 1000) { warned = true; printf("Reset nastaveni: drz pin dal...\n"); }
    // V posledních 5 s ukazujeme na displeji odpočet (5, 4, 3, 2, 1); překreslí se jen při změně čísla.
    uint32_t held = now - pressed_since;
    int left = held >= FACTORY_RESET_HOLD_MS ? 0 : (int)((FACTORY_RESET_HOLD_MS - held + 999) / 1000);
    if (left > 5) left = 0;
    if (left != s_reset_left_s) { s_reset_left_s = left; if (left) draw_display(); }
    if (now - pressed_since >= FACTORY_RESET_HOLD_MS) {
        printf("Reset nastaveni: vracim tovarni nastaveni a restartuji\n");
        settings_defaults(&g_set);
        settings_save_now();
        diag_reboot();
    }
}

int main(void) {
    stdio_init_all();                         // sériový výstup přes USB (pro ladění)
    // Počkáme, až se připojí sériový monitor, aby se neztratily úvodní hlášky.
    for (int i = 0; i < DEBUG_USB_WAIT_MS / 100 && !stdio_usb_connected(); i++) sleep_ms(100);
    sleep_ms(200);
    printf("\n=== RP2350 metering station boot (fw %s) ===\n", FW_VERSION);

    diag_init();                              // diagnostika: důvod restartu, teplota čipu
    if (diag_watchdog_reboot()) printf("POZOR: predchozi beh se zasekl, restart provedl hlidaci obvod\n");
    settings_load();                          // nastavení z flash (nebo výchozí)

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
    apply_display_settings();                 // jas a otočení z nastavení
    printf("OLED: init done (flipX=%d flipY=%d brightness=%d%%)\n", g_set.disp_flip_x, g_set.disp_flip_y, g_set.disp_bright);

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

    gpio_init(FACTORY_RESET_PIN);             // pin pro hardwarový reset nastavení (do země = reset)
    gpio_set_dir(FACTORY_RESET_PIN, GPIO_IN);
    gpio_pull_up(FACTORY_RESET_PIN);

    led_init();                               // indikační LED na desce (WS2812, GP25)

    // Hlídací obvod zapneme až teď, po všech úvodních čekáních. Smyčka ho musí pravidelně "krmit".
    watchdog_enable(WATCHDOG_MS, true);       // true = při ladění přes SWD se zastavuje spolu s procesorem

    uint32_t last = 0, last_print = 0;
    while (true) {
        watchdog_update();                    // "krmení" hlídacího obvodu: smyčka žije
        led_heartbeat();                      // pomalé blikání LED (neblokující)

        // Každých MEASURE_PERIOD_MS: měření a překreslení displeje (výpis na sériový výstup řídí SERIAL_PERIOD_MS).
        if ((time_us_32() - last) / 1000 >= MEASURE_PERIOD_MS) {
            last = time_us_32();

            if (!bme_ok) bme_ok = bme280_init(BME_I2C);       // senzor mohl být připojen dodatečně
            float t, h, p;
            if (bme_ok && bme280_read(&t, &h, &p)) {
                // Kalibrace: posun z nastavení (v setinách) se přičte k naměřené hodnotě.
                t += g_set.cal_t / 100.0f;
                h += g_set.cal_h / 100.0f;
                p += g_set.cal_p / 100.0f;
                if (h < 0.0f) h = 0.0f;                       // vlhkost nemůže být mimo 0 až 100 %
                if (h > 100.0f) h = 100.0f;
                g_data.temp = t; g_data.hum = h; g_data.press = p; g_data.status = 0;
                history_add(to_ms_since_boot(get_absolute_time()), t, h, p);   // bod do historie pro graf na webu
            } else {
                g_data.status = 1;                            // chyba senzoru
                g_diag.sensor_err++;
                bme_ok = false;                               // příště zkusíme senzor najít znovu
            }
            apply_display_settings();                         // změna jasu nebo otočení z webu/Modbusu
            publish();
            draw_display();

            if ((time_us_32() - last_print) / 1000 >= SERIAL_PERIOD_MS) {
                last_print = time_us_32();
                if (g_data.status == 0)
                    printf("T=%.2f C  RH=%.1f %%  P=%.1f hPa  [%s]\n",
                           g_data.temp, g_data.hum, g_data.press, net_status_text());
                else
                    printf("sensor error (status=1)  [%s]\n", net_status_text());
            }
        }

        // Obsluha sítě: musí se volat co nejčastěji (proto je mimo blok měření výše).
        net_poll();
        settings_task();                      // odložené uložení nastavení do flash (po změně z Modbusu)
        serial_console();                     // příkazy z USB sériového portu
        factory_reset_pin_task();             // dlouhé sepnutí pinu = tovární nastavení
    }
}
