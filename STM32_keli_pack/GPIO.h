#ifndef _GPIO_H
#define _GPIO_H

#include "stm32f4xx.h"

/*============================================================
 *  GPIO.h – GPIO Register Map & Pin Configuration API
 *  MCU : STM32F411VET6 (AHB1 bus)
 *
 *  Pin assignments are defined in board.h (Section 2).
 *  This module only configures the registers — it does not
 *  contain any pin number decisions.
 *============================================================*/

/* ═══════════ GPIO Base Addresses (AHB1) ═══════════ */
#define GPIOA_BASE_ADDR   (0x40020000UL)
#define GPIOB_BASE_ADDR   (0x40020400UL)
#define GPIOC_BASE_ADDR   (0x40020800UL)
#define GPIOD_BASE_ADDR   (0x40020C00UL)
#define GPIOE_BASE_ADDR   (0x40021000UL)
#define GPIOH_BASE_ADDR   (0x40021C00UL)

/*─────────────────────────────────────────────────────────
 * GPIO register map  (RM0383 §8.4, 40 bytes per port)
 *
 * Offset  Register   Bits/pin  Description
 * ──────  ─────────  ────────  ──────────────────────────
 * 0x00    MODER      2         Mode: 00=Input 01=Output
 *                               10=AF 11=Analog
 * 0x04    OTYPER     1         0=Push-pull  1=Open-drain
 * 0x08    OSPEEDR    2         00=Low 01=Med 10=Fast 11=VHi
 * 0x0C    PUPDR      2         00=None 01=PU 10=PD
 * 0x10    IDR        1 (RO)    Input data
 * 0x14    ODR        1         Output data
 * 0x18    BSRR       1+1       Atomic set(low) / reset(high)
 * 0x1C    LCKR       1         Lock configuration
 * 0x20    AFRL       4         Alternate function pin 0-7
 * 0x24    AFRH       4         Alternate function pin 8-15
 *─────────────────────────────────────────────────────────*/
typedef struct
{
    volatile uint32_t MODER;    /* 0x00  port mode             */
    volatile uint32_t OTYPER;   /* 0x04  output type           */
    volatile uint32_t OSPEEDR;  /* 0x08  output speed          */
    volatile uint32_t PUPDR;    /* 0x0C  pull-up / pull-down   */
    volatile uint32_t IDR;      /* 0x10  input data (read-only)*/
    volatile uint32_t ODR;      /* 0x14  output data           */
    volatile uint32_t BSRR;     /* 0x18  bit set/reset         */
    volatile uint32_t LCKR;     /* 0x1C  lock                  */
    volatile uint32_t AFRL;     /* 0x20  AF select pin 0-7     */
    volatile uint32_t AFRH;     /* 0x24  AF select pin 8-15    */
} GPIO_TypeDef_Mini;

#define GPIOA   ((GPIO_TypeDef_Mini *) GPIOA_BASE_ADDR)
#define GPIOB   ((GPIO_TypeDef_Mini *) GPIOB_BASE_ADDR)
#define GPIOC   ((GPIO_TypeDef_Mini *) GPIOC_BASE_ADDR)
#define GPIOD   ((GPIO_TypeDef_Mini *) GPIOD_BASE_ADDR)
#define GPIOH   ((GPIO_TypeDef_Mini *) GPIOH_BASE_ADDR)

/* ═══════════ Configuration Functions ═══════════
 * Each function configures the exact pins listed in
 * board.h Section 2 (Pin Map).  Call AFTER RCC enable.
 */

/* PA0-PA3 → Analog mode (ADC input, no pull) */
void GPIO_Config_ADC_PA0_PA3_Analog(void);

/* PA4-PA7 → AF5 (SPI1: NSS, SCK, MISO, MOSI) */
void GPIO_Config_SPI1_PA4_PA7_AF5(void);

/* PB0 → Push-pull output (Buzzer control) */
void GPIO_Config_Buzzer_PB0_Output(void);

/* PB1 → Push-pull output (Motor / Fan control) */
void GPIO_Config_Motor_PB1_Output(void);

#endif /* _GPIO_H */
