/*
 * radio_link.h
 *
 *  Created on: Feb 12, 2026
 *      Author: johng
 */

#ifndef RADIO_LINK_H_
#define RADIO_LINK_H_

#include <main.h>
#include <stdint.h>
#include <stdbool.h>
#include "ada1897_mb85rs64b.h"

#include "sx1262.h"
#include "radio_wire.h"

// === RADIOLINK_CRYPTO_ENABLE (compile-time) ===
#ifndef RADIOLINK_CRYPTO_ENABLE
#define RADIOLINK_CRYPTO_ENABLE    (1)
#endif


/* Debug switches (set to 1 only during testing) */
#define RADIOLINK_DEBUG_TAMPER_ENABLE 0

#define RADIOLINK_DEBUG_REPLAY_REJECT_ENABLE 0
#define RADIOLINK_DEBUG_TX_REPLAY_ONESHOT_ENABLE 0

/* Persistence policy */
#define RL_PERSIST_ENABLE                 1
#define RL_PERSIST_DISABLE_WHEN_DEBUGGER  1


/* Wire format v0:
 *   [ u8 payload_len | payload_bytes... ]
 *
 * payload_len is the number of payload bytes that follow.
 * Total on-air length = 1 + payload_len.
 */
#define RADIOLINK_WIRE_V0_HDR_LEN          (1U)
#define RADIOLINK_WIRE_V0_MAX_PAYLOAD_LEN  (255U)
#define RADIOLINK_WIRE_V0_MAX_FRAME_LEN    (256U)

/* Wire v1 (Counters / Replay) */
#define RADIOLINK_WIRE_V1_VERSION            (0x01U)

/* v1 header: [ver(1) | node_id(1) | counter_le(4) | payload_len(1)] */
#define RADIOLINK_WIRE_V1_HDR_LEN            (1U + 1U + 4U + 1U)

/* Keep payload bound small for now (matches planned radio_msg_t payload[64]) */
#define RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN    (64U)
#define RADIOLINK_WIRE_V1_MAX_FRAME_LEN      (RADIOLINK_WIRE_V1_HDR_LEN + RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN)

/* Wire v2 (Session + Counters / Replay)
 * v2 header: [ver(1) | node_id(1) | sessionSeqId_le(4) | msgCounter_le(4) | payload_len(1)]
 * Layout:
 *   [0]    version=2
 *   [1]    nodeId
 *   [2..5] sessionSeqId LE32
 *   [6..9] msgCounter  LE32
 *   [10]   payloadLen
 *   [11..] payload bytes
 */
#define RADIOLINK_WIRE_V2_VERSION (0x02U)

/* Header length is derived from canonical v2 layout in radio_wire.h */
#define RADIOLINK_WIRE_V2_HDR_LEN (RADIOLINK_WIRE_V2_HDR_LEN_DERIVED)

#define RADIOLINK_WIRE_V2_MAX_PAYLOAD_LEN (RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN)
#define RADIOLINK_WIRE_V2_MAX_FRAME_LEN (RADIOLINK_WIRE_V2_HDR_LEN + RADIOLINK_WIRE_V2_MAX_PAYLOAD_LEN)

// === WIRE_V3_CRYPTO_CONSTANTS (public routing; derived in radio_wire.h) ===
#define RADIOLINK_W3_VERSION                 (RADIOLINK_WIRE_V3_VERSION)
#define RADIOLINK_W3_HDR_LEN                 (RADIOLINK_WIRE_V3_HDR_LEN_DERIVED)
#define RADIOLINK_W3_TAG_LEN                 (RADIOLINK_WIRE_V3_TAG_LEN)
// === END WIRE_V3_CRYPTO_CONSTANTS ===

typedef struct radioLinkParsedV2_t{
    uint8_t nodeId;
    uint32_t sessionSeqId;
    uint32_t msgCounter;
    const uint8_t *payload;
    uint8_t payloadLen;
} radioLinkParsedV2_t;

// === RADIOLINK_CRYPTO_CONTEXT (placeholders; unused for now) ===
typedef struct radioLinkCryptoCtx_t {
    uint8_t masterKey[16];
    uint8_t encKey[16];
    uint8_t macKey[16];
    uint8_t keyIsValid;   // 0/1
} radioLinkCryptoCtx_t;


bool RadioLink_SendString(SX1262_Handle *sx, const char *s);
bool RadioLink_TryDecodeToString(const uint8_t *rx, uint8_t rx_len, char *out, uint8_t out_max);
uint8_t RadioLink_WireV0_FrameLenFromPayloadLen(uint8_t payload_len);
bool RadioLink_SendBytes(SX1262_Handle *sx, const uint8_t *buf, uint8_t len);

// === WIRE_V3_CRYPTO_STUBS (no behavior change marker; now becomes real AES-CTR) ===
bool RadioLink_BuildWireV3Frame_Stub(uint8_t *out, uint8_t outMax,
                                     uint8_t nodeId,
                                     uint32_t sessionSeqId,
                                     uint32_t msgCounter,
                                     const uint8_t *plain, uint8_t plainLen,
                                     uint8_t *outLen);

bool RadioLink_ParseWireV3Frame_Stub(const uint8_t *rx, uint8_t rxLen,
                                     uint8_t *outPlain, uint8_t outPlainMax,
                                     uint8_t *outPlainLen);

// === END WIRE_V3_CRYPTO_STUBS ===

#endif /* RADIO_LINK_H_ */
