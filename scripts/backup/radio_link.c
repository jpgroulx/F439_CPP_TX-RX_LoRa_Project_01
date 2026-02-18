/*
 * radio_link.c
 *
 *  Created on: Feb 12, 2026
 *      Author: johng
 */




#include "radio_link.h"
#include "stdio.h"
#include "memory.h"

static uint32_t g_radiolink_tx_counter;

static uint32_t g_radiolink_last_seen_counter[256];
static uint8_t g_radiolink_seen[256];


static bool RadioLink_DebuggerAttached(void)
{
	volatile uint32_t dhcsr;
	bool attached;

#if defined(CoreDebug)
	/* DHCSR.C_DEBUGEN bit indicates debugger connected/enabled */
	dhcsr = CoreDebug->DHCSR;
	attached = ((dhcsr & 0x00000001UL) != 0U);
#else
	attached = false;
#endif

	return attached;
}

static bool RadioLink_PersistAllowed(void)
{
	bool allowed;

	allowed = true;

#if (RL_PERSIST_ENABLE == 0)
	allowed = false;
#endif

#if (RL_PERSIST_DISABLE_WHEN_DEBUGGER == 1)
	if (RadioLink_DebuggerAttached()) {
		allowed = false;
	}
#endif

	return allowed;
}

static bool RadioLink_TxCounter_Store(uint32_t counter)
{
    bool ok;
    uint8_t buf[4];

    ok = false;

    buf[0] = (uint8_t)((counter >> 0) & 0xFFU);
    buf[1] = (uint8_t)((counter >> 8) & 0xFFU);
    buf[2] = (uint8_t)((counter >> 16) & 0xFFU);
    buf[3] = (uint8_t)((counter >> 24) & 0xFFU);

    ok = FRAM_WriteBytes(FRAM_BASE_ADDR,
                         buf,
                         (uint16_t)sizeof(buf));
    if (!ok)
    {
        goto done;
    }

    ok = true;

done:
    return ok;
}

static bool RadioLink_TxCounter_Load(uint32_t *out_counter)
{
    bool ok;
    uint8_t buf[4];
    uint32_t v;

    ok = false;
    v = 0U;

    if (out_counter == NULL)
    {
        return false;
    }

    ok = FRAM_ReadBytes(FRAM_BASE_ADDR,
                        buf,
                        (uint16_t)sizeof(buf));
    if (!ok)
    {
        goto done;
    }

    v = ((uint32_t)buf[0] << 0) |
        ((uint32_t)buf[1] << 8) |
        ((uint32_t)buf[2] << 16) |
        ((uint32_t)buf[3] << 24);

    *out_counter = v;
    ok = true;

done:
    return ok;
}

static void RadioLink_EncodeLe32(uint8_t out[4], uint32_t v)
{
	out[0] = (uint8_t)(v & 0xFFU);
	out[1] = (uint8_t)((v >> 8) & 0xFFU);
	out[2] = (uint8_t)((v >> 16) & 0xFFU);
	out[3] = (uint8_t)((v >> 24) & 0xFFU);
}

static uint32_t RadioLink_DecodeLe32(const uint8_t in[4])
{
	uint32_t v = 0;

	v |= (uint32_t)in[0];
	v |= ((uint32_t)in[1] << 8);
	v |= ((uint32_t)in[2] << 16);
	v |= ((uint32_t)in[3] << 24);

	return v;
}

/* Default node_id derivation: stable u8 from STM32 UID words. */
static uint8_t RadioLink_GetNodeId(void)
{
	uint8_t id = 0;
	uint32_t w0;
	uint32_t w1;
	uint32_t w2;
	uint32_t x;

	/* These are provided by STM32 HAL. */
	extern uint32_t HAL_GetUIDw0(void);
	extern uint32_t HAL_GetUIDw1(void);
	extern uint32_t HAL_GetUIDw2(void);

	w0 = HAL_GetUIDw0();
	w1 = HAL_GetUIDw1();
	w2 = HAL_GetUIDw2();

	x = (w0 ^ w1 ^ w2);
	id = (uint8_t)(x ^ (x >> 8) ^ (x >> 16) ^ (x >> 24));

	return id;
}



uint8_t RadioLink_WireV0_FrameLenFromPayloadLen(uint8_t payload_len)
{
    uint8_t frame_len = 0;

    /* 1 + payload_len (wrap-safe because payload_len is u8) */
    frame_len = (uint8_t)(RADIOLINK_WIRE_V0_HDR_LEN + payload_len);

    return frame_len;
}

