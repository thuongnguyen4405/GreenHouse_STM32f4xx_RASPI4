#ifndef _BOARD_H_
#define _BOARD_H_

#include <stdint.h>
#include "stm32f4xx.h"

/*============================================================
 *  board.h – C?u hình ph?n c?ng & h?ng s? h? th?ng
 *  Project : Smart Greenhouse + Automatic Fire Alarm
 *  MCU     : STM32F411VET6 (Cortex-M4F)
 *  IDE     : Keil µVision 5 (CMSIS bare-metal, không HAL)
 *
 *  File này t?p trung T?T C? magic number & pin mapping,
 *  giúp thay d?i c?u hình d? dàng mà không s?a logic code.
 *============================================================*/

/* ----------- System Clock -----------
 * M?c d?nh STM32F411 boot v?i HSI = 16 MHz (chua c?u hình PLL).
 * N?u dã c?u hình PLL trong system_stm32f4xx.c, s?a giá tr? này.
 */
#define SYS_CLOCK_HZ         16000000UL
#define SYSTICK_FREQ_HZ       1000U        /* SysTick ? 1 ms tick */

/* ----------- ADC Channel Map -----------
 * Pin   ADC Channel    Sensor
 * PA0 ? ADC1_IN0     : LM35 (nhi?t d?, 10 mV/°C)
 * PA1 ? ADC1_IN1     : Gas sensor (MQ-2 / MQ-5, analog out)
 * PA2 ? ADC1_IN2     : Sensor 3 (soil moisture / humidity)
 * PA3 ? ADC1_IN3     : Sensor 4 (light level / extra)
 */
#define ADC_NUM_CHANNELS      4

/* Index trong g_adc_buf[] (th? t? scan sequence) */
#define ADC_IDX_LM35          0
#define ADC_IDX_GAS           1
#define ADC_IDX_S3            2
#define ADC_IDX_S4            3

/* ADC1 input channel number (INx) */
#define ADC_CH_LM35           0    /* PA0 = IN0 */
#define ADC_CH_GAS            1    /* PA1 = IN1 */
#define ADC_CH_S3             2    /* PA2 = IN2 */
#define ADC_CH_S4             3    /* PA3 = IN3 */

/* ----------- ADC Filter -----------
 * Moving average window. S? m?u càng l?n ? càng mu?t, nhung
 * ph?n h?i ch?m hon. 8 m?u là cân b?ng t?t cho sensor analog.
 */
#define ADC_FILTER_SAMPLES    8

/* ----------- LM35 Temperature Conversion -----------
 * LM35: 10 mV/°C,  Vref = 3.3 V,  12-bit ADC (0–4095)
 *
 *   voltage_mV = adc_raw × 3300 / 4095
 *   temp_x10   = voltage_mV   (vì 10 mV = 1°C ? 1 mV = 0.1°C)
 *
 * Ví d?: ADC = 620 ? 620 × 3300 / 4095 ˜ 499 mV ? 49.9°C
 */
#define ADC_VREF_MV           3300U
#define ADC_RESOLUTION        4095U

/* ----------- Alarm Thresholds (Hysteresis) -----------
 *
 * *** HYSTERESIS LÀ GÌ? ***
 * N?u ch? dùng 1 ngu?ng (ví d? 35°C), khi nhi?t d? dao d?ng
 * quanh 35°C (34.9 ? 35.1 ? 34.8 ? 35.2...), alarm s? b?t/t?t
 * liên t?c (flickering). Hysteresis gi?i quy?t b?ng cách dùng
 * 2 ngu?ng: ON cao hon OFF.
 *
 * So d? chuy?n tr?ng thái:
 *
 *   NORMAL --[= WARN_ON]--? WARN --[= ALARM_ON]--? ALARM
 *     ?                       ¦                       ¦
 *     +---[= WARN_OFF]-------+                       ¦
 *                             ?                       ¦
 *                             +---[= ALARM_OFF]-------+
 *
 * Ðon v? temperature: × 10 (0.1°C). Ví d? 350 = 35.0°C
 * Ðon v? gas: raw ADC (0–4095)
 */

/* Temperature thresholds (× 10, don v? 0.1°C) */
#define TEMP_WARN_ON_X10      350U    /* = 35.0°C ? vào WARN     */
#define TEMP_WARN_OFF_X10     330U    /* = 33.0°C ? thoát WARN   */
#define TEMP_ALARM_ON_X10     500U    /* = 50.0°C ? vào ALARM    */
#define TEMP_ALARM_OFF_X10    450U    /* = 45.0°C ? thoát ALARM  */

/* Gas sensor thresholds (raw ADC 12-bit) */
#define GAS_WARN_ON_ADC       2000U   /* = 2000 ? vào WARN       */
#define GAS_WARN_OFF_ADC      1800U   /* = 1800 ? thoát WARN     */
#define GAS_ALARM_ON_ADC      2500U   /* = 2500 ? vào ALARM      */
#define GAS_ALARM_OFF_ADC     2300U   /* = 2300 ? thoát ALARM    */

/* Backward-compatible aliases (dùng b?i code cu / GUI Python) */
#define TEMP_ALARM_X10        TEMP_ALARM_ON_X10
#define GAS_ALARM_ADC         GAS_ALARM_ON_ADC

/* ----------- SPI Protocol -----------
 * Frame 16 bytes: [AA 55] [SEQ] [STATUS] [ADC×4] [TEMP] [XOR] [0D]
 */
#define PACKET_LEN            16

/* ----------- Buzzer Beep Patterns (ms) -----------
 * NORMAL : t?t hoàn toàn
 * WARN   : beep ch?m ~1 Hz  ? 100ms ON, 900ms OFF
 * ALARM  : beep nhanh ~10 Hz ? 50ms ON, 50ms OFF
 */
#define BUZZER_WARN_ON_MS     100U
#define BUZZER_WARN_OFF_MS    900U
#define BUZZER_ALARM_ON_MS    50U
#define BUZZER_ALARM_OFF_MS   50U

/* ----------- NVIC Interrupt Priority -----------
 * S? nh? = uu tiên cao hon (0 = highest trên Cortex-M4)
 *
 * DMA (ADC data ready) : prio 1 (cao nh?t ? packet luôn up-to-date)
 * SPI (slave TX/RX)    : prio 2 (trung bình)
 * SysTick (1ms tick)   : prio 3 (th?p nh?t ? không block ADC/SPI)
 */
#define IRQ_PRIO_DMA_ADC      1
#define IRQ_PRIO_SPI          2
#define IRQ_PRIO_SYSTICK      3

#endif /* _BOARD_H_ */
