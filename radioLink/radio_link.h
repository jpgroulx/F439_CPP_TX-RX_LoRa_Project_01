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
#define RADIOLINK_CRYPTO_ENABLE (1)
#endif

/* Wire v3-only policy: legacy versions are not supported and have been removed. */

/* Debug switches (set to 1 only during testing) */
#define RADIOLINK_DEBUG_TAMPER_ENABLE 0

#define RADIOLINK_DEBUG_REPLAY_REJECT_ENABLE 0
#define RADIOLINK_DEBUG_TX_REPLAY_ONESHOT_ENABLE 0

/* Persistence policy */
#define RL_PERSIST_ENABLE                 1
#define RL_PERSIST_DISABLE_WHEN_DEBUGGER  1

#define RADIOLINK_WIRE_V3_HDR_LEN (RADIOLINK_WIRE_V3_HDR_LEN_DERIVED)

// === WIRE_V3_CRYPTO_CONSTANTS (public routing; derived in radio_wire.h) ===
#define RADIOLINK_W3_VERSION                 (RADIOLINK_WIRE_V3_VERSION)
#define RADIOLINK_W3_HDR_LEN                 (RADIOLINK_WIRE_V3_HDR_LEN_DERIVED)
#define RADIOLINK_W3_TAG_LEN                 (RADIOLINK_WIRE_V3_TAG_LEN)
// === END WIRE_V3_CRYPTO_CONSTANTS ===


// === RADIOLINK_CRYPTO_CONTEXT (placeholders; unused for now) ===
typedef struct radioLinkCryptoCtx_t {
    uint8_t masterKey[16];
    uint8_t encKey[16];
    uint8_t macKey[16];
    uint8_t keyIsValid;   // 0/1
} radioLinkCryptoCtx_t;


bool RadioLink_SendString(SX1262_Handle *sx, const char *s);
bool RadioLink_TryDecodeToString(const uint8_t *rx, uint8_t rx_len, char *out, uint8_t out_max);
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
