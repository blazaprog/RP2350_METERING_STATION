#pragma once
#include <stdint.h>
#include <stddef.h>

// Handles one Modbus TCP request (MBAP + PDU). Returns response length, 0 = no reply.
// Supports FC 0x03 and 0x04 (same register map).
size_t modbus_handle(const uint8_t *req, size_t len, uint8_t *resp,
                     const uint16_t *regs, uint16_t nregs);
