/*
 * radio_app.c
 *
 *  Created on: Feb 1, 2026
 *      Author: johng
 */


#include "radio_app.h"
#include <stdio.h>
#include <string.h>

static SX1262_Handle *g_sx;
static volatile uint8_t g_irq;

#if defined(SX_ROLE_RX)
static void Radio_IrqService(void) {
    SX1262_IrqResult r;

    if (!SX1262_ProcessIrq(g_sx, &r))
        return;

    if (r.rx_done && !r.crc_error) {
        uint8_t n = r.payload[0];
        char s[256];

        memcpy(s, &r.payload[1], n);
        s[n] = 0;
        printf("RX: %s  RSSI=%u  SNR=%d\r\n", s, r.rssi_pkt, r.snr_pkt);
    }
}
#endif


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


#if defined(SX_ROLE_RX)
    SX1262_StartRxContinuous(g_sx);
#endif
}

void RadioApp_OnDio1Exti(uint16_t pin) {
#if defined(SX_ROLE_RX)
    if (pin == g_sx->DIO1_Pin)
        g_irq = 1;
#endif
}

void RadioApp_Loop(void) {
#if defined(SX_ROLE_RX)

    if (g_irq) {
        SX1262_IrqResult r;

        g_irq = 0;

        if (!SX1262_ProcessIrq(g_sx, &r)) {
            return;
        }

        printf("IRQ: rx_done=%u tx_done=%u crc_error=%u timeout=%u payload_len=%u first=%02X\r\n",
               (unsigned)r.rx_done,
               (unsigned)r.tx_done,
               (unsigned)r.crc_error,
               (unsigned)r.timeout,
               (unsigned)r.payload_len,
               (unsigned)r.payload[0]);

        if (r.rx_done && !r.crc_error) {
            uint8_t n = r.payload[0];
            char s[256];

            if (n > (uint8_t)(r.payload_len - 1)) {
                n = (uint8_t)(r.payload_len - 1);
            }

            memcpy(s, &r.payload[1], n);
            s[n] = 0;

            printf("RX: %s RSSI=%u SNR=%d\r\n", s, r.rssi_pkt, r.snr_pkt);
        }
    }

#else
    static uint32_t last_send_ms;
    static uint32_t counter;
    static uint8_t tx_in_flight;
    uint32_t t1;
    uint32_t t2;

//    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_0);

    uint32_t now = HAL_GetTick();

    if (!tx_in_flight && (now - last_send_ms) >= 1000U) {
        char msg[64];
        uint32_t t0 = HAL_GetTick();

        last_send_ms = now;
        sprintf(msg, "Hello %lu", (unsigned long)counter++);

        printf("[%lu] TX start request\r\n", (unsigned long)t0);

        t1 = HAL_GetTick();

        if (SX1262_SendString(g_sx, msg)) {
        	t2 = HAL_GetTick();
            printf("[%lu] SendString dt=%lums\r\n",
                   (unsigned long)t2,
                   (unsigned long)(t2 - t1));
            tx_in_flight = 1;
        } else {
            printf("[%lu] TX start failed\r\n", (unsigned long)HAL_GetTick());
        }
    }

    if (tx_in_flight) {
    	SX1262_TxPollResult tr = SX1262_TxPoll(g_sx);

    	if (tr == SX1262_TXPOLL_DONE) {
    		tx_in_flight = 0;
    		printf("[%lu] TX done\r\n", (unsigned long)HAL_GetTick());
    	} else if (tr == SX1262_TXPOLL_TIMEOUT) {
    		tx_in_flight = 0;
    		printf("TX timeout\r\n");
    	}
    }


#endif
}


