#ifndef _USART_H_
#define _USART_H_

#include <stdint.h>

/* USART base address (STM32F411) */
#define USART1_BASE_ADDR   (0x40011000UL)
#define USART2_BASE_ADDR   (0x40004400UL)
#define USART6_BASE_ADDR   (0x40011400UL)

/* USART register map */
typedef struct
{
    volatile uint32_t SR;    /* 0x00 */
    volatile uint32_t DR;    /* 0x04 */
    volatile uint32_t BRR;   /* 0x08 */
    volatile uint32_t CR1;   /* 0x0C */
    volatile uint32_t CR2;   /* 0x10 */
    volatile uint32_t CR3;   /* 0x14 */
    volatile uint32_t GTPR;  /* 0x18 */
} USART_TypeDef_Mini;

#define USART1_REG   ((USART_TypeDef_Mini*) USART1_BASE_ADDR)
#define USART2_REG   ((USART_TypeDef_Mini*) USART2_BASE_ADDR)
#define USART6_REG   ((USART_TypeDef_Mini*) USART6_BASE_ADDR)

#endif /* _USART_H_ */