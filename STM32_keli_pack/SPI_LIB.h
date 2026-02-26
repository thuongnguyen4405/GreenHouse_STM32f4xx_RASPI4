#ifndef _SPI_H_
#define _SPI_H_

#include "stm32f4xx.h"

/* SPI base address (STM32F411) */
#define SPI1_BASE_ADDR   (0x40013000UL)
#define SPI2_BASE_ADDR   (0x40003800UL)
#define SPI3_BASE_ADDR   (0x40003C00UL)

/* SPI register map */
typedef struct
{
    volatile uint32_t CR1;      /* 0x00 */
    volatile uint32_t CR2;      /* 0x04 */
    volatile uint32_t SR;       /* 0x08 */
    volatile uint32_t DR;       /* 0x0C */
    volatile uint32_t CRCPR;    /* 0x10 */
    volatile uint32_t RXCRCR;   /* 0x14 */
    volatile uint32_t TXCRCR;   /* 0x18 */
    volatile uint32_t I2SCFGR;  /* 0x1C */
    volatile uint32_t I2SPR;    /* 0x20 */
} SPI_TypeDef_Mini;

#define SPI1_REG   ((SPI_TypeDef_Mini*) SPI1_BASE_ADDR)
#define SPI2_REG   ((SPI_TypeDef_Mini*) SPI2_BASE_ADDR)
#define SPI3_REG   ((SPI_TypeDef_Mini*) SPI3_BASE_ADDR)
// function declaration 
void SPI1_Slave_Init(void);

/* link v?i greenhouse packet */
void SPI1_Slave_SetTxBuffer(volatile uint8_t *buf, uint16_t len);
void SPI1_Slave_ResetIndex(void);
#endif /* _SPI_H_ */