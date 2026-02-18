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

#endif /* RADIO_WIRE_H_ */
