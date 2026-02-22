/*
 * radio_link.c
 *
 *  Created on: Feb 12, 2026
 *      Author: johng
 */





#include "radio_link.h"
#include "stdio.h"
#include "memory.h"

static uint32_t g_radiolink_tx_counter;
static uint32_t g_radiolink_sessionSeqId;
static uint32_t g_radiolink_last_seen_counter[256];
static uint8_t g_radiolink_seen[256];

#if (RADIOLINK_CRYPTO_ENABLE == 0)
/* Wire v2 replay state: per-node (sessionSeqId, msgCounter) */
static uint32_t g_radiolink_last_seen_sessionSeqId_v2[256];
static uint32_t g_radiolink_last_seen_counter_v2[256];
static uint8_t g_radiolink_seen_v2[256];
#endif

/* Wire v3 replay state: per-node (sessionSeqId, lastAcceptedMsgCounter)
 * NOTE: Enforcement is implemented in a later task (after CMAC verify).
 */
static uint32_t gRadioLinkLastSeenSessionSeqIdV3[256];
static uint32_t gRadioLinkLastSeenCounterV3[256];
static uint8_t gRadioLinkSeenV3[256];

#if (RADIOLINK_DEBUG_TX_REPLAY_ONESHOT_ENABLE == 1)
/* Debug: TX periodic replay injection (Wire v3 test support) */
static uint32_t gRadioLinkDebugTxSendCount;
#endif

static radioLinkCryptoCtx_t gRlCryptoCtx;

static const uint8_t gRadioLinkMasterKey_Default[16] = {
    0x00u, 0x01u, 0x02u, 0x03u, 0x04u, 0x05u, 0x06u, 0x07u,
    0x08u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu, 0x0Du, 0x0Eu, 0x0Fu
};


static void radioLinkDeriveKeys_Stub(radioLinkCryptoCtx_t *ctx, uint8_t nodeId);
// === RADIOLINK_CRYPTO_LAZY_INIT (AES-CTR only; CMAC later) ===
static uint8_t gRlCryptoInitDone = 0u;

static void radioLinkCryptoEnsureInit(uint8_t nodeId)
{
    if (gRlCryptoInitDone != 0u) {
        return;
    }

    memcpy(gRlCryptoCtx.masterKey, gRadioLinkMasterKey_Default, sizeof(gRlCryptoCtx.masterKey));
    radioLinkDeriveKeys_Stub(&gRlCryptoCtx, nodeId);

    gRlCryptoInitDone = 1u;
}
// === END RADIOLINK_CRYPTO_LAZY_INIT ===
// === END RADIOLINK_CRYPTO_CONTEXT ===


// === RADIOLINK_AES_CTR_HELPER (unused for now) ===
// NOTE: This is a placeholder skeleton. It does NOT yet configure CRYP
// for AES-CTR mode. That will be done in a separate task.

extern CRYP_HandleTypeDef hcryp;

// === RADIOLINK_AES_ECB_CMAC_HELPERS (unused for now) ===
static bool radioLinkAesEcbEncryptBlock_Stub(const uint8_t key[16],
                                            const uint8_t inBlock[16],
                                            uint8_t outBlock[16]) __attribute__((unused));

static void radioLinkAesCmac128_Stub(const uint8_t key[16],
                                     const uint8_t *msg,
                                     uint32_t msgLen,
                                     uint8_t outTag[16]) __attribute__((unused));
// === END RADIOLINK_AES_ECB_CMAC_HELPERS ===

static bool radioLinkAesCtrXor_Stub(uint8_t *data,
                                    uint32_t len,
                                    const uint8_t key[16],
                                    const uint8_t nonce[16])
{
    HAL_StatusTypeDef status;

    if ((data == NULL) || (key == NULL) || (nonce == NULL) || (len == 0u)) {
        return false;
    }

    /* CRYP driver expects key/IV pointers via init struct (CubeMX style) */
    hcryp.Init.pKey = (uint32_t *)key;
    hcryp.Init.pInitVect = (uint32_t *)nonce;

    status = HAL_CRYP_Init(&hcryp);
    if (status != HAL_OK) {
        return false;
    }

    /*
     * AES-CTR is symmetric: "encrypt" produces keystream XOR.
     * Use Encrypt for both encrypt/decrypt.
     *
     * HAL_CRYP_Encrypt length unit follows DataWidthUnit.
     * Your IOC sets DataWidthUnit=BYTE, so Nothing to do.
     */

    status = HAL_CRYP_Encrypt(&hcryp,
                              (uint32_t *)data,
                              len,
                              (uint32_t *)data,
                              HAL_MAX_DELAY);

    if (status != HAL_OK) {
        return false;
    }

    return true;
}
// === END RADIOLINK_AES_CTR_HELPER ===

