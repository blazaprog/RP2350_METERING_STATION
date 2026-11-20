// ============================================================================
//  config.h - nastavení sítě a služeb měřiče
//  Zde se mění IP adresa, porty a podobně. Nic jiného v kódu upravovat nemusíš.
// ============================================================================
#pragma once

// --- Získání IP adresy ------------------------------------------------------
// 1 = DHCP (adresu přidělí router), 0 = pevná (statická) adresa z hodnot níže.
#define NET_USE_DHCP        1

// Hodnoty pro statickou adresu (použijí se jen když NET_USE_DHCP == 0).
#define NET_STATIC_IP       {192, 168, 1, 100}   // IP adresa měřiče
#define NET_STATIC_MASK     {255, 255, 255, 0}   // maska podsítě
#define NET_STATIC_GW       {192, 168, 1, 1}     // brána (router)
#define NET_STATIC_DNS      {8, 8, 8, 8}         // DNS server

// --- Porty služeb -----------------------------------------------------------
#define HTTP_PORT           80                   // webový server (prohlížeč)
#define MODBUS_PORT         502                  // Modbus TCP (standardní port)
#define SERVICE_PORT        2323                 // servisní kanál (připoj se přes telnet nebo PuTTY v režimu raw)
#define MODBUS_NREGS        31                   // počet registrů v tabulce (adresy 0 až 30, mapa v docs/modbus.md)

// Pořadí dvou 16bitových registrů u hodnot typu float (32 bitů):
//   0 = nejdřív horní slovo (big-endian, "ABCD") - nejběžnější, výchozí
//   1 = nejdřív dolní slovo (prohozená slova, "CDAB") - vyžadují ho některé PLC a programy
#define MODBUS_FLOAT_WORD_SWAP  0

// --- Čísla socketů čipu W6300 (čip jich má 8: 0 až 7) -----------------------
// Socket 0 používá DHCP klient, socket 1 je rezerva (např. pro DNS/MQTT později).
// Dva sokety pro web (prohlížeč často otevře víc spojení naráz) a dva pro Modbus.
#define SOCK_HTTP_A         2
#define SOCK_HTTP_B         3
#define SOCK_MODBUS_A       4
#define SOCK_MODBUS_B       5
#define SOCK_SERVICE        6                    // servisní textový kanál (jeden klient najednou)

// --- Ladění -----------------------------------------------------------------
// Jak dlouho (ms) firmware při startu počká na připojení sériového monitoru přes USB,
// aby ses nepřipravil o úvodní hlášky. Při provozu bez USB (napájení z PoE) to jen
// zdrží start o tuto dobu; pokud to nechceš, nastav 0.
#define DEBUG_USB_WAIT_MS   3000

// --- Indikační LED (WS2812 na desce) ----------------------------------------
#define LED_PIN             25                   // pin, na kterém je LED připojená
#define LED_BLINK_MS        1000                 // jak dlouho svítí / nesvítí (ms) - větší číslo = pomalejší blikání
#define LED_ON_R            0                    // barva při svitu (0 až 255); nízké hodnoty = slabší svit
#define LED_ON_G            40
#define LED_ON_B            0

// --- Displej OLED (SH1106) --------------------------------------------------
// Když displej ukazuje obraz zrcadlově nebo vzhůru nohama, změň tyto přepínače
// (stačí přeložit a nahrát znovu, v kódu se nic jiného upravovat nemusí).
#define OLED_FLIP_X         1                    // 1 = výchozí směr sloupců (příkaz 0xA1), 0 = zrcadlově otočit (0xA0)
#define OLED_FLIP_Y         1                    // 1 = výchozí směr řádků (příkaz 0xC8), 0 = otočit vzhůru nohama (0xC0)
#define OLED_COL_OFFSET     2                    // posun sloupců: SH1106 má 132 sloupců, panel jich ukazuje 128.
                                                 // Když je obraz o 2 pixely posunutý nebo na kraji svítí svislá čára, zkus 0 nebo 4.
#define OLED_CONTRAST       0xCF                 // jas displeje 0x00 až 0xFF
#define OLED_SPI_HZ         (4 * 1000 * 1000)    // rychlost SPI; při šumu v obraze zkus 1 MHz
#define OLED_SPLASH_MS      1200                 // jak dlouho při startu svítí testovací obrazec (0 = vypnout)

// --- Obnovování měření a displeje -------------------------------------------
#define MEASURE_PERIOD_MS   250                  // jak často se měří a překresluje displej (ms); menší číslo = rychlejší
#define SERIAL_PERIOD_MS    1000                 // jak často se vypisuje řádek do sériového monitoru (ms)

// --- Zobrazení hodnot na displeji -------------------------------------------
#define DISPLAY_TEMP_DECIMALS   2                // počet desetinných míst teploty na displeji (2 = setiny stupně, 1 = desetiny)

// --- Historie pro graf na webu ----------------------------------------------
// Zařízení si v paměti drží posledních HISTORY_POINTS bodů; každý bod je průměr za HISTORY_STEP_S sekund.
// Při 360 bodech po 60 s je to posledních 6 hodin. Po restartu se historie smaže (je jen v RAM).
// Pozor: JSON s historií musí vejít do vyrovnávací paměti odpovědi (8 kB v net_w6300.c), 360 bodů je maximum.
#define HISTORY_POINTS      360
#define HISTORY_STEP_S      60

// Verze firmwaru (zobrazuje se na webu a v JSON).
#define FW_VERSION          "0.4"

// --- Nastavení uložená ve flash ----------------------------------------------
// Hodnoty výše (síť, displej, ...) jsou jen VÝCHOZÍ. Skutečné nastavení se mění na webu, přes Modbus
// nebo servisní kanál a ukládá se do flash (viz settings.h).
#define DEVICE_NAME_DEFAULT     "Meteo stanice"  // výchozí název zařízení
#define SETTINGS_SAVE_DELAY_MS  5000             // změna z Modbusu se zapíše do flash až po této době klidu (šetří flash)
#define SETTINGS_SAVE_MAX_MS    60000            // ...ale nejpozději po této době od první změny

// --- Registry Modbus, které jdou zapisovat (funkce 06 a 16) ---------------------
#define MB_REG_DISP_BRIGHT      10               // jas displeje v % (0 = displej vypnutý, 100 = nejjasnější)
#define MB_REG_LED_LEVEL        11               // jas indikační LED v % (0 = zhasnutá)

// --- Diagnostika ------------------------------------------------------------
#define FW_VERSION_NUM          4                // verze firmwaru krát 10 (pro Modbus), 4 = "0.4"
// Hardwarový reset nastavení: pin spojený se zemí (GND) po dobu FACTORY_RESET_HOLD_MS vrátí tovární nastavení
// (síť na DHCP, heslo zrušeno, kalibrace a displej výchozí) a zařízení se restartuje. Funguje i při zapomenutém hesle.
// Pin je s vnitřním pull-up rezistorem, takže nezapojený je ve stavu "nic". Ověř, že zvolený GPIO je na desce vyvedený.
#define FACTORY_RESET_PIN      14
#define FACTORY_RESET_HOLD_MS  10000

#define WATCHDOG_MS             8000             // když smyčka neodpoví do této doby, zařízení se samo restartuje
