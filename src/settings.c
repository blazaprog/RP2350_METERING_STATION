// ============================================================================
//  settings.c - logika nastavení: výchozí hodnoty, kontrola hodnot, zpracování
//  formuláře z webu, převod na JSON/text a kódování do slotu s kontrolním součtem.
//
//  Tenhle soubor nepracuje s hardwarem, takže se dá vyzkoušet i na počítači.
//  Zápis do flash je v settings_flash.c.
// ============================================================================
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "settings.h"
#include "config.h"

settings_t g_set;

// Meze kalibrace v setinách (teplota a vlhkost ±20, tlak ±50).
#define CAL_T_MAX 2000
#define CAL_H_MAX 2000
#define CAL_P_MAX 5000

// ----------------------------------------------------------------------------
//  Výchozí hodnoty
// ----------------------------------------------------------------------------
void settings_defaults(settings_t *s) {
    memset(s, 0, sizeof *s);
    strncpy(s->name, DEVICE_NAME_DEFAULT, SET_NAME_MAX - 1);
    s->dhcp = NET_USE_DHCP ? 1 : 0;
    const uint8_t ip[4] = NET_STATIC_IP, mask[4] = NET_STATIC_MASK;
    const uint8_t gw[4] = NET_STATIC_GW, dns[4] = NET_STATIC_DNS;
    memcpy(s->ip, ip, 4);  memcpy(s->mask, mask, 4);
    memcpy(s->gw, gw, 4);  memcpy(s->dns, dns, 4);
    s->disp_bright   = (uint8_t)((OLED_CONTRAST * 100 + 127) / 255);   // 0xCF = asi 81 %
    s->disp_flip_x   = OLED_FLIP_X ? 1 : 0;
    s->disp_flip_y   = OLED_FLIP_Y ? 1 : 0;
    s->disp_temp_dec = DISPLAY_TEMP_DECIMALS;
    s->led_level     = (uint8_t)((LED_ON_G * 100 + 127) / 255);        // 40/255 = asi 16 %
    s->mb_swap       = MODBUS_FLOAT_WORD_SWAP ? 1 : 0;
    s->mb_unit       = 0;
}

// ----------------------------------------------------------------------------
//  Kontroly jednotlivých hodnot
// ----------------------------------------------------------------------------

// Název: 1 až 23 znaků, jen písmena a číslice bez diakritiky, mezera, pomlčka, podtržítko a tečka.
static bool name_ok(const char *n) {
    size_t len = strnlen(n, SET_NAME_MAX);
    if (len < 1 || len >= SET_NAME_MAX) return false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)n[i];
        if (!(isalnum(c) || c == ' ' || c == '-' || c == '_' || c == '.') || c > 127) return false;
    }
    return true;
}

// Heslo: 1 až 16 viditelných znaků ASCII bez mezer.
static bool pass_ok(const char *p) {
    size_t len = strlen(p);
    if (len < 1 || len >= SET_PASS_MAX) return false;
    for (size_t i = 0; i < len; i++) if ((unsigned char)p[i] < 0x21 || (unsigned char)p[i] > 0x7E) return false;
    return true;
}

// Přísně přečte IP adresu ve tvaru a.b.c.d (každé číslo 0 až 255, nic navíc).
static bool parse_ip(const char *s, uint8_t out[4]) {
    unsigned v[4]; int n = 0;
    const char *p = s;
    while (n < 4) {
        if (!isdigit((unsigned char)*p)) return false;
        unsigned x = 0; int digits = 0;
        while (isdigit((unsigned char)*p)) { x = x * 10 + (unsigned)(*p - '0'); if (++digits > 3 || x > 255) return false; p++; }
        v[n++] = x;
        if (n < 4) { if (*p != '.') return false; p++; }
    }
    if (*p != 0) return false;
    for (int i = 0; i < 4; i++) out[i] = (uint8_t)v[i];
    return true;
}

static uint32_t ip32(const uint8_t a[4]) { return ((uint32_t)a[0] << 24) | (a[1] << 16) | (a[2] << 8) | a[3]; }

// Použitelná adresa zařízení: ne 0.x, ne smyčka 127.x, ne multicast/rezervované (224 a výš).
static bool unicast_ok(const uint8_t a[4]) { return a[0] >= 1 && a[0] <= 223 && a[0] != 127; }

