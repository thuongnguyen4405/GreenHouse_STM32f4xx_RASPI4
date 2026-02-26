#ifndef _TIMER_H_
#define _TIMER_H_

#include <stdint.h>

/* TIM base address (STM32F411) */
#define TIM1_BASE_ADDR    (0x40010000UL)
#define TIM2_BASE_ADDR    (0x40000000UL)
#define TIM3_BASE_ADDR    (0x40000400UL)
#define TIM4_BASE_ADDR    (0x40000800UL)
#define TIM5_BASE_ADDR    (0x40000C00UL)

#define TIM9_BASE_ADDR    (0x40014000UL)
#define TIM10_BASE_ADDR   (0x40014400UL)
#define TIM11_BASE_ADDR   (0x40014800UL)

/*
 * TIM register map (superset).
 * V?i timer không có vài thanh ghi (vd TIM9/10/11), m?y field dó có th? là reserved trong silicon,
 * nhung map này v?n ok d? truy c?p ph?n chung (CR1, PSC, ARR, CCRx...).
 */
typedef struct
{
    volatile uint32_t CR1;      /* 0x00 */
    volatile uint32_t CR2;      /* 0x04 */
    volatile uint32_t SMCR;     /* 0x08 */
    volatile uint32_t DIER;     /* 0x0C */
    volatile uint32_t SR;       /* 0x10 */
    volatile uint32_t EGR;      /* 0x14 */
    volatile uint32_t CCMR1;    /* 0x18 */
    volatile uint32_t CCMR2;    /* 0x1C */
    volatile uint32_t CCER;     /* 0x20 */
    volatile uint32_t CNT;      /* 0x24 */
    volatile uint32_t PSC;      /* 0x28 */
    volatile uint32_t ARR;      /* 0x2C */
    volatile uint32_t RCR;      /* 0x30 (advanced timers) */
    volatile uint32_t CCR1;     /* 0x34 */
    volatile uint32_t CCR2;     /* 0x38 */
    volatile uint32_t CCR3;     /* 0x3C */
    volatile uint32_t CCR4;     /* 0x40 */
    volatile uint32_t BDTR;     /* 0x44 (advanced timers) */
    volatile uint32_t DCR;      /* 0x48 */
    volatile uint32_t DMAR;     /* 0x4C */
} TIM_TypeDef_Mini;

#define TIM1_REG    ((TIM_TypeDef_Mini*) TIM1_BASE_ADDR)
#define TIM2_REG    ((TIM_TypeDef_Mini*) TIM2_BASE_ADDR)
#define TIM3_REG    ((TIM_TypeDef_Mini*) TIM3_BASE_ADDR)
#define TIM4_REG    ((TIM_TypeDef_Mini*) TIM4_BASE_ADDR)
#define TIM5_REG    ((TIM_TypeDef_Mini*) TIM5_BASE_ADDR)

#define TIM9_REG    ((TIM_TypeDef_Mini*) TIM9_BASE_ADDR)
#define TIM10_REG   ((TIM_TypeDef_Mini*) TIM10_BASE_ADDR)
#define TIM11_REG   ((TIM_TypeDef_Mini*) TIM11_BASE_ADDR)

#endif /* _TIMER_H_ */