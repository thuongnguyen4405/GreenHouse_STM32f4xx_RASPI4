# 🌿 Smart Greenhouse & Automatic Fire Alarm System

> **Embedded IoT project** — STM32F411VET6 (bare-metal CMSIS) communicates sensor data to Raspberry Pi 4 via SPI for real-time monitoring GUI.

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
- [Firmware (STM32) — Build & Flash](#firmware-stm32--build--flash)
- [GUI (Raspberry Pi) — Setup & Run](#gui-raspberry-pi--setup--run)
- [Wiring Diagram](#wiring-diagram)
- [Configuration & Thresholds](#configuration--thresholds)
- [Data Flow](#data-flow)
- [Troubleshooting](#troubleshooting)
- [Future Improvements](#future-improvements)
- [Authors](#authors)

---

## Overview

This project implements a **Smart Greenhouse Monitoring System** combined with an **Automatic Fire/Gas Alarm** mechanism. The system is split into two independent subsystems that communicate over **SPI**:

| Subsystem | Platform | Role | Description |
|-----------|----------|------|-------------|
| **Firmware** | STM32F411VET6 | SPI Slave | Reads 4 analog sensors via ADC+DMA, evaluates alarm thresholds, controls buzzer & motor, and packs a 16-byte binary frame for SPI transmission. |
| **Dashboard GUI** | Raspberry Pi 4 | SPI Master | Polls the STM32 at ~50 Hz over SPI, parses incoming frames, and renders a live Tkinter dashboard showing temperature, gas levels, raw ADC values, and actuator states. |

### Key Features

- ⚡ **Bare-metal firmware** — register-level CMSIS (no HAL), maximising transparency and control.
- 🔄 **ADC Scan + DMA circular mode** — 4-channel continuous conversion with zero CPU overhead.
- � **Moving-average filter** — 8-sample sliding window on all ADC channels, reducing noise jitter.
- 🔥 **3-state alarm with hysteresis** — NORMAL → WARN → ALARM state machine, independent for temperature & gas, with separate ON/OFF thresholds to prevent flickering.
- 🔔 **Buzzer beep patterns** — WARN: slow beep ~1 Hz, ALARM: fast beep ~10 Hz, driven by SysTick 1 ms tick.
- 📡 **Custom binary SPI protocol** — 16-byte frame with magic header, XOR checksum, and end-of-frame marker.
- 🖥️ **Real-time GUI** — Python/Tkinter dashboard on Raspberry Pi, updating at 10 Hz.
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
│  │           │ │ (hysteresis)│ │ ON/OFF     │ │            ││
│  └───────────┘ └─────────────┘ └────────────┘ └────────────┘│
├───────────────────────────────────────────────────────────────┤
│  BSP LAYER  (bare-metal register-level CMSIS)                 │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌─────────────────┐ │
│  │ RCC      │ │ GPIO     │ │ SPI1     │ │ ADC1 + DMA2     │ │
│  │ Clock EN │ │ Pin Cfg  │ │ Slave    │ │ Scan + Circular │ │
│  └──────────┘ └──────────┘ └──────────┘ └─────────────────┘ │
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
│                                      └─────────┬─────────┘   │
│  ┌──────────────┐                              │ 16-byte     │
│  │ SysTick 1ms  │ ◀── Actuator_Tick1ms()       │ frame       │
│  │ (prio 3)     │ ──▶ PB0 Buzzer (pattern)     ▼             │
│  └──────────────┘     PB1 Motor  (ON/OFF)  ┌─────────────┐   │
│                                            │ SPI1 Slave  │   │
│                                            │ RXNE IRQ    │   │
│                                            └──────┬──────┘   │
└───────────────────────────────────────────────────┼───────────┘
                                                    │  SPI bus
                                    PA4=NSS ────────┤  (Mode 0)
                                    PA5=SCK ────────┤  (1 MHz)
                                    PA6=MISO ───────┤
                                    PA7=MOSI ───────┤
┌───────────────────────────────────────────────────┼───────────┐
│                Raspberry Pi 4 (SPI Master)         │           │
│                                    ┌──────────────▼────────┐ │
│                                    │  spidev xfer2 @50 Hz  │ │
│                                    └──────────┬────────────┘ │
│                                               │ parse frame  │
│                                    ┌──────────▼────────────┐ │
│                                    │  Tkinter GUI @10 Hz   │ │
│                                    │  Temp, Gas, ADC, State│ │
│                                    └───────────────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

---

## Hardware Requirements

### Components

| # | Component | Quantity | Notes |
|---|-----------|----------|-------|
| 1 | STM32F411VET6 dev board | 1 | e.g. WeAct BlackPill V3.1 or Discovery |
| 2 | Raspberry Pi 4 Model B | 1 | Any RAM variant; Raspberry Pi OS |
| 3 | LM35 Temperature Sensor | 1 | Analog, 10 mV/°C, connected to PA0 |
| 4 | MQ-series Gas Sensor (MQ-2/MQ-5) | 1 | Analog output to PA1 |
| 5 | Soil Moisture / Light / extra analog sensor | 2 | Connected to PA2, PA3 |
| 6 | Active Buzzer | 1 | Driven from PB0 (GPIO push-pull) |
| 7 | DC Motor / Fan | 1 | Driven from PB1 (GPIO push-pull, use MOSFET/relay for high current) |
| 8 | Jumper wires | — | Dupont male-female |
| 9 | Breadboard | 1 | Optional |

### Software / Toolchain

| Tool | Version | Purpose |
|------|---------|---------|
| **Keil µVision 5** | ≥ 5.38 | STM32 firmware IDE, build & flash |
| **STM32F4 DFP** | ≥ 2.6.8 | CMSIS device pack for STM32F411 |
| **Python 3** | ≥ 3.7 | Raspberry Pi GUI runtime |
| **spidev** (pip) | ≥ 3.5 | Python SPI driver |
| **tkinter** | built-in | GUI framework |

---

## Pin Mapping

### STM32F411VET6

| Pin | Function | Peripheral | Description |
|-----|----------|------------|-------------|
| **PA0** | ADC1_IN0 | ADC1 Ch0 | 🌡️ LM35 temperature sensor |
| **PA1** | ADC1_IN1 | ADC1 Ch1 | 💨 Gas sensor (MQ-2/MQ-5) analog out |
| **PA2** | ADC1_IN2 | ADC1 Ch2 | 🌱 Sensor 3 (e.g. soil moisture) |
| **PA3** | ADC1_IN3 | ADC1 Ch3 | 💡 Sensor 4 (e.g. light level) |
| **PA4** | SPI1_NSS | SPI1 AF5 | Chip Select (active low, HW managed) |
| **PA5** | SPI1_SCK | SPI1 AF5 | SPI Clock |
| **PA6** | SPI1_MISO | SPI1 AF5 | Master-In Slave-Out (STM32 → Pi) |
| **PA7** | SPI1_MOSI | SPI1 AF5 | Master-Out Slave-In (Pi → STM32) |
| **PB0** | GPIO OUT | — | 🔔 Buzzer control (push-pull) |
| **PB1** | GPIO OUT | — | ⚙️ Motor/Fan control (push-pull) |

### Raspberry Pi 4 (SPI0)

| Pi Pin (BCM) | Function | Connect to |
|-------------|----------|------------|
| GPIO 8 (CE0) | SPI0_CE0 | STM32 PA4 (NSS) |
| GPIO 11 | SPI0_SCLK | STM32 PA5 (SCK) |
| GPIO 9 | SPI0_MISO | STM32 PA6 (MISO) |
| GPIO 10 | SPI0_MOSI | STM32 PA7 (MOSI) |
| GND | Ground | STM32 GND |

> ⚠️ **Voltage Warning:** STM32F411 GPIO is **3.3 V** tolerant and Raspberry Pi SPI is also **3.3 V** — direct connection is safe. **Do NOT use a 5 V logic level.**

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
 [4]   ADC0_L           1      LM35 raw ADC value (low byte)
 [5]   ADC0_H           1      LM35 raw ADC value (high byte)
 [6]   ADC1_L           1      Gas sensor raw ADC (low byte)
 [7]   ADC1_H           1      Gas sensor raw ADC (high byte)
 [8]   ADC2_L           1      Sensor 3 raw ADC (low byte)
 [9]   ADC2_H           1      Sensor 3 raw ADC (high byte)
[10]   ADC3_L           1      Sensor 4 raw ADC (low byte)
[11]   ADC3_H           1      Sensor 4 raw ADC (high byte)
[12]   TEMP_X10_L       1      Temperature × 10 (low byte) e.g. 325 = 32.5 °C
[13]   TEMP_X10_H       1      Temperature × 10 (high byte)
[14]   XOR_CHECKSUM     1      XOR of bytes [0]–[13]
[15]   END_MARKER       1      0x0D  — end-of-frame
```

### STATUS Byte (bit-field)

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `BUZZER` | 1 = Buzzer ON |
| 1 | `MOTOR` | 1 = Motor/Fan ON |
| 2 | `GAS_ALARM` | 1 = Gas level exceeds threshold |
| 3 | `TEMP_ALARM` | 1 = Temperature exceeds threshold |
| 4–7 | Reserved | 0 |

### SPI Parameters

| Parameter | Value |
|-----------|-------|
| Mode | 0 (CPOL=0, CPHA=0) |
| Speed | 1 MHz |
| Bit order | MSB first |
| Word size | 8 bits |
| NSS | Hardware (active low) |
| Direction | Full-duplex; Master sends 16× `0x00`, Slave returns 16-byte frame |

### Checksum Algorithm

```
XOR of bytes [0] through [13]:

    checksum = 0
    for i in range(14):
        checksum ^= frame[i]
    frame[14] = checksum & 0xFF
```

---

## Repository Structure

```
MiniProject_GreenHouse/
│
├── README.md                       ← You are here
├── gui_spi_greenhouse.py           ← 🐍 Raspberry Pi: Tkinter GUI + SPI master
│
└── STM32_keli_pack/                ← 🔧 Keil µVision project (firmware)
    └── STM32_LIB/
        │
        │  ╔═══ APP LAYER ═══╗
        ├── main.c                  ← Entry point: init → SysTick → WFI sleep loop
        ├── board.h                 ← ALL config: pins, thresholds, hysteresis, timing
        │
        │  ╔═══ SERVICE LAYER ═══╗
        ├── adc_mgr.c/.h        ★NEW  Moving-average filter (8 samples) for ADC
        ├── fire_logic.c/.h     ★NEW  State machine NORMAL→WARN→ALARM + hysteresis
        ├── actuators.c/.h          ← Buzzer beep patterns + Motor ON/OFF by FireState
        ├── greenhouse.c/.h         ← Central logic: filter→alarm→actuator→SPI packet
        │
        │  ╔═══ BSP LAYER (bare-metal CMSIS) ═══╗
        ├── RCC_STM32_LIB.c/.h     ← Clock enable: GPIOA/B, DMA2, ADC1, SPI1
        ├── GPIO.c/.h               ← GPIO config: analog (PA0–3), AF5 (PA4–7), out (PB0–1)
        ├── ADC_DMA_LIB.c           ← ADC1 scan mode + DMA2 Stream0 circular transfer
        ├── ADC_LIB.h               ← ADC register-level type definitions
        ├── DMA_LIB.h               ← DMA register-level type definitions + g_adc_buf
        ├── SPI_LIB.c/.h            ← SPI1 slave init + RXNE IRQ handler (byte-by-byte TX)
        │
        │  ╔═══ REGISTER MAPS (reference) ═══╗
        ├── TIMER.h                 ← TIM register map (reserved for future PWM)
        ├── UART_LIB.h              ← USART register map (reserved for debug)
        │
        │  ╔═══ KEIL PROJECT FILES ═══╗
        ├── stm32f411.uvmpw         ← Keil multi-project workspace file
        ├── DebugConfig/             ← Keil debugger configuration
        ├── Objects/                 ← Build artifacts (.hex, .axf, .o, .d)
        └── RTE/                    ← Keil Run-Time Environment
            ├── _Target_1/
            │   └── RTE_Components.h
            └── Device/STM32F411VETx/
                ├── startup_stm32f411xe.s      ← CMSIS startup (vector table)
                └── system_stm32f4xx.c         ← SystemInit, clock config
```

> 📌 **Note:** The `STM32_keli_pack/` folder is a **standalone Keil µVision project** built and flashed independently onto the STM32. The `gui_spi_greenhouse.py` file runs **separately** on the Raspberry Pi 4 — it only communicates with the STM32 via the SPI bus.
>
> ⚠️ **Keil project update required:** After adding `adc_mgr.c` and `fire_logic.c`, you must add them to the Keil project: **Project → Manage Project Items → Add Existing Files**.

---

## Firmware (STM32) — Build & Flash

### Prerequisites

1. Install **Keil µVision 5** (MDK-ARM) on a Windows PC.
2. Install the **STM32F4xx_DFP** pack (≥ v2.6.8) via Keil Pack Installer.

### Open & Build

1. Open Keil µVision.
2. **File → Open Multi-Project Workspace** → navigate to:
   ```
   STM32_keli_pack/STM32_LIB/stm32f411.uvmpw
   ```
3. In **Project → Options for Target**:
   - **Device tab:** Verify `STM32F411VETx` is selected.
   - **C/C++ tab → Preprocessor Symbols → Define:** ensure `STM32F411xE` is defined.
   - **C/C++ tab → Include Paths:** should include the `STM32_LIB/` folder and CMSIS paths.
4. Press **F7** (Build) or **Project → Build Target**.
5. Verify **0 Errors, 0 Warnings** in the Build Output window.

### Flash

1. Connect the STM32 board via **ST-Link V2** (SWD).
2. In Keil: **Flash → Download** (or press **F8**).
3. The MCU resets and starts running automatically.

### What Happens on Boot

```
main()
  ├── RCC_Enable_For_GPIO_ADC_SPI_DMA()    // Enable clocks: GPIOA/B, DMA2, ADC1, SPI1
  ├── GPIO_Config_ADC_PA0_PA3_Analog()     // PA0–PA3 → Analog mode
  ├── GPIO_Config_SPI1_PA4_PA7_AF5()       // PA4–PA7 → SPI1 AF5
  ├── GPIO_Config_Buzzer_PB0_Output()      // PB0 → Push-pull (Buzzer)
  ├── GPIO_Config_Motor_PB1_Output()       // PB1 → Push-pull (Motor)
  │
  ├── ADC_Mgr_Init()                       // Reset moving-average filter
  ├── FireLogic_Init()                     // State machine → NORMAL
  ├── Actuator_Init()                      // Buzzer OFF, Motor OFF
  │
  ├── SPI1_Slave_Init()                    // SPI1 slave, RXNE IRQ
  ├── Greenhouse_InitPacket()              // Build zero-frame → SPI TX
  ├── ADC1_DMA2_Stream0_InitStart()        // Start continuous ADC scan
  ├── SysTick_Init()                       // 1 ms tick for buzzer patterns
  └── while(1) { __WFI(); }               // Sleep — all interrupt-driven
```

### Interrupt Map

| ISR | Priority | Frequency | Function |
|-----|----------|-----------|----------|
| `DMA2_Stream0` | 1 (highest) | ~continuous | ADC ready → filter → alarm → packet |
| `SPI1` | 2 | per-byte from Pi | Return frame byte to Raspberry Pi |
| `SysTick` | 3 (lowest) | 1 kHz | Buzzer beep pattern timing |

### Interrupt-Driven Data Flow

```
[DMA2_Stream0 Transfer Complete IRQ]  (priority 1)
    │
    ▼
Greenhouse_OnAdcReady()
    ├── ADC_Mgr_FeedSample(g_adc_buf)     ← push into moving-average filter
    ├── temp_x10 = ADC_Mgr_GetTempX10()   ← filtered temperature (0.1°C)
    ├── gas_raw  = ADC_Mgr_GetGasRaw()    ← filtered gas ADC
    ├── FireLogic_Update(temp_x10, gas)    ← state machine with hysteresis
    ├── Actuator_SetState(state)           ← set buzzer/motor target
    ├── Build STATUS byte (buzzer | motor | gas_alarm | temp_alarm)
    ├── build_packet() → fill g_spi_packet[16]
    └── SPI1_Slave_ResetIndex() → Pi reads new frame from byte 0

[SysTick_Handler]  (every 1 ms, priority 3)
    └── Actuator_Tick1ms()
        ├── NORMAL: buzzer OFF, motor OFF
        ├── WARN:   buzzer ON 100ms → OFF 900ms (repeat), motor OFF
        └── ALARM:  buzzer ON 50ms  → OFF 50ms  (repeat), motor ON

[SPI1 RXNE IRQ]  (triggered each time Pi clocks in a byte, priority 2)
    └── Reply g_spi_packet[idx++] via SPI1->DR
```

---

## GUI (Raspberry Pi) — Setup & Run

### Prerequisites

1. **Enable SPI** on the Raspberry Pi:
   ```bash
   sudo raspi-config
   # → Interface Options → SPI → Enable
   ```
   Or add to `/boot/config.txt`:
   ```
   dtparam=spi=on
   ```
   Reboot after enabling.

2. **Install dependencies:**
   ```bash
   sudo apt update
   sudo apt install python3-tk
   pip3 install spidev
   ```

3. **Verify SPI devices exist:**
   ```bash
   ls /dev/spidev*
   # Expected: /dev/spidev0.0  /dev/spidev0.1
   ```

### Run

```bash
python3 gui_spi_greenhouse.py
```

### GUI Features

| Display Element | Source | Description |
|----------------|--------|-------------|
| **Temp: XX.X °C** | `temp_x10 / 10.0` | Real-time temperature from LM35 |
| **ADC0 (LM35 raw)** | `g_adc_buf[0]` | Raw 12-bit ADC value for LM35 |
| **ADC1 (Gas raw)** | `g_adc_buf[1]` | Raw 12-bit ADC value for gas sensor |
| **ADC2 raw** | `g_adc_buf[2]` | Raw 12-bit ADC value for sensor 3 |
| **ADC3 raw** | `g_adc_buf[3]` | Raw 12-bit ADC value for sensor 4 |
| **Status bar** | STATUS byte | SEQ counter, Buzzer state, Motor state, Gas alarm, Temp alarm |

### How It Works

```python
# SPI Master polls STM32 every 20 ms (50 Hz) in a background thread
data = spi.xfer2([0x00] * 16)   # send 16 dummy bytes, receive 16-byte frame

# Frame validation
if data[0]==0xAA and data[1]==0x55 and data[15]==0x0D:
    if data[14] == xor_checksum(data[:14]):
        # Parse fields...

# Tkinter UI refreshes every 100 ms (10 Hz) from main thread
root.after(100, ui_update)
```

---

## Wiring Diagram

```
     STM32F411VET6                          Raspberry Pi 4
    ┌──────────────┐                      ┌──────────────────┐
    │         PA0 ─┤◄── LM35 Vout        │                  │
    │         PA1 ─┤◄── Gas Sensor Aout   │                  │
    │         PA2 ─┤◄── Sensor 3          │                  │
    │         PA3 ─┤◄── Sensor 4          │                  │
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
    │         PB1 ─┤──▶ Motor Driver (IN)
    │         GND ─┤──▶ Buzzer (−) / Motor GND
    └──────────────┘
```

> 💡 **Tip:** For the motor, use a transistor (e.g. 2N2222) or MOSFET with a flyback diode. Do **not** drive motors directly from STM32 GPIO — max sink/source is ~25 mA.

---

## Configuration & Thresholds

All tunable parameters are centralized in [STM32_keli_pack/STM32_LIB/board.h](STM32_keli_pack/STM32_LIB/board.h):

### Alarm Thresholds (Hysteresis)

| Macro | Default | Unit | Description |
|-------|---------|------|-------------|
| `TEMP_WARN_ON_X10` | `350` | 0.1 °C | ≥ 35.0 °C → enter WARN |
| `TEMP_WARN_OFF_X10` | `330` | 0.1 °C | ≤ 33.0 °C → exit WARN |
| `TEMP_ALARM_ON_X10` | `500` | 0.1 °C | ≥ 50.0 °C → enter ALARM |
| `TEMP_ALARM_OFF_X10` | `450` | 0.1 °C | ≤ 45.0 °C → exit ALARM |
| `GAS_WARN_ON_ADC` | `2000` | raw (0–4095) | ≥ 2000 → enter WARN |
| `GAS_WARN_OFF_ADC` | `1800` | raw (0–4095) | ≤ 1800 → exit WARN |
| `GAS_ALARM_ON_ADC` | `2500` | raw (0–4095) | ≥ 2500 → enter ALARM |
| `GAS_ALARM_OFF_ADC` | `2300` | raw (0–4095) | ≤ 2300 → exit ALARM |

### Buzzer Patterns

| Macro | Default | Description |
|-------|---------|-------------|
| `BUZZER_WARN_ON_MS` | `100` | WARN: buzzer ON duration (ms) |
| `BUZZER_WARN_OFF_MS` | `900` | WARN: buzzer OFF duration → ~1 Hz |
| `BUZZER_ALARM_ON_MS` | `50` | ALARM: buzzer ON duration (ms) |
| `BUZZER_ALARM_OFF_MS` | `50` | ALARM: buzzer OFF duration → ~10 Hz |

### Other Parameters

| Macro | Default | Unit | Description |
|-------|---------|------|-------------|
| `ADC_FILTER_SAMPLES` | `8` | samples | Moving-average window size |
| `PACKET_LEN` | `16` | bytes | SPI frame length |
| `SYS_CLOCK_HZ` | `16000000` | Hz | System clock (HSI default) |
| `ADC_VREF_MV` | `3300` | mV | ADC reference voltage |

### LM35 Temperature Calculation

```
Vref = 3.3 V,  ADC resolution = 12-bit (0–4095)
LM35 output = 10 mV/°C

temp_mV   = adc_filtered × 3300 / 4095     (after moving-average filter)
temp_x10  = temp_mV           (since 10 mV = 0.1 °C → mV value = temp × 10)
temp_°C   = temp_x10 / 10.0
```

**Example:** ADC filtered = `620` → `620 × 3300 / 4095 ≈ 499 mV` → `49.9 °C`

---

## Data Flow

```
                    ┌──────────────────────────────────────────┐
                    │     STM32F411VET6  (interrupt-driven)     │
                    │                                          │
 ┌──────────┐      │  ┌────────┐ TC  ┌──────────┐  filtered   │
 │ 4× Analog│ ADC  │  │ DMA2   │────▶│ adc_mgr  │─────┐      │
 │ Sensors  │─────▶│  │Stream0 │ IRQ │ (moving  │     │      │
 │          │ scan │  │(circ.) │     │ average) │     │      │
 └──────────┘      │  └────────┘     └──────────┘     │      │
                    │                                  ▼      │
                    │                          ┌──────────────┐│
                    │                          │ fire_logic   ││
                    │                          │ (hysteresis  ││
                    │        SysTick 1ms       │  state mach.)││
                    │       ┌──────────┐       └──────┬───────┘│
                    │       │actuators │◀─────────────┘ state  │
                    │ PB0◀──│ pattern  │                       │
                    │ PB1◀──│ + motor  │  ┌──────────────┐     │
                    │       └──────────┘  │ greenhouse.c │     │
                    │                     │ build_packet │     │
                    │                     └──────┬───────┘     │
                    │                            │ 16-byte     │
                    │                     ┌──────▼───────┐     │
                    │                     │ SPI1 Slave   │     │
                    │                     │ RXNE → TX DR │     │
                    │                     └──────┬───────┘     │
                    └────────────────────────────┼─────────────┘
                                                 │ SPI bus
                    ┌────────────────────────────┼─────────────┐
                    │       Raspberry Pi 4       │             │
                    │                     ┌──────▼──────┐      │
                    │                     │ spidev poll │      │
                    │                     │ @ 50 Hz     │      │
                    │                     └──────┬──────┘      │
                    │                     ┌──────▼──────┐      │
                    │                     │ Tkinter GUI │      │
                    │                     │ @ 10 Hz     │      │
                    │                     └─────────────┘      │
                    └──────────────────────────────────────────┘
```

---

## Troubleshooting

### SPI Communication Issues

| Symptom | Possible Cause | Fix |
|---------|---------------|-----|
| GUI shows `--.-` forever | SPI not enabled on Pi | Run `sudo raspi-config` → enable SPI |
| All ADC values = 0 | Wiring issue or STM32 not flashed | Check power, re-flash firmware |
| Checksum mismatch | Clock speed too high / noise | Reduce SPI speed: change `hz=1_000_000` to `500_000` |
| Frame misaligned (magic ≠ 0xAA 0x55) | NSS not wired or floating | Ensure PA4 ↔ CE0 connected; check pull-down |
| Intermittent data corruption | CPOL/CPHA mismatch | Both sides must be Mode 0 (`CPOL=0, CPHA=0`) |
| `Permission denied` on `/dev/spidev0.0` | User not in spi group | `sudo usermod -aG spi $USER` then reboot |

### STM32 Firmware Issues

| Symptom | Possible Cause | Fix |
|---------|---------------|-----|
| Build fails: "undefined SPI1" | Missing CMSIS device header | Ensure `STM32F411xE` is defined in Keil preprocessor |
| ADC reads constant value | Wrong GPIO mode | Verify PA0–PA3 set to Analog mode (`MODER = 0b11`) |
| Buzzer never triggers | Threshold too high | Lower `GAS_ALARM_ADC` in `board.h` |
| Motor always ON | Temp threshold too low | Raise `TEMP_ALARM_X10` in `board.h` |
| HardFault on boot | Stack overflow or bad ISR | Check startup file and vector table alignment |
| Build: "undefined ADC_Mgr_Init" | New .c files not in Keil project | Project → Manage Items → Add `adc_mgr.c`, `fire_logic.c` |
| Alarm flickering ON/OFF | Hysteresis gap too small | Increase gap between `_ON` and `_OFF` thresholds in `board.h` |

### Debugging Tips

1. **Logic Analyzer on SPI:**
   Connect a logic analyzer (e.g. Saleae) to PA4–PA7. Verify:
   - NSS goes LOW before SCK toggles.
   - 16 bytes clocked per transaction.
   - MISO data matches expected packet.

2. **Keil Debugger:**
   - Set breakpoint in `Greenhouse_OnAdcReady()` — inspect `g_adc_buf[]`.
   - Watch `g_spi_packet[]` in Memory window.
   - Check `SPI1->SR` for OVR (overrun) flag.

3. **Pi-side quick test (without GUI):**
   ```python
   import spidev
   spi = spidev.SpiDev()
   spi.open(0, 0)
   spi.max_speed_hz = 1_000_000
   spi.mode = 0b00
   data = spi.xfer2([0x00]*16)
   print([hex(b) for b in data])
   spi.close()
   ```

---

## Future Improvements

- [ ] **PWM-driven actuators** — Replace GPIO on/off with TIM-based PWM for variable buzzer tone and motor speed control.
- [x] **~~Hysteresis on alarms~~** — ✅ Done: 3-state machine (NORMAL/WARN/ALARM) with separate ON/OFF thresholds.
- [x] **~~Moving average filter~~** — ✅ Done: 8-sample sliding window on all ADC channels.
- [x] **~~Buzzer beep patterns~~** — ✅ Done: WARN ~1 Hz, ALARM ~10 Hz via SysTick 1ms.
- [ ] **CRC-16 checksum** — Upgrade from XOR to CRC-16-CCITT for stronger error detection.
- [ ] **Extended frame protocol** — Add version, flags, and variable-length payload fields.
- [ ] **MQTT / Wi-Fi bridge** — Forward data from Pi to a cloud dashboard (e.g. ThingsBoard, Grafana).
- [ ] **UART debug output** — Print sensor data over serial for development without Pi.
- [ ] **GUI enhancements** — Add live charts (matplotlib), alarm history log, and configuration panel.
- [ ] **Double-buffer SPI TX** — Build new frame on separate buffer, atomic pointer swap.
- [ ] **Watchdog timer** — Add IWDG to auto-reset on firmware hang.

---

## Authors

| Name | Role |
|------|------|
| **Thanh** | Embedded Firmware & System Integration |

---

<p align="center">
  <i>Built with ❤️ — STM32 bare-metal + Raspberry Pi</i>
</p>