// Maska: souvislé jedničky zleva a pak nuly; aspoň 2 bity pro adresy zařízení (nejvýše /30).
static bool mask_ok(const uint8_t m[4]) {
    uint32_t v = ip32(m), inv = ~v;
    return v != 0 && inv >= 3 && ((inv & (inv + 1)) == 0);
}

static bool same_subnet(const uint8_t a[4], const uint8_t b[4], const uint8_t m[4]) {
    return (ip32(a) & ip32(m)) == (ip32(b) & ip32(m));
}

// Zkontroluje celé nastavení sítě pro pevnou adresu. Vrací text chyby, nebo NULL když je vše v pořádku.
static const char *check_static_net(const settings_t *s) {
    if (!unicast_ok(s->ip)) return "IP adresa není platná";
    if (!mask_ok(s->mask)) return "Maska není platná";
    uint32_t host = ip32(s->ip) & ~ip32(s->mask);
    if (host == 0 || host == (~ip32(s->mask))) return "IP adresa nesmí být adresa sítě ani broadcast";
    if (ip32(s->gw) != 0) {
        if (!unicast_ok(s->gw)) return "Brána není platná";
        if (!same_subnet(s->ip, s->gw, s->mask)) return "Brána musí být ve stejné síti jako IP adresa";
        if (ip32(s->gw) == ip32(s->ip)) return "Brána nesmí být stejná jako IP adresa";
    }
    if (ip32(s->dns) != 0 && !unicast_ok(s->dns)) return "DNS není platné";
    return NULL;
}

// Přečte celé číslo (volitelně se znaménkem) v mezích lo až hi. Nic jiného než číslice nepovolí.
static bool parse_int(const char *s, long lo, long hi, long *out) {
    if (!*s) return false;
    char *end;
    long v = strtol(s, &end, 10);
    if (*end != 0 || v < lo || v > hi) return false;
    *out = v;
    return true;
}

// Přečte desetinné číslo (např. "-0.35") a vrátí ho v setinách (-35); mez je max v setinách.
static bool parse_hundredths(const char *s, int max, int16_t *out) {
    if (!*s) return false;
    char *end;
    double v = strtod(s, &end);
    if (*end != 0 || v != v) return false;          // v != v je NaN
    long h = (long)(v * 100.0 + (v < 0 ? -0.5 : 0.5));
    if (h < -max || h > max) return false;
    *out = (int16_t)h;
    return true;
}

// ----------------------------------------------------------------------------
//  Oprava neplatných hodnot (po načtení z flash)
// ----------------------------------------------------------------------------
bool settings_sanitize(settings_t *s) {
    settings_t d;
    settings_defaults(&d);
    bool ok = true;
    s->name[SET_NAME_MAX - 1] = 0;
    s->pass[SET_PASS_MAX - 1] = 0;
    if (!name_ok(s->name)) { memcpy(s->name, d.name, sizeof s->name); ok = false; }
    if (s->pass[0] && !pass_ok(s->pass)) { memset(s->pass, 0, sizeof s->pass); ok = false; }
    if (s->dhcp > 1) { s->dhcp = d.dhcp; ok = false; }
    if (check_static_net(s)) {                      // pevná adresa neplatná: vrátíme výchozí
        memcpy(s->ip, d.ip, 4); memcpy(s->mask, d.mask, 4); memcpy(s->gw, d.gw, 4); memcpy(s->dns, d.dns, 4);
        ok = false;
    }
    if (s->cal_t < -CAL_T_MAX || s->cal_t > CAL_T_MAX) { s->cal_t = 0; ok = false; }
    if (s->cal_h < -CAL_H_MAX || s->cal_h > CAL_H_MAX) { s->cal_h = 0; ok = false; }
    if (s->cal_p < -CAL_P_MAX || s->cal_p > CAL_P_MAX) { s->cal_p = 0; ok = false; }
    if (s->disp_bright > 100) { s->disp_bright = d.disp_bright; ok = false; }
    if (s->led_level > 100)   { s->led_level = d.led_level; ok = false; }
    if (s->disp_flip_x > 1)   { s->disp_flip_x = d.disp_flip_x; ok = false; }
    if (s->disp_flip_y > 1)   { s->disp_flip_y = d.disp_flip_y; ok = false; }
    if (s->disp_temp_dec < 1 || s->disp_temp_dec > 2) { s->disp_temp_dec = d.disp_temp_dec; ok = false; }
    if (s->mb_swap > 1)       { s->mb_swap = d.mb_swap; ok = false; }
    if (s->mb_unit > 247)     { s->mb_unit = 0; ok = false; }
    return ok;
}

