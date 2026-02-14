/*
 * radio_link.c
 *
 *  Created on: Feb 12, 2026
 *      Author: johng
 */




#include "radio_link.h"
#include <memory.h>


static bool RadioLink_WireV0_EncodeString(const char *s,
                                         char *out,
                                         size_t out_max,
                                         uint8_t *payload_len_out)
{
    bool status = false;
    size_t len;
    size_t max_payload;

    if (!s || !out || !payload_len_out || out_max == 0U) {
        return false;
    }

    /* Leave room for NUL terminator in out[]. */
    max_payload = out_max - 1U;
    if (max_payload > RADIOLINK_WIRE_V0_MAX_PAYLOAD_LEN) {
        max_payload = RADIOLINK_WIRE_V0_MAX_PAYLOAD_LEN;
    }

    len = strlen(s);
    if (len > max_payload) {
        len = max_payload;
    }

    memcpy(out, s, len);
    out[len] = 0;

    *payload_len_out = (uint8_t)len;

    status = true;

    return status;
}


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
    char tx[256];
    uint8_t payload_len = 0;

    if (!sx || !s) {
        return false;
    }

    status = RadioLink_WireV0_EncodeString(s, tx, sizeof(tx), &payload_len);
    if (!status) {
        return false;
    }

    /* On-air v0 framing is still applied by the driver today. */
    status = SX1262_SendString(sx, tx);

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



