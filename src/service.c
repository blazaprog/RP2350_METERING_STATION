// ============================================================================
//  service.c - příkazy servisního kanálu (viz service.h)
// ============================================================================
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include "service.h"
#include "diag.h"
#include "settings.h"
#include "net_w6300.h"
#include "config.h"

const char *service_banner(void) {
    return "RP2350 meteo service. Type 'help' for commands.\r\n";
}

// Vrací true, když je akce povolená (heslo nenastavené nebo zadané); jinak do out napíše výzvu.
static bool allowed(const svc_session_t *ss, char *out, size_t cap) {
    if (!g_set.pass[0] || ss->authed) return true;
    snprintf(out, cap, "Password required: auth <password>\r\n");
    return false;
}

svc_action_t service_exec(svc_session_t *ss, const char *line, char *out, size_t cap) {
    out[0] = 0;
    while (*line == ' ') line++;                       // úvodní mezery přeskočíme
    char cmd[16] = {0};
    const char *arg = line;
    size_t i = 0;
    while (*arg && *arg != ' ' && i < sizeof cmd - 1) cmd[i++] = *arg++;
    while (*arg == ' ') arg++;                         // arg ukazuje na zbytek řádku (argument příkazu)

    if (cmd[0] == 0) return SVC_NONE;                  // prázdný řádek

    if (strcasecmp(cmd, "help") == 0) {
        snprintf(out, cap,
            "help             this list\r\n"
            "diag             diagnostics (uptime, errors, network, counters)\r\n"
            "show             current settings\r\n"
            "auth <password>  unlock defaults and reboot (when a password is set)\r\n"
            "defaults         restore factory settings (also clears the password)\r\n"
            "reboot           restart the device\r\n"
            "quit             close this connection\r\n");
    } else if (strcasecmp(cmd, "diag") == 0) {
        diag_text(out, cap);
    } else if (strcasecmp(cmd, "show") == 0) {
        settings_to_text(out, cap);
    } else if (strcasecmp(cmd, "auth") == 0) {
        if (settings_pass_ok(arg)) { ss->authed = true; snprintf(out, cap, "OK\r\n"); }
        else snprintf(out, cap, "Wrong password\r\n");
    } else if (strcasecmp(cmd, "defaults") == 0) {
        if (!allowed(ss, out, cap)) return SVC_NONE;
        settings_defaults(&g_set);
        bool ok = settings_save_now();
        net_request_reconfig();                        // síť se přenastaví podle výchozích hodnot
        snprintf(out, cap, ok ? "Factory settings restored. Network is reconfiguring.\r\n"
                              : "Defaults applied, but writing to flash failed.\r\n");
    } else if (strcasecmp(cmd, "reboot") == 0) {
        if (!allowed(ss, out, cap)) return SVC_NONE;
        snprintf(out, cap, "Restarting...\r\n");
        return SVC_REBOOT;
    } else if (strcasecmp(cmd, "quit") == 0 || strcasecmp(cmd, "exit") == 0) {
        snprintf(out, cap, "Bye\r\n");
        return SVC_CLOSE;
    } else {
        snprintf(out, cap, "Unknown command. Type 'help'.\r\n");
    }
    return SVC_NONE;
}