// ----------------------------------------------------------------------------
//  Zpracování formuláře "klíč=hodnota&..."
// ----------------------------------------------------------------------------

// Dekóduje hodnotu zakódovanou pro URL ("%41" -> "A", "+" -> mezera) do out (nejvýše cap-1 znaků).
static void url_decode(const char *src, size_t n, char *out, size_t cap) {
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < cap; i++) {
        if (src[i] == '+') out[o++] = ' ';
        else if (src[i] == '%' && i + 2 < n && isxdigit((unsigned char)src[i + 1]) && isxdigit((unsigned char)src[i + 2])) {
            char h[3] = {src[i + 1], src[i + 2], 0};
            out[o++] = (char)strtol(h, NULL, 16);
            i += 2;
        } else out[o++] = src[i];
    }
    out[o] = 0;
}

// Najde ve formuláři hodnotu klíče; vrací true a hodnotu v val (dekódovanou).
static bool form_get(const char *body, const char *key, char *val, size_t cap) {
    size_t klen = strlen(key);
    const char *p = body;
    while (*p) {
        const char *end = strchr(p, '&');
        size_t plen = end ? (size_t)(end - p) : strlen(p);
        const char *eq = memchr(p, '=', plen);
        if (eq && (size_t)(eq - p) == klen && strncmp(p, key, klen) == 0) {
            url_decode(eq + 1, plen - klen - 1, val, cap);
            return true;
        }
        if (!end) break;
        p = end + 1;
    }
    return false;
}

// Porovnání hesel bez časových rozdílů (délka porovnání nezávisí na tom, kde se liší).
static bool pass_equal(const char *a, const char *b) {
    size_t la = strlen(a), lb = strlen(b);
    unsigned diff = (unsigned)(la ^ lb);
    for (size_t i = 0; i < SET_PASS_MAX - 1; i++)
        diff |= (unsigned)((unsigned char)(i < la ? a[i] : 0) ^ (unsigned char)(i < lb ? b[i] : 0));
    return diff == 0;
}

static void set_err(char *err, size_t cap, const char *msg) { snprintf(err, cap, "%s", msg); }

// Platí, když heslo není nastavené, nebo když zadané heslo souhlasí.
bool settings_pass_ok(const char *given) {
    if (!g_set.pass[0]) return true;
    return strlen(given) < SET_PASS_MAX && pass_equal(given, g_set.pass);
}

