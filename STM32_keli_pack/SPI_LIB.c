#include "SPI_LIB.h"

static volatile uint8_t  *g_tx = 0;
static volatile uint16_t  g_len = 0;
static volatile uint16_t  g_idx = 0;

void SPI1_Slave_SetTxBuffer(volatile uint8_t *buf, uint16_t len)
{
    g_tx = buf;
    g_len = len;
    g_idx = 0;
}

void SPI1_Slave_ResetIndex(void)
{
    g_idx = 0;
}

void SPI1_Slave_Init(void)
{
    /* disable */
    SPI1->CR1 &= ~SPI_CR1_SPE;

    /* slave, mode0, 8-bit, HW NSS (SSM=0) */
    SPI1->CR1 = 0;

    /* RXNE interrupt */
    SPI1->CR2 = SPI_CR2_RXNEIE;

    NVIC_SetPriority(SPI1_IRQn, 2);
    NVIC_EnableIRQ(SPI1_IRQn);

    SPI1->CR1 |= SPI_CR1_SPE;
}

/* master clock -> RXNE set -> read DR -> write next byte */
void SPI1_IRQHandler(void)
{
    if (SPI1->SR & SPI_SR_RXNE)
    {
        volatile uint8_t dummy = (uint8_t)SPI1->DR;
        (void)dummy;

        if (g_tx && g_len)
        {
            /* wait TXE just in case */
            if (SPI1->SR & SPI_SR_TXE)
            {
                SPI1->DR = g_tx[g_idx++];
                if (g_idx >= g_len) g_idx = 0;
            }
        }
        else
        {
            /* no buffer -> send 0 */
            if (SPI1->SR & SPI_SR_TXE) SPI1->DR = 0x00;
        }
    }
}