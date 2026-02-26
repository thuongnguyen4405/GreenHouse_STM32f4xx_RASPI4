#ifndef _DMA_H_
#define _DMA_H_

#include <stdint.h>

/* DMA base address (STM32F411) */
#define DMA1_BASE_ADDR   (0x40026000UL)
#define DMA2_BASE_ADDR   (0x40026400UL)

/* DMA stream register map */
typedef struct
{
    volatile uint32_t CR;     /* 0x00 */
    volatile uint32_t NDTR;   /* 0x04 */
    volatile uint32_t PAR;    /* 0x08 */
    volatile uint32_t M0AR;   /* 0x0C */
    volatile uint32_t M1AR;   /* 0x10 */
    volatile uint32_t FCR;    /* 0x14 */
} DMA_Stream_TypeDef_Mini;

/* DMA controller register map */
typedef struct
{
    volatile uint32_t LISR;     /* 0x00 */
    volatile uint32_t HISR;     /* 0x04 */
    volatile uint32_t LIFCR;    /* 0x08 */
    volatile uint32_t HIFCR;    /* 0x0C */
    DMA_Stream_TypeDef_Mini S0; /* 0x10 */
    DMA_Stream_TypeDef_Mini S1; /* 0x28 */
    DMA_Stream_TypeDef_Mini S2; /* 0x40 */
    DMA_Stream_TypeDef_Mini S3; /* 0x58 */
    DMA_Stream_TypeDef_Mini S4; /* 0x70 */
    DMA_Stream_TypeDef_Mini S5; /* 0x88 */
    DMA_Stream_TypeDef_Mini S6; /* 0xA0 */
    DMA_Stream_TypeDef_Mini S7; /* 0xB8 */
} DMA_TypeDef_Mini;

#define DMA1_REG   ((DMA_TypeDef_Mini*) DMA1_BASE_ADDR)
#define DMA2_REG   ((DMA_TypeDef_Mini*) DMA2_BASE_ADDR)

/* Optional: shortcut stream pointers */
#define DMA1_STREAM0   (&(DMA1_REG->S0))
#define DMA1_STREAM1   (&(DMA1_REG->S1))
#define DMA1_STREAM2   (&(DMA1_REG->S2))
#define DMA1_STREAM3   (&(DMA1_REG->S3))
#define DMA1_STREAM4   (&(DMA1_REG->S4))
#define DMA1_STREAM5   (&(DMA1_REG->S5))
#define DMA1_STREAM6   (&(DMA1_REG->S6))
#define DMA1_STREAM7   (&(DMA1_REG->S7))

#define DMA2_STREAM0   (&(DMA2_REG->S0))
#define DMA2_STREAM1   (&(DMA2_REG->S1))
#define DMA2_STREAM2   (&(DMA2_REG->S2))
#define DMA2_STREAM3   (&(DMA2_REG->S3))
#define DMA2_STREAM4   (&(DMA2_REG->S4))
#define DMA2_STREAM5   (&(DMA2_REG->S5))
#define DMA2_STREAM6   (&(DMA2_REG->S6))
#define DMA2_STREAM7   (&(DMA2_REG->S7))
//ADC_DMA -> interrupt 
extern volatile uint16_t g_adc_buf[4];

void ADC1_DMA2_Stream0_InitStart(void);

#endif /* _DMA_H_ */