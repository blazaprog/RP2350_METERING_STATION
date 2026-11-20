// ============================================================================
//  web.c - HTTP server: webová stránka a JSON rozhraní
// ============================================================================
#include <stdio.h>
#include <string.h>
#include "web.h"

size_t http_handle(const char *req, char *resp, size_t cap, const sensor_data_t *d) {
    int n;

    if (strncmp(req, "GET /api/data", 13) == 0) {
        // --- JSON rozhraní pro další zpracování (skripty, Home Assistant, Node-RED...) ---
        char body[160];
        int blen = snprintf(body, sizeof body,
            "{\"temperature\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f,\"status\":%d}\n",
            d->temp, d->hum, d->press, d->status);
        // Hlavičky: typ obsahu, přesná délka těla a "Connection: close" (po odpovědi se spojení zavře).
        n = snprintf(resp, cap,
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Length: %d\r\nConnection: close\r\n\r\n%s", blen, body);

    } else if (strncmp(req, "GET / ", 6) == 0) {
        // --- Webová stránka pro člověka; meta refresh ji sám obnovuje každé 2 sekundy ---
        char body[640];
        int blen;
        if (d->status == 0) {
            blen = snprintf(body, sizeof body,
                "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                "<meta http-equiv='refresh' content='2'><title>Meteo</title>"
                "<style>body{font-family:sans-serif;margin:2em;background:#111;color:#eee}"
                "h1{font-size:1.2em;color:#8ab}p{font-size:2em;margin:.3em 0}</style></head><body>"
                "<h1>RP2350 Meteo stanice</h1>"
                "<p>Teplota: %.1f &deg;C</p><p>Vlhkost: %.1f %%</p><p>Tlak: %.1f hPa</p>"
                "<p style='font-size:.8em'><a style='color:#8ab' href='/api/data'>JSON</a></p>"
                "</body></html>",
                d->temp, d->hum, d->press);
        } else {
            // Senzor neodpovídá: nechceme zobrazovat nesmyslná čísla.
            blen = snprintf(body, sizeof body,
                "<!DOCTYPE html><html><head><meta charset='utf-8'>"
                "<meta http-equiv='refresh' content='2'><title>Meteo</title></head><body>"
                "<h1>Chyba senzoru BME280</h1></body></html>");
        }
        n = snprintf(resp, cap,
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %d\r\nConnection: close\r\n\r\n%s", blen, body);

    } else {
        // Cokoliv jiného (například /favicon.ico) -> 404 Not Found.
        n = snprintf(resp, cap,
            "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    }

    if (n < 0) return 0;
    return (size_t)n < cap ? (size_t)n : cap - 1;        // ochrana proti přetečení bufferu
}
