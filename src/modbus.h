// ============================================================================
//  modbus.h - jednoduchý Modbus TCP server (jen čtení registrů)
//  Čistá logika bez závislosti na hardwaru, takže ji lze snadno testovat.
// ============================================================================
#pragma once
#include <stdint.h>
#include <stddef.h>

// Zpracuje jeden Modbus TCP rámec (hlavička MBAP + PDU) a sestaví odpověď.
//   req, len   přijatý rámec a jeho délka
//   resp       sem se zapíše odpověď (musí mít aspoň 9 + 2*125 bajtů)
//   regs       pole registrů (hodnoty ke čtení), nregs jejich počet
// Podporované funkce: 0x03 (Read Holding Registers) a 0x04 (Read Input Registers),
// obě čtou stejnou tabulku registrů.
// Vrací délku odpovědi v bajtech, nebo 0 pokud se nemá odpovídat (neplatný rámec).
size_t modbus_handle(const uint8_t *req, size_t len, uint8_t *resp,
                     const uint16_t *regs, uint16_t nregs);
