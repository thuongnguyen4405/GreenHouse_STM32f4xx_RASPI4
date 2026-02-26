#include "ADC_LIB.h"
#include "DMA_LIB.h"
#include "board.h"
/* 4 sensors buffer */
volatile uint16_t g_adc_buf[4];

static inline void small_delay(volatile uint32_t t){ while(t--) __NOP(); }

static void DMA2_Stream0_Init(void)
{
    /* Disable stream */
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    while (DMA2_Stream0->CR & DMA_SxCR_EN) {}

    /* Clear stream0 flags */
    DMA2->LIFCR = DMA_LIFCR_CFEIF0 | DMA_LIFCR_CDMEIF0 | DMA_LIFCR_CTEIF0 |
                  DMA_LIFCR_CHTIF0 | DMA_LIFCR_CTCIF0;

    DMA2_Stream0->PAR  = (uint32_t)&ADC1->DR;
    DMA2_Stream0->M0AR = (uint32_t)g_adc_buf;
    DMA2_Stream0->NDTR = 4;

    /* CH0, P->M, 16-bit, MINC, CIRC, TCIE */
    DMA2_Stream0->CR =
        (0U << DMA_SxCR_CHSEL_Pos) |
        DMA_SxCR_PL_1 |
        DMA_SxCR_MSIZE_0 |
        DMA_SxCR_PSIZE_0 |
        DMA_SxCR_MINC |
        DMA_SxCR_CIRC |
        (0U << DMA_SxCR_DIR_Pos) |
        DMA_SxCR_TCIE;

    DMA2_Stream0->FCR = 0;

    NVIC_SetPriority(DMA2_Stream0_IRQn, 1);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    DMA2_Stream0->CR |= DMA_SxCR_EN;
}

static void ADC1_Init_Scan_DMA(void)
{
    /* prescaler PCLK2/2 */
    ADC->CCR &= ~(3U<<16);

    ADC1->CR1 = ADC_CR1_SCAN;
    ADC1->CR2 = ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_CONT;

    /* sample time for ch0..3 (SMPR2), set 84 cycles */
    ADC1->SMPR2 &= ~((7U<<(0*3))|(7U<<(1*3))|(7U<<(2*3))|(7U<<(3*3)));
    ADC1->SMPR2 |=  ((4U<<(0*3))|(4U<<(1*3))|(4U<<(2*3))|(4U<<(3*3)));

    /* 4 conversions => L=3 */
    ADC1->SQR1 &= ~(0xFU<<20);
    ADC1->SQR1 |=  (3U<<20);

    /* order: ch0,ch1,ch2,ch3 */
    ADC1->SQR3 = (ADC_CH_LM35<<0) | (ADC_CH_GAS<<5) | (ADC_CH_S3<<10) | (ADC_CH_S4<<15);

    /* enable ADC */
    ADC1->CR2 |= ADC_CR2_ADON;
    small_delay(10000);

    /* start */
    ADC1->CR2 |= ADC_CR2_SWSTART;
}

void ADC1_DMA2_Stream0_InitStart(void)
{
    DMA2_Stream0_Init();
    ADC1_Init_Scan_DMA();
}

/* callback ? greenhouse.c */
void Greenhouse_OnAdcReady(void);

/* DMA TC IRQ */
void DMA2_Stream0_IRQHandler(void)
{
    if (DMA2->LISR & DMA_LISR_TCIF0)
    {
        DMA2->LIFCR = DMA_LIFCR_CTCIF0;
        Greenhouse_OnAdcReady();
    }
}