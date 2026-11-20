// ============================================================================
//  settings.h - nastavení zařízení (ukládá se do flash)
//
//  Nastavení se mění třemi cestami: webová stránka (POST /api/settings), Modbus
//  (jas displeje a LED) a servisní kanál. Všechny ho ukládají do flash, takže
//  přežije restart. Výchozí hodnoty jsou v config.h.
// ============================================================================
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SET_NAME_MAX 24     // název zařízení včetně koncové nuly
#define SET_PASS_MAX 17     // heslo včetně koncové nuly (nejvýše 16 znaků)

typedef struct {
    char    name[SET_NAME_MAX];     // název zařízení (písmena bez diakritiky, číslice, mezera, - _ .)
    char    pass[SET_PASS_MAX];     // heslo pro změny nastavení; prázdné = bez hesla
    uint8_t dhcp;                   // 1 = adresu přidělí router (DHCP), 0 = pevná adresa níže
    uint8_t ip[4], mask[4], gw[4], dns[4];
    int16_t cal_t;                  // posun teploty v setinách °C (přičte se k naměřené hodnotě)
    int16_t cal_h;                  // posun vlhkosti v setinách %
    int16_t cal_p;                  // posun tlaku v setinách hPa
    uint8_t disp_bright;            // jas displeje v % (0 = vypnuto)
    uint8_t disp_flip_x;            // 1 = výchozí směr sloupců, 0 = zrcadlově
    uint8_t disp_flip_y;            // 1 = výchozí směr řádků, 0 = vzhůru nohama
    uint8_t disp_temp_dec;          // desetinná místa teploty na displeji (1 nebo 2)
    uint8_t led_level;              // jas indikační LED v % (0 = zhasnutá)
    uint8_t mb_swap;                // 1 = u floatu v Modbusu prohodit slova (CDAB)
    uint8_t mb_unit;                // Modbus Unit ID (0 = odpovídat na jakékoli)
} settings_t;

extern settings_t g_set;            // aktuální nastavení (v RAM)

// Výsledek změny nastavení formulářem.
typedef enum { SET_OK = 0, SET_ERR_AUTH, SET_ERR_VALUE } set_result_t;

// ---- Čistá logika (bez závislosti na hardwaru) ----
void settings_defaults(settings_t *s);                    // výchozí hodnoty z config.h
bool settings_sanitize(settings_t *s);                    // opraví neplatné hodnoty; true = vše bylo v pořádku
// Použije formulář "klíč=hodnota&klíč=hodnota" (jak ho posílá webová stránka). Změní g_set jen když je
// všechno v pořádku. *net_changed = true, když se změnilo něco v nastavení sítě. Při chybě je v err text.
set_result_t settings_apply_form(const char *body, char *err, size_t errcap, bool *net_changed);
bool settings_pass_ok(const char *given);                  // true, když heslo není nastavené nebo souhlasí
size_t settings_to_json(char *buf, size_t cap);           // nastavení jako JSON (bez hesla)
size_t settings_to_text(char *buf, size_t cap);           // nastavení jako text pro servisní kanál

// Uložení do jednoho "slotu" (256 bajtů) s kontrolním součtem.
#define SETTINGS_SLOT_SIZE 256
void settings_encode(const settings_t *s, uint32_t seq, uint8_t *slot);
bool settings_decode(const uint8_t *slot, settings_t *s, uint32_t *seq);
uint32_t settings_crc32(const uint8_t *data, size_t len);

// ---- Ukládání do flash (settings_flash.c) ----
void settings_load(void);          // načte nastavení z flash (nebo výchozí, když tam žádné není)
bool settings_save_now(void);      // uloží do flash hned (trvá desítky ms); true = úspěch nebo beze změny
void settings_save_later(void);    // uloží se až po chvíli klidu (pro časté změny, např. z Modbusu)
void settings_task(void);          // volat z hlavní smyčky: provede odložené uložení
extern bool g_settings_defaulted;  // true, když při startu nebylo ve flash platné nastavení
extern uint32_t g_settings_saves;  // kolikrát se nastavení od startu zapsalo do flash