set_result_t settings_apply_form(const char *body, char *err, size_t errcap, bool *net_changed) {
    char v[64];
    err[0] = 0;
    *net_changed = false;

    // 1) Heslo: když je nastavené, musí formulář obsahovat správné "pw".
    char given[SET_PASS_MAX + 8] = {0};
    form_get(body, "pw", given, sizeof given);
    if (!settings_pass_ok(given)) {
        set_err(err, errcap, "Špatné heslo");
        return SET_ERR_AUTH;
    }

    settings_t n = g_set;                            // pracujeme na kopii, změní se jen když projde všechno

    // 2) Výchozí nastavení (smaže i heslo).
    if (form_get(body, "defaults", v, sizeof v) && strcmp(v, "1") == 0) settings_defaults(&n);

    long x;
    if (form_get(body, "name", v, sizeof v)) {
        if (!name_ok(v)) { set_err(err, errcap, "Název: 1 až 23 znaků, písmena bez diakritiky, číslice, mezera, - _ ."); return SET_ERR_VALUE; }
        memset(n.name, 0, sizeof n.name); strncpy(n.name, v, SET_NAME_MAX - 1);
    }
    if (form_get(body, "dhcp", v, sizeof v)) {
        if (!parse_int(v, 0, 1, &x)) { set_err(err, errcap, "DHCP: hodnota 0 nebo 1"); return SET_ERR_VALUE; }
        n.dhcp = (uint8_t)x;
    }
    struct { const char *key; uint8_t *dst; const char *msg; } ips[] = {
        {"ip", n.ip, "IP adresa není ve tvaru a.b.c.d"}, {"mask", n.mask, "Maska není ve tvaru a.b.c.d"},
        {"gw", n.gw, "Brána není ve tvaru a.b.c.d"},     {"dns", n.dns, "DNS není ve tvaru a.b.c.d"},
    };
    for (size_t i = 0; i < sizeof ips / sizeof ips[0]; i++) {
        if (!form_get(body, ips[i].key, v, sizeof v)) continue;       // klíč ve formuláři není: hodnota se nemění
        if (v[0] == 0 && i >= 2) { memset(ips[i].dst, 0, 4); continue; }   // prázdná brána nebo DNS = nepoužívat
        if (!parse_ip(v, ips[i].dst)) { set_err(err, errcap, ips[i].msg); return SET_ERR_VALUE; }
    }
    if (n.dhcp == 0) {                               // pevná adresa se musí dát dohromady
        const char *e = check_static_net(&n);
        if (e) { set_err(err, errcap, e); return SET_ERR_VALUE; }
    }
    if (form_get(body, "cal_t", v, sizeof v) && !parse_hundredths(v, CAL_T_MAX, &n.cal_t)) { set_err(err, errcap, "Posun teploty: číslo od -20 do 20"); return SET_ERR_VALUE; }
    if (form_get(body, "cal_h", v, sizeof v) && !parse_hundredths(v, CAL_H_MAX, &n.cal_h)) { set_err(err, errcap, "Posun vlhkosti: číslo od -20 do 20"); return SET_ERR_VALUE; }
    if (form_get(body, "cal_p", v, sizeof v) && !parse_hundredths(v, CAL_P_MAX, &n.cal_p)) { set_err(err, errcap, "Posun tlaku: číslo od -50 do 50"); return SET_ERR_VALUE; }

    struct { const char *key; uint8_t *dst; long lo, hi; const char *msg; } ints[] = {
        {"bright",   &n.disp_bright,   0, 100, "Jas displeje: 0 až 100"},
        {"led",      &n.led_level,     0, 100, "Jas LED: 0 až 100"},
        {"flip_x",   &n.disp_flip_x,   0, 1,   "Zrcadlení: 0 nebo 1"},
        {"flip_y",   &n.disp_flip_y,   0, 1,   "Otočení: 0 nebo 1"},
        {"temp_dec", &n.disp_temp_dec, 1, 2,   "Desetinná místa teploty: 1 nebo 2"},
        {"mb_swap",  &n.mb_swap,       0, 1,   "Pořadí slov: 0 nebo 1"},
        {"mb_unit",  &n.mb_unit,       0, 247, "Unit ID: 0 až 247"},
    };
    for (size_t i = 0; i < sizeof ints / sizeof ints[0]; i++)
        if (form_get(body, ints[i].key, v, sizeof v)) {
            if (!parse_int(v, ints[i].lo, ints[i].hi, &x)) { set_err(err, errcap, ints[i].msg); return SET_ERR_VALUE; }
            *ints[i].dst = (uint8_t)x;
        }

    if (form_get(body, "newpass", v, sizeof v) && v[0]) {
        if (!pass_ok(v)) { set_err(err, errcap, "Heslo: 1 až 16 viditelných znaků bez mezer"); return SET_ERR_VALUE; }
        memset(n.pass, 0, sizeof n.pass); strncpy(n.pass, v, SET_PASS_MAX - 1);
    }
    if (form_get(body, "clearpass", v, sizeof v) && strcmp(v, "1") == 0) memset(n.pass, 0, sizeof n.pass);

    *net_changed = n.dhcp != g_set.dhcp || memcmp(n.ip, g_set.ip, 4) || memcmp(n.mask, g_set.mask, 4) ||
                   memcmp(n.gw, g_set.gw, 4) || memcmp(n.dns, g_set.dns, 4);
    g_set = n;
    return SET_OK;
}

