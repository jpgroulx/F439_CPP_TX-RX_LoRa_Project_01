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

void RadioApp_Init(SX1262_Handle *sx);
void RadioApp_Loop(void);
void RadioApp_OnDio1Exti(uint16_t pin);



#endif /* RADIO_APP_H_ */
