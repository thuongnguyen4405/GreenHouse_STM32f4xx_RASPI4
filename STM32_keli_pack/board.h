#ifndef _BOARD_H_
#define _BOARD_H_

#include <stdint.h>
#include "stm32f4xx.h"

/*╔═══════════════════════════════════════════════════════════╗
 *║  board.h – SINGLE SOURCE OF TRUTH                        ║
 *║  Project : Smart Greenhouse + Automatic Fire Alarm        ║
 *║  MCU     : STM32F411VET6 (Cortex-M4F)                    ║
 *║  IDE     : Keil µVision 5 (CMSIS bare-metal, no HAL)      ║
 *║                                                           ║
 *║  Every magic number, pin assignment, threshold, and SPI   ║
 *║  protocol constant lives HERE.  The Python GUI on the     ║
 *║  Raspberry Pi mirrors these values — change here first,   ║
 *║  then update gui_spi_greenhouse.py to match.              ║
 *║                                                           ║
 *║  Sections:                                                ║
 *║   1. System Clock                                         ║
 *║   2. Pin Map (GPIO)                                       ║
 *║   3. ADC Channel Map & Conversion                         ║
 *║   4. ADC Filter                                           ║
 *║   5. Alarm Thresholds (Hysteresis)                        ║
 *║   6. Buzzer Beep Patterns                                 ║
 *║   7. SPI Protocol Specification  ← SHARED WITH PYTHON    ║
 *║   8. NVIC Interrupt Priorities                            ║
 *╚═══════════════════════════════════════════════════════════╝*/

/* ╔═══════════════════════════════════════════════════════╗
 * ║  1. SYSTEM CLOCK                                      ║
 * ╚═══════════════════════════════════════════════════════╝
 * Default: STM32F411 boots on HSI = 16 MHz (no PLL).
 * If PLL is configured in system_stm32f4xx.c, update here.
 */
#define SYS_CLOCK_HZ          16000000UL
#define SYSTICK_FREQ_HZ       1000U        /* SysTick → 1 ms tick  */

/* ╔═══════════════════════════════════════════════════════╗
 * ║  2. PIN MAP (GPIO)                                    ║
 * ╠═══════════════════════════════════════════════════════╣
 * ║  Pin  │ Function        │ Peripheral  │ Description   ║
 * ║───────┼─────────────────┼─────────────┼───────────────║
 * ║  PA0  │ ADC1_IN0        │ ADC1 Ch0    │ LM35 (temp)   ║
 * ║  PA1  │ ADC1_IN1        │ ADC1 Ch1    │ MQ-2 (gas)    ║
 * ║  PA2  │ ADC1_IN2        │ ADC1 Ch2    │ Soil moisture ║
 * ║  PA3  │ ADC1_IN3        │ ADC1 Ch3    │ Light sensor  ║
 * ║  PA4  │ SPI1_NSS (AF5)  │ SPI1        │ Chip-select   ║
 * ║  PA5  │ SPI1_SCK (AF5)  │ SPI1        │ SPI clock     ║
 * ║  PA6  │ SPI1_MISO (AF5) │ SPI1        │ STM32 → Pi    ║
 * ║  PA7  │ SPI1_MOSI (AF5) │ SPI1        │ Pi → STM32    ║
 * ║  PB0  │ GPIO OUT PP     │ —           │ Buzzer        ║
 * ║  PB1  │ GPIO OUT PP     │ —           │ Motor / Fan   ║
 * ╚═══════════════════════════════════════════════════════╝
 *
 * Raspberry Pi 4 SPI0 wiring:
 *   GPIO8  (CE0)  ↔ PA4 (NSS)     3.3V logic, direct connect
 *   GPIO11 (SCLK) ↔ PA5 (SCK)     3.3V logic, direct connect
 *   GPIO9  (MISO) ↔ PA6 (MISO)    3.3V logic, direct connect
 *   GPIO10 (MOSI) ↔ PA7 (MOSI)    3.3V logic, direct connect
 *   GND           ↔ GND           common ground REQUIRED
 */

/* GPIO pin numbers (used in GPIO.c bit-shift macros) */
#define PIN_ADC_LM35          0U   /* PA0 */
#define PIN_ADC_GAS           1U   /* PA1 */
#define PIN_ADC_S3            2U   /* PA2 */
#define PIN_ADC_S4            3U   /* PA3 */
#define PIN_SPI_NSS           4U   /* PA4 */
#define PIN_SPI_SCK           5U   /* PA5 */
#define PIN_SPI_MISO          6U   /* PA6 */
#define PIN_SPI_MOSI          7U   /* PA7 */
#define PIN_BUZZER            0U   /* PB0 */
#define PIN_MOTOR             1U   /* PB1 */

/* SPI1 Alternate Function index on STM32F411 */
#define SPI1_AF               5U   /* AF5 for PA4-PA7 */

/* ╔═══════════════════════════════════════════════════════╗
 * ║  3. ADC CHANNEL MAP & CONVERSION                      ║
 * ╚═══════════════════════════════════════════════════════╝*/

#define ADC_NUM_CHANNELS      4

