/*
 * ada1897_mb85rs64b.h
 *
 *  Created on: Nov 13, 2024
 *      Author: jpgroulx
 */

#ifndef ADA1897_MB85RS64B_H_
#define ADA1897_MB85RS64B_H_

#include "main.h"

#define FRAM_CS_ENABLE HAL_GPIO_WritePin(SPI1_FRAM_CS_GPIO_Port, SPI1_FRAM_CS_Pin, 0);
#define FRAM_CS_DISABLE HAL_GPIO_WritePin(SPI1_FRAM_CS_GPIO_Port, SPI1_FRAM_CS_Pin, 1);

void FRAM_init(SPI_HandleTypeDef *hspix);
void FRAM_write(uint16_t address, uint8_t byte);
void FRAM_read(uint16_t address, uint8_t *byte);
void FRAM_WriteBytes(uint16_t address, uint8_t *pData, uint16_t size);
void FRAM_ReadBytes(uint16_t address, uint8_t *pData, uint16_t size);

#endif /* ADA1897_MB85RS64B_H_ */
