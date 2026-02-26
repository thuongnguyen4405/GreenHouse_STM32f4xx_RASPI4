#ifndef _GPIO_H
#define _GPIO_H

#include "stm32f4xx.h"
/* =========================
 * GPIO base address (STM32F411, AHB1)
 * ========================= */
#define GPIOA_BASE_ADDR   (0x40020000UL)
#define GPIOB_BASE_ADDR   (0x40020400UL)
#define GPIOC_BASE_ADDR   (0x40020800UL)
#define GPIOD_BASE_ADDR   (0x40020C00UL)
#define GPIOE_BASE_ADDR   (0x40021000UL)
#define GPIOH_BASE_ADDR   (0x40021C00UL)


/* =========================
 * GPIO register map (Table 26)
 * =========================
 * 0x00 MODER
 * 0x04 OTYPER
 * 0x08 OSPEEDR
 * 0x0C PUPDR
 * 0x10 IDR
 * 0x14 ODR
 * 0x18 BSRR
 * 0x1C LCKR
 * 0x20 AFRL
 * 0x24 AFRH
 */
typedef struct
{
    volatile uint32_t MODER;    /* 0x00 */
    volatile uint32_t OTYPER;   /* 0x04 */
    volatile uint32_t OSPEEDR;  /* 0x08 */
    volatile uint32_t PUPDR;    /* 0x0C */
    volatile uint32_t IDR;      /* 0x10 */
    volatile uint32_t ODR;      /* 0x14 */
    volatile uint32_t BSRR;     /* 0x18 */
    volatile uint32_t LCKR;     /* 0x1C */
    volatile uint32_t AFRL;     /* 0x20 */
    volatile uint32_t AFRH;     /* 0x24 */
} GPIO_TypeDef_Mini;
#define GPIOA   ((GPIO_TypeDef_Mini *)GPIOA_BASE_ADDR)
#define GPIOB   ((GPIO_TypeDef_Mini *)GPIOB_BASE_ADDR)
#define GPIOC   ((GPIO_TypeDef_Mini *)GPIOC_BASE_ADDR)
#define GPIOD   ((GPIO_TypeDef_Mini *)GPIOE_BASE_ADDR)
#define GPIOH   ((GPIO_TypeDef_Mini *)GPIOH_BASE_ADDR)

//function config 

void GPIO_Config_ADC_PA0_PA3_Analog(void);
void GPIO_Config_SPI1_PA4_PA7_AF5(void);
void GPIO_Config_Buzzer_PB0_Output(void);
void GPIO_Config_Motor_PB1_Output(void);

#endif // _GPIO_H 