/* Index into g_adc_buf[] (DMA scan sequence order) */
#define ADC_IDX_LM35          0
#define ADC_IDX_GAS           1
#define ADC_IDX_S3            2    /* soil moisture           */
#define ADC_IDX_S4            3    /* light level             */

/* ADC1 input channel number (INx) — determines SQR3 sequence */
#define ADC_CH_LM35           0    /* PA0 = IN0 */
#define ADC_CH_GAS            1    /* PA1 = IN1 */
#define ADC_CH_S3             2    /* PA2 = IN2 */
#define ADC_CH_S4             3    /* PA3 = IN3 */

/* ADC sample time selection (written to SMPR2)
 * STM32F411 options:  0→3cy  1→15cy  2→28cy  3→56cy
 *                     4→84cy 5→112cy 6→144cy 7→480cy
 * 84 cycles (value=4) is a good balance for analog sensors.
 */
#define ADC_SAMPLE_TIME_SEL   4U   /* 84 cycles per channel   */

/* LM35 temperature conversion:
 *   voltage_mV = adc_raw × Vref_mV / (2^12 - 1)
 *   LM35: 10 mV/°C → 1 mV = 0.1°C → voltage_mV = temp_x10
 *   Example: ADC=620 → 620×3300/4095 ≈ 499 mV → 49.9°C
 */
#define ADC_VREF_MV           3300U
#define ADC_RESOLUTION        4095U  /* 12-bit: 2^12 - 1       */

/* ╔═══════════════════════════════════════════════════════╗
 * ║  4. ADC FILTER                                        ║
 * ╚═══════════════════════════════════════════════════════╝
 * Moving-average window size.  Larger → smoother but slower.
 * 8 is a good balance for analog sensor noise.
 */
#define ADC_FILTER_SAMPLES    8

/* ╔═══════════════════════════════════════════════════════╗
 * ║  5. ALARM THRESHOLDS (Hysteresis)                     ║
 * ╠═══════════════════════════════════════════════════════╣
 * ║                                                       ║
 * ║  NORMAL ──[≥ WARN_ON]──▶ WARN ──[≥ ALARM_ON]──▶ ALARM║
 * ║    ▲                       │                      │   ║
 * ║    └───[≤ WARN_OFF]───────┘                      │   ║
 * ║                            ▲                      │   ║
 * ║                            └───[≤ ALARM_OFF]──────┘   ║
 * ║                                                       ║
 * ║  Hysteresis prevents alarm flickering when the value  ║
 * ║  oscillates around a single threshold.                ║
 * ╚═══════════════════════════════════════════════════════╝
 *
 * Python GUI mirrors these thresholds for colour display:
 *   TEMP_WARN_ON  = 35.0  (board.h 350 / 10.0)
 *   TEMP_ALARM_ON = 50.0  (board.h 500 / 10.0)
 *   GAS_WARN_ON   = 2000  (same unit)
 *   GAS_ALARM_ON  = 2500  (same unit)
 */

/* Temperature thresholds (× 10, unit = 0.1°C) */
#define TEMP_WARN_ON_X10      350U    /* ≥ 35.0°C → enter WARN    */
#define TEMP_WARN_OFF_X10     330U    /* ≤ 33.0°C → exit  WARN    */
#define TEMP_ALARM_ON_X10     500U    /* ≥ 50.0°C → enter ALARM   */
#define TEMP_ALARM_OFF_X10    450U    /* ≤ 45.0°C → exit  ALARM   */

/* Gas sensor thresholds (raw ADC, 12-bit 0–4095) */
#define GAS_WARN_ON_ADC       2000U   /* ≥ 2000 → enter WARN      */
#define GAS_WARN_OFF_ADC      1800U   /* ≤ 1800 → exit  WARN      */
#define GAS_ALARM_ON_ADC      2500U   /* ≥ 2500 → enter ALARM     */
#define GAS_ALARM_OFF_ADC     2300U   /* ≤ 2300 → exit  ALARM     */

/* Legacy aliases (backward compatibility) */
#define TEMP_ALARM_X10        TEMP_ALARM_ON_X10
#define GAS_ALARM_ADC         GAS_ALARM_ON_ADC

/* ╔═══════════════════════════════════════════════════════╗
 * ║  6. BUZZER BEEP PATTERNS (milliseconds)               ║
 * ╚═══════════════════════════════════════════════════════╝
 * NORMAL : OFF completely
 * WARN   : slow beep  ~1 Hz  (100 ms ON, 900 ms OFF)
 * ALARM  : fast beep ~10 Hz  ( 50 ms ON,  50 ms OFF)
 */
#define BUZZER_WARN_ON_MS     100U
#define BUZZER_WARN_OFF_MS    900U
#define BUZZER_ALARM_ON_MS    50U
#define BUZZER_ALARM_OFF_MS   50U

