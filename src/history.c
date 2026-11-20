// ============================================================================
//  history.c - kruhová paměť s průměrovanými body (viz history.h)
//  Spotřeba RAM: HISTORY_POINTS * 6 bajtů (u 360 bodů asi 2,2 kB).
// ============================================================================
#include <math.h>
#include <stdbool.h>
#include "history.h"
#include "config.h"

static int16_t  s_t[HISTORY_POINTS];    // teplota v setinách °C
static uint16_t s_h[HISTORY_POINTS];    // vlhkost v setinách %
static uint16_t s_p[HISTORY_POINTS];    // tlak v desetinách hPa
static size_t   s_head;                 // kam se zapíše další bod
static size_t   s_cnt;                  // kolik bodů je platných

// Průběžný součet měření za právě probíhající interval.
static double   s_sum_t, s_sum_h, s_sum_p;
static uint32_t s_num;                  // kolik měření je v součtu
static uint32_t s_start_ms;             // začátek aktuálního intervalu
static bool     s_started;

// Zaokrouhlí a ořízne na rozsah typu (aby případná chybná hodnota nezpůsobila přetečení).
static int32_t clampi(float v, int32_t lo, int32_t hi) {
    long r = lroundf(v);
    return r < lo ? lo : (r > hi ? hi : (int32_t)r);
}

void history_add(uint32_t now_ms, float temp, float hum, float press) {
    if (!s_started) { s_started = true; s_start_ms = now_ms; }
    s_sum_t += temp; s_sum_h += hum; s_sum_p += press; s_num++;

    if (now_ms - s_start_ms >= HISTORY_STEP_S * 1000u) {      // interval uplynul: uložíme průměr
        s_t[s_head] = (int16_t) clampi((float)(s_sum_t / s_num) * 100.0f, -32768, 32767);
        s_h[s_head] = (uint16_t)clampi((float)(s_sum_h / s_num) * 100.0f, 0, 65535);
        s_p[s_head] = (uint16_t)clampi((float)(s_sum_p / s_num) * 10.0f,  0, 65535);
        s_head = (s_head + 1) % HISTORY_POINTS;
        if (s_cnt < HISTORY_POINTS) s_cnt++;
        s_sum_t = s_sum_h = s_sum_p = 0; s_num = 0;           // nový interval
        s_start_ms = now_ms;
    }
}

size_t history_count(void) { return s_cnt; }

void history_get(size_t i, int16_t *t, uint16_t *h, uint16_t *p) {
    size_t idx = (s_head + HISTORY_POINTS - s_cnt + i) % HISTORY_POINTS;   // i = 0 je nejstarší bod
    *t = s_t[idx]; *h = s_h[idx]; *p = s_p[idx];
}
