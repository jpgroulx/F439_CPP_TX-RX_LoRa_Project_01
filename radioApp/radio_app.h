/*
 * radio_app.h
 *
 *  Created on: Feb 1, 2026
 *      Author: johng
 */

#ifndef RADIO_APP_H_
#define RADIO_APP_H_



#pragma once
#include "main.h"
#include "sx1262.h"

#pragma pack(push, 1)
typedef struct {
    uint8_t node_id;
    uint8_t counter_le[4];
} radio_hdr_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct {
    radio_hdr_t hdr;
    uint8_t payload_len;
    uint8_t payload[64];
} radio_msg_t;
#pragma pack(pop)



void RadioApp_Init(SX1262_Handle *sx);
void RadioApp_Loop(void);
void RadioApp_OnDio1Exti(uint16_t pin);



#endif /* RADIO_APP_H_ */