/* ╔═══════════════════════════════════════════════════════╗
 * ║  7. SPI PROTOCOL SPECIFICATION                        ║
 * ║     ──────────────────────────────────────────────     ║
 * ║     THIS SECTION IS THE CONTRACT BETWEEN STM32 AND    ║
 * ║     THE RASPBERRY PI PYTHON GUI.  Both sides MUST     ║
 * ║     agree on every value here.                        ║
 * ╚═══════════════════════════════════════════════════════╝
 *
 * SPI Bus Parameters (Pi = Master, STM32 = Slave):
 *   Mode    : 0 (CPOL=0, CPHA=0)
 *   Speed   : 1 MHz
 *   Bit     : MSB first
 *   Word    : 8-bit
 *   NSS     : Hardware, active-low
 *   Transfer: Full-duplex; Pi sends 16× 0x00, STM32 returns frame
 *
 * ┌──────┬────────────────┬──────┬──────────────────────────────┐
 * │ Byte │ Field          │ Size │ Description                  │
 * ├──────┼────────────────┼──────┼──────────────────────────────┤
 * │  [0] │ MAGIC_0        │  1   │ 0xAA  start-of-frame        │
 * │  [1] │ MAGIC_1        │  1   │ 0x55  start-of-frame        │
 * │  [2] │ SEQ            │  1   │ Sequence counter (0–255)     │
 * │  [3] │ STATUS         │  1   │ Bit-field (see below)        │
 * │ [4-5]│ ADC0 (LM35)    │  2   │ uint16 LE – raw ADC         │
 * │ [6-7]│ ADC1 (Gas)     │  2   │ uint16 LE – raw ADC         │
 * │ [8-9]│ ADC2 (Soil)    │  2   │ uint16 LE – raw ADC         │
 * │[10-11│ ADC3 (Light)   │  2   │ uint16 LE – raw ADC         │
 * │[12-13│ TEMP_X10       │  2   │ uint16 LE – temp × 10       │
 * │ [14] │ XOR_CHECKSUM   │  1   │ XOR of bytes [0..13]        │
 * │ [15] │ END_MARKER     │  1   │ 0x0D  end-of-frame          │
 * └──────┴────────────────┴──────┴──────────────────────────────┘
 *
 * STATUS byte bit-field:
 *   Bit 0 : BUZZER     1 = buzzer currently ON
 *   Bit 1 : MOTOR      1 = motor / fan currently ON
 *   Bit 2 : GAS_ALARM  1 = gas level in WARN or ALARM
 *   Bit 3 : TEMP_ALARM 1 = temperature in WARN or ALARM
 *   Bit 4-7: reserved (0)
 *
 * Checksum algorithm:
 *   cs = 0; for (i=0; i<14; i++) cs ^= frame[i]; frame[14] = cs;
 */

/* Frame geometry */
#define PACKET_LEN            16

/* Magic bytes (start-of-frame) */
#define FRAME_MAGIC_0         0xAAU
#define FRAME_MAGIC_1         0x55U
#define FRAME_END_MARKER      0x0DU

/* Byte offsets inside the 16-byte frame */
#define FRAME_OFF_MAGIC0      0
#define FRAME_OFF_MAGIC1      1
#define FRAME_OFF_SEQ         2
#define FRAME_OFF_STATUS      3
#define FRAME_OFF_ADC0_L      4
#define FRAME_OFF_ADC0_H      5
#define FRAME_OFF_ADC1_L      6
#define FRAME_OFF_ADC1_H      7
#define FRAME_OFF_ADC2_L      8
#define FRAME_OFF_ADC2_H      9
#define FRAME_OFF_ADC3_L      10
#define FRAME_OFF_ADC3_H      11
#define FRAME_OFF_TEMP_L      12
#define FRAME_OFF_TEMP_H      13
#define FRAME_OFF_XOR         14
#define FRAME_OFF_END         15

/* STATUS byte bit positions */
#define STATUS_BIT_BUZZER     0
#define STATUS_BIT_MOTOR      1
#define STATUS_BIT_GAS_ALARM  2
#define STATUS_BIT_TEMP_ALARM 3

/* SPI bus parameters (must match Python spidev config) */
#define SPI_CLOCK_HZ          1000000UL  /* 1 MHz                  */
#define SPI_CPOL              0          /* clock polarity          */
#define SPI_CPHA              0          /* clock phase             */

/* ╔═══════════════════════════════════════════════════════╗
 * ║  8. NVIC INTERRUPT PRIORITIES                         ║
 * ╠═══════════════════════════════════════════════════════╣
 * ║  Lower number = higher priority (0 = highest, Cortex-M4)    ║
 * ║                                                       ║
 * ║  DMA (ADC data ready) : prio 1 (highest, packet fresh)║
 * ║  SPI (slave TX/RX)    : prio 2 (middle)               ║
 * ║  SysTick (1ms tick)   : prio 3 (lowest, buzzer only)  ║
 * ║                                                       ║
 * ║  DMA > SPI ensures build_packet() completes before    ║
 * ║  SPI can send any byte → no partial frame.            ║
 * ╚═══════════════════════════════════════════════════════╝*/
#define IRQ_PRIO_DMA_ADC      1
#define IRQ_PRIO_SPI          2
#define IRQ_PRIO_SYSTICK      3

#endif /* _BOARD_H_ */
