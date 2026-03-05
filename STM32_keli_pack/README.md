# 🌿 Smart Greenhouse & Automatic Fire Alarm System

> **Embedded IoT project** — STM32F411VET6 (bare-metal CMSIS) communicates sensor data to Raspberry Pi 4 via SPI for real-time monitoring dashboard.

![STM32](https://img.shields.io/badge/MCU-STM32F411VET6-blue?logo=stmicroelectronics)
![Raspberry Pi](https://img.shields.io/badge/SBC-Raspberry%20Pi%204-red?logo=raspberrypi)
![Language C](https://img.shields.io/badge/Firmware-C%20(CMSIS)-green)
![Python](https://img.shields.io/badge/GUI-Python%203%20(Tkinter)-yellow?logo=python)
![License](https://img.shields.io/badge/License-MIT-lightgrey)

---

## 📑 Table of Contents

- [Overview](#overview)
- [System Architecture](#system-architecture)
- [Hardware Requirements](#hardware-requirements)
- [Pin Mapping](#pin-mapping)
- [SPI Protocol Specification](#spi-protocol-specification)
- [Repository Structure](#repository-structure)
- [Firmware Modules — Detailed Explanation](#firmware-modules--detailed-explanation)
- [Firmware (STM32) — Build & Flash](#firmware-stm32--build--flash)
- [GUI (Raspberry Pi) — Setup & Run](#gui-raspberry-pi--setup--run)
- [Wiring Diagram](#wiring-diagram)
- [Configuration & Thresholds](#configuration--thresholds)
- [Data Flow](#data-flow)
- [SPI Driver Evolution & Bug Fix History](#spi-driver-evolution--bug-fix-history)
- [Troubleshooting](#troubleshooting)
- [Future Improvements](#future-improvements)
- [Authors](#authors)

---

## Overview

This project implements a **Smart Greenhouse Monitoring System** combined with an **Automatic Fire/Gas Alarm** mechanism. The system is split into two independent subsystems that communicate over **SPI**:

| Subsystem | Platform | Role | Description |
|-----------|----------|------|-------------|
| **Firmware** | STM32F411VET6 | SPI Slave | Reads 4 analog sensors via ADC+DMA, evaluates alarm thresholds with hysteresis, controls buzzer & motor, and packs a 16-byte binary frame for SPI transmission. |
| **Dashboard GUI** | Raspberry Pi 4 | SPI Master | Polls the STM32 over SPI, parses incoming frames, and renders a live Tkinter dashboard with gauges, charts, temperature, gas levels, raw ADC values, and actuator states. |

### Key Features

- ⚡ **Bare-metal firmware** — register-level CMSIS (no HAL), maximising transparency and control.
- 🔄 **ADC Scan + DMA circular mode** — 4-channel continuous conversion with zero CPU overhead.
- 📊 **Moving-average filter** — 8-sample O(1) sliding window on all ADC channels, reducing noise.
- 🔥 **3-state alarm with hysteresis** — NORMAL → WARN → ALARM state machine, independent for temperature & gas, with separate ON/OFF thresholds to prevent flickering.
- 🔔 **Buzzer beep patterns** — WARN: slow beep ~1 Hz, ALARM: fast beep ~10 Hz, driven by SysTick 1 ms tick.
- 📡 **Custom binary SPI protocol** — 16-byte frame with magic header `AA 55`, XOR checksum, end marker `0D`, and double-buffer for atomic updates.
- 🔀 **TXE-only SPI driver (v3)** — Robust slave TX using TXE interrupt with self-wrapping counter; no EXTI, no frame-reset race conditions.
- 🖥️ **Real-time GUI** — Python/Tkinter dashboard on Raspberry Pi with retained-mode matplotlib charts, auto-resync on bad frames, and simulation mode for development.
- 💤 **Low-power main loop** — `__WFI()` in `while(1)`: all work is interrupt-driven.
- 🏗️ **3-layer architecture** — BSP (register-level) → Service (logic, filter, protocol) → App (init + sleep).

---

## System Architecture

### 3-Layer Firmware Design

```
┌───────────────────────────────────────────────────────────────┐
│  APP LAYER  (main.c)                                          │
│    main() → Init all → while(1) { __WFI(); }                 │
├───────────────────────────────────────────────────────────────┤
│  SERVICE LAYER                                                │
│  ┌───────────┐ ┌─────────────┐ ┌────────────┐ ┌────────────┐│
│  │ adc_mgr   │ │ fire_logic  │ │ actuators  │ │ greenhouse ││
│  │           │ │             │ │            │ │            ││
│  │ Moving    │ │ State mach. │ │ Buzzer     │ │ Central    ││
│  │ average   │ │ NORMAL →    │ │ beep       │ │ logic +    ││
│  │ filter    │ │ WARN →      │ │ pattern    │ │ SPI packet ││
│  │ (8 samp.) │ │ ALARM       │ │ + Motor    │ │ builder    ││
│  │    O(1)   │ │ (hysteresis)│ │ ON/OFF     │ │ + dbl-buf  ││
│  └───────────┘ └─────────────┘ └────────────┘ └────────────┘│
├───────────────────────────────────────────────────────────────┤
│  BSP LAYER  (bare-metal register-level CMSIS)                 │
│  ┌──────────┐ ┌──────────┐ ┌──────────────┐ ┌─────────────┐ │
│  │ RCC      │ │ GPIO     │ │ SPI1 Slave   │ │ ADC1 + DMA2 │ │
│  │ Clock EN │ │ Pin Cfg  │ │ TXE IRQ (v3) │ │ Scan+Circ.  │ │
│  └──────────┘ └──────────┘ └──────────────┘ └─────────────┘ │
└───────────────────────────────────────────────────────────────┘
```

### Full System Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                   STM32F411VET6 (SPI Slave)                   │
│                                                               │
│  ┌─────────┐  DMA TC IRQ  ┌───────────┐  ┌──────────────┐   │
│  │ ADC1    │ ────────────▶ │ adc_mgr   │─▶│ fire_logic   │   │
│  │ Scan    │ g_adc_buf[4]  │ (filter)  │  │ (hysteresis) │   │
│  │ 4-ch    │               └───────────┘  └──────┬───────┘   │
│  │ + DMA2  │                                     │ state     │
│  └─────────┘                                     ▼           │
│       ▲                              ┌───────────────────┐   │
│       │                              │   greenhouse.c    │   │
│  PA0 PA1 PA2 PA3                     │ • Actuator_Set()  │   │
│  LM35 GAS  S3  S4                    │ • build_packet()  │   │
│                                      │ • SwapBuffer()    │   │
│  ┌──────────────┐                    └─────────┬─────────┘   │
│  │ SysTick 1ms  │ ◀── Actuator_Tick1ms()       │ 16-byte    │
│  │ (prio 3)     │ ──▶ PB0 Buzzer (pattern)     │ frame      │
│  └──────────────┘     PB1 Motor  (ON/OFF)      ▼            │
│                                        ┌──────────────────┐  │
│                                        │ SPI1 Slave (v3)  │  │
│                                        │ TXE IRQ → DR     │  │
│                                        │ Double-buffer     │  │
│                                        └────────┬─────────┘  │
└─────────────────────────────────────────────────┼────────────┘
                                                  │  SPI bus
                                  PA4=NSS ────────┤  (Mode 0)
                                  PA5=SCK ────────┤  (1 MHz)
                                  PA6=MISO ───────┤
                                  PA7=MOSI ───────┤
┌─────────────────────────────────────────────────┼────────────┐
│                Raspberry Pi 4 (SPI Master)       │            │
│                                  ┌──────────────▼─────────┐  │
│                                  │ spidev xfer2 (polling) │  │
│                                  │ + auto-resync on error │  │
│                                  └──────────┬─────────────┘  │
│                                  ┌──────────▼─────────────┐  │
│                                  │ Tkinter GUI + Matplotlib│  │
│                                  │ Temp, Gas, ADC, Charts  │  │
│                                  └────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

---

## Hardware Requirements

### Components

| # | Component | Qty | Notes |
|---|-----------|-----|-------|
| 1 | STM32F411VET6 dev board | 1 | e.g. WeAct BlackPill V3.1 or Discovery |
| 2 | Raspberry Pi 4 Model B | 1 | Any RAM variant; Raspberry Pi OS |
| 3 | LM35 Temperature Sensor | 1 | Analog, 10 mV/°C, connected to PA0 |
| 4 | MQ-series Gas Sensor (MQ-2/MQ-5) | 1 | Analog output to PA1 |
| 5 | Soil Moisture Sensor | 1 | Analog output to PA2 |
| 6 | Light Sensor (LDR module) | 1 | Analog output to PA3 |
| 7 | Active Buzzer | 1 | Driven from PB0 (GPIO push-pull) |
| 8 | DC Motor / Fan | 1 | PB1 via MOSFET/relay (not direct GPIO!) |
| 9 | Jumper wires + Breadboard | — | Dupont male-female |

### Software / Toolchain

| Tool | Version | Purpose |
|------|---------|---------|
| **Keil µVision 5** | ≥ 5.38 | STM32 firmware IDE, build & flash |
| **STM32F4 DFP** | ≥ 2.6.8 | CMSIS device pack for STM32F411 |
| **Python 3** | ≥ 3.7 | Raspberry Pi GUI runtime |
| **spidev** (pip) | ≥ 3.5 | Python SPI driver |
| **matplotlib** (pip) | ≥ 3.5 | GUI live charts |
| **tkinter** | built-in | GUI framework |

---

## Pin Mapping

### STM32F411VET6

| Pin | Function | Peripheral | Description |
|-----|----------|------------|-------------|
| **PA0** | ADC1_IN0 | ADC1 Ch0 | 🌡️ LM35 temperature sensor |
| **PA1** | ADC1_IN1 | ADC1 Ch1 | 💨 Gas sensor (MQ-2/MQ-5) |
| **PA2** | ADC1_IN2 | ADC1 Ch2 | 🌱 Soil moisture sensor |
| **PA3** | ADC1_IN3 | ADC1 Ch3 | 💡 Light level sensor |
| **PA4** | SPI1_NSS | SPI1 AF5 | Chip Select (active low, HW managed) |
| **PA5** | SPI1_SCK | SPI1 AF5 | SPI Clock |
| **PA6** | SPI1_MISO | SPI1 AF5 | STM32 → Pi (data line) |
| **PA7** | SPI1_MOSI | SPI1 AF5 | Pi → STM32 (dummy bytes) |
| **PB0** | GPIO OUT PP | — | 🔔 Buzzer control |
| **PB1** | GPIO OUT PP | — | ⚙️ Motor/Fan control |

### Raspberry Pi 4 (SPI0)

| Pi Pin (BCM) | Function | Connect to |
|-------------|----------|------------|
| GPIO 8 (CE0) | SPI0_CE0 | STM32 PA4 (NSS) |
| GPIO 11 | SPI0_SCLK | STM32 PA5 (SCK) |
| GPIO 9 | SPI0_MISO | STM32 PA6 (MISO) |
| GPIO 10 | SPI0_MOSI | STM32 PA7 (MOSI) |
| GND | Ground | STM32 GND |

> ⚠️ **Voltage:** STM32F411 GPIO and Raspberry Pi SPI are both **3.3 V** — direct connection is safe. **Do NOT use 5 V logic.**

---

## SPI Protocol Specification

### Frame Format (16 bytes, Little-Endian)

```
Byte   Field            Size   Description
─────  ───────────────  ─────  ─────────────────────────────────
 [0]   MAGIC_0          1      0xAA  — start-of-frame marker
 [1]   MAGIC_1          1      0x55  — start-of-frame marker
 [2]   SEQ              1      Sequence number (0–255, wraps)
 [3]   STATUS           1      Bit-field (see below)
 [4]   ADC0_L           1      LM35 raw ADC (low byte)
 [5]   ADC0_H           1      LM35 raw ADC (high byte)
 [6]   ADC1_L           1      Gas sensor raw ADC (low byte)
 [7]   ADC1_H           1      Gas sensor raw ADC (high byte)
 [8]   ADC2_L           1      Sensor 3 raw ADC (low byte)
 [9]   ADC2_H           1      Sensor 3 raw ADC (high byte)
[10]   ADC3_L           1      Sensor 4 raw ADC (low byte)
[11]   ADC3_H           1      Sensor 4 raw ADC (high byte)
[12]   TEMP_X10_L       1      Temperature × 10 (low byte)
[13]   TEMP_X10_H       1      Temperature × 10 (high byte)
[14]   XOR_CHECKSUM     1      XOR of bytes [0]–[13]
[15]   END_MARKER       1      0x0D  — end-of-frame
```

### STATUS Byte (bit-field)

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `BUZZER` | 1 = Buzzer currently ON |
| 1 | `MOTOR` | 1 = Motor/Fan currently ON |
| 2 | `GAS_ALARM` | 1 = Gas level ≥ WARN threshold |
| 3 | `TEMP_ALARM` | 1 = Temperature ≥ WARN threshold |
| 4–7 | Reserved | 0 |

### SPI Parameters

| Parameter | Value |
|-----------|-------|
| Mode | 0 (CPOL=0, CPHA=0) |
| Speed | 1 MHz (configurable) |
| Bit order | MSB first |
| Word size | 8 bits |
| NSS | Hardware (active low) |
| Transfer | Full-duplex; Master sends 16× `0x00`, Slave returns 16-byte frame |

### Checksum Algorithm

```c
uint8_t checksum = 0;
for (int i = 0; i < 14; i++)
    checksum ^= frame[i];
frame[14] = checksum;
```

---

## Repository Structure

```
MiniProject_GreenHouse/
│
├── README.md                       ← This file
├── gui_spi_greenhouse.py           ← 🐍 Raspberry Pi: Tkinter GUI + SPI master + charts
├── test_spidev_master.py           ← 🔍 SPI diagnostic tool (hex dump + auto-resync)
│
├── other/
│   └── README.md                   ← Previous README draft (archived)
│
└── STM32_keli_pack/                ← 🔧 Keil µVision project (firmware)
    └── STM32_LIB/
        │
        │  ╔═══ APP LAYER ═══╗
        ├── main.c                  ← Entry point: init → SysTick → WFI sleep loop
        ├── board.h                 ← ALL config: pins, thresholds, protocol, NVIC
        │
        │  ╔═══ SERVICE LAYER ═══╗
        ├── adc_mgr.c/.h           ← Moving-average filter (8 samples, O(1))
        ├── fire_logic.c/.h        ← State machine NORMAL→WARN→ALARM + hysteresis
        ├── actuators.c/.h         ← Buzzer beep patterns + Motor ON/OFF by FireState
        ├── greenhouse.c/.h        ← Central logic: filter→alarm→actuator→SPI packet
        │                            + double-buffer atomic swap
        │
        │  ╔═══ BSP LAYER (bare-metal CMSIS) ═══╗
        ├── RCC_STM32_LIB.c/.h     ← Clock enable: GPIOA/B, DMA2, ADC1, SPI1, SYSCFG
        ├── GPIO.c/.h               ← Pin config: analog (PA0–3), AF5 (PA4–7), out (PB0–1)
        ├── ADC_DMA_LIB.c           ← ADC1 scan + DMA2 Stream0 circular transfer
        ├── ADC_LIB.h               ← ADC register-level type definitions
        ├── DMA_LIB.h               ← DMA register-level type definitions + g_adc_buf extern
        ├── SPI_LIB.c/.h            ← SPI1 slave TXE IRQ driver (v3 — no EXTI)
        │
        │  ╔═══ REGISTER MAPS (reserved for future) ═══╗
        ├── TIMER.h                 ← TIM register map (for future PWM)
        ├── UART_LIB.h              ← USART register map (for future debug)
        │
        │  ╔═══ KEIL PROJECT FILES ═══╗
        ├── stm32f411.uvmpw         ← Keil multi-project workspace
        ├── DebugConfig/             ← Keil debugger configuration
        ├── Objects/                 ← Build artifacts (.hex, .axf, .o)
        └── RTE/                     ← Keil Run-Time Environment
            ├── _Target_1/
            │   └── RTE_Components.h
            └── Device/STM32F411VETx/
                ├── startup_stm32f411xe.s
                └── system_stm32f4xx.c
```

---

## Firmware Modules — Detailed Explanation

### `board.h` — Single Source of Truth

All magic numbers, pin assignments, thresholds, and protocol constants are defined in this one header. Both the C firmware and the Python GUI must agree on these values. The file is organized into 8 sections:

1. **System Clock** — HSI 16 MHz, SysTick 1 kHz
2. **Pin Map** — PA0–PA3 (ADC), PA4–PA7 (SPI1), PB0–PB1 (actuators)
3. **ADC Channel Map** — Channel indices, sample time, LM35 conversion formula
4. **ADC Filter** — Moving-average window size (8 samples)
5. **Alarm Thresholds** — Hysteresis values for temperature and gas
6. **Buzzer Patterns** — ON/OFF durations in milliseconds
7. **SPI Protocol** — Frame layout, magic bytes, STATUS bit positions, offsets
8. **NVIC Priorities** — DMA=1 (highest), SPI=2, SysTick=3

### `RCC_STM32_LIB.c` — Clock Enable

Enables peripheral clocks on AHB1 (GPIOA, GPIOB, DMA2) and APB2 (ADC1, SPI1, SYSCFG). Must be called first before any peripheral register access.

### `GPIO.c` — Pin Configuration

Four functions configure all project pins at the register level:

| Function | Pins | Mode | Notes |
|----------|------|------|-------|
| `GPIO_Config_ADC_PA0_PA3_Analog()` | PA0–PA3 | Analog | No pull-up/down (required for ADC) |
| `GPIO_Config_SPI1_PA4_PA7_AF5()` | PA4–PA7 | AF5 | Very high speed, pull-down on NSS+SCK |
| `GPIO_Config_Buzzer_PB0_Output()` | PB0 | Push-pull output | Starts OFF |
| `GPIO_Config_Motor_PB1_Output()` | PB1 | Push-pull output | Starts OFF |

### `ADC_DMA_LIB.c` — ADC + DMA Hardware Driver

Configures **ADC1** in 4-channel scan mode (PA0→PA3) with continuous conversion, and **DMA2 Stream0 Channel0** in circular mode to transfer results into `g_adc_buf[4]` automatically.

Key configuration:
- **ADC clock:** PCLK2/2 = 8 MHz
- **Sample time:** 84 cycles per channel (configurable via `board.h`)
- **DMA:** 16-bit peripheral-to-memory, circular, transfer-complete interrupt
- **Callback:** `DMA2_Stream0_IRQHandler()` calls `Greenhouse_OnAdcReady()` on every scan completion (~48 µs period)

### `adc_mgr.c` — Moving-Average Filter

Implements an **O(1) ring-buffer moving-average** filter with `ADC_FILTER_SAMPLES` (default 8) entries per channel.

**Algorithm:**
```
On new sample:
  sum[ch] -= ring[ch][oldest]     // subtract oldest sample
  ring[ch][oldest] = new_sample   // overwrite with new
  sum[ch] += new_sample           // add new sample
  GetFiltered(ch) = sum[ch] / N   // instant average, no loop needed
```

**Key APIs:**
- `ADC_Mgr_FeedSample(raw[4])` — push 4 raw ADC values into filter (called from DMA ISR)
- `ADC_Mgr_GetFiltered(ch)` — return filtered ADC value for channel `ch`
- `ADC_Mgr_GetTempX10()` — return LM35 temperature × 10 (0.1°C unit)
  - Formula: `adc_filtered × 3300 / 4095` (LM35: 10 mV/°C → mV = temp×10)
- `ADC_Mgr_GetGasRaw()` — return filtered gas sensor ADC value

### `fire_logic.c` — Alarm State Machine with Hysteresis

Evaluates temperature and gas independently through a 3-state machine:

```
NORMAL ──[≥ WARN_ON]──▶ WARN ──[≥ ALARM_ON]──▶ ALARM
  ▲                       │                      │
  └───[≤ WARN_OFF]───────┘                      │
                          ▲                      │
                          └───[≤ ALARM_OFF]──────┘
```

**Hysteresis principle:** The "enter" threshold (ON) is higher than the "exit" threshold (OFF). This creates a dead zone that prevents alarm flickering when sensor values oscillate near a boundary.

**Safety rule:** From ALARM, the system can only step down to WARN (never jump directly to NORMAL).

**Combined state:** `FireLogic_GetState()` returns `max(temp_state, gas_state)` — the most severe condition wins.

### `actuators.c` — Buzzer & Motor Control

Controls physical actuators based on the alarm state:

| State | Buzzer | Motor |
|-------|--------|-------|
| NORMAL | OFF | OFF |
| WARN | Slow beep ~1 Hz (100ms ON / 900ms OFF) | OFF |
| ALARM | Fast beep ~10 Hz (50ms ON / 50ms OFF) | ON |

- `Actuator_SetState()` — called from DMA ISR context (sets target state)
- `Actuator_Tick1ms()` — called from SysTick every 1 ms (runs beep pattern)
- Uses **BSRR** (Bit Set/Reset Register) for atomic GPIO writes safe from any ISR context

### `greenhouse.c` — Central Logic + SPI Packet Builder

The "brain" that ties everything together. Called from `DMA2_Stream0_IRQHandler()` at priority 1.

**Processing pipeline (inside `Greenhouse_OnAdcReady()`):**
1. Feed raw ADC samples into moving-average filter
2. Read filtered temperature and gas values
3. Update fire-logic state machine
4. Set actuator target state
5. Build STATUS byte (buzzer | motor | gas_alarm | temp_alarm)
6. Collect 4 filtered ADC values
7. Build 16-byte SPI frame into **back-buffer**
8. **Atomic swap** — back-buffer becomes active SPI buffer

**Double-Buffer Strategy:**
```
g_spi_buf_a[16]  ◄── SPI reads from here (active)
g_spi_buf_b[16]  ◄── greenhouse writes here (back)

After build_packet():
  SwapBuffer() swaps the pointers.
  g_idx is NEVER touched → ongoing transfer continues uninterrupted.
  At worst, one mixed frame → XOR checksum fails → Pi retries.
```

### `SPI_LIB.c` — SPI1 Slave Driver (v3 — TXE-only)

The most critical module. **Third rewrite** after two previous approaches failed.

**Architecture:**
- **TXE interrupt only** — fires when the TX buffer is empty and ready for the next byte
- **Self-wrapping counter** — `g_idx` increments and wraps at `g_len` (16), auto-aligning frames
- **No EXTI on NSS** — previous designs used EXTI4 on PA4 which caused spurious resets
- **No RXNE handling** — we don't need received data (Pi sends dummy 0x00)

**Key APIs:**

| Function | Called From | Purpose |
|----------|-----------|---------|
| `SPI1_Slave_Init()` | `main()` | Configure SPI1 slave, enable TXE IRQ, pre-fill DR with byte[0] |
| `SPI1_Slave_SetTxBuffer()` | `Greenhouse_InitPacket()` | Set initial buffer pointer + length |
| `SPI1_Slave_SwapBuffer()` | `Greenhouse_OnAdcReady()` | Atomic pointer swap (g_idx untouched) |
| `SPI1_IRQHandler()` | Hardware | Load `g_tx[g_idx++]` into DR, wrap at g_len |

**Register Setup:**
```c
CR1 = 0x0000   // Slave, CPOL=0, CPHA=0, 8-bit, MSB-first, HW NSS (SSM=0)
CR2 = TXEIE    // TXE interrupt enable only
```

**Timing Analysis (@ 1 MHz SPI, 16 MHz CPU):**
- TXE fires every 8 µs (one byte period)
- ISR executes in ~625 ns (~10 instructions)
- DMA can preempt for up to ~5 µs
- Margin: 8 − 0.625 − 5 = 2.375 µs → safe

### `main.c` — Entry Point

**Critical init order:**
```
1. RCC_Enable_For_GPIO_ADC_SPI_DMA()    ← clocks MUST be first
2. GPIO config (ADC, SPI, Buzzer, Motor) ← pins before peripherals
3. Software module init (ADC_Mgr, FireLogic, Actuator)
4. Greenhouse_InitPacket()               ← builds zero-frame, sets g_tx
5. SPI1_Slave_Init()                     ← reads g_tx[0] to pre-fill DR
6. ADC1_DMA2_Stream0_InitStart()         ← starts DMA interrupts
7. SysTick_Init()                        ← 1 ms tick for buzzer
8. while(1) { __WFI(); }                ← sleep, all interrupt-driven
```

> ⚠️ **Init order matters:** `Greenhouse_InitPacket()` MUST run before `SPI1_Slave_Init()` because SPI init pre-fills `DR` with `g_tx[0]`. If SPI init runs first, `g_tx` is NULL and DR gets garbage.

---

## Firmware (STM32) — Build & Flash

### Prerequisites

1. Install **Keil µVision 5** (MDK-ARM) on Windows.
2. Install the **STM32F4xx_DFP** pack (≥ v2.6.8) via Pack Installer.

### Build

1. Open Keil µVision.
2. **File → Open Multi-Project Workspace** → `STM32_keli_pack/STM32_LIB/stm32f411.uvmpw`
3. In **Project → Options for Target**:
   - **Device:** `STM32F411VETx`
   - **C/C++ → Define:** `STM32F411xE`
   - **C/C++ → Include Paths:** must include `STM32_LIB/` and CMSIS paths
4. Ensure all `.c` files are added to the project (Project → Manage Project Items):
   - `main.c`, `RCC_STM32_LIB.c`, `GPIO.c`, `ADC_DMA_LIB.c`, `SPI_LIB.c`
   - `adc_mgr.c`, `fire_logic.c`, `actuators.c`, `greenhouse.c`
5. Press **F7** (Build) → expect **0 Errors, 0 Warnings**.

### Flash

1. Connect STM32 board via **ST-Link V2** (SWD).
2. **Flash → Download** (or **F8**).
3. MCU resets and starts running automatically.

### Interrupt Map

| ISR | Priority | Frequency | Function |
|-----|----------|-----------|----------|
| `DMA2_Stream0` | 1 (highest) | ~21 kHz | ADC ready → filter → alarm → packet → swap |
| `SPI1` | 2 | per-byte from Pi | Load next frame byte into SPI DR |
| `SysTick` | 3 (lowest) | 1 kHz | Buzzer beep pattern timing |

---

## GUI (Raspberry Pi) — Setup & Run

### Enable SPI

```bash
sudo raspi-config
# → Interface Options → SPI → Enable
sudo reboot
```

Verify:
```bash
ls /dev/spidev*
# Expected: /dev/spidev0.0  /dev/spidev0.1
```

### Install Dependencies

```bash
sudo apt update && sudo apt install python3-tk
pip3 install spidev matplotlib
```

### Fix Permissions (if needed)

```bash
sudo usermod -aG spi $USER
# Log out and back in (or reboot)
```

### Run the Dashboard

```bash
python3 gui_spi_greenhouse.py
```

The GUI provides:
- **Bus/Dev/Hz** controls in the header
- **Reconnect SPI** button
- **Simulation mode** checkbox (for development without hardware)
- Real-time gauges for temperature and gas level
- 4× ADC raw value displays
- STATUS indicators (buzzer, motor, gas alarm, temp alarm)
- Live matplotlib charts with 60-second rolling window
- Stale-data detection (greys out if no new frames for 3 seconds)
- Auto-resync after consecutive bad frames

### Quick SPI Test (without GUI)

```bash
python3 test_spidev_master.py --hz 100000 -n 20
```

Expected output for a working system:
```
[   1] OK  AA 55 0A 00 6C 02 C8 00 5A 01 FF 00 CC 01 5B 0D  SEQ= 10 ...
[   2] OK  AA 55 0B 00 6C 02 C8 00 5A 01 FF 00 CC 01 48 0D  SEQ= 11 ...
```

---

## Wiring Diagram

```
     STM32F411VET6                          Raspberry Pi 4
    ┌──────────────┐                      ┌──────────────────┐
    │         PA0 ─┤◄── LM35 Vout        │                  │
    │         PA1 ─┤◄── Gas Sensor Aout   │                  │
    │         PA2 ─┤◄── Soil Moisture     │                  │
    │         PA3 ─┤◄── Light Sensor      │                  │
    │              │                      │                  │
    │  SPI1        │     SPI Bus          │   SPI0           │
    │         PA4 ─┤◄────────────────────►├─ GPIO8  (CE0)    │
    │         PA5 ─┤◄────────────────────►├─ GPIO11 (SCLK)   │
    │         PA6 ─┤─────────────────────►├─ GPIO9  (MISO)   │
    │         PA7 ─┤◄────────────────────►├─ GPIO10 (MOSI)   │
    │              │                      │                  │
    │         GND ─┤◄────────────────────►├─ GND             │
    │              │                      └──────────────────┘
    │  Actuators   │
    │         PB0 ─┤──▶ Buzzer (+)
    │         PB1 ─┤──▶ Motor Driver (MOSFET IN)
    │         GND ─┤──▶ Buzzer (−) / Motor GND
    └──────────────┘
```

> 💡 **Motor:** Use a MOSFET (e.g. IRLZ44N) or transistor (2N2222) with flyback diode. STM32 GPIO max current is ~25 mA — not enough for most motors.

---

## Configuration & Thresholds

All parameters are in `board.h`:

### Alarm Thresholds (Hysteresis)

| Macro | Default | Unit | Description |
|-------|---------|------|-------------|
| `TEMP_WARN_ON_X10` | `350` | 0.1°C | ≥ 35.0°C → enter WARN |
| `TEMP_WARN_OFF_X10` | `330` | 0.1°C | ≤ 33.0°C → exit WARN |
| `TEMP_ALARM_ON_X10` | `500` | 0.1°C | ≥ 50.0°C → enter ALARM |
| `TEMP_ALARM_OFF_X10` | `450` | 0.1°C | ≤ 45.0°C → exit ALARM |
| `GAS_WARN_ON_ADC` | `2000` | raw | ≥ 2000 → enter WARN |
| `GAS_WARN_OFF_ADC` | `1800` | raw | ≤ 1800 → exit WARN |
| `GAS_ALARM_ON_ADC` | `2500` | raw | ≥ 2500 → enter ALARM |
| `GAS_ALARM_OFF_ADC` | `2300` | raw | ≤ 2300 → exit ALARM |

### Buzzer Patterns

| Macro | Default | Description |
|-------|---------|-------------|
| `BUZZER_WARN_ON_MS` | `100` | WARN: ON duration → ~1 Hz |
| `BUZZER_WARN_OFF_MS` | `900` | WARN: OFF duration |
| `BUZZER_ALARM_ON_MS` | `50` | ALARM: ON duration → ~10 Hz |
| `BUZZER_ALARM_OFF_MS` | `50` | ALARM: OFF duration |

### LM35 Temperature Calculation

```
ADC resolution = 12-bit (0–4095)
Vref = 3.3 V
LM35: 10 mV/°C

temp_mV  = adc_filtered × 3300 / 4095
temp_x10 = temp_mV                       (since 1 mV = 0.1°C)
temp_°C  = temp_x10 / 10.0

Example: ADC filtered = 620 → 620 × 3300 / 4095 ≈ 499 mV → 49.9°C
```

---

## Data Flow

### Complete Interrupt-Driven Pipeline

```
[DMA2_Stream0 Transfer Complete IRQ]  (priority 1, ~48 µs period)
    │
    ▼
Greenhouse_OnAdcReady()
    ├── ADC_Mgr_FeedSample(g_adc_buf)     ← push 4 raw values into ring buffer
    ├── temp_x10 = ADC_Mgr_GetTempX10()   ← filtered temperature (0.1°C)
    ├── gas_raw  = ADC_Mgr_GetGasRaw()    ← filtered gas ADC
    ├── FireLogic_Update(temp_x10, gas)    ← state machine with hysteresis
    ├── Actuator_SetState(state)           ← set buzzer/motor target
    ├── build_packet(back_buf, ...)        ← 16-byte frame into back-buffer
    └── SPI1_Slave_SwapBuffer(back_buf)    ← atomic pointer swap (g_idx untouched)

[SPI1_IRQHandler]  (priority 2, fires every 8 µs at 1 MHz)
    └── TXE: SPI1->DR = g_tx[g_idx++]     ← load next byte
              if (g_idx >= g_len) g_idx=0  ← wrap → frame auto-aligned

[SysTick_Handler]  (every 1 ms, priority 3)
    └── Actuator_Tick1ms()
        ├── NORMAL:  buzzer OFF, motor OFF
        ├── WARN:    100ms ON / 900ms OFF, motor OFF
        └── ALARM:   50ms ON / 50ms OFF, motor ON
```

---

## SPI Driver Evolution & Bug Fix History

This section documents the debugging journey of the SPI slave driver — useful for understanding design decisions and avoiding similar pitfalls.

### v1: RXNE + TXE Dual Handler

**Approach:** Both RXNE and TXE interrupts enabled, each advancing `g_idx` independently.

**Bug:** Double-advance race condition. RXNE and TXE could fire in quick succession, causing `g_idx` to skip a byte. Result: duplicate `byte[0]` and shifted frames.

### v2: RXNE + EXTI4 (NSS Edge) Preload

**Approach:** Only RXNE interrupt for byte-by-byte TX. EXTI4 configured on PA4 (NSS falling edge) to detect start of transfer → write `0xAA` to DR and reset `g_idx = 1`.

**Bug: THE ALL-0xAA PROBLEM.** This was the infamous "bad frame: aaaaaaaaaaaaaaaa" bug. Root cause:

1. PA4 is configured as **AF5 (SPI1_NSS)**, but EXTI4 was also enabled on the same pin.
2. Electrical noise on the NSS line (or internal toggling from AF5 hardware) caused **EXTI4 to fire spuriously between every SPI byte**.
3. Each spurious EXTI4 wrote `0xAA` to DR and reset `g_idx = 1`.
4. **Result:** Every byte the Pi reads is `0xAA` — the first byte of the frame header.

Additionally, `SwapBuffer()` called `preload_first_byte()` from the DMA ISR, which had the same effect (writing `0xAA` to DR at random times).

### v3: TXE-only, No EXTI (Current — Working)

**Approach:** Complete removal of EXTI4. TXE interrupt only. Self-wrapping counter.

**Why it works:**
- No external event can reset `g_idx` — it only increments in the TXE ISR
- Frame alignment is achieved by the counter wrapping at `g_len` (16)
- `SwapBuffer()` only changes the buffer pointer, never touches `g_idx`
- `SPI1_Slave_Init()` pre-fills DR with `g_tx[0]` and sets `g_idx = 1`
- The first 1–2 frames after power-on may be misaligned; the Pi auto-resyncs

**EXTI4 stub handler** is kept to prevent Hard Fault if a pending bit exists from a previous firmware load.

---

## Troubleshooting

### SPI Communication Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| All bytes are `0xAA` | EXTI4 spurious resets (v2 bug) | Use v3 SPI driver (TXE-only, no EXTI) |
| First few frames misaligned | Normal for v3 (counter not yet synced) | Pi auto-resyncs; wait 2–3 frames |
| XOR checksum fails occasionally | DMA preemption during SPI transfer | Normal; Pi retries. Reduce SPI speed if frequent |
| GUI shows `--.-` forever | SPI not enabled on Pi | `sudo raspi-config` → enable SPI |
| `Permission denied` on spidev | User not in spi group | `sudo usermod -aG spi $USER` → reboot |
| All ADC values = 0 | Sensors not connected or STM32 not flashed | Check wiring; re-flash firmware |
| Frame always shifted by N bytes | NSS not wired or floating | Ensure PA4 ↔ CE0 connected |
| Alarm flickering ON/OFF | Hysteresis gap too small | Increase gap between `_ON` and `_OFF` in `board.h` |

### STM32 Firmware Issues

| Symptom | Cause | Fix |
|---------|-------|-----|
| Build error: undefined symbol | `.c` file not added to Keil project | Project → Manage Items → add file |
| HardFault on boot | Clock not enabled before peripheral access | Check init order in `main()` |
| SPI sends garbage first byte | `Greenhouse_InitPacket()` runs after `SPI1_Slave_Init()` | Swap order: InitPacket BEFORE SPI Init |
| Build: STM32F411xE not defined | Missing preprocessor symbol | Options → C/C++ → Define → add `STM32F411xE` |

### Debugging Tips

1. **Logic Analyzer:** Connect to PA4–PA7. Verify NSS goes LOW, 16 bytes clocked, MISO data matches frame.
2. **Keil Debugger:** Breakpoint in `Greenhouse_OnAdcReady()`, inspect `g_adc_buf[]` and watch `g_spi_buf_a[]` in Memory window.
3. **Pi Quick Test:**
   ```python
   import spidev
   spi = spidev.SpiDev()
   spi.open(0, 0)
   spi.max_speed_hz = 100_000
   spi.mode = 0
   data = spi.xfer2([0x00]*16)
   print([hex(b) for b in data])
   spi.close()
   ```

---

## Future Improvements

- [ ] **PWM-driven actuators** — Replace GPIO push-pull with TIM-based PWM for variable buzzer tone and motor speed.
- [ ] **CRC-16 checksum** — Upgrade from XOR to CRC-16-CCITT for stronger error detection.
- [ ] **Extended frame protocol** — Add version, flags, and variable-length payload.
- [ ] **MQTT / Wi-Fi bridge** — Forward data from Pi to cloud dashboard (ThingsBoard, Grafana).
- [ ] **UART debug output** — Print sensor data over serial for development without Pi.
- [ ] **Watchdog timer (IWDG)** — Auto-reset on firmware hang.
- [x] ~~Hysteresis on alarms~~ — ✅ 3-state machine with separate ON/OFF thresholds.
- [x] ~~Moving-average filter~~ — ✅ 8-sample O(1) sliding window.
- [x] ~~Buzzer beep patterns~~ — ✅ WARN ~1 Hz, ALARM ~10 Hz via SysTick.
- [x] ~~Double-buffer SPI TX~~ — ✅ Atomic pointer swap, no partial frames.
- [x] ~~TXE-only SPI driver~~ — ✅ v3: eliminated all-0xAA bug.
- [x] ~~Auto-resync (Pi side)~~ — ✅ Reads 48 bytes and scans for valid frame.

---

## Authors

| Name | Role |
|------|------|
| **Thanh** | Embedded Firmware & System Integration |

---

<p align="center">
  <i>Built with ❤️ — STM32 bare-metal CMSIS + Raspberry Pi 4 + Python</i>
</p>
