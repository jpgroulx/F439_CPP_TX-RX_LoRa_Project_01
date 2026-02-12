/*
 * SX1262.c
 *
 *  Created on: Feb 1, 2026
 *      Author: johng
 */

#include <main.h>
#include "sx1262.h"
#include <stdio.h>
#include <string.h>

/* ===================== SX126x command set ===================== */
#define SX126X_CMD_SET_STANDBY              0x80
#define SX126X_CMD_SET_PACKETTYPE           0x8A
#define SX126X_CMD_SET_RF_FREQUENCY         0x86
#define SX126X_CMD_SET_BUFFER_BASE_ADDRESS  0x8F
#define SX126X_CMD_SET_MODULATION_PARAMS    0x8B
#define SX126X_CMD_SET_PACKET_PARAMS        0x8C
#define SX126X_CMD_SET_DIO_IRQ_PARAMS       0x08
#define SX126X_CMD_SET_TX                   0x83
#define SX126X_CMD_SET_RX                   0x82
#define SX126X_CMD_GET_IRQ_STATUS           0x12
#define SX126X_CMD_CLEAR_IRQ_STATUS         0x02
#define SX126X_CMD_WRITE_BUFFER             0x0E
#define SX126X_CMD_READ_BUFFER              0x1E
#define SX126X_CMD_GET_RX_BUFFER_STATUS     0x13
#define SX126X_CMD_GET_PACKET_STATUS        0x14
#define SX126X_CMD_SET_TX_PARAMS            0x8E
#define SX126X_CMD_SET_RX_TX_FALLBACK_MODE  0x93
#define SX126X_CMD_SET_REGULATOR_MODE       0x96
#define SX126X_CMD_SET_PA_CONFIG 			0x95
//define SX126X_CMD_SET_OCP       			0x97
#define SX126X_CMD_SET_DIO3_AS_TCXO_CTRL    0x97
/* keep SX126X_CMD_SET_OCP undefined for now (or set it to the correct opcode later) */

#define SX126X_CMD_CALIBRATE_IMAGE          0x98

#define SX126X_PACKET_LORA                  0x01
#define SX126X_STANDBY_RC                   0x00

/* IRQ masks */
#define SX126X_IRQ_TX_DONE                  (1u << 0)
#define SX126X_IRQ_RX_DONE                  (1u << 1)
#define SX126X_IRQ_CRC_ERROR                (1u << 6)
#define SX126X_IRQ_RX_TX_TIMEOUT            (1u << 9)

#ifdef RF_DEBUG
static uint8_t g_last_mod[4];
static uint8_t g_last_pkt[6];
#endif

extern SX1262_ROLE sx1262Role;

/* ===================== GPIO helpers ===================== */

static bool wait_busy(SX1262_Handle *sx, uint32_t timeout_ms) {
    uint32_t t0 = HAL_GetTick();

    while (HAL_GPIO_ReadPin(sx->BUSY_Port, sx->BUSY_Pin) == GPIO_PIN_SET) {
        if ((HAL_GetTick() - t0) > timeout_ms) {
        	return false;
        }
    }

    return true;
}

static void cs_low(SX1262_Handle *sx) {
    if (!wait_busy(sx, 10)) {
        Error_Handler();
    }

    HAL_GPIO_WritePin(sx->NSS_Port, sx->NSS_Pin, GPIO_PIN_RESET);
}

static void cs_high(SX1262_Handle *sx) {
    HAL_GPIO_WritePin(sx->NSS_Port, sx->NSS_Pin, GPIO_PIN_SET);
}