bool RadioLink_SendString(SX1262_Handle *sx, const char *s)
{
	bool status = false;
	size_t len;
	uint8_t payload_len;

	if (!sx || !s)
	{
		return false;
	}

	len = strlen(s);
	if (len > RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN)
	{
		len = RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN;
	}

	payload_len = (uint8_t)len;
	status = RadioLink_SendBytes(sx, (const uint8_t *)s, payload_len);

	return status;
}

bool RadioLink_TryDecodeToString(const uint8_t *rx, uint8_t rx_len, char *out, uint8_t out_max)
{
	bool status = false;
	const uint8_t *payload = NULL;
	uint8_t payload_len = 0;

	if (!rx || !out || (out_max == 0U))
	{
		return false;
	}

	/* Prefer Wire v1 when structurally valid */
	if (rx_len >= RADIOLINK_WIRE_V1_HDR_LEN && rx[0] == RADIOLINK_WIRE_V1_VERSION)
	{
		uint8_t node_id = rx[1];
		uint32_t counter = RadioLink_DecodeLe32(&rx[2]);
		uint8_t n = rx[6];

		/* Structural validation: exact frame length must be present */
		if ((n <= RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN) &&
		    ((uint8_t)(RADIOLINK_WIRE_V1_HDR_LEN + n) == rx_len))
		{
			/* Replay protection */
			if (g_radiolink_seen[node_id])
			{
				if (counter <= g_radiolink_last_seen_counter[node_id])
				{
					return false;
				}
			}

			g_radiolink_seen[node_id] = 1U;
			g_radiolink_last_seen_counter[node_id] = counter;

			payload = &rx[RADIOLINK_WIRE_V1_HDR_LEN];
			payload_len = n;

		}
	}

	/* Fallback: Wire v0 */
	if (!payload)
	{
		uint8_t n;
		uint8_t avail;

		if (rx_len < RADIOLINK_WIRE_V0_HDR_LEN)
		{
			return false;
		}

		n = rx[0];
		avail = (uint8_t)(rx_len - RADIOLINK_WIRE_V0_HDR_LEN);
		if (n > avail)
		{
			n = avail;
		}

		payload = &rx[RADIOLINK_WIRE_V0_HDR_LEN];
		payload_len = n;
	}

	/* Copy to output buffer and NUL-terminate */
	if (payload_len >= out_max)
	{
		payload_len = (uint8_t)(out_max - 1U);
	}

	memcpy(out, payload, payload_len);
	out[payload_len] = 0;

	status = true;
	return status;
}



bool RadioLink_SendBytes(SX1262_Handle *sx, const uint8_t *buf, uint8_t len)
{
	bool status = false;
	uint8_t frame[RADIOLINK_WIRE_V1_MAX_FRAME_LEN];
	uint8_t node_id;
	uint32_t counter;
	uint8_t frame_len;

	if (!sx || !buf || (len == 0U))
	{
		return false;
	}

	if (len > RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN)
	{
		len = RADIOLINK_WIRE_V1_MAX_PAYLOAD_LEN;
	}

	node_id = RadioLink_GetNodeId();

	if (g_radiolink_tx_counter == 0U)
	{
	    /* Initialize once per boot from persistent store. */
	    if (RadioLink_PersistAllowed())
	    {
	        bool ok;
	        uint32_t loaded;

	        loaded = 0U;
	        ok = RadioLink_TxCounter_Load(&loaded);
	        if (ok)
	        {
	            g_radiolink_tx_counter = loaded;
	        }
	        else
	        {
	            g_radiolink_tx_counter = 0U;
	        }
	    }
	    else
	    {
	        g_radiolink_tx_counter = 0U;
	    }
	}


	counter = g_radiolink_tx_counter;


	printf("RL: TX ctr=%lu\r\n", (unsigned long)counter);

	frame[0] = RADIOLINK_WIRE_V1_VERSION;
	frame[1] = node_id;
	RadioLink_EncodeLe32(&frame[2], counter);
	frame[6] = len;

	memcpy(&frame[RADIOLINK_WIRE_V1_HDR_LEN], buf, len);

	frame_len = (uint8_t)(RADIOLINK_WIRE_V1_HDR_LEN + len);
	status = SX1262_SendBytes(sx, frame, frame_len);

	if (status)
	{
		if (RadioLink_PersistAllowed()) {
			g_radiolink_tx_counter = counter + 1U;
			RadioLink_TxCounter_Store(g_radiolink_tx_counter);
		}
	}


	return status;
}



