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

#include "sx1262.h"

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



bool RadioLink_SendString(SX1262_Handle *sx, const char *s);
bool RadioLink_TryDecodeToString(const uint8_t *rx, uint8_t rx_len, char *out, uint8_t out_max);
uint8_t RadioLink_WireV0_FrameLenFromPayloadLen(uint8_t payload_len);
bool RadioLink_SendBytes(SX1262_Handle *sx, const uint8_t *buf, uint8_t len);


#endif /* RADIO_LINK_H_ */
