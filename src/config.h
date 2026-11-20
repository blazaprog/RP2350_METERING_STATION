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
