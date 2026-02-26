#include "RCC_STM32_LIB.h"

/*============================================================
 *  RCC_Enable_For_GPIO_ADC_SPI_DMA
 *
 *  Enable peripheral clocks used by this project.
 *  Must be called FIRST in main(), before any GPIO/ADC/SPI
 *  register access — otherwise Hard Fault.
 *
 *  AHB1ENR (offset 0x30):
 *    Bit  0 : GPIOAEN  – PA0-PA7 (ADC + SPI pins)
 *    Bit  1 : GPIOBEN  – PB0-PB1 (Buzzer + Motor)
 *    Bit 22 : DMA2EN   – DMA2 for ADC1 circular transfer
 *
 *  APB2ENR (offset 0x44):
 *    Bit  8 : ADC1EN   – ADC1 (4-channel scan)
 *    Bit 12 : SPI1EN   – SPI1 slave (data → Raspberry Pi)
 *============================================================*/
void RCC_Enable_For_GPIO_ADC_SPI_DMA(void)
{
    /* AHB1: GPIOA (PA0-PA7), GPIOB (PB0-PB1), DMA2 */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN
                  | RCC_AHB1ENR_GPIOBEN
                  | RCC_AHB1ENR_DMA2EN;

    /* APB2: ADC1 + SPI1 */
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN
                  | RCC_APB2ENR_SPI1EN;
}
