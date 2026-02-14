/*
 * radio_link.c
 *
 *  Created on: Feb 12, 2026
 *      Author: johng
 */




#include "radio_link.h"
#include <memory.h>

bool RadioLink_SendString(SX1262_Handle *sx, const char *s)
{
    bool status = false;
    uint8_t buf[256];
    size_t len;

    if (!sx || !s) {
        return false;
    }

    status = SX1262_SendBuffer(sx, (const char *)s);

    return status;
}



bool RadioLink_TryDecodeToString(const uint8_t *rx,
                                uint8_t rx_len,
                                char *out,
                                uint8_t out_max)
{
    bool status = false;

    if (!rx || !out || out_max == 0U) {
        return false;
    }

    /* Expect: [len | bytes...] */
    if (rx_len < 1U) {
        return false;
    }

    uint8_t n = rx[0];
    if (n > (uint8_t)(rx_len - 1U)) {
        n = (uint8_t)(rx_len - 1U);
    }

    if (n >= out_max) {
        n = (uint8_t)(out_max - 1U);
    }

    memcpy(out, &rx[1], n);
    out[n] = 0;

    status = true;

    return status;
}

