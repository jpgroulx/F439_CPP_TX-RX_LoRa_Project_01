/*
 * radio_app.c
 *
 *  Created on: Feb 1, 2026
 *      Author: johng
 */


#include "radio_app.h"
#include "radio_link.h"
#include <stdio.h>
#include <string.h>

static SX1262_Handle *g_sx;
static volatile uint8_t g_irq;
extern SX1262_ROLE sx1262Role;

void RadioApp_Init(SX1262_Handle *sx) {
    g_sx = sx;
    g_irq = 0;

    SX1262_LoRaConfig cfg = {
        .rf_hz = 915000000,
        .tx_dbm = 14,
        .sf = 7,
        .bw = 0,
        .cr = 1,
        .preamble_len = 8,
        .crc_on = true,
        .iq_invert = false
    };

    SX1262_Init(g_sx);
    SX1262_ConfigureLoRa(g_sx, &cfg);

    SX1262_ClearIrq(g_sx, 0xFFFF);


    if (sx1262Role == SX_ROLE_RX) {
    	SX1262_StartRxContinuous(g_sx);
    }
}

void RadioApp_OnDio1Exti(uint16_t pin) {
    if (pin == g_sx->DIO1_Pin)
        g_irq = 1;
}

void RadioApp_Loop(void) {
	if (sx1262Role == SX_ROLE_RX) {
		if (!g_irq) {
			return;
		}

		SX1262_IrqResult r;
		g_irq = 0;

		if (!SX1262_ProcessIrq(g_sx, &r)) {
			return;
		}

#ifdef RF_DEBUG
		printf("IRQ: rx_done=%u tx_done=%u crc_error=%u timeout=%u payload_len=%u first=%02X\r\n",
		       (unsigned)r.rx_done,
		       (unsigned)r.tx_done,
		       (unsigned)r.crc_error,
		       (unsigned)r.timeout,
		       (unsigned)r.payload_len,
		       (unsigned)r.payload[0]);
#endif

		if (r.rx_done && !r.crc_error) {
			char s[256];
			bool status = false;

			{
			    uint8_t ver = 0U;
			    uint8_t hdrBytes = 0U;

			    if (r.payload_len >= 1U) {
			        ver = r.payload[0];
			    }

			    /* Raw header dump (debug): print the whole header for known versions */
			    if (ver == RADIOLINK_WIRE_V2_VERSION) {
			        hdrBytes = (uint8_t)((r.payload_len < RADIOLINK_WIRE_V2_HDR_LEN) ? r.payload_len : RADIOLINK_WIRE_V2_HDR_LEN);
			    } else if (ver == RADIOLINK_WIRE_V1_VERSION) {
			        hdrBytes = (uint8_t)((r.payload_len < RADIOLINK_WIRE_V1_HDR_LEN) ? r.payload_len : RADIOLINK_WIRE_V1_HDR_LEN);
			    } else {
			        /* Unknown version: show up to 8 bytes max */
			        hdrBytes = (uint8_t)((r.payload_len < 8U) ? r.payload_len : 8U);
			    }

			    printf("RXHDR:");
			    for (uint8_t i = 0U; i < hdrBytes; i++) {
			        printf(" %02X", (unsigned)r.payload[i]);
			    }
			    printf(" len=%u\r\n", (unsigned)r.payload_len);
			}


			status = RadioLink_TryDecodeToString(r.payload, r.payload_len, s, (uint8_t)(sizeof(s) - 1U));
#ifndef RF_DEBUG
			if (status) {
				printf("RX: %s RSSI=%d SNR=%d\r\n", s, r.rssi_pkt, r.snr_pkt);
			}
#endif
		}
	} else {	/* TX role */
		static uint32_t last_send_ms;
		static uint32_t counter;
		static uint8_t tx_in_flight;

		uint32_t now = HAL_GetTick();

		if (!tx_in_flight && (now - last_send_ms) >= 1000U) {
			char msg[64];
			bool status = false;

#ifdef RF_DEBUG
			uint32_t t1;
			uint32_t t2;
			uint32_t t0 = HAL_GetTick();
#endif

			last_send_ms = now;
			sprintf(msg, "The quick brown fox jumps over the lazy dog %lu", (unsigned long)counter++);

#ifdef RF_DEBUG
			printf("[%lu] TX start request\r\n", (unsigned long)t0);
			t1 = HAL_GetTick();
#endif

			status = RadioLink_SendString(g_sx, msg);

#ifndef RF_DEBUG
			if (status) {
				printf("TX: STR [%s]\r\n", msg);
			}
#endif

#ifdef RF_DEBUG
			if (status) {
				t2 = HAL_GetTick();
				printf("[%lu] TX started dt=%lums\r\n",
				       (unsigned long)t2,
				       (unsigned long)(t2 - t1));
			} else {
				printf("[%lu] TX start failed\r\n", (unsigned long)HAL_GetTick());
			}
#endif

			if (status) {
				tx_in_flight = 1;
			}
		}

		if (g_irq) {
			SX1262_IrqResult r;
			g_irq = 0;

			if (!SX1262_ProcessIrq(g_sx, &r)) {
				return;
			}

			if (r.tx_done || r.timeout) {
				tx_in_flight = 0;

#ifdef RF_DEBUG
				if (r.tx_done) {
					printf("[%lu] TX done\r\n", (unsigned long)HAL_GetTick());
				}
				else
				{
					printf("TX timeout\r\n");
				}
#endif
			}
		}
	}
}
