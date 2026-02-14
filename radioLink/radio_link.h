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

bool RadioLink_SendString(SX1262_Handle *sx, const char *s);

bool RadioLink_TryDecodeToString(const uint8_t *rx, uint8_t rx_len, char *out, uint8_t out_max);


#endif /* RADIO_LINK_H_ */
