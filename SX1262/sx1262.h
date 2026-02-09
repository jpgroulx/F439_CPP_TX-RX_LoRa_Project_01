/*
 * SX1262.h
 *
 *  Created on: Feb 1, 2026
 *      Author: johng
 */

#ifndef SX1262_H_
#define SX1262_H_

#pragma once

#include "main.h"
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    SPI_HandleTypeDef *hspi;

    GPIO_TypeDef *NSS_Port;
    uint16_t NSS_Pin;

    GPIO_TypeDef *RESET_Port;
    uint16_t RESET_Pin;

    GPIO_TypeDef *BUSY_Port;
    uint16_t BUSY_Pin;

    GPIO_TypeDef *DIO1_Port;
    uint16_t DIO1_Pin;

    GPIO_TypeDef *RXEN_Port;
    uint16_t RXEN_Pin;

    GPIO_TypeDef *TXEN_Port;
    uint16_t TXEN_Pin;
} SX1262_Handle;

typedef struct {
    uint32_t rf_hz;
    int8_t tx_dbm;

    uint8_t sf;          // 7..12
    uint8_t bw;          // 0=125k,1=250k,2=500k
    uint8_t cr;          // 1..4 => 4/5..4/8

    uint16_t preamble_len;
    bool crc_on;
    bool iq_invert;
} SX1262_LoRaConfig;

typedef struct {
    bool rx_done;
    bool tx_done;
    bool crc_error;
    bool timeout;

    int8_t rssi_pkt;
    int8_t snr_pkt;

    uint8_t payload[256];
    uint8_t payload_len;
} SX1262_IrqResult;

typedef enum {
    SX1262_TXPOLL_NONE = 0,
    SX1262_TXPOLL_DONE,
    SX1262_TXPOLL_TIMEOUT
} SX1262_TxPollResult;


void SX1262_Init(SX1262_Handle *sx);
bool SX1262_ConfigureLoRa(SX1262_Handle *sx, const SX1262_LoRaConfig *cfg);

bool SX1262_StartRxContinuous(SX1262_Handle *sx);
bool SX1262_SendString(SX1262_Handle *sx, const char *s);
uint8_t SX1262_GetStatusRaw(SX1262_Handle *sx);
uint16_t SX1262_GetIrqStatusRaw(SX1262_Handle *sx);
uint16_t SX1262_ClearAndReadIrq(SX1262_Handle *sx);
bool SX1262_TxDonePoll(SX1262_Handle *sx);
void SX1262_ClearIrq(SX1262_Handle *sx, uint16_t mask);
bool SX1262_SetStandbyRc(SX1262_Handle *sx);

/*
 * Call SX1262_ProcessIrq() when DIO1 interrupt fires.
 * It reads/clears IRQ status, reads RX payload (if any), and re-arms continuous RX.
 */
bool SX1262_ProcessIrq(SX1262_Handle *sx, SX1262_IrqResult *out);



#endif /* SX1262_H_ */
