// ============================================================================
//  diag.h - diagnostika zařízení (čítače, stav, teplota čipu, restarty)
//
//  Diagnostiku najdeš na třech místech: registry Modbus 16 až 30 (jen pro čtení),
//  JSON na /api/diag a příkaz "diag" v servisním kanálu (i přes USB sériový port).
// ============================================================================
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Čítače od startu zařízení.
typedef struct {
    uint32_t sensor_err;      // kolikrát se nepodařilo přečíst senzor
    uint32_t http_req;        // počet obsloužených HTTP požadavků
    uint32_t modbus_req;      // počet Modbus požadavků
    uint32_t modbus_exc;      // z toho odpovědí s chybou (exception)
    uint32_t link_changes;    // kolikrát se změnil stav síťového kabelu
} diag_t;

extern diag_t g_diag;

// ---- Část závislá na hardwaru (diag_hw.c) ----
void diag_init(void);                 // zapne měření teploty čipu a zjistí důvod posledního restartu
uint32_t diag_uptime_s(void);         // doba běhu v sekundách
float diag_chip_temp(void);           // teplota čipu RP2350 v °C (orientační)
bool diag_watchdog_reboot(void);      // true, když poslední restart způsobil hlídací obvod (zařízení se zaseklo)
void diag_reboot(void);               // restartuje zařízení

// ---- Sestavení výstupů (diag.c) ----
// Příznaky chyb (registr 19): bit0 chyba senzoru, bit1 chybí Ethernet čip, bit2 není kabel,
// bit3 čeká na IP adresu, bit4 restart hlídacím obvodem, bit5 při startu chybělo uložené nastavení.
uint16_t diag_flags(void);
size_t diag_text(char *buf, size_t cap);     // čitelný výpis (servisní kanál)
size_t diag_json(char *buf, size_t cap);     // JSON (/api/diag)
void diag_fill_regs(uint16_t *regs);         // naplní registry 16 až 30
