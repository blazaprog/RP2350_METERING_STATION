// ============================================================================
//  web.h - jednoduchý HTTP server (webová stránka a JSON)
// ============================================================================
#pragma once
#include <stddef.h>
#include "sensor_data.h"

// Podle textu HTTP požadavku sestaví kompletní HTTP odpověď (hlavičky + tělo).
//   req   přijatý požadavek jako text zakončený nulou
//   resp  sem se odpověď zapíše, cap je velikost bufferu
//   d     aktuální naměřené hodnoty
// Podporované adresy:  GET /          webová stránka s hodnotami (obnovuje se po 2 s)
//                      GET /api/data  stejné hodnoty ve formátu JSON
// Vrací počet zapsaných bajtů.
size_t http_handle(const char *req, char *resp, size_t cap, const sensor_data_t *d);
