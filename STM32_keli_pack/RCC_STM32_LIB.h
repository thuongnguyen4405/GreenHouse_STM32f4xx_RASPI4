#ifndef _RCC_STM32_LIB_H_
#define _RCC_STM32_LIB_H_

#include "stm32f4xx.h"
/* =========================
 * RCC base address (STM32F411)
 * ========================= */

#define RCC_BASE     0x40023800U

void RCC_Enable_For_GPIO_ADC_SPI_DMA(void);
 
/* =========================
 * RCC register map (reference manual)
 * Offsets match your table: 0x00..0x74...
 * ========================= */
typedef struct
{
    volatile uint32_t CR;            /* 0x00 */
    volatile uint32_t PLLCFGR;       /* 0x04 */
    volatile uint32_t CFGR;          /* 0x08 */
    volatile uint32_t CIR;           /* 0x0C */
    volatile uint32_t AHB1RSTR;      /* 0x10 */
    volatile uint32_t AHB2RSTR;      /* 0x14 */
                                     /* 0x18 */
    uint32_t          RESERVED0[2];  /* 0x1C */
    volatile uint32_t APB1RSTR;      /* 0x20 */
    volatile uint32_t APB2RSTR;      /* 0x24 */
    uint32_t          RESERVED1[2];  /* 0x28,0x2C */
    volatile uint32_t AHB1ENR;       /* 0x30 */
    volatile uint32_t AHB2ENR;       /* 0x34 */
                                     /* 0x38 */
    uint32_t          RESERVED2[2];  /* 0x3C */
    volatile uint32_t APB1ENR;       /* 0x40 */
    volatile uint32_t APB2ENR;       /* 0x44 */
    uint32_t          RESERVED3[2];  /* 0x48,0x4C */
    volatile uint32_t AHB1LPENR;     /* 0x50 */
    volatile uint32_t AHB2LPENR;     /* 0x54 */
                                     /* 0x58 */
    uint32_t          RESERVED4[2];  /* 0x5C */
    volatile uint32_t APB1LPENR;     /* 0x60 */
    volatile uint32_t APB2LPENR;     /* 0x64 */
    uint32_t          RESERVED5[2];  /* 0x68,0x6C */
    volatile uint32_t BDCR;          /* 0x70 */
    volatile uint32_t CSR;           /* 0x74 */
    uint32_t          RESERVED6[2];  /* 0x78,0x7C */
    volatile uint32_t SSCGR;         /* 0x80 */
    volatile uint32_t PLLI2SCFGR;    /* 0x84 */
    uint32_t          RESERVED7;     /* 0x88 */
    volatile uint32_t DCKCFGR;       /* 0x8C (STM32F411 has DCKCFGR) */
} RCC_TypeDef_Mini;

#define RCC_REG ((RCC_TypeDef_Mini*) RCC_BASE) 



#endif // _RCC_STM32_LIB_H_
