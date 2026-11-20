// ============================================================================
//  modbus.c - Modbus TCP server (funkce 0x03, 0x04, 0x06 a 0x10)
//
//  Struktura rámce Modbus TCP:
//    bajty 0-1  Transaction ID (klient ho vymyslí, my ho opíšeme do odpovědi)
//    bajty 2-3  Protocol ID (vždy 0)
//    bajty 4-5  délka zbytku rámce (od bajtu 6 dál)
//    bajt  6    Unit ID (adresa zařízení; u TCP se většinou ignoruje)
//    bajt  7    kód funkce
//    bajty 8-9  adresa prvního registru
//    bajty 10-11 počet registrů (u funkce 0x06 hodnota k zapsání)
//    funkce 0x10 má navíc bajt 12 = počet datových bajtů a od bajtu 13 data
// ============================================================================
#include <string.h>
#include "modbus.h"

// Sestaví chybovou odpověď (exception). Kód funkce dostane nejvyšší bit nastavený
// a následuje kód chyby: 0x01 = nepodporovaná funkce, 0x02 = neplatná adresa/počet, 0x03 = neplatná hodnota.
static size_t exception_resp(const uint8_t *req, uint8_t *resp, uint8_t code) {
    memcpy(resp, req, 8);          // opíšeme Transaction ID, Protocol ID, délku, Unit ID, funkci
    resp[4] = 0; resp[5] = 3;      // délka zbytku: Unit ID + funkce + kód chyby = 3 bajty
    resp[7] = req[7] | 0x80;       // označení chyby
    resp[8] = code;
    return 9;
}

size_t modbus_handle(const uint8_t *req, size_t len, uint8_t *resp,
                     const uint16_t *regs, uint16_t nregs, modbus_write_fn wr, uint8_t unit) {
    if (len < 12) return 0;                              // příliš krátký rámec
    if (req[2] || req[3]) return 0;                      // Protocol ID musí být 0
    if (unit && req[6] != unit) return 0;                // jiné Unit ID: mlčíme (zpráva není pro nás)

    uint8_t  fc   = req[7];                              // kód funkce
    uint16_t addr = (req[8] << 8) | req[9];              // adresa prvního registru
    uint16_t qty  = (req[10] << 8) | req[11];            // počet registrů (u funkce 0x06 hodnota)

    if (fc == 0x03 || fc == 0x04) {
        if (qty < 1 || qty > 125 || (uint32_t)addr + qty > nregs)  // mimo tabulku registrů
            return exception_resp(req, resp, 0x02);

        // Úspěšná odpověď: hlavička + počet bajtů dat + samotná data (každý registr 2 bajty, big-endian).
        memcpy(resp, req, 6);                            // Transaction ID + Protocol ID + (délka níže)
        resp[4] = 0;
        resp[5] = 3 + 2 * qty;                           // Unit ID + funkce + počet bajtů + data
        resp[6] = req[6];                                // Unit ID opíšeme
        resp[7] = fc;
        resp[8] = 2 * qty;                               // počet datových bajtů
        for (uint16_t i = 0; i < qty; i++) {
            resp[9 + 2 * i]  = regs[addr + i] >> 8;      // horní bajt registru
            resp[10 + 2 * i] = regs[addr + i] & 0xFF;    // dolní bajt registru
        }
        return 9 + 2 * qty;
    }

    if (fc == 0x06) {                                    // zápis jednoho registru (hodnota je v bajtech 10-11)
        if (!wr) return exception_resp(req, resp, 0x01);
        int r = wr(addr, qty, false);                    // nejdřív jen zkontrolovat
        if (r) return exception_resp(req, resp, (uint8_t)r);
        wr(addr, qty, true);                             // pak provést
        memcpy(resp, req, 12);                           // odpověď je přesná kopie požadavku
        return 12;
    }

    if (fc == 0x10) {                                    // zápis více registrů
        if (!wr) return exception_resp(req, resp, 0x01);
        if (len < 13) return exception_resp(req, resp, 0x03);
        uint8_t bytes = req[12];
        if (qty < 1 || qty > 123 || bytes != 2 * qty || len < (size_t)13 + bytes)
            return exception_resp(req, resp, 0x03);
        if ((uint32_t)addr + qty > nregs)                // mimo tabulku (také chrání před přetečením adresy)
            return exception_resp(req, resp, 0x02);
        for (uint16_t i = 0; i < qty; i++) {             // všechny registry se zkontrolují dřív, než se cokoli změní
            int r = wr(addr + i, (uint16_t)((req[13 + 2 * i] << 8) | req[14 + 2 * i]), false);
            if (r) return exception_resp(req, resp, (uint8_t)r);
        }
        for (uint16_t i = 0; i < qty; i++)
            wr(addr + i, (uint16_t)((req[13 + 2 * i] << 8) | req[14 + 2 * i]), true);
        memcpy(resp, req, 12);                           // odpověď: adresa a počet zapsaných registrů
        resp[4] = 0; resp[5] = 6;
        return 12;
    }

    return exception_resp(req, resp, 0x01);              // nepodporovaná funkce
}
