#include "GPIO.h"

void GPIO_Config_ADC_PA0_PA3_Analog(void)
{
    /* PA0..PA3 = analog */
    GPIOA->MODER |= (3U<<(0*2)) | (3U<<(1*2)) | (3U<<(2*2)) | (3U<<(3*2));
    GPIOA->PUPDR &= ~((3U<<(0*2)) | (3U<<(1*2)) | (3U<<(2*2)) | (3U<<(3*2)));
}

void GPIO_Config_SPI1_PA4_PA7_AF5(void)
{
    /* PA4..PA7 alternate function */
    GPIOA->MODER &= ~((3U<<(4*2))|(3U<<(5*2))|(3U<<(6*2))|(3U<<(7*2)));
    GPIOA->MODER |=  ((2U<<(4*2))|(2U<<(5*2))|(2U<<(6*2))|(2U<<(7*2)));

    /* AF5 for SPI1 */
  GPIOA->AFRL &= ~((0xFUL<<(4*4))|(0xFUL<<(5*4))|(0xFUL<<(6*4))|(0xFUL<<(7*4)));
  GPIOA->AFRL |=  ((5UL<<(4*4))|(5UL<<(5*4))|(5UL<<(6*4))|(5UL<<(7*4)));

    /* speed very high */
    GPIOA->OSPEEDR |= (3U<<(4*2))|(3U<<(5*2))|(3U<<(6*2))|(3U<<(7*2));

    /* optional pull-down NSS/SCK (?n bus) */
    GPIOA->PUPDR &= ~((3U<<(4*2))|(3U<<(5*2))|(3U<<(6*2))|(3U<<(7*2)));
    GPIOA->PUPDR |=  (2U<<(4*2)) | (2U<<(5*2));
}

void GPIO_Config_Buzzer_PB0_Output(void)
{
    GPIOB->MODER &= ~(3U<<(0*2));
    GPIOB->MODER |=  (1U<<(0*2));     /* output */
    GPIOB->OTYPER &= ~(1U<<0);        /* push-pull */
    GPIOB->PUPDR  &= ~(3U<<(0*2));    /* no pull */
    GPIOB->ODR    &= ~(1U<<0);        /* OFF */
}

void GPIO_Config_Motor_PB1_Output(void)
{
    GPIOB->MODER &= ~(3U<<(1*2));
    GPIOB->MODER |=  (1U<<(1*2));     /* output */
    GPIOB->OTYPER &= ~(1U<<1);        /* push-pull */
    GPIOB->PUPDR  &= ~(3U<<(1*2));    /* no pull */
    GPIOB->ODR    &= ~(1U<<1);        /* OFF */
}