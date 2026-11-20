// ============================================================================
//  service.h - servisní textový příkazový řádek
//
//  Stejné příkazy běží na dvou místech: v servisním TCP kanálu (port SERVICE_PORT,
//  jeden socket) a na USB sériovém portu. Slouží k diagnostice a k záchraně, když si
//  špatným nastavením sítě zablokuješ web.
//
//  Příkazy: help, diag, show, auth <heslo>, defaults, reboot, quit
// ============================================================================
#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef struct { bool authed; } svc_session_t;      // stav jednoho připojení (zadal správné heslo?)
typedef enum { SVC_NONE, SVC_CLOSE, SVC_REBOOT } svc_action_t;

// Úvodní text po připojení.
const char *service_banner(void);

// Provede jeden příkaz. Odpověď se zapíše do out (cap = velikost). Vrací, co má volající ještě udělat:
// zavřít spojení (quit) nebo restartovat zařízení (reboot).
svc_action_t service_exec(svc_session_t *ss, const char *line, char *out, size_t cap);