static void rf_to_rx(SX1262_Handle *sx) {
    /* Waveshare: RX mode = RXEN low, TXEN high */
    HAL_GPIO_WritePin(sx->RXEN_Port, sx->RXEN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(sx->TXEN_Port, sx->TXEN_Pin, GPIO_PIN_SET);
}

static void rf_to_tx(SX1262_Handle *sx) {
    /* Waveshare: TX mode = RXEN high, TXEN low */
    HAL_GPIO_WritePin(sx->RXEN_Port, sx->RXEN_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(sx->TXEN_Port, sx->TXEN_Pin, GPIO_PIN_RESET);
}

static void rf_off(SX1262_Handle *sx) {
    HAL_GPIO_WritePin(sx->RXEN_Port, sx->RXEN_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(sx->TXEN_Port, sx->TXEN_Pin, GPIO_PIN_RESET);
}


/* ===================== SPI helpers ===================== */

static bool spi_tx(SX1262_Handle *sx, const uint8_t *tx, uint16_t len) {
    return HAL_SPI_Transmit(sx->hspi, (uint8_t *)tx, len, 100) == HAL_OK;
}

static bool spi_xfer(SX1262_Handle *sx, uint8_t tx, uint8_t *rx) {
    return HAL_SPI_TransmitReceive(sx->hspi, &tx, rx, 1, 100) == HAL_OK;
}

/* ===================== SX1262 low-level ===================== */

static bool sx_cmd_write(SX1262_Handle *sx, uint8_t cmd, const uint8_t *args, uint8_t n) {

	uint32_t after_ms = 50U;

	if (cmd == SX126X_CMD_CALIBRATE_IMAGE) {  /* 0x98 */
	    after_ms = 2000U;
	}


	if (!wait_busy(sx, after_ms)) {
#ifdef RF_DEBUG
        printf("BUSY timeout BEFORE cmd 0x%02X (BUSY=%d DIO1=%d)\r\n",
               cmd,
               (int)HAL_GPIO_ReadPin(sx->BUSY_Port, sx->BUSY_Pin),
               (int)HAL_GPIO_ReadPin(sx->DIO1_Port, sx->DIO1_Pin));
#endif
        if (cmd == SX126X_CMD_CALIBRATE_IMAGE) { /* 0x98 */
            return true; /* non-fatal */
        }

        return false;
	}

#ifdef RF_DEBUG
	if (cmd == SX126X_CMD_SET_MODULATION_PARAMS && args && n == 4) {
	    memcpy(g_last_mod, args, 4);
	}
	if (cmd == SX126X_CMD_SET_PACKET_PARAMS && args && n == 6) {
	    memcpy(g_last_pkt, args, 6);
	}
#endif

    cs_low(sx);

    if (!spi_tx(sx, &cmd, 1)) {
        cs_high(sx);
        return false;
    }

    if (n && args) {
        if (!spi_tx(sx, args, n)) {
            cs_high(sx);
            return false;
        }
    }

    cs_high(sx);

    if (cmd == SX126X_CMD_SET_TX || cmd == SX126X_CMD_SET_RX) {
        return true;
    }

    return true;
}

static uint32_t rf_freq_to_pll(uint32_t hz) {
    return (uint32_t)(((uint64_t)hz << 25) / 32000000ULL);
}

/* ===================== Driver API ===================== */

void SX1262_Init(SX1262_Handle *sx) {
    uint8_t v;

    rf_off(sx);

    HAL_GPIO_WritePin(sx->RESET_Port, sx->RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(sx->RESET_Port, sx->RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(10);

    if (!wait_busy(sx, 10)) {
        Error_Handler();
    }

    v = 0x01;
    sx_cmd_write(sx, SX126X_CMD_SET_REGULATOR_MODE, &v, 1);

    v = SX126X_STANDBY_RC;
    sx_cmd_write(sx, SX126X_CMD_SET_STANDBY, &v, 1);

    v = 0x20;
    sx_cmd_write(sx, SX126X_CMD_SET_RX_TX_FALLBACK_MODE, &v, 1);
}

bool SX1262_ConfigureLoRa(SX1262_Handle *sx, const SX1262_LoRaConfig *cfg) {
    uint8_t v;
    uint32_t pll;
    uint8_t buf[8];

    /* Enable TCXO via DIO3 (Waveshare Core1262 powers TCXO from DIO3) */
    {
        uint8_t b[4];

        b[0] = 0x07;  	  /* 3.3V */
        b[1] = 0x00;      /* delay[23:16] */
        b[2] = 0x02;      /* delay[15:8]  -> 0x000280 = 640 ticks */
        b[3] = 0x80;      /* delay[7:0]   -> 640 * 15.625us ≈ 10ms */

        if (!sx_cmd_write(sx, SX126X_CMD_SET_DIO3_AS_TCXO_CTRL, b, 4)) {
            return false;
        }

        /* Give TCXO time to start (matches the programmed delay; extra margin is fine) */
        HAL_Delay(15);
    }

    /* Force regulator mode: 0x01 = DC-DC, 0x00 = LDO */
    {
        uint8_t m = 0x01; /* DC-DC */
        if (!sx_cmd_write(sx, SX126X_CMD_SET_REGULATOR_MODE, &m, 1)) {
            return false;
        }
    }


    v = SX126X_PACKET_LORA;
    if (!sx_cmd_write(sx, SX126X_CMD_SET_PACKETTYPE, &v, 1))
        return false;

    pll = rf_freq_to_pll(cfg->rf_hz);
    buf[0] = pll >> 24;
    buf[1] = pll >> 16;
    buf[2] = pll >> 8;
    buf[3] = pll;
    sx_cmd_write(sx, SX126X_CMD_SET_RF_FREQUENCY, buf, 4);

    /* CalibrateImage for 902-928 MHz band */
    {
        uint8_t cal[2] = { 0xE1, 0xE9 };
        sx_cmd_write(sx, SX126X_CMD_CALIBRATE_IMAGE, cal, 2);
    }

    /* Calibration can legitimately take longer */
    if (!wait_busy(sx, 2000)) {
#ifdef RF_DEBUG
        printf("BUSY timeout AFTER cmd 0x98 (BUSY=%d DIO1=%d)\r\n",
               (int)HAL_GPIO_ReadPin(sx->BUSY_Port, sx->BUSY_Pin),
               (int)HAL_GPIO_ReadPin(sx->DIO1_Port, sx->DIO1_Pin));
#endif
    }

    buf[0] = 0x00;
    buf[1] = 0x00;
    sx_cmd_write(sx, SX126X_CMD_SET_BUFFER_BASE_ADDRESS, buf, 2);

    buf[0] = cfg->sf;
    buf[1] = (cfg->bw == 0) ? 0x04 : (cfg->bw == 1) ? 0x05 : 0x06;
    buf[2] = cfg->cr;
    buf[3] = (cfg->sf >= 11) ? 0x01 : 0x00;
    sx_cmd_write(sx, SX126X_CMD_SET_MODULATION_PARAMS, buf, 4);

    buf[0] = cfg->preamble_len >> 8;
    buf[1] = cfg->preamble_len;
    buf[2] = 0x00;
    buf[3] = 0xFF;
    buf[4] = cfg->crc_on ? 1 : 0;
    buf[5] = cfg->iq_invert ? 1 : 0;
    sx_cmd_write(sx, SX126X_CMD_SET_PACKET_PARAMS, buf, 6);

    /* Required on many SX1262 modules: PA/OCP configuration */
    {
        uint8_t pa[4] = { 0x04, 0x07, 0x00, 0x01 }; /* paDuty, hpMax, deviceSel(SX1262), paLut */

        sx_cmd_write(sx, SX126X_CMD_SET_PA_CONFIG, pa, 4);
    }

#ifdef DELETE_NOT_NEEDED_CODE
    /* Set TX power and ramp time */
    {
        uint8_t p[2];

        p[0] = 0x16; /* +22 dBm for SX1262 (example, adjust if needed) */
        p[1] = 0x04; /* ramp time = 200 us */

        if (!sx_cmd_write(sx, SX126X_CMD_SET_TX_PARAMS, p, 2)) {
            return false;
        }
    }
#endif

    buf[0] = cfg->tx_dbm;
    buf[1] = 0x09;
    sx_cmd_write(sx, SX126X_CMD_SET_TX_PARAMS, buf, 2);


    uint16_t mask;

    if (sx1262Role == SX_ROLE_RX) {
    	mask = SX126X_IRQ_RX_DONE | SX126X_IRQ_CRC_ERROR | SX126X_IRQ_RX_TX_TIMEOUT;
    } else {
    	mask = SX126X_IRQ_TX_DONE | SX126X_IRQ_RX_TX_TIMEOUT;
    }


    buf[0] = mask >> 8; buf[1] = mask;
    buf[2] = mask >> 8; buf[3] = mask;
    buf[4] = buf[5] = buf[6] = buf[7] = 0;
    sx_cmd_write(sx, SX126X_CMD_SET_DIO_IRQ_PARAMS, buf, 8);

#ifdef RF_DEBUG
    printf("DIO IRQ mask=0x%04X\r\n", (unsigned)mask);
#endif

    buf[0] = 0xFF; buf[1] = 0xFF;
    sx_cmd_write(sx, SX126X_CMD_CLEAR_IRQ_STATUS, buf, 2);

    return true;
}

bool SX1262_StartRxContinuous(SX1262_Handle *sx) {
    uint8_t t[3] = { 0xFF, 0xFF, 0xFF };
    rf_to_rx(sx);
    return sx_cmd_write(sx, SX126X_CMD_SET_RX, t, 3);
}

bool SX1262_SendString(SX1262_Handle *sx, const char *s) {
	 bool ok = false;

#ifdef RADIOAPP_USE_BINARY_PACKET
	ok = SX1262_SendBuffer(sx, s);
#else
	ok = SX1262_SendStringPlainText(sx, s);
#endif

	return ok;
}


bool SX1262_SendStringPlainText(SX1262_Handle *sx, const char *s) {
    uint8_t msg[256];
    size_t n;
    uint8_t total;
    uint8_t pktp[6];
    uint8_t t[3];

    n = strlen(s);
    if (n > 254U) {
        n = 254U;
    }

    msg[0] = (uint8_t)n;
    memcpy(&msg[1], s, n);
    total = (uint8_t)(n + 1U);

    /* Set packet params for this payload length */
    pktp[0] = 0x00;
    pktp[1] = 0x08;          /* preamble 8 */
    pktp[2] = 0x00;          /* explicit header */
    pktp[3] = total;         /* payload length */
    pktp[4] = 0x01;          /* CRC on */
    pktp[5] = 0x00;          /* IQ normal */
    if (!sx_cmd_write(sx, SX126X_CMD_SET_PACKET_PARAMS, pktp, 6)) {
        return false;
    }

    /* Clear TX_DONE + TIMEOUT before starting */
    {
        uint16_t m = (uint16_t)(SX126X_IRQ_TX_DONE | SX126X_IRQ_RX_TX_TIMEOUT);
        uint8_t clr[2] = { (uint8_t)(m >> 8), (uint8_t)m };
        sx_cmd_write(sx, SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);
    }

    /* WRITE_BUFFER (0x0E), offset 0 */
    {
        uint8_t hdr[2] = { SX126X_CMD_WRITE_BUFFER, 0x00 };

        if (!wait_busy(sx, 10)) {
            Error_Handler();
        }

        cs_low(sx);

        if (!spi_tx(sx, hdr, 2)) {
            cs_high(sx);
            return false;
        }
        if (!spi_tx(sx, msg, total)) {
            cs_high(sx);
            return false;
        }

        cs_high(sx);

        if (!wait_busy(sx, 100)) {
        	Error_Handler();
        }
    }

#ifdef RF_DEBUG
    printf("Last 8B: %02X %02X %02X %02X | Last 8C: %02X %02X %02X %02X %02X %02X\r\n",
           g_last_mod[0], g_last_mod[1], g_last_mod[2], g_last_mod[3],
           g_last_pkt[0], g_last_pkt[1], g_last_pkt[2], g_last_pkt[3], g_last_pkt[4], g_last_pkt[5]);
#endif

    /* Start TX with ~2s radio timeout (doesn't block CPU) */
    t[0] = 0x01;
    t[1] = 0xF4;
    t[2] = 0x00;

    bool ok;

    rf_to_tx(sx);

    /* tiny settle (debug only) */
    for (volatile int i = 0; i < 2000; i++) {
    }

    ok = sx_cmd_write(sx, SX126X_CMD_SET_TX, t, 3);

#ifdef RF_DEBUG
    uint8_t status = SX1262_GetStatusRaw(sx);

    printf("after SET_TX status=0x%02X BUSY=%d\r\n",
           status,
           (int)HAL_GPIO_ReadPin(sx->BUSY_Port, sx->BUSY_Pin));
#endif
    return ok;
}

bool SX1262_SendBuffer(SX1262_Handle *sx, const char *s) {
    uint8_t pktp[6];
    uint8_t t[3];
	uint8_t len = (uint8_t)strlen(s);
	uint8_t buf[1 + 64];

    if (len == 0U) {
        return false;
    }

	if (len > 64U) {
		len = 64U;
	}

	uint8_t total = (uint8_t)(1U + len);

	buf[0] = len;
	memcpy(&buf[1], s, len);
    /* Set packet params for this payload length */
    pktp[0] = 0x00;
    pktp[1] = 0x08;          /* preamble 8 */
    pktp[2] = 0x00;          /* explicit header */
    pktp[3] = total;           /* payload length */
    pktp[4] = 0x01;          /* CRC on */
    pktp[5] = 0x00;          /* IQ normal */
    if (!sx_cmd_write(sx, SX126X_CMD_SET_PACKET_PARAMS, pktp, 6)) {
        return false;
    }

    /* Clear TX_DONE + TIMEOUT before starting */
    {
        uint16_t m = (uint16_t)(SX126X_IRQ_TX_DONE | SX126X_IRQ_RX_TX_TIMEOUT);
        uint8_t clr[2] = { (uint8_t)(m >> 8), (uint8_t)m };
        sx_cmd_write(sx, SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);
    }

    /* WRITE_BUFFER (0x0E), offset 0 */
    {
        uint8_t hdr[2] = { SX126X_CMD_WRITE_BUFFER, 0x00 };

        if (!wait_busy(sx, 10)) {
            Error_Handler();
        }

        cs_low(sx);

        if (!spi_tx(sx, hdr, 2)) {
            cs_high(sx);
            return false;
        }
        if (!spi_tx(sx, (uint8_t *)buf, len)) {
            cs_high(sx);
            return false;
        }

        cs_high(sx);

        if (!wait_busy(sx, 100)) {
            Error_Handler();
        }
    }

    /* Start TX with ~2s radio timeout (doesn't block CPU) */
    t[0] = 0x01;
    t[1] = 0xF4;
    t[2] = 0x00;

    bool ok;

    rf_to_tx(sx);

    /* tiny settle (debug only) */
    for (volatile int i = 0; i < 2000; i++) {
    }

    ok = sx_cmd_write(sx, SX126X_CMD_SET_TX, t, 3);
    return ok;
}

bool SX1262_ProcessIrq(SX1262_Handle *sx, SX1262_IrqResult *out) {
    uint8_t hi, lo, dummy = 0;
    memset(out, 0, sizeof(*out));

    cs_low(sx);
    spi_xfer(sx, SX126X_CMD_GET_IRQ_STATUS, &dummy);
    spi_xfer(sx, 0, &dummy);                         /* status */
    spi_xfer(sx, 0, &hi);
    spi_xfer(sx, 0, &lo);
    cs_high(sx);

    uint16_t irq = (hi << 8) | lo;

#ifdef RF_DEBUG
    printf("IRQ raw=0x%04X\r\n", (unsigned)irq);
#endif

    uint8_t clr[2] = { hi, lo };
    sx_cmd_write(sx, SX126X_CMD_CLEAR_IRQ_STATUS, clr, 2);

    out->rx_done = irq & SX126X_IRQ_RX_DONE;
    out->tx_done = irq & SX126X_IRQ_TX_DONE;
    out->crc_error = irq & SX126X_IRQ_CRC_ERROR;
    out->timeout = irq & SX126X_IRQ_RX_TX_TIMEOUT;

    if (out->crc_error || out->timeout) {
        out->payload_len = 0;
        out->payload[0] = 0;
    }

   	if (out->rx_done && !out->crc_error) {
        uint8_t pl, ptr;

        cs_low(sx);
        spi_xfer(sx, SX126X_CMD_GET_RX_BUFFER_STATUS, &dummy);
        spi_xfer(sx, 0, &dummy);                         /* status */
        spi_xfer(sx, 0, &pl);
        spi_xfer(sx, 0, &ptr);
        cs_high(sx);

        out->payload_len = pl;

        cs_low(sx);
        spi_xfer(sx, SX126X_CMD_READ_BUFFER, &dummy);
        spi_xfer(sx, ptr, &dummy);
        spi_xfer(sx, 0, &dummy);
        HAL_SPI_Receive(sx->hspi, out->payload, pl, 100);
        cs_high(sx);

        {
            uint8_t p0, p1, p2;

            cs_low(sx);
            spi_xfer(sx, SX126X_CMD_GET_PACKET_STATUS, &dummy);
            spi_xfer(sx, 0, &dummy);
            spi_xfer(sx, 0, &p0);
            spi_xfer(sx, 0, &p1);
            spi_xfer(sx, 0, &p2);
            cs_high(sx);

            out->rssi_pkt = (int8_t)(-((int16_t)p2) / 2); /* LoRa SignalRssiPkt: -raw/2 dBm */
            out->snr_pkt = (int8_t)p1 / 4;
        }

    }

    if (out->rx_done || out->crc_error || out->timeout) {
        SX1262_StartRxContinuous(sx);
    }

    return true;
}

uint8_t SX1262_GetStatusRaw(SX1262_Handle *sx) {
    uint8_t cmd = 0xC0;    // GetStatus
    uint8_t status = 0;
    uint8_t dummy = 0x00;

    if (!wait_busy(sx, 100)) {
    	Error_Handler();
    }
    cs_low(sx);

    HAL_SPI_TransmitReceive(sx->hspi, &cmd, &status, 1, 100);
    HAL_SPI_TransmitReceive(sx->hspi, &dummy, &status, 1, 100);

    cs_high(sx);
    return status;
}

uint16_t SX1262_GetIrqStatusRaw(SX1262_Handle *sx) {
	uint8_t tx[4];
	uint8_t rx[4];
	uint16_t irq;

	tx[0] = SX126X_CMD_GET_IRQ_STATUS; /* 0x12 */
	tx[1] = 0x00; /* dummy */
	tx[2] = 0x00;
	tx[3] = 0x00;

	if (!wait_busy(sx, 10)) {
		return 0;
	}

	cs_low(sx);
	HAL_SPI_TransmitReceive(sx->hspi, tx, rx, 4, 100);
	cs_high(sx);

	/* SX126x: rx[0]=garbage, rx[1]=status, rx[2]=IRQ MSB, rx[3]=IRQ LSB */
	irq = (uint16_t)(((uint16_t)rx[2] << 8) | rx[3]);

#ifdef RF_DEBUG
	/* Print occasionally to avoid flooding */
	{
		static uint32_t last_ms;
		uint32_t now = HAL_GetTick();

		if ((now - last_ms) >= 250U) {
			last_ms = now;
			printf("GET_IRQ_STATUS(0x12) rx: %02X %02X %02X %02X -> irq=0x%04X\r\n",
			       rx[0], rx[1], rx[2], rx[3], irq);
		}
	}
#endif

	return irq;
}

void SX1262_ClearIrq(SX1262_Handle *sx, uint16_t mask) {
    uint8_t b[2];

    b[0] = (uint8_t)(mask >> 8);
    b[1] = (uint8_t)(mask);
    sx_cmd_write(sx, SX126X_CMD_CLEAR_IRQ_STATUS, b, 2);

	if (!wait_busy(sx, 10)) {
		Error_Handler();
	}

}

bool SX1262_SetStandbyRc(SX1262_Handle *sx) {
    uint8_t b = 0x00; /* STDBY_RC */
    return sx_cmd_write(sx, SX126X_CMD_SET_STANDBY, &b, 1);
}


