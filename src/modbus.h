// ============================================================================
//  modbus.h - jednoduchý Modbus TCP server (čtení a zápis vybraných registrů)
//  Čistá logika bez závislosti na hardwaru, takže ji lze snadno testovat.
// ============================================================================
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// Funkce, která rozhoduje o zápisu do registru. Volá se dvakrát: nejdřív s commit = false
// (jen zkontrolovat, nic neměnit), potom s commit = true (provést). Vrací:
//   0 = v pořádku, 2 = neplatná adresa (registr se nedá zapisovat), 3 = neplatná hodnota.
typedef int (*modbus_write_fn)(uint16_t addr, uint16_t value, bool commit);

// Zpracuje jeden Modbus TCP rámec (hlavička MBAP + PDU) a sestaví odpověď.
//   req, len   přijatý rámec a jeho délka
//   resp       sem se zapíše odpověď (musí mít aspoň 9 + 2*125 bajtů)
//   regs       pole registrů (hodnoty ke čtení), nregs jejich počet
//   wr         funkce pro zápis (NULL = zápis zakázán)
//   unit       Unit ID, na které se odpovídá (0 = na jakékoli)
// Podporované funkce: 0x03 a 0x04 (čtení registrů, stejná tabulka),
// 0x06 (zápis jednoho registru) a 0x10 (zápis více registrů).
// Vrací délku odpovědi v bajtech, nebo 0 pokud se nemá odpovídat (neplatný rámec, cizí Unit ID).
size_t modbus_handle(const uint8_t *req, size_t len, uint8_t *resp,
                     const uint16_t *regs, uint16_t nregs, modbus_write_fn wr, uint8_t unit);
