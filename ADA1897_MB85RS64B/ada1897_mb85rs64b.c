/*
 * ada1897_mb85rs64b.c
 *
 *  Created on: Nov 13, 2024
 *      Author: jpgroulx
 */

#include "main.h"
#include "ada1897_mb85rs64b.h"

const uint8_t FRAM_WRSR		= 0b00000001;	// 0x01
const uint8_t FRAM_WRITE	= 0b00000010;	// 0x02
const uint8_t FRAM_READ		= 0b00000011;	// 0x03
const uint8_t FRAM_WRDI		= 0b00000100;	// 0x04
const uint8_t FRAM_RDSR		= 0b00000101;	// 0x05
const uint8_t FRAM_WREN		= 0b00000110;	// 0x06

SPI_HandleTypeDef *hspi;

void FRAM_init(SPI_HandleTypeDef *hspix)
{
	FRAM_CS_DISABLE;

	uint8_t spiCMD;
	HAL_StatusTypeDef halStatus = HAL_OK;

	hspi = hspix;

    spiCMD = FRAM_WRDI;	// 0x04
    FRAM_CS_ENABLE;
    halStatus = HAL_SPI_Transmit(hspi, &spiCMD, sizeof(spiCMD), HAL_MAX_DELAY);
    FRAM_CS_DISABLE;

    if (halStatus != HAL_OK) {
    	Error_Handler();
    }

    spiCMD = FRAM_WREN;	// 0x06
    FRAM_CS_ENABLE;
    halStatus = HAL_SPI_Transmit(hspi, &spiCMD, sizeof(spiCMD), HAL_MAX_DELAY);
    FRAM_CS_DISABLE;

    if (halStatus != HAL_OK) {
    	Error_Handler();
    }

    spiCMD = FRAM_WRSR;	// 0x01
    FRAM_CS_ENABLE;
    halStatus = HAL_SPI_Transmit(hspi, &spiCMD, sizeof(spiCMD), HAL_MAX_DELAY);
    FRAM_CS_DISABLE;

    if (halStatus != HAL_OK) {
    	Error_Handler();
    }

    spiCMD = FRAM_RDSR;	// 0x05
    FRAM_CS_ENABLE;
    halStatus = HAL_SPI_Transmit(hspi, &spiCMD, sizeof(spiCMD), HAL_MAX_DELAY);
    FRAM_CS_DISABLE;

    if (halStatus != HAL_OK) {
    	Error_Handler();
    }
}

bool FRAM_write(uint16_t address, uint8_t byte)
{
	bool status = false;

	status = FRAM_WriteBytes(address, &byte, sizeof(byte));

	return status;
}

bool FRAM_read(uint16_t address, uint8_t *byte)
{
	uint8_t data;
	bool status = false;

	status = FRAM_ReadBytes(address, &data, sizeof(data));

	if (status) {
		*byte = data;
	}

	return status;
}

bool FRAM_WriteBytes(uint16_t address, uint8_t *pData, uint16_t size)
{
	uint8_t spiCMD;
	uint8_t spiAddrByte;
	HAL_StatusTypeDef halStatus = HAL_OK;

	spiCMD = FRAM_WREN;

    FRAM_CS_ENABLE;
    halStatus = HAL_SPI_Transmit(hspi, &spiCMD, sizeof(spiCMD), HAL_MAX_DELAY);
    FRAM_CS_DISABLE;

    if (halStatus != HAL_OK) {
    	return false;
    }

    spiCMD = FRAM_WRITE;

    //enable Chip Select
    FRAM_CS_ENABLE;
    //send WRITE command
    halStatus = HAL_SPI_Transmit(hspi, &spiCMD, sizeof(spiCMD), HAL_MAX_DELAY);

    if (halStatus != HAL_OK) {
    	FRAM_CS_DISABLE
    	return false;
    }

    //send upper 8 bits of address
    spiAddrByte = ((address & 0x3f00) >> 8);
    halStatus = HAL_SPI_Transmit(hspi, &spiAddrByte, sizeof(spiAddrByte), HAL_MAX_DELAY);

    if (halStatus != HAL_OK) {
    	FRAM_CS_DISABLE
    	return false;
    }

    //send lower 8 bits of address
    spiAddrByte = (address & 0x00ff);
    halStatus = HAL_SPI_Transmit(hspi, &spiAddrByte, sizeof(spiAddrByte), HAL_MAX_DELAY);

    if (halStatus != HAL_OK) {
    	FRAM_CS_DISABLE
    	return false;
    }

    //sent data byte
    halStatus = HAL_SPI_Transmit(hspi, pData, size, HAL_MAX_DELAY);

    if (halStatus != HAL_OK) {
    	FRAM_CS_DISABLE
    	return false;
    }

    //disable Chip Select
    FRAM_CS_DISABLE;

    return true;
}

bool FRAM_ReadBytes(uint16_t address, uint8_t *pData, uint16_t size)
{
	uint8_t spiCMD;
	uint8_t spiAddrByte;
	HAL_StatusTypeDef halStatus = HAL_OK;

	spiCMD = FRAM_READ;

    //enable Chip Select
    FRAM_CS_ENABLE;

    //send READ command
    halStatus = HAL_SPI_Transmit(hspi, &spiCMD, sizeof(spiCMD), HAL_MAX_DELAY);

    if (halStatus != HAL_OK) {
        FRAM_CS_DISABLE;
    	return false;
    }

    //send upper 8 bits of address
    spiAddrByte = ((address & 0x3f00) >> 8);
    halStatus = HAL_SPI_Transmit(hspi, &spiAddrByte, sizeof(spiAddrByte), HAL_MAX_DELAY);

    if (halStatus != HAL_OK) {
        FRAM_CS_DISABLE;
    	return false;
    }

    //send lower 8 bits of address
    spiAddrByte = (address & 0x00ff);
    halStatus = HAL_SPI_Transmit(hspi, &spiAddrByte, sizeof(spiAddrByte), HAL_MAX_DELAY);

    if (halStatus != HAL_OK) {
    	FRAM_CS_DISABLE;
    	return false;
    }

    //receive data byte
    halStatus = HAL_SPI_Receive(hspi, pData, size, HAL_MAX_DELAY);

    if (halStatus != HAL_OK) {
    	FRAM_CS_DISABLE;
    	return false;
    }

    //disable Chip Select
    FRAM_CS_DISABLE;

    return true;
}
