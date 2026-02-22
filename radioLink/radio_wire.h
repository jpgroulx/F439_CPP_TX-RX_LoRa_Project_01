/*
 * radio_wire.h
 *
 *  Created on: Feb 17, 2026
 *      Author: johng
 */


/*
 * radio_wire.h
 *
 * Wire-format packet structs (protocol-level).
 * Transport-agnostic by design (LoRa now, USB later).
 *
 * NOTE:
 * - Intentionally may be unused for now (do not delete).
 * - These represent the planned v1+ header/payload model.
 */

#ifndef RADIO_WIRE_H_
#define RADIO_WIRE_H_

#pragma once

#include <stdint.h>
#include <stddef.h> /* offsetof */

/*
 * radio_wire.h
 *
 * Wire v3-only layout definitions.
 *
 * This header exists to provide:
 * - The on-wire version constant
 * - The fixed header length (derived)
 * - The fixed tag length (CMAC)
 * - Byte offsets used by RadioLink parsing/formatting
 *
 * IMPORTANT:
 * - Offsets are derived using offsetof() from a packed layout stub.
 * - Do not cast raw RX buffers to this struct type; it is for offsetof() only.
 */

#define RADIOLINK_WIRE_V3_VERSION            (0x03u)
#define RADIOLINK_WIRE_V3_TAG_LEN            (16u)

typedef struct __attribute__((packed)) radioWireV3_t {
    uint8_t version;           /* 0x03 */
    uint8_t nodeId;
    uint32_t sessionSeqId_le;  /* LE32 on wire */
    uint32_t msgCounter_le;    /* LE32 on wire */
    uint8_t payloadLen;        /* plaintext/ciphertext length (bytes) */
    uint8_t payload[1];        /* payload starts here (variable), followed by tag */
} radioWireV3_t;

/* Offsets used by RadioLink */
#define RL_W3_OFF_VERSION                        (offsetof(radioWireV3_t, version))
#define RL_W3_OFF_NODE_ID                        (offsetof(radioWireV3_t, nodeId))
#define RL_W3_OFF_SESSION_SEQ_ID                 (offsetof(radioWireV3_t, sessionSeqId_le))
#define RL_W3_OFF_MSG_COUNTER                    (offsetof(radioWireV3_t, msgCounter_le))
#define RL_W3_OFF_PAYLOAD_LEN                    (offsetof(radioWireV3_t, payloadLen))
#define RL_W3_OFF_PAYLOAD                        (offsetof(radioWireV3_t, payload))

#define RADIOLINK_WIRE_V3_HDR_LEN_DERIVED    (offsetof(radioWireV3_t, payload))

_Static_assert(RADIOLINK_WIRE_V3_HDR_LEN_DERIVED == 11U, "Wire v3 header length must be 11 bytes");

#define RADIOLINK_WIRE_RADIO_MAX_LEN         (255u)
#define RADIOLINK_WIRE_V3_OVERHEAD_LEN       (RADIOLINK_WIRE_V3_HDR_LEN_DERIVED + RADIOLINK_WIRE_V3_TAG_LEN)
#define RADIOLINK_WIRE_V3_MAX_PLAINTEXT_LEN  (RADIOLINK_WIRE_RADIO_MAX_LEN - RADIOLINK_WIRE_V3_OVERHEAD_LEN)

#endif /* RADIO_WIRE_H_ */
