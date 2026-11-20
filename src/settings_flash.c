// ============================================================================
//  settings_flash.c - ukládání nastavení do flash paměti
//
//  Poslední 4kB sektor flash (mimo firmware) je rozdělený na 16 "slotů" po 256 bajtech.
//  Každé uložení zapíše nový slot hned za předchozí a sektor se maže až když je plný.
//  Díky tomu je opotřebení flash 16x menší a při výpadku napájení uprostřed zápisu
//  zůstane platný předchozí slot (kontrolní součet pozná rozepsaný slot).
//
//  Aktualizace firmwaru přes USB (BOOTSEL) nastavení nemaže, protože zapisuje jen
//  oblast firmwaru.
// ============================================================================
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "settings.h"
#include "config.h"

#define SLOT_COUNT   (FLASH_SECTOR_SIZE / SETTINGS_SLOT_SIZE)           // 16 slotů
#define SECTOR_OFF   (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)        // poslední sektor flash

bool g_settings_defaulted = false;
uint32_t g_settings_saves = 0;

static settings_t s_saved;          // poslední zapsaná podoba (porovnává se, aby se nezapisovalo zbytečně)
static bool s_have_saved = false;   // true, když je ve flash platný slot
static uint32_t s_seq = 0;          // pořadové číslo posledního slotu
static int s_cur = -1;              // číslo posledního platného slotu (-1 = žádný)

// Odložené uložení
static bool s_dirty = false;
static uint32_t s_dirty_first_ms, s_dirty_last_ms;

static uint8_t s_buf[SETTINGS_SLOT_SIZE];   // zapisovaný slot (musí být v RAM, ne ve flash)

// Ukazatel na slot ve flash (flash je přístupná jako běžná paměť od adresy XIP_BASE).
static const uint8_t *slot_ptr(int i) {
    return (const uint8_t *)(XIP_BASE + SECTOR_OFF + (uint32_t)i * SETTINGS_SLOT_SIZE);
}

static bool slot_blank(int i) {
    const uint8_t *p = slot_ptr(i);
    for (size_t k = 0; k < SETTINGS_SLOT_SIZE; k++) if (p[k] != 0xFF) return false;
    return true;
}

// Parametry zápisu pro funkci, která běží při vypnutém přístupu do flash.
typedef struct { bool erase; uint32_t offset; } write_job_t;

// Volá se přes flash_safe_execute: během ní se nesmí spouštět kód z flash ani běžet přerušení.
static void do_write(void *param) {
    write_job_t *j = (write_job_t *)param;
    if (j->erase) flash_range_erase(SECTOR_OFF, FLASH_SECTOR_SIZE);
    flash_range_program(j->offset, s_buf, SETTINGS_SLOT_SIZE);
}

void settings_load(void) {
    settings_defaults(&g_set);
    int best = -1;
    uint32_t best_seq = 0;
    settings_t tmp;
    for (int i = 0; i < SLOT_COUNT; i++) {
        uint32_t seq;
        if (settings_decode(slot_ptr(i), &tmp, &seq) && (best < 0 || seq > best_seq)) {
            best = i; best_seq = seq; g_set = tmp;
        }
    }
    if (best >= 0) {
        bool clean = settings_sanitize(&g_set);
        s_cur = best; s_seq = best_seq; s_have_saved = true;
        g_settings_defaulted = false;
        printf("Settings: loaded from flash (slot %d, #%lu)%s\n", best, (unsigned long)best_seq,
               clean ? "" : ", some values were invalid and were reset");
    } else {
        g_settings_defaulted = true;
        printf("Settings: no valid settings in flash, using defaults\n");
    }
    s_saved = g_set;
}

bool settings_save_now(void) {
    s_dirty = false;
    if (s_have_saved ? memcmp(&g_set, &s_saved, sizeof g_set) == 0 : false) return true;   // beze změny
    if (!s_have_saved) {                     // nikdy nic uloženo: pokud jsou hodnoty výchozí, není co zapisovat
        settings_t d; settings_defaults(&d);
        if (memcmp(&g_set, &d, sizeof d) == 0) return true;
    }

    int target = (s_cur + 1) % SLOT_COUNT;
    bool erase = (s_cur < 0 && !slot_blank(0)) || !slot_blank(target);   // další slot není prázdný: smažeme sektor
    if (erase) target = 0;
    settings_encode(&g_set, s_seq + 1, s_buf);

    write_job_t job = { erase, SECTOR_OFF + (uint32_t)target * SETTINGS_SLOT_SIZE };
    int rc = flash_safe_execute(do_write, &job, 1000);
    if (rc != PICO_OK) { printf("Settings: flash write failed (%d)\n", rc); return false; }

    // Ověření: slot přečteme zpět.
    settings_t chk; uint32_t seq;
    if (!settings_decode(slot_ptr(target), &chk, &seq) || seq != s_seq + 1 || memcmp(&chk, &g_set, sizeof chk) != 0) {
        printf("Settings: verification failed\n");
        return false;
    }
    s_cur = target; s_seq = seq; s_saved = g_set; s_have_saved = true; g_settings_saves++;
    printf("Settings: saved to flash (slot %d, #%lu%s)\n", target, (unsigned long)seq, erase ? ", sector erased" : "");
    return true;
}

void settings_save_later(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (!s_dirty) { s_dirty = true; s_dirty_first_ms = now; }
    s_dirty_last_ms = now;
}

void settings_task(void) {
    if (!s_dirty) return;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - s_dirty_last_ms >= SETTINGS_SAVE_DELAY_MS || now - s_dirty_first_ms >= SETTINGS_SAVE_MAX_MS)
        settings_save_now();
}
