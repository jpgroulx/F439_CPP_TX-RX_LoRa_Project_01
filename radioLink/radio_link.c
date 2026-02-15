/*
 * radio_link.c
 *
 *  Created on: Feb 12, 2026
 *      Author: johng
 */




#include "radio_link.h"
#include <memory.h>

uint8_t RadioLink_WireV0_FrameLenFromPayloadLen(uint8_t payload_len)
{
    uint8_t frame_len = 0;

    /* 1 + payload_len (wrap-safe because payload_len is u8) */
    frame_len = (uint8_t)(RADIOLINK_WIRE_V0_HDR_LEN + payload_len);

    return frame_len;
}

bool RadioLink_SendString(SX1262_Handle *sx, const char *s)
{
    bool status = false;
    uint8_t frame[RADIOLINK_WIRE_V0_MAX_FRAME_LEN];
    size_t len;
    uint8_t payload_len;
    uint8_t frame_len;

    if (!sx || !s) {
        return false;
    }

    len = strlen(s);
    if (len > RADIOLINK_WIRE_V0_MAX_PAYLOAD_LEN) {
        len = RADIOLINK_WIRE_V0_MAX_PAYLOAD_LEN;
    }

    payload_len = (uint8_t)len;

    frame[0] = payload_len;
    memcpy(&frame[RADIOLINK_WIRE_V0_HDR_LEN], s, payload_len);

    frame_len = (uint8_t)(RADIOLINK_WIRE_V0_HDR_LEN + payload_len);

    status = RadioLink_SendBytes(sx, frame, frame_len);

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

    /* Wire v0 requires at least the length byte. */
    if (rx_len < RADIOLINK_WIRE_V0_HDR_LEN) {
        return false;
    }

    uint8_t n = rx[0];

    /* Clamp n to available bytes actually received. */
    uint8_t avail = (uint8_t)(rx_len - RADIOLINK_WIRE_V0_HDR_LEN);
    if (n > avail) {
        n = avail;
    }

    /* Clamp n to output buffer capacity (leave room for NUL). */
    if (n >= out_max) {
        n = (uint8_t)(out_max - 1U);
    }

    memcpy(out, &rx[RADIOLINK_WIRE_V0_HDR_LEN], n);
    out[n] = 0;

    status = true;

    return status;
}

bool RadioLink_SendBytes(SX1262_Handle *sx, const uint8_t *buf, uint8_t len)
{
    bool status = false;

    if (!sx || !buf || len == 0U) {
        return false;
    }

    status = SX1262_SendBytes(sx, buf, len);

    return status;
}


