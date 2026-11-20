#include <stdio.h>
#include <string.h>
#include "web.h"

size_t http_handle(const char *req, char *resp, size_t cap, const sensor_data_t *d) {
    int n;
    if (strncmp(req, "GET /api/data", 13) == 0) {
        char body[160];
        int blen = snprintf(body, sizeof body,
            "{\"temperature\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f,\"status\":%d}\n",
            d->temp, d->hum, d->press, d->status);
        n = snprintf(resp, cap,
            "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
            "Content-Length: %d\r\nConnection: close\r\n\r\n%s", blen, body);
    } else if (strncmp(req, "GET / ", 6) == 0) {
        char body[400];
        int blen = snprintf(body, sizeof body,
            "<!DOCTYPE html><html><head><meta charset='utf-8'>"
            "<meta http-equiv='refresh' content='2'><title>Meteo</title></head><body>"
            "<h2>Teplota: %.1f &deg;C</h2><h2>Vlhkost: %.1f %%</h2>"
            "<h2>Tlak: %.0f hPa</h2></body></html>",
            d->temp, d->hum, d->press);
        n = snprintf(resp, cap,
            "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
            "Content-Length: %d\r\nConnection: close\r\n\r\n%s", blen, body);
    } else {
        n = snprintf(resp, cap,
            "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
    }
    if (n < 0) return 0;
    return (size_t)n < cap ? (size_t)n : cap - 1;
}