// === RADIOLINK_AES_ECB_ENCRYPT_BLOCK (unused for now) ===
static bool radioLinkAesEcbEncryptBlock_Stub(const uint8_t key[16],
                                            const uint8_t inBlock[16],
                                            uint8_t outBlock[16])
{
    HAL_StatusTypeDef status;
    uint32_t savedAlg;
    uint32_t *savedKey;
    uint32_t *savedIv;

    if ((key == NULL) || (inBlock == NULL) || (outBlock == NULL)) {
        return false;
    }

    /* Save/restore CRYP init fields (IOC config is AES-CTR). */
    savedAlg = hcryp.Init.Algorithm;
    savedKey = hcryp.Init.pKey;
    savedIv = hcryp.Init.pInitVect;

    hcryp.Init.Algorithm = CRYP_AES_ECB;
    hcryp.Init.pKey = (uint32_t *)key;
    hcryp.Init.pInitVect = NULL;

    status = HAL_CRYP_Init(&hcryp);
    if (status != HAL_OK) {
        hcryp.Init.Algorithm = savedAlg;
        hcryp.Init.pKey = savedKey;
        hcryp.Init.pInitVect = savedIv;
        (void)HAL_CRYP_Init(&hcryp);
        return false;
    }

    /* One 16-byte block. DataWidthUnit is BYTE in IOC. */
    status = HAL_CRYP_Encrypt(&hcryp,
                             (uint32_t *)inBlock,
                             16u,
                             (uint32_t *)outBlock,
                             HAL_MAX_DELAY);

    /* Restore AES-CTR (or whatever IOC set). */
    hcryp.Init.Algorithm = savedAlg;
    hcryp.Init.pKey = savedKey;
    hcryp.Init.pInitVect = savedIv;
    (void)HAL_CRYP_Init(&hcryp);

    return (status == HAL_OK);
}
// === END RADIOLINK_AES_ECB_ENCRYPT_BLOCK ===

// === RADIOLINK_CT_COMPARE (CMAC tag) ===
static bool radioLinkConstTimeEq16(const uint8_t a[16], const uint8_t b[16])
{
    uint8_t diff = 0u;

    for (uint32_t i = 0u; i < 16u; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }

    return (diff == 0u);
}
// === END RADIOLINK_CT_COMPARE ===

// === RADIOLINK_AES_CMAC_128 (unused for now) ===
static void radioLinkCmacLeftShiftOne(uint8_t out[16], const uint8_t in[16])
{
    uint8_t carry = 0u;

    for (int i = 15; i >= 0; i--) {
        uint8_t newCarry = (uint8_t)((in[i] & 0x80u) ? 1u : 0u);
        out[i] = (uint8_t)((in[i] << 1) | carry);
        carry = newCarry;
    }
}

static void radioLinkAesCmacSubkeys(const uint8_t key[16], uint8_t k1[16], uint8_t k2[16])
{
    /* CMAC Rb for 128-bit block size */
    const uint8_t Rb = 0x87u;
    uint8_t zero[16];
    uint8_t L[16];

    memset(zero, 0, sizeof(zero));
    memset(L, 0, sizeof(L));
    (void)radioLinkAesEcbEncryptBlock_Stub(key, zero, L);

    radioLinkCmacLeftShiftOne(k1, L);
    if ((L[0] & 0x80u) != 0u) {
        k1[15] ^= Rb;
    }

    radioLinkCmacLeftShiftOne(k2, k1);
    if ((k1[0] & 0x80u) != 0u) {
        k2[15] ^= Rb;
    }
}

