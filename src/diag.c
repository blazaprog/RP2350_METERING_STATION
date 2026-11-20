// ============================================================================
//  diag.c - sestavení diagnostických výstupů (registry, JSON, text)
//  Čte jen stavové proměnné, hardware je v diag_hw.c, takže se dá vyzkoušet na počítači.
// ============================================================================
#include <stdio.h>
#include <string.h>
#include "diag.h"
#include "settings.h"
#include "config.h"
#include "sensor_data.h"
#include "net_w6300.h"

extern sensor_data_t g_data;

static const char *const NET_NAMES[] = {"no_chip", "no_link", "dhcp", "up"};

uint16_t diag_flags(void) {
    int ns = net_state_code();
    uint16_t f = 0;
    if (g_data.status != 0)       f |= 1u << 0;
    if (ns == 0)                  f |= 1u << 1;
    if (ns == 1)                  f |= 1u << 2;
    if (ns == 2)                  f |= 1u << 3;
    if (diag_watchdog_reboot())   f |= 1u << 4;
    if (g_settings_defaulted)     f |= 1u << 5;
    return f;
}

// Uloží 32bitové číslo do dvou registrů (horní slovo první).
static void put32(uint16_t *r, uint32_t v) { r[0] = (uint16_t)(v >> 16); r[1] = (uint16_t)v; }

void diag_fill_regs(uint16_t *r) {
    uint8_t ip[4];
    net_ip_bytes(ip);
    put32(&r[16], diag_uptime_s());
    r[18] = FW_VERSION_NUM;
    r[19] = diag_flags();
    r[20] = diag_watchdog_reboot() ? 1 : 0;
    r[21] = (uint16_t)(int16_t)(diag_chip_temp() * 10.0f);
    put32(&r[22], g_diag.sensor_err);
    put32(&r[24], g_diag.modbus_req);
    put32(&r[26], g_diag.http_req);
    r[28] = (uint16_t)net_state_code();
    r[29] = (uint16_t)((ip[0] << 8) | ip[1]);
    r[30] = (uint16_t)((ip[2] << 8) | ip[3]);
}

static size_t clampn(int n, size_t cap) { return n < 0 ? 0 : ((size_t)n >= cap ? cap - 1 : (size_t)n); }

size_t diag_json(char *buf, size_t cap) {
    int ns = net_state_code();
    int n = snprintf(buf, cap,
        "{\"fw\":\"%s\",\"uptime_s\":%lu,\"reset\":\"%s\",\"chip_temp\":%.1f,\"flags\":%u,"
        "\"sensor_errors\":%lu,\"http_requests\":%lu,\"modbus_requests\":%lu,\"modbus_exceptions\":%lu,"
        "\"net_state\":\"%s\",\"link_changes\":%lu,\"settings_saves\":%lu}\n",
        FW_VERSION, (unsigned long)diag_uptime_s(), diag_watchdog_reboot() ? "watchdog" : "power-on",
        diag_chip_temp(), diag_flags(), (unsigned long)g_diag.sensor_err, (unsigned long)g_diag.http_req,
        (unsigned long)g_diag.modbus_req, (unsigned long)g_diag.modbus_exc, NET_NAMES[ns & 3],
        (unsigned long)g_diag.link_changes, (unsigned long)g_settings_saves);
    return clampn(n, cap);
}

size_t diag_text(char *buf, size_t cap) {
    uint32_t up = diag_uptime_s();
    int ns = net_state_code();
    int n = snprintf(buf, cap,
        "firmware:   %s\r\n"
        "uptime:     %lud %02lu:%02lu:%02lu\r\n"
        "last reset: %s\r\n"
        "chip temp:  %.1f C\r\n"
        "sensor:     %s (read errors since boot: %lu)\r\n"
        "network:    %s  ip %s  mac %s  (cable changes: %lu)\r\n"
        "requests:   http %lu  modbus %lu (exceptions %lu)\r\n"
        "settings:   saved %lu times since boot%s\r\n"
        "flags:      0x%04X\r\n",
        FW_VERSION, (unsigned long)(up / 86400), (unsigned long)(up % 86400 / 3600), (unsigned long)(up % 3600 / 60),
        (unsigned long)(up % 60), diag_watchdog_reboot() ? "watchdog (device had hung)" : "power-on / other",
        diag_chip_temp(), g_data.status ? "ERROR" : "OK", (unsigned long)g_diag.sensor_err,
        NET_NAMES[ns & 3], net_ip_text(), net_mac_text(), (unsigned long)g_diag.link_changes,
        (unsigned long)g_diag.http_req, (unsigned long)g_diag.modbus_req, (unsigned long)g_diag.modbus_exc,
        (unsigned long)g_settings_saves, g_settings_defaulted ? ", defaults at boot (flash was empty)" : "",
        diag_flags());
    return clampn(n, cap);
}
