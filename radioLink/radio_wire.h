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


#pragma pack(push, 1)

typedef struct {
    uint8_t nodeId;

    /* Stored as little-endian bytes on wire. */
    uint8_t sessionSeqIdLe[4];

    /* Per-message counter (little-endian bytes on wire). */
    uint8_t msgCounterLe[4];
} radioHdr_t;

typedef struct {
    radioHdr_t hdr;
    uint8_t payloadLen;
    uint8_t payload[64];
} radioMsg_t;

#pragma pack(pop)

/* =========================================================================
 * Wire v2 layout definition (byte-oriented, alignment-proof)
 *
 * This struct is NOT meant to be dereferenced as "native integers" for multi-byte
 * fields; it exists so we can derive offsets and header length via offsetof().
 *
 * Layout:
 * [0]  ver=2
 * [1]  nodeId
 * [2..5]  sessionSeqId LE32
 * [6..9]  msgCounter  LE32
 * [10] payloadLen
 * [11..] payload bytes
 * ========================================================================= */
#pragma pack(push, 1)
typedef struct {
    uint8_t ver;
    uint8_t nodeId;
    uint8_t sessionSeqIdLe[4];
    uint8_t msgCounterLe[4];
    uint8_t payloadLen;
    uint8_t payload[1]; /* flexible tail marker (do not sizeof for full frame) */
} radioWireV2_t;
#pragma pack(pop)

/* Offsets derived from struct layout (single source of truth) */
#define RL_W2_OFF_VER          ((uint8_t)offsetof(radioWireV2_t, ver))
#define RL_W2_OFF_NODE_ID      ((uint8_t)offsetof(radioWireV2_t, nodeId))
#define RL_W2_OFF_SESSION_LE   ((uint8_t)offsetof(radioWireV2_t, sessionSeqIdLe))
#define RL_W2_OFF_COUNTER_LE   ((uint8_t)offsetof(radioWireV2_t, msgCounterLe))
#define RL_W2_OFF_PAYLOAD_LEN  ((uint8_t)offsetof(radioWireV2_t, payloadLen))
#define RL_W2_OFF_PAYLOAD      ((uint8_t)offsetof(radioWireV2_t, payload))


#define RADIOLINK_WIRE_V2_HDR_LEN_DERIVED \
    ((uint8_t)offsetof(radioWireV2_t, payload))

_Static_assert(RADIOLINK_WIRE_V2_HDR_LEN_DERIVED == 11U, "Wire v2 header length must be 11 bytes");

// === WIRE_V3_CRYPTO_LAYOUT (Wire v3: AES-CTR + CMAC; fixed tag) ===
// NOTE: This struct exists ONLY to derive offsetof() layout macros.
//       It must not be used for memcpy/casting due to alignment/packing risks.

#define RADIOLINK_WIRE_V3_VERSION                (0x03u)
#define RADIOLINK_WIRE_V3_TAG_LEN                (16u)

typedef struct __attribute__((packed)) radioWireV3_t {
    uint8_t  version;         // 0x03
    uint8_t  nodeId;
    uint32_t sessionSeqId_le;  // LE32 on wire
    uint32_t msgCounter_le;    // LE32 on wire
    uint8_t  payloadLen;       // ciphertext length (bytes)
    uint8_t  payload[1];       // ciphertext starts here (variable), followed by tag
} radioWireV3_t;

#define RL_W3_OFF_VERSION                        (offsetof(radioWireV3_t, version))
#define RL_W3_OFF_NODE_ID                        (offsetof(radioWireV3_t, nodeId))
#define RL_W3_OFF_SESSION_SEQ_ID                 (offsetof(radioWireV3_t, sessionSeqId_le))
#define RL_W3_OFF_MSG_COUNTER                    (offsetof(radioWireV3_t, msgCounter_le))
#define RL_W3_OFF_PAYLOAD_LEN                    (offsetof(radioWireV3_t, payloadLen))
#define RL_W3_OFF_PAYLOAD                        (offsetof(radioWireV3_t, payload))

#define RADIOLINK_WIRE_V3_HDR_LEN_DERIVED        (offsetof(radioWireV3_t, payload))

#define RADIOLINK_WIRE_RADIO_MAX_LEN          (255u)
#define RADIOLINK_WIRE_V3_OVERHEAD_LEN        (RADIOLINK_WIRE_V3_HDR_LEN_DERIVED + RADIOLINK_WIRE_V3_TAG_LEN)
#define RADIOLINK_WIRE_V3_MAX_PLAINTEXT_LEN   (RADIOLINK_WIRE_RADIO_MAX_LEN - RADIOLINK_WIRE_V3_OVERHEAD_LEN)

_Static_assert(RADIOLINK_WIRE_V3_HDR_LEN_DERIVED == 11u, "Wire v3 header length must be 11 bytes");

/* Radio on-air max (SX1262 LoRa payload max) */
#define RADIOLINK_WIRE_RADIO_MAX_LEN          (255u)
#define RADIOLINK_WIRE_V3_OVERHEAD_LEN        (RADIOLINK_WIRE_V3_HDR_LEN_DERIVED + RADIOLINK_WIRE_V3_TAG_LEN)
#define RADIOLINK_WIRE_V3_MAX_PLAINTEXT_LEN   (RADIOLINK_WIRE_RADIO_MAX_LEN - RADIOLINK_WIRE_V3_OVERHEAD_LEN)
// === END WIRE_V3_CRYPTO_LAYOUT ===

#endif /* RADIO_WIRE_H_ */