static void radioLinkAesCmac128_Stub(const uint8_t key[16],
                                     const uint8_t *msg,
                                     uint32_t msgLen,
                                     uint8_t outTag[16])
{
    uint8_t k1[16];
    uint8_t k2[16];
    uint8_t X[16];
    uint8_t Mlast[16];
    uint8_t Y[16];
    uint32_t n;
    bool lastComplete;

    if ((key == NULL) || (outTag == NULL)) {
        return;
    }

    /* CMAC allows msgLen=0 */
    radioLinkAesCmacSubkeys(key, k1, k2);

    n = (msgLen + 15u) / 16u;
    if (n == 0u) {
        n = 1u;
    }

    lastComplete = ((msgLen != 0u) && ((msgLen % 16u) == 0u));

    memset(X, 0, sizeof(X));
    memset(Mlast, 0, sizeof(Mlast));

    if (lastComplete) {
        const uint8_t *last = &msg[16u * (n - 1u)];
        for (uint32_t i = 0u; i < 16u; i++) {
            Mlast[i] = (uint8_t)(last[i] ^ k1[i]);
        }
    } else {
        uint32_t lastLen = (msgLen % 16u);
        if (msgLen == 0u) {
            lastLen = 0u;
        }

        if ((msg != NULL) && (msgLen != 0u)) {
            const uint8_t *last = &msg[16u * (n - 1u)];
            if (lastLen != 0u) {
                memcpy(Mlast, last, lastLen);
            }
        }

        /* Padding: 0x80 then zeros */
        Mlast[lastLen] = 0x80u;

        for (uint32_t i = 0u; i < 16u; i++) {
            Mlast[i] = (uint8_t)(Mlast[i] ^ k2[i]);
        }
    }

    /* CBC-MAC over blocks 1..n-1 */
    for (uint32_t block = 0u; block < (n - 1u); block++) {
        const uint8_t *Mi = &msg[16u * block];
        for (uint32_t i = 0u; i < 16u; i++) {
            Y[i] = (uint8_t)(X[i] ^ Mi[i]);
        }
        (void)radioLinkAesEcbEncryptBlock_Stub(key, Y, X);
    }

    /* Final block */
    for (uint32_t i = 0u; i < 16u; i++) {
        Y[i] = (uint8_t)(X[i] ^ Mlast[i]);
    }
    (void)radioLinkAesEcbEncryptBlock_Stub(key, Y, outTag);
}
// === END RADIOLINK_AES_CMAC_128 ===

// === RADIOLINK_KEY_DERIVATION_STUB (unused for now) ===
static void radioLinkDeriveKeys_Stub(radioLinkCryptoCtx_t *ctx, uint8_t nodeId)
{
    (void)nodeId;

    if (ctx == NULL) {
        return;
    }

    // Placeholder: copy masterKey into encKey/macKey so we have deterministic bytes.
    // Real implementation will derive separate keys (ENC/MAC labels) using CMAC.
    for (uint32_t i = 0u; i < 16u; i++) {
        ctx->encKey[i] = ctx->masterKey[i];
        ctx->macKey[i] = ctx->masterKey[i];
    }

    ctx->keyIsValid = 1u;
}
// === END RADIOLINK_KEY_DERIVATION_STUB ===

static bool RadioLink_SessionSeqId_Store(uint32_t v) {
    bool ok;
    uint8_t buf[4];
    uint16_t addr;

    ok = false;
    addr = (uint16_t)(FRAM_BASE_ADDR + 4U);

    buf[0] = (uint8_t)((v >> 0) & 0xFFU);
    buf[1] = (uint8_t)((v >> 8) & 0xFFU);
    buf[2] = (uint8_t)((v >> 16) & 0xFFU);
    buf[3] = (uint8_t)((v >> 24) & 0xFFU);

    ok = FRAM_WriteBytes(addr, buf, (uint16_t)sizeof(buf));
    return ok;
}

static bool RadioLink_SessionSeqId_Load(uint32_t *out_sessionSeqId) {
    bool ok;
    uint8_t buf[4];
    uint32_t v;
    uint16_t addr;

    ok = false;
    v = 0U;
    addr = (uint16_t)(FRAM_BASE_ADDR + 4U);

    if (out_sessionSeqId == NULL) {
        return false;
    }

    ok = FRAM_ReadBytes(addr, buf, (uint16_t)sizeof(buf));
    if (!ok) {
        return false;
    }

    v = ((uint32_t)buf[0] << 0) |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24);

    *out_sessionSeqId = v;
    return true;
}


static bool RadioLink_DebuggerAttached(void) {
    volatile uint32_t dhcsr;
    bool attached;

#if defined(CoreDebug)
    /* DHCSR.C_DEBUGEN bit indicates debugger connected/enabled */
    dhcsr = CoreDebug->DHCSR;
    attached = ((dhcsr & 0x00000001UL) != 0U);
#else
    attached = false;
#endif

    return attached;
}

static bool RadioLink_PersistAllowed(void) {
    bool allowed;

    allowed = true;

#if (RL_PERSIST_ENABLE == 0)
    allowed = false;
#endif

#if (RL_PERSIST_DISABLE_WHEN_DEBUGGER == 1)
    if (RadioLink_DebuggerAttached()) {
        allowed = false;
    }
#endif

    return allowed;
}

