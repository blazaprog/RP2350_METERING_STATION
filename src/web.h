// ============================================================================
//  web.h - jednoduchý HTTP server (webová stránka a JSON)
// ============================================================================
#pragma once
#include <stddef.h>
#include <stdint.h>
#include "sensor_data.h"

// Údaje, které web zobrazuje kromě naměřených hodnot.
typedef struct {
    const sensor_data_t *d;     // aktuální naměřené hodnoty
    const char *ip;             // IP adresa zařízení jako text
    const char *mac;            // MAC adresa jako text
    uint32_t uptime_s;          // doba běhu od startu v sekundách
} web_info_t;

// Podle textu HTTP požadavku sestaví kompletní HTTP odpověď (hlavičky + tělo).
//   req      přijatý požadavek jako text zakončený nulou
//   resp     pracovní buffer pro odpověď, cap je jeho velikost (aspoň 8 kB kvůli historii)
//   info     údaje pro zobrazení
//   out_len  sem se zapíše délka odpovědi
// Vrací ukazatel na začátek odpovědi (leží uvnitř resp) a v out_len její délku.
// Podporované adresy:
//   GET /             webová stránka (karty s hodnotami a grafy; stránka sama čte JSON)
//   GET /api/data     aktuální hodnoty a údaje o zařízení (JSON)
//   GET /api/history  historie pro graf (JSON, celá čísla)
//   GET /api/settings aktuální nastavení (JSON, bez hesla)
//   POST /api/settings změna nastavení (tělo "klíč=hodnota&...", viz settings_apply_form)
//   GET /api/diag     diagnostika (JSON)
const char *http_handle(const char *req, char *resp, size_t cap, const web_info_t *info, size_t *out_len);
