#include <string.h>
#include "modbus.h"

static size_t exception_resp(const uint8_t *req, uint8_t *resp, uint8_t code) {
    memcpy(resp, req, 8);
    resp[4] = 0; resp[5] = 3;
    resp[7] = req[7] | 0x80;
    resp[8] = code;
    return 9;
}

size_t modbus_handle(const uint8_t *req, size_t len, uint8_t *resp,
                     const uint16_t *regs, uint16_t nregs) {
    if (len < 12) return 0;
    if (req[2] || req[3]) return 0;                 // protocol id must be 0

    uint8_t  fc   = req[7];
    uint16_t addr = (req[8] << 8) | req[9];
    uint16_t qty  = (req[10] << 8) | req[11];

    if (fc != 0x03 && fc != 0x04) return exception_resp(req, resp, 0x01);
    if (qty < 1 || qty > 125 || (uint32_t)addr + qty > nregs)
        return exception_resp(req, resp, 0x02);

    memcpy(resp, req, 6);                           // transaction id + protocol id
    resp[4] = 0;
    resp[5] = 3 + 2 * qty;
    resp[6] = req[6];
    resp[7] = fc;
    resp[8] = 2 * qty;
    for (uint16_t i = 0; i < qty; i++) {
        resp[9 + 2 * i]  = regs[addr + i] >> 8;
        resp[10 + 2 * i] = regs[addr + i] & 0xFF;
    }
    return 9 + 2 * qty;
}
