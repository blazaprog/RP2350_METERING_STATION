// ============================================================================
//  history.h - historie naměřených hodnot v paměti (pro graf na webové stránce)
//
//  Každých HISTORY_STEP_S sekund se uloží jeden bod = průměr všech měření za tu dobu.
//  Nejstarší body se přepisují (kruhová paměť). Hodnoty se ukládají jako celá čísla:
//    teplota v setinách °C, vlhkost v setinách %, tlak v desetinách hPa.
// ============================================================================
#pragma once
#include <stddef.h>
#include <stdint.h>

// Přidá platné měření. now_ms je čas v milisekundách (např. od startu zařízení).
void history_add(uint32_t now_ms, float temp, float hum, float press);

// Počet uložených bodů (0 až HISTORY_POINTS).
size_t history_count(void);

// Vrátí bod číslo i (0 = nejstarší, history_count()-1 = nejnovější).
void history_get(size_t i, int16_t *temp_c100, uint16_t *hum_c100, uint16_t *press_d10);
