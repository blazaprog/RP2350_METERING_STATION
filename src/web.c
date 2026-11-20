// ============================================================================
//  web.c - HTTP server: webová stránka a JSON rozhraní
//
//  Stránka (src/web/index.html) je statická. Při sestavení se převede na pole bajtů
//  (web_page.h) a prohlížeč si pak sám dotazuje JSON na /api/data a /api/history.
//  Zařízení tak posílá jen malé odpovědi a graf kreslí prohlížeč.
// ============================================================================
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "web.h"
#include "history.h"
#include "config.h"
#include "web_page.h"      // vygenerováno při sestavení z src/web/index.html (pole web_page)

// Hlavičky se zapisují těsně před tělo odpovědi; tolik bajtů pro ně necháme na začátku bufferu.
#define HDR_RESERVE 220

// Jeden bod historie se v JSON vejde nejvýše do tolika znaků (tři čísla s čárkami).
#define BYTES_PER_POINT 20

// Je to požadavek GET na danou cestu? Za cestou musí být mezera nebo '?'
// (aby "/api/data" neodpovídalo i na "/api/datax").
static bool route(const char *req, const char *path) {
    if (strncmp(req, "GET ", 4) != 0) return false;
    size_t n = strlen(path);
    return strncmp(req + 4, path, n) == 0 && (req[4 + n] == ' ' || req[4 + n] == '?');
}

// Připíše text na pozici pos v bufferu velikosti room a vrátí novou pozici (nikdy nepřeteče).
static size_t put(char *buf, size_t room, size_t pos, const char *fmt, ...) {
    if (pos >= room) return room;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + pos, room - pos, fmt, ap);
    va_end(ap);
    if (n < 0) return pos;
    return (size_t)n >= room - pos ? room : pos + (size_t)n;
}

// Dopíše HTTP hlavičky před tělo, které leží v resp od offsetu HDR_RESERVE, a vrátí začátek odpovědi.
static const char *finish(char *resp, size_t body_len, const char *status, const char *ctype,
                          const char *extra, size_t *out_len) {
    char hdr[HDR_RESERVE];
    int hl = snprintf(hdr, sizeof hdr,
        "HTTP/1.1 %s\r\nContent-Type: %s\r\n%sContent-Length: %u\r\nConnection: close\r\n\r\n",
        status, ctype, extra, (unsigned)body_len);
    if (hl <= 0 || hl > HDR_RESERVE) { *out_len = 0; return resp; }
    char *start = resp + HDR_RESERVE - hl;       // hlavičky přilepíme přesně před tělo
    memcpy(start, hdr, (size_t)hl);
    *out_len = (size_t)hl + body_len;
    return start;
}

// Aktuální hodnoty a údaje o zařízení. Při chybě senzoru jsou hodnoty null.
static size_t json_data(char *body, size_t room, const web_info_t *w) {
    const sensor_data_t *d = w->d;
    size_t p = 0;
    if (d->status == 0)
        p = put(body, room, p, "{\"temperature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f,", d->temp, d->hum, d->press);
    else
        p = put(body, room, p, "{\"temperature\":null,\"humidity\":null,\"pressure\":null,");
    p = put(body, room, p, "\"status\":%d,\"uptime_s\":%lu,\"ip\":\"%s\",\"mac\":\"%s\",\"fw\":\"%s\"}\n",
            d->status, (unsigned long)w->uptime_s, w->ip, w->mac, FW_VERSION);
    return p;
}

// Historie pro graf: tři pole celých čísel od nejstaršího bodu po nejnovější.
// Jednotky: t = setiny °C, h = setiny %, p = desetiny hPa. Když by se všechny body nevešly
// do bufferu, pošleme jen nejnovější.
static size_t json_history(char *body, size_t room) {
    size_t count = history_count();
    size_t max_pts = room > 200 ? (room - 200) / BYTES_PER_POINT : 0;
    size_t n = count < max_pts ? count : max_pts;
    size_t first = count - n;
    size_t p = put(body, room, 0, "{\"step\":%d,\"n\":%u", HISTORY_STEP_S, (unsigned)n);
    for (int series = 0; series < 3; series++) {
        p = put(body, room, p, ",\"%c\":[", "thp"[series]);
        for (size_t i = 0; i < n; i++) {
            int16_t t; uint16_t h, pr;
            history_get(first + i, &t, &h, &pr);
            int v = series == 0 ? t : (series == 1 ? h : pr);
            p = put(body, room, p, i ? ",%d" : "%d", v);
        }
        p = put(body, room, p, "]");
    }
    return put(body, room, p, "}\n");
}

const char *http_handle(const char *req, char *resp, size_t cap, const web_info_t *info, size_t *out_len) {
    *out_len = 0;
    if (cap <= HDR_RESERVE + 64) return resp;                // buffer je příliš malý
    char *body = resp + HDR_RESERVE;
    size_t room = cap - HDR_RESERVE;
    const char *json_hdrs = "Access-Control-Allow-Origin: *\r\nCache-Control: no-store\r\n";

    if (route(req, "/api/data"))
        return finish(resp, json_data(body, room, info), "200 OK", "application/json", json_hdrs, out_len);

    if (route(req, "/api/history"))
        return finish(resp, json_history(body, room), "200 OK", "application/json", json_hdrs, out_len);

    if (route(req, "/")) {
        size_t len = sizeof web_page - 1;                    // bez koncové nuly
        if (len > room) return finish(resp, 0, "500 Internal Server Error", "text/plain", "", out_len);
        memcpy(body, web_page, len);
        return finish(resp, len, "200 OK", "text/html; charset=utf-8", "Cache-Control: no-cache\r\n", out_len);
    }

    if (route(req, "/favicon.ico"))                          // prohlížeč se na ikonu ptá sám; odpovíme prázdně
        return finish(resp, 0, "204 No Content", "text/plain", "", out_len);

    const char *msg = "Nenalezeno\n";
    size_t m = strlen(msg);
    memcpy(body, msg, m);
    return finish(resp, m, "404 Not Found", "text/plain; charset=utf-8", "", out_len);
}
