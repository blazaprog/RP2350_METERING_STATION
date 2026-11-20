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

// --- Čísla socketů čipu W6300 (čip jich má 8: 0 až 7) -----------------------
// Socket 0 používá DHCP klient, socket 1 je rezerva (např. pro DNS/MQTT později).
// Dva sokety pro web (prohlížeč často otevře víc spojení naráz) a dva pro Modbus.
#define SOCK_HTTP_A         2
#define SOCK_HTTP_B         3
#define SOCK_MODBUS_A       4
#define SOCK_MODBUS_B       5

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