// ----------------------------------------------------------------------------
//  Výpis nastavení
// ----------------------------------------------------------------------------
size_t settings_to_json(char *buf, size_t cap) {
    const settings_t *s = &g_set;
    int n = snprintf(buf, cap,
        "{\"name\":\"%s\",\"dhcp\":%u,\"ip\":\"%u.%u.%u.%u\",\"mask\":\"%u.%u.%u.%u\",\"gw\":\"%u.%u.%u.%u\","
        "\"dns\":\"%u.%u.%u.%u\",\"cal_t\":%.2f,\"cal_h\":%.2f,\"cal_p\":%.2f,\"bright\":%u,\"led\":%u,"
        "\"flip_x\":%u,\"flip_y\":%u,\"temp_dec\":%u,\"mb_swap\":%u,\"mb_unit\":%u,\"locked\":%s}\n",
        s->name, s->dhcp, s->ip[0], s->ip[1], s->ip[2], s->ip[3], s->mask[0], s->mask[1], s->mask[2], s->mask[3],
        s->gw[0], s->gw[1], s->gw[2], s->gw[3], s->dns[0], s->dns[1], s->dns[2], s->dns[3],
        s->cal_t / 100.0, s->cal_h / 100.0, s->cal_p / 100.0, s->disp_bright, s->led_level,
        s->disp_flip_x, s->disp_flip_y, s->disp_temp_dec, s->mb_swap, s->mb_unit, s->pass[0] ? "true" : "false");
    return n < 0 ? 0 : ((size_t)n >= cap ? cap - 1 : (size_t)n);
}

size_t settings_to_text(char *buf, size_t cap) {
    const settings_t *s = &g_set;
    int n = snprintf(buf, cap,
        "name:       %s\r\n"
        "network:    %s  ip %u.%u.%u.%u  mask %u.%u.%u.%u  gw %u.%u.%u.%u  dns %u.%u.%u.%u\r\n"
        "calibrate:  temp %+.2f C  hum %+.2f %%  press %+.2f hPa\r\n"
        "display:    brightness %u %%  flipX %u  flipY %u  temp decimals %u\r\n"
        "led:        %u %%\r\n"
        "modbus:     word swap %u  unit id %u\r\n"
        "password:   %s\r\n",
        s->name, s->dhcp ? "DHCP" : "static", s->ip[0], s->ip[1], s->ip[2], s->ip[3], s->mask[0], s->mask[1], s->mask[2], s->mask[3],
        s->gw[0], s->gw[1], s->gw[2], s->gw[3], s->dns[0], s->dns[1], s->dns[2], s->dns[3],
        s->cal_t / 100.0, s->cal_h / 100.0, s->cal_p / 100.0, s->disp_bright, s->disp_flip_x, s->disp_flip_y,
        s->disp_temp_dec, s->led_level, s->mb_swap, s->mb_unit, s->pass[0] ? "set" : "none");
    return n < 0 ? 0 : ((size_t)n >= cap ? cap - 1 : (size_t)n);
}

// ----------------------------------------------------------------------------
//  Slot s kontrolním součtem
// ----------------------------------------------------------------------------
#define SLOT_MAGIC   0x4D534554u      // "MSET"
#define SLOT_VERSION 1                // při změně struktury settings_t zvýšit (staré uložení se pak zahodí)

typedef struct __attribute__((packed)) {
    uint32_t magic;
    uint16_t version;
    uint16_t size;                    // velikost settings_t (ochrana proti změně struktury)
    uint32_t seq;                     // pořadové číslo uložení (nejvyšší = nejnovější)
    settings_t s;
    uint32_t crc;                     // CRC32 všeho před tímto polem
} slot_t;

_Static_assert(sizeof(slot_t) <= SETTINGS_SLOT_SIZE, "nastaveni se nevejde do slotu");

uint32_t settings_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(crc & 1));
    }
    return ~crc;
}

void settings_encode(const settings_t *s, uint32_t seq, uint8_t *slot) {
    memset(slot, 0xFF, SETTINGS_SLOT_SIZE);           // nepoužité bajty zůstanou ve stavu smazané flash
    slot_t t;
    memset(&t, 0, sizeof t);
    t.magic = SLOT_MAGIC; t.version = SLOT_VERSION; t.size = sizeof(settings_t); t.seq = seq; t.s = *s;
    t.crc = settings_crc32((const uint8_t *)&t, offsetof(slot_t, crc));
    memcpy(slot, &t, sizeof t);
}

bool settings_decode(const uint8_t *slot, settings_t *s, uint32_t *seq) {
    slot_t t;
    memcpy(&t, slot, sizeof t);
    if (t.magic != SLOT_MAGIC || t.version != SLOT_VERSION || t.size != sizeof(settings_t)) return false;
    if (t.crc != settings_crc32((const uint8_t *)&t, offsetof(slot_t, crc))) return false;
    *s = t.s;
    *seq = t.seq;
    return true;
}
