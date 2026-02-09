# STM32F439 + SX1262 LoRa TX/RX Project

This project implements a **robust, IRQ-driven LoRa TX/RX system** using an **STM32F439 (Nucleo)** and an **SX1262 (Waveshare Core1262)** module.

The design prioritizes:
- deterministic radio behavior
- hardware-signaled events (DIO1 IRQ)
- minimal polling
- clear separation between **radio driver** and **application logic**

---

## Hardware

- **MCU:** STM32F439 (Nucleo-F439)
- **Radio:** SX1262 (Waveshare Core1262)
- **TCXO:** Enabled via DIO3
- **RF Switch:** Controlled via TXEN / RXEN GPIOs
- **IRQ:** DIO1 only (EXTI)

---

## Key Features

- Same firmware image for TX and RX nodes
- Runtime role selection using STM32 **Unique Device ID (UID)**
- IRQ-driven TX and RX (no blocking, no polling)
- Automatic RX restart on:
  - RX_DONE
  - CRC_ERROR
  - RX_TX_TIMEOUT
- CRC error detection and handling
- Logic-analyzer-validated SPI + IRQ timing
- Debug output gated via `RF_DEBUG`

---

## Architecture Overview

Application (radio_app.c)
        |
        v
SX1262 Driver (sx1262.c/.h)
        |
        v
SPI + GPIO

### Design Principles

- Driver is role-agnostic
- Application layer owns behavior
- IRQ over polling
- Minimal diffs, one change at a time

---

## Runtime Role Selection

Role is determined **at boot** using the STM32’s factory-programmed **96-bit Unique Device ID**:

```
uid[0] = HAL_GetUIDw0();
uid[1] = HAL_GetUIDw1();
uid[2] = HAL_GetUIDw2();
```

The UID is folded into a 12-byte buffer and hashed via `Hash32Len5to12()` to produce a deterministic 32-bit value.

This value selects the runtime role:
- `SX_ROLE_RX`
- `SX_ROLE_TX`

---

## Radio Configuration (LoRa)

- Frequency: 915 MHz
- Spreading Factor: SF7
- Bandwidth: 125 kHz
- Coding Rate: 4/5
- Preamble: 8 symbols
- CRC: Enabled
- IQ Inversion: Disabled
- TX Power: +14 dBm

---

## IRQ Configuration

**RX Role**
- RX_DONE
- CRC_ERROR
- RX_TX_TIMEOUT

**TX Role**
- TX_DONE
- RX_TX_TIMEOUT

All IRQs are routed through **DIO1**.

---

## Debugging

- `RF_DEBUG` enables detailed debug prints
- `_write()` redirects `printf()` to UART
- Any polling paths are diagnostic-only
- Logic analyzer used to verify SPI transactions and IRQ timing

---

## Project Status

- **Step 40:** Complete
- **Step 41:** Pending (RSSI / SNR normalization)

---

## Rules of Engagement

1. One logical change at a time
2. Explicit scope for every change
3. No silent renames or invented symbols
4. Minimal diffs
5. Explain the “why” before code
6. No cascading steps
7. Instrumentation discipline
8. Prefer hardware signals over polling
9. Clean pause/resume summaries

---

## Files of Record

- main.c / main.h
- radio_app.c / radio_app.h
- sx1262.c / sx1262.h
- stm32_uidhash.c / stm32_uidhash.h

---

## Branch Policy

- Authoritative branch: `groulx`

---

## Notes

This project is intentionally conservative and methodical.
The goal is correct, explainable RF behavior, not rapid feature accumulation.
