#ifndef _ADC_H_
#define _ADC_H_

#include <stdint.h>
#include "stm32f4xx.h"

/* ADC base address (STM32F411) */
#define ADC1_BASE_ADDR   (0x40012000UL)

/* ADC register map */
typedef struct
{
    volatile uint32_t SR;       /* 0x00 */
    volatile uint32_t CR1;      /* 0x04 */
    volatile uint32_t CR2;      /* 0x08 */
    volatile uint32_t SMPR1;    /* 0x0C */
    volatile uint32_t SMPR2;    /* 0x10 */
    volatile uint32_t JOFR1;    /* 0x14 */
    volatile uint32_t JOFR2;    /* 0x18 */
    volatile uint32_t JOFR3;    /* 0x1C */
    volatile uint32_t JOFR4;    /* 0x20 */
    volatile uint32_t HTR;      /* 0x24 */
    volatile uint32_t LTR;      /* 0x28 */
    volatile uint32_t SQR1;     /* 0x2C */
    volatile uint32_t SQR2;     /* 0x30 */
    volatile uint32_t SQR3;     /* 0x34 */
    volatile uint32_t JSQR;     /* 0x38 */
    volatile uint32_t JDR1;     /* 0x3C */
    volatile uint32_t JDR2;     /* 0x40 */
    volatile uint32_t JDR3;     /* 0x44 */
    volatile uint32_t JDR4;     /* 0x48 */
    volatile uint32_t DR;       /* 0x4C */
} ADC_TypeDef_Mini;

#define ADC1  ((ADC_TypeDef_Mini*) ADC1_BASE_ADDR)

#endif /* _ADC_H_ */