static bool RadioLink_TxCounter_Store(uint32_t counter) {
    bool ok;
    uint8_t buf[4];

    ok = false;

    buf[0] = (uint8_t)((counter >> 0) & 0xFFU);
    buf[1] = (uint8_t)((counter >> 8) & 0xFFU);
    buf[2] = (uint8_t)((counter >> 16) & 0xFFU);
    buf[3] = (uint8_t)((counter >> 24) & 0xFFU);

    ok = FRAM_WriteBytes(FRAM_BASE_ADDR, buf, (uint16_t)sizeof(buf));
    if (!ok) {
        goto done;
    }

    ok = true;

done:
    return ok;
}

static bool RadioLink_TxCounter_Load(uint32_t *out_counter) {
    bool ok;
    uint8_t buf[4];
    uint32_t v;

    ok = false;
    v = 0U;

    if (out_counter == NULL) {
        return false;
    }

    ok = FRAM_ReadBytes(FRAM_BASE_ADDR, buf, (uint16_t)sizeof(buf));
    if (!ok) {
        goto done;
    }

    v = ((uint32_t)buf[0] << 0) |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24);

    *out_counter = v;
    ok = true;

done:
    return ok;
}

static void RadioLink_EncodeLe32(uint8_t out[4], uint32_t v) {
    out[0] = (uint8_t)(v & 0xFFU);
    out[1] = (uint8_t)((v >> 8) & 0xFFU);
    out[2] = (uint8_t)((v >> 16) & 0xFFU);
    out[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint32_t RadioLink_DecodeLe32(const uint8_t in[4]) {
    uint32_t v = 0;

    v |= (uint32_t)in[0];
    v |= ((uint32_t)in[1] << 8);
    v |= ((uint32_t)in[2] << 16);
    v |= ((uint32_t)in[3] << 24);

    return v;
}

#if (RADIOLINK_CRYPTO_ENABLE == 0)
static bool RadioLink_BuildWireV2Frame(uint8_t *frame,
                                       uint8_t frameMax,
                                       uint8_t nodeId,
                                       uint32_t sessionSeqId,
                                       uint32_t msgCounter,
                                       const uint8_t *payload,
                                       uint8_t payloadLen,
                                       uint8_t *outFrameLen)
{
    bool ok = false;
    uint8_t frameLen = 0U;

    if (!frame || !outFrameLen || (!payload && payloadLen != 0U)) {
        ok = false;
    } else if (payloadLen > RADIOLINK_WIRE_V2_MAX_PAYLOAD_LEN) {
        ok = false;
    } else {
        frameLen = (uint8_t)(RADIOLINK_WIRE_V2_HDR_LEN + payloadLen);
        if (frameLen > frameMax) {
            ok = false;
        } else {
            frame[RL_W2_OFF_VER] = RADIOLINK_WIRE_V2_VERSION;
            frame[RL_W2_OFF_NODE_ID] = nodeId;
            RadioLink_EncodeLe32(&frame[RL_W2_OFF_SESSION_LE], sessionSeqId);
            RadioLink_EncodeLe32(&frame[RL_W2_OFF_COUNTER_LE], msgCounter);
            frame[RL_W2_OFF_PAYLOAD_LEN] = payloadLen;

            if (payloadLen != 0U) {
                memcpy(&frame[RL_W2_OFF_PAYLOAD], payload, payloadLen);
            }

            *outFrameLen = frameLen;
            ok = true;
        }
    }

    return ok;
}
#endif

/* Default node_id derivation: stable u8 from STM32 UID words. */
static uint8_t RadioLink_GetNodeId(void) {
    uint8_t id = 0;
    uint32_t w0;
    uint32_t w1;
    uint32_t w2;
    uint32_t x;

    /* These are provided by STM32 HAL. */
    extern uint32_t HAL_GetUIDw0(void);
    extern uint32_t HAL_GetUIDw1(void);
    extern uint32_t HAL_GetUIDw2(void);

    w0 = HAL_GetUIDw0();
    w1 = HAL_GetUIDw1();
    w2 = HAL_GetUIDw2();

    x = (w0 ^ w1 ^ w2);
    id = (uint8_t)(x ^ (x >> 8) ^ (x >> 16) ^ (x >> 24));

    return id;
}

// === WIRE_V3_CRYPTO_STUBS (no behavior change) ===

bool RadioLink_BuildWireV3Frame_Stub(uint8_t *out, uint8_t outMax,
                                     uint8_t nodeId,
                                     uint32_t sessionSeqId,
                                     uint32_t msgCounter,
                                     const uint8_t *plain, uint8_t plainLen,
                                     uint8_t *outLen)
{
    uint32_t totalLen;
    uint8_t nonce[16];

    if ((out == NULL) || (outLen == NULL) || (plain == NULL)) {
        return false;
    }

    radioLinkCryptoEnsureInit(nodeId);

    totalLen = (uint32_t)RADIOLINK_WIRE_V3_HDR_LEN_DERIVED +
               (uint32_t)plainLen +
               (uint32_t)RADIOLINK_WIRE_V3_TAG_LEN;

    if (totalLen > (uint32_t)outMax) {
        return false;
    }

    out[RL_W3_OFF_VERSION] = RADIOLINK_WIRE_V3_VERSION;
    out[RL_W3_OFF_NODE_ID] = nodeId;
    RadioLink_EncodeLe32(&out[RL_W3_OFF_SESSION_SEQ_ID], sessionSeqId);
    RadioLink_EncodeLe32(&out[RL_W3_OFF_MSG_COUNTER], msgCounter);
    out[RL_W3_OFF_PAYLOAD_LEN] = plainLen;

    if (plainLen > 0u) {
        memcpy(&out[RL_W3_OFF_PAYLOAD], plain, plainLen);
    }

    memset(nonce, 0, sizeof(nonce));
    RadioLink_EncodeLe32(&nonce[0], sessionSeqId);
    RadioLink_EncodeLe32(&nonce[4], msgCounter);

    if (plainLen > 0u) {
        if (!radioLinkAesCtrXor_Stub(&out[RL_W3_OFF_PAYLOAD],
                                     (uint32_t)plainLen,
                                     gRlCryptoCtx.encKey,
                                     nonce)) {
            return false;
        }
    }

    /* CMAC over header||ciphertext (tag excluded) */
    {
        uint32_t macLen;

        macLen = (uint32_t)RADIOLINK_WIRE_V3_HDR_LEN_DERIVED + (uint32_t)plainLen;

        /* Write tag directly into the frame's tag field */
        radioLinkAesCmac128_Stub(gRlCryptoCtx.macKey,
                                 out,
                                 macLen,
                                 &out[RL_W3_OFF_PAYLOAD + plainLen]);
    }

    *outLen = (uint8_t)totalLen;
    return true;
}

bool RadioLink_ParseWireV3Frame_Stub(const uint8_t *rx, uint8_t rxLen,
                                     uint8_t *outPlain, uint8_t outPlainMax,
                                     uint8_t *outPlainLen)
{
    uint8_t nodeId;
    uint32_t sessionSeqId;
    uint32_t msgCounter;
    uint8_t payloadLen;
    uint32_t expectedLen;
    uint8_t nonce[16];

    /* Task 1: replay state is added but not enforced yet */
    (void)gRadioLinkLastSeenSessionSeqIdV3;
    (void)gRadioLinkLastSeenCounterV3;
    (void)gRadioLinkSeenV3;

    if ((rx == NULL) || (outPlain == NULL) || (outPlainLen == NULL)) {
        return false;
    }

    if (rxLen < (uint8_t)(RADIOLINK_WIRE_V3_HDR_LEN_DERIVED + RADIOLINK_WIRE_V3_TAG_LEN)) {
        return false;
    }

    if (rx[RL_W3_OFF_VERSION] != RADIOLINK_WIRE_V3_VERSION) {
        return false;
    }

    nodeId = rx[RL_W3_OFF_NODE_ID];
    radioLinkCryptoEnsureInit(nodeId);

    sessionSeqId = RadioLink_DecodeLe32(&rx[RL_W3_OFF_SESSION_SEQ_ID]);
    msgCounter = RadioLink_DecodeLe32(&rx[RL_W3_OFF_MSG_COUNTER]);
    payloadLen = rx[RL_W3_OFF_PAYLOAD_LEN];

    expectedLen = (uint32_t)RADIOLINK_WIRE_V3_HDR_LEN_DERIVED +
                  (uint32_t)payloadLen +
                  (uint32_t)RADIOLINK_WIRE_V3_TAG_LEN;

    if (expectedLen != (uint32_t)rxLen) {
        return false;
    }

    if (payloadLen > outPlainMax) {
        return false;
    }

    /* CMAC verify over header||ciphertext (tag excluded) */
    {
        uint8_t expectedTag[16];
        uint32_t macLen;
        const uint8_t *rxTag;

#if (RADIOLINK_DEBUG_TAMPER_ENABLE == 1)
    /* Debug tamper: compute CMAC over a modified copy (rx is const).
     * Use static storage to avoid stack usage. */
    static uint8_t macBuf[RADIOLINK_WIRE_V3_HDR_LEN_DERIVED + 255u];
#endif

        macLen = (uint32_t)RADIOLINK_WIRE_V3_HDR_LEN_DERIVED + (uint32_t)payloadLen;
        rxTag = &rx[RL_W3_OFF_PAYLOAD + payloadLen];

    #if (RADIOLINK_DEBUG_TAMPER_ENABLE == 1)
        memcpy(macBuf, rx, macLen);

        if (payloadLen > 0u) {
            macBuf[RL_W3_OFF_PAYLOAD] ^= 0x01u;
        } else {
            macBuf[RL_W3_OFF_VERSION] ^= 0x01u;
        }

        radioLinkAesCmac128_Stub(gRlCryptoCtx.macKey, macBuf, macLen, expectedTag);
#else
        radioLinkAesCmac128_Stub(gRlCryptoCtx.macKey, rx, macLen, expectedTag);
#endif
    if (!radioLinkConstTimeEq16(expectedTag, rxTag)) {
        return false;
    }
}

/* Wire v3 replay enforcement (AFTER CMAC verification)
 * Reject if msgCounter <= lastSeenCounter for same
 * (nodeId, sessionSeqId).
 */
if (gRadioLinkSeenV3[nodeId] != 0u) {
    if (gRadioLinkLastSeenSessionSeqIdV3[nodeId] == sessionSeqId) {
    	if (msgCounter <= gRadioLinkLastSeenCounterV3[nodeId]) {
    	#if (RADIOLINK_DEBUG_REPLAY_REJECT_ENABLE == 1)
    	    printf("RL: W3 REPLAY REJECT node=%u sess=%lu ctr=%lu last=%lu\r\n",
    	           (unsigned)nodeId,
    	           (unsigned long)sessionSeqId,
    	           (unsigned long)msgCounter,
    	           (unsigned long)gRadioLinkLastSeenCounterV3[nodeId]);
    	#endif
    	    return false;
    	}
    }
}

if (payloadLen > 0u) {
    memcpy(outPlain, &rx[RL_W3_OFF_PAYLOAD], payloadLen);
}

    memset(nonce, 0, sizeof(nonce));
    RadioLink_EncodeLe32(&nonce[0], sessionSeqId);
    RadioLink_EncodeLe32(&nonce[4], msgCounter);

    if (payloadLen > 0u) {
        if (!radioLinkAesCtrXor_Stub(outPlain,
                                     (uint32_t)payloadLen,
                                     gRlCryptoCtx.encKey,
                                     nonce)) {
            return false;
        }
    }

    /* Accept: update replay state only after
     * CMAC verified AND decrypt succeeded.
     */
    gRadioLinkSeenV3[nodeId] = 1u;
    gRadioLinkLastSeenSessionSeqIdV3[nodeId] = sessionSeqId;
    gRadioLinkLastSeenCounterV3[nodeId] = msgCounter;

    *outPlainLen = payloadLen;
    return true;
}

// === END WIRE_V3_CRYPTO_STUBS ===

uint8_t RadioLink_WireV0_FrameLenFromPayloadLen(uint8_t payload_len) {
    uint8_t frame_len = 0;

    /* 1 + payload_len (wrap-safe because payload_len is u8) */
    frame_len = (uint8_t)(RADIOLINK_WIRE_V0_HDR_LEN + payload_len);

    return frame_len;
}

bool RadioLink_SendString(SX1262_Handle *sx, const char *s) {
    bool status = false;
    size_t len;
    uint8_t payload_len;

    if (!sx || !s) {
        return false;
    }

    len = strlen(s);
    if (len > RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN) {
        printf("RL: TX string too long: %lu > %u (truncating; v3 max=%u)\r\n",
               (unsigned long)len,
               (unsigned)RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN,
               (unsigned)RADIOLINK_WIRE_V3_MAX_PLAINTEXT_LEN);
        len = RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN;
    }

    payload_len = (uint8_t)len;

    status = RadioLink_SendBytes(sx, (const uint8_t *)s, payload_len);
    return status;
}

bool RadioLink_TryDecodeToString(const uint8_t *rx, uint8_t rx_len, char *out, uint8_t out_max) {
    bool status = false;
    const uint8_t *payload = NULL;
    uint8_t payload_len = 0;

    if (!rx || !out || (out_max == 0U)) {
        return false;
    }

    // === WIRE_VERSION_SELECT_RX (compile-time; no behavior change yet) ===
#if (RADIOLINK_CRYPTO_ENABLE == 0)
#if (RADIOLINK_RX_ACCEPT_WIRE_V2 != 0)
    /* Prefer Wire v2 when structurally valid */
    if (rx_len >= RADIOLINK_WIRE_V2_HDR_LEN && rx[0] == RADIOLINK_WIRE_V2_VERSION) {
        uint8_t node_id = rx[1];
        uint32_t sessionSeqId = RadioLink_DecodeLe32(&rx[2]);
        uint32_t counter = RadioLink_DecodeLe32(&rx[6]);
        uint8_t n = rx[10];

        /* Structural validation: exact frame length must be present */
        if ((n <= RADIOLINK_WIRE_V2_MAX_PAYLOAD_LEN) && ((uint8_t)(RADIOLINK_WIRE_V2_HDR_LEN + n) == rx_len)) {
            /* Replay protection: (sessionSeqId, counter) per node_id */
            if (g_radiolink_seen_v2[node_id]) {
                uint32_t lastSession = g_radiolink_last_seen_sessionSeqId_v2[node_id];
                uint32_t lastCounter = g_radiolink_last_seen_counter_v2[node_id];

                if (sessionSeqId < lastSession) {
                    return false;
                }
                if ((sessionSeqId == lastSession) && (counter <= lastCounter)) {
                    return false;
                }
            }

            g_radiolink_seen_v2[node_id] = 1U;
            g_radiolink_last_seen_sessionSeqId_v2[node_id] = sessionSeqId;
            g_radiolink_last_seen_counter_v2[node_id] = counter;

            payload = &rx[RADIOLINK_WIRE_V2_HDR_LEN];
            payload_len = n;
        }
    }
#endif
    /* Prefer Wire v2 when structurally valid */
    #else

    // Crypto-enabled build will switch to Wire v3 later.
    // For now, keep Wire v2 to avoid behavior change until crypto is implemented.

    // === WIRE_V3_ATTEMPT_RX (CMAC verified) ===
    if (rx_len >= (uint8_t)(RADIOLINK_WIRE_V3_HDR_LEN_DERIVED + RADIOLINK_WIRE_V3_TAG_LEN) &&
        (rx[RL_W3_OFF_VERSION] == RADIOLINK_WIRE_V3_VERSION)) {
        uint8_t plainLen = 0u;

        if (RadioLink_ParseWireV3Frame_Stub(rx, rx_len, (uint8_t *)out, (uint8_t)(out_max - 1u), &plainLen)) {
            out[plainLen] = '\0';
            return true;
        }

        /* If it looks like v3 but fails validation, reject (do not fall through to legacy). */
        return false;
    }
    // === END WIRE_V3_ATTEMPT_RX ===

    /* Crypto-enabled build: v3-only RX policy (no Wire v2 fallback). */
    if (rx_len >= RADIOLINK_WIRE_V2_HDR_LEN && rx[0] == RADIOLINK_WIRE_V2_VERSION) {
        printf("RL: RX reject wire v2 (v3-only)\r\n");
        return false;
    }

    #endif
    // === END WIRE_VERSION_SELECT_RX ===

    /* ============================================================
     * LEGACY_WIRE_SUPPORT (V0/V1)
     * ------------------------------------------------------------
     * Historical development support for early radio bring-up.
     *
     * TX no longer emits V0/V1 frames (Wire v2 only).
     * RX retains legacy parsing temporarily for rollback safety.
     *
     * TODO:
     *   Remove this entire section once project is fully stabilized
     *   and legacy compatibility is no longer required.
     *
     * Search tag: LEGACY_WIRE_SUPPORT
     * ============================================================ */

    /* Fallback: Wire v1 when structurally valid */
    if (!payload) {
        if (rx_len >= RADIOLINK_WIRE_V1_HDR_LEN && rx[0] == RADIOLINK_WIRE_V1_VERSION) {
            uint8_t node_id = rx[1];
            uint32_t counter = RadioLink_DecodeLe32(&rx[2]);
            uint8_t n = rx[6];

            if ((n <= RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN) &&
                ((uint8_t)(RADIOLINK_WIRE_V1_HDR_LEN + n) == rx_len)) {

                if (g_radiolink_seen[node_id]) {
                    if (counter <= g_radiolink_last_seen_counter[node_id]) {
                        return false;
                    }
                }
                g_radiolink_seen[node_id] = 1U;
                g_radiolink_last_seen_counter[node_id] = counter;

                payload = &rx[RADIOLINK_WIRE_V1_HDR_LEN];
                payload_len = n;
            }
        }
    }


    /* Fallback: Wire v0 */
    if (!payload) {
        uint8_t n;
        uint8_t avail;

        if (rx_len < RADIOLINK_WIRE_V0_HDR_LEN) {
            return false;
        }

        n = rx[0];
        avail = (uint8_t)(rx_len - RADIOLINK_WIRE_V0_HDR_LEN);
        if (n > avail) {
            n = avail;
        }

        payload = &rx[RADIOLINK_WIRE_V0_HDR_LEN];
        payload_len = n;
    }

    /* Copy to output buffer and NUL-terminate */
    if (payload_len >= out_max) {
        payload_len = (uint8_t)(out_max - 1U);
    }

    memcpy(out, payload, payload_len);
    out[payload_len] = 0;

    status = true;
    return status;
}

bool RadioLink_SendBytes(SX1262_Handle *sx, const uint8_t *buf, uint8_t len) {
    bool status = false;
    uint8_t frame[RADIOLINK_WIRE_RADIO_MAX_LEN];
    uint8_t node_id;
    uint32_t counter;

    if (!sx || !buf || (len == 0U)) {
        return false;
    }

    if (len > RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN) {
        printf("RL: TX buf too long: %u > %u (truncating; v3 max=%u)\r\n",
               (unsigned)len,
               (unsigned)RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN,
               (unsigned)RADIOLINK_WIRE_V3_MAX_PLAINTEXT_LEN);
        len = RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN;
    }

    node_id = RadioLink_GetNodeId();

    if (g_radiolink_tx_counter == 0U) {
        /* Initialize once per boot from persistent store. */
        if (RadioLink_PersistAllowed()) {
            bool ok;
            uint32_t loadedCounter;
            uint32_t loadedSessionSeqId;
            uint32_t nextSessionSeqId;

            loadedCounter = 0U;
            ok = RadioLink_TxCounter_Load(&loadedCounter);
            if (ok) {
                g_radiolink_tx_counter = loadedCounter;
            } else {
                g_radiolink_tx_counter = 0U;
            }

            /* SessionSeqId: increments once per boot/session. */
            loadedSessionSeqId = 0U;
            ok = RadioLink_SessionSeqId_Load(&loadedSessionSeqId);
            if (ok) {
                nextSessionSeqId = loadedSessionSeqId + 1U;
            } else {
                nextSessionSeqId = 1U;
            }

            g_radiolink_sessionSeqId = nextSessionSeqId;

            /* Persist the new sessionSeqId once per boot. */
            (void)RadioLink_SessionSeqId_Store(nextSessionSeqId);

            /* TEMP: verify sessionSeqId increments once per boot */
        } else {
            g_radiolink_tx_counter = 0U;
            g_radiolink_sessionSeqId = 0U;
        }
    }


    counter = g_radiolink_tx_counter;

    printf("RL: TX ctr=%lu\r\n", (unsigned long)counter);

    uint8_t frameLen = 0U;
    bool ok = false;

    // === WIRE_VERSION_SELECT_TX (compile-time; no behavior change yet) ===
    #if (RADIOLINK_CRYPTO_ENABLE == 0)
        ok = RadioLink_BuildWireV2Frame(frame,
                                       (uint8_t)sizeof(frame),
                                       node_id,
                                       g_radiolink_sessionSeqId,
                                       counter,
                                       buf,
                                       len,
                                       &frameLen);
#else
// Crypto-enabled build: TX must emit Wire v3 only (no downgrade).
        ok = RadioLink_BuildWireV3Frame_Stub(frame, (uint8_t)sizeof(frame), node_id, g_radiolink_sessionSeqId, counter, buf, len, &frameLen);
        if (!ok) {
        	printf("RL: TX v3 build failed (crypto enabled) - not sending\r\n");
        }
#endif

    // === END WIRE_VERSION_SELECT_TX ===

    if (!ok) {
        status = false;
    } else {
        status = SX1262_SendBytes(sx, frame, frameLen);
    }

#if (RADIOLINK_DEBUG_TX_REPLAY_ONESHOT_ENABLE == 1)
if (status) {
    gRadioLinkDebugTxSendCount++;

    /* Replay every Nth successful send */
    if ((gRadioLinkDebugTxSendCount % 5u) == 0u) {
        printf("RL: TX PERIODIC REPLAY ctr=%lu len=%u\r\n",
               (unsigned long)counter,
               (unsigned)frameLen);

        HAL_Delay(200);

        (void)SX1262_SendBytes(sx, frame, frameLen);
    }
}
#endif

    if (status) {
        if (RadioLink_PersistAllowed()) {
            g_radiolink_tx_counter = counter + 1U;
            RadioLink_TxCounter_Store(g_radiolink_tx_counter);
        }
    }

    return status;
}
