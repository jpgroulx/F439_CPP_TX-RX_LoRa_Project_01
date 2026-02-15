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

void RadioApp_Loop(void)
{
    if (sx1262Role == SX_ROLE_RX) {
        if (g_irq) {
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
#ifndef RF_DEBUG
                /* Binary-test auto-detect:
                 * Wire v0 frame is [len | payload...]
                 * For test frames, payload[0] == 0xA5 (i.e., r.payload[1] == 0xA5).
                 */
                if (r.payload_len >= 2U && r.payload[1] == 0xA5U) {
                    uint8_t n = r.payload[0];
                    uint8_t avail = (uint8_t)(r.payload_len - 1U);

                    if (n > avail) {
                        n = avail;
                    }

                    printf("RX: BIN n=%u data=", (unsigned)n);
                    for (uint8_t i = 0; i < n; i++) {
                        printf("%02X ", (unsigned)r.payload[1U + i]);
                    }
                    printf("RSSI=%d SNR=%d\r\n", r.rssi_pkt, r.snr_pkt);
                } else
#endif
                {
                    char s[256];
                    bool status = false;

                    status = RadioLink_TryDecodeToString(r.payload,
                                                        r.payload_len,
                                                        s,
                                                        (uint8_t)(sizeof(s) - 1U));

#ifndef RF_DEBUG
                    if (status) {
                        printf("RX: %s RSSI=%d SNR=%d\r\n", s, r.rssi_pkt, r.snr_pkt);
                    }
#endif
                }
            }
        }
    } else {
        static uint32_t last_send_ms;
        static uint32_t counter;
        static uint8_t tx_in_flight;
        static uint32_t bin_toggle;

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
            sprintf(msg, "Hello World!!! %lu", (unsigned long)counter++);

#ifdef RF_DEBUG
            printf("[%lu] TX start request\r\n", (unsigned long)t0);
            t1 = HAL_GetTick();
#endif

            /* TEMP(INSTRUMENT): Alternate String / Binary each send.
             * Remove after binary end-to-end proof is complete.
             */
            if ((bin_toggle++ & 1U) == 0U) {
                status = RadioLink_SendString(g_sx, msg);
#ifndef RF_DEBUG
                if (status) {
                    printf("TX: STR [%s]\r\n", msg);
                }
#endif
            } else {
                /* Wire v0 frame [len | payload...], payload begins with 0xA5 marker */
                static const uint8_t raw[] = {
                    0xA5, 0x48, 0x65, 0x00, 0x6C, 0x6C, 0x6F, 0x21, 0xFF, 0x10
                };
                uint8_t frame[1U + sizeof(raw)];
                uint8_t frame_len = (uint8_t)sizeof(frame);

                frame[0] = (uint8_t)sizeof(raw);
                memcpy(&frame[1], raw, sizeof(raw));

                status = RadioLink_SendBytes(g_sx, frame, frame_len);

#ifndef RF_DEBUG
                if (status) {
                    printf("TX: BIN n=%u frame_len=%u\r\n",
                           (unsigned)frame[0],
                           (unsigned)frame_len);
                }
#endif
            }

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
                } else {
                    printf("TX timeout\r\n");
                }
#endif
            }
        }
    }
}


#ifdef COMMENT
void RadioApp_Loop(void) {
    if (sx1262Role == SX_ROLE_RX) {
		if (g_irq) {
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

			    status = RadioLink_TryDecodeToString(r.payload,
			                                        r.payload_len,
			                                        s,
													(uint8_t)(sizeof(s) - 1U));

			#ifndef RF_DEBUG
			    if (status) {
			        printf("RX: %s RSSI=%d SNR=%d\r\n", s, r.rssi_pkt, r.snr_pkt);
			    }
			#endif
			}


		}
    } else {
		static uint32_t last_send_ms;
		static uint32_t counter;
		static uint8_t tx_in_flight;


	//    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);

		uint32_t now = HAL_GetTick();

		if (!tx_in_flight && (now - last_send_ms) >= 1000U) {
			char msg[64];
#ifdef RF_DEBUG
			uint32_t t1;
			uint32_t t2;
			uint32_t t0 = HAL_GetTick();
#endif

			last_send_ms = now;
			sprintf(msg, "Hello World!!! %lu", (unsigned long)counter++);

#ifdef RF_DEBUG
			printf("[%lu] TX start request\r\n", (unsigned long)t0);

			t1 = HAL_GetTick();
#endif
			bool status = false;

			status = RadioLink_SendString(g_sx, msg);

			if (status) {
#ifdef RF_DEBUG
				t2 = HAL_GetTick();

#ifdef RADIOAPP_USE_BINARY_PACKET
				printf("[%lu] SendString with SendBuffer dt=%lums\r\n",
					   (unsigned long)t2,
					   (unsigned long)(t2 - t1));
#else
				printf("[%lu] SendString with Plain Text dt=%lums\r\n",
					   (unsigned long)t2,
					   (unsigned long)(t2 - t1));
#endif
#endif
#ifndef RADIOAPP_USE_BINARY_PACKET
				printf("TX: Send with Plain Text string: [%s]\r\n", msg);
#else
				printf("TX: Send with Buffer string: [%s]\r\n", msg);
#endif

				tx_in_flight = 1;
			} else {
#ifdef RF_DEBUG
				printf("[%lu] TX start failed\r\n", (unsigned long)HAL_GetTick());
#endif
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
				} else {
					printf("TX timeout\r\n");
				}
#endif
			}
		}
    }
}
#endif

