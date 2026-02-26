#include "ADC_LIB.h"
#include "DMA_LIB.h"
#include "board.h"

/*============================================================
 *  ADC_DMA_LIB.c – ADC1 Scan + DMA2 Stream0 Circular Transfer
 *
 *  Hardware path:
 *    PA0-PA3 (analog) → ADC1 scan (4 channels, continuous)
 *    → DMA2 Stream0 Ch0 → g_adc_buf[4] (circular, 16-bit)
 *    → TC interrupt → Greenhouse_OnAdcReady()
 *
 *  DMA Transfer Complete fires every time all 4 channels
 *  have been sampled.  The callback processes the data
 *  (filter → alarm → actuators → SPI packet) entirely
 *  within ISR context at priority 1 (highest).
 *============================================================*/

/* DMA destination buffer — 4 × uint16, written by DMA hardware */
volatile uint16_t g_adc_buf[ADC_NUM_CHANNELS];

/* Small busy-wait for ADC ADON stabilization (~10 µs @ 16 MHz) */
static inline void small_delay(volatile uint32_t t) { while (t--) __NOP(); }

/*------------------------------------------------------------
 *  DMA2_Stream0_Init — Peripheral-to-memory, circular
 *
 *  DMA2 Stream 0 Channel 0 is hardwired to ADC1 on F411.
 *
 *  CR register bits:
 *    CHSEL[27:25] = 000  → Channel 0 (ADC1)
 *    PL[17:16]    = 10   → Priority High
 *    MSIZE[14:13] = 01   → 16-bit memory
 *    PSIZE[12:11] = 01   → 16-bit peripheral
 *    MINC  [10]   = 1    → Memory increment
 *    CIRC  [8]    = 1    → Circular mode
 *    DIR[7:6]     = 00   → Peripheral → Memory
 *    TCIE  [4]    = 1    → Transfer-complete interrupt
 *------------------------------------------------------------*/
static void DMA2_Stream0_Init(void)
{
    /* Disable stream before configuration */
    DMA2_Stream0->CR &= ~DMA_SxCR_EN;
    while (DMA2_Stream0->CR & DMA_SxCR_EN) { /* wait */ }

    /* Clear all Stream 0 interrupt flags (write-1-to-clear) */
    DMA2->LIFCR = DMA_LIFCR_CFEIF0 | DMA_LIFCR_CDMEIF0
                | DMA_LIFCR_CTEIF0 | DMA_LIFCR_CHTIF0
                | DMA_LIFCR_CTCIF0;

    /* Source: ADC1 data register (0x4C offset from ADC1 base) */
    DMA2_Stream0->PAR  = (uint32_t)&ADC1->DR;

    /* Destination: RAM buffer for 4 sensor readings */
    DMA2_Stream0->M0AR = (uint32_t)g_adc_buf;

    /* Number of data items = number of ADC channels */
    DMA2_Stream0->NDTR = ADC_NUM_CHANNELS;

    /* Configure CR: Channel 0, P→M, 16-bit, MINC, CIRC, TCIE */
    DMA2_Stream0->CR =
        (0U << DMA_SxCR_CHSEL_Pos) |   /* Channel 0 = ADC1       */
        DMA_SxCR_PL_1              |   /* Priority: High         */
        DMA_SxCR_MSIZE_0           |   /* Memory: 16-bit         */
        DMA_SxCR_PSIZE_0           |   /* Peripheral: 16-bit     */
        DMA_SxCR_MINC              |   /* Memory addr increment  */
        DMA_SxCR_CIRC              |   /* Circular mode          */
        (0U << DMA_SxCR_DIR_Pos)   |   /* Direction: P → M       */
        DMA_SxCR_TCIE;                 /* TC interrupt enable     */

    /* Direct mode (no FIFO) */
    DMA2_Stream0->FCR = 0;

    /* NVIC: highest priority (see board.h Section 8) */
    NVIC_SetPriority(DMA2_Stream0_IRQn, IRQ_PRIO_DMA_ADC);
    NVIC_EnableIRQ(DMA2_Stream0_IRQn);

    /* Enable stream — DMA now waits for ADC DMA requests */
    DMA2_Stream0->CR |= DMA_SxCR_EN;
}

/*------------------------------------------------------------
 *  ADC1_Init_Scan_DMA — 4-channel scan, continuous, DMA
 *
 *  Key register settings:
 *    CCR.ADCPRE   = 00  → PCLK2/2 = 8 MHz ADC clock
 *    CR1.SCAN     = 1   → Scan mode (convert all channels)
 *    CR2.DMA      = 1   → DMA request on each conversion
 *    CR2.DDS      = 1   → DMA requests continue in circular
 *    CR2.CONT     = 1   → Continuous conversion mode
 *    SMPR2        = ADC_SAMPLE_TIME_SEL per channel (board.h)
 *    SQR1.L       = ADC_NUM_CHANNELS - 1
 *    SQR3         = Channel sequence order (board.h defines)
 *------------------------------------------------------------*/
static void ADC1_Init_Scan_DMA(void)
{
    /* ADC clock prescaler: PCLK2/2 (CCR bits [17:16] = 00) */
    ADC->CCR &= ~(3U << 16);

    /* CR1: Enable scan mode */
    ADC1->CR1 = ADC_CR1_SCAN;

    /* CR2: DMA enable + DDS (keep issuing DMA) + Continuous */
    ADC1->CR2 = ADC_CR2_DMA | ADC_CR2_DDS | ADC_CR2_CONT;

    /* Sample time: ADC_SAMPLE_TIME_SEL (84 cycles) for ch0-ch3
     * SMPR2 has 3 bits per channel, channels 0-9 */
    ADC1->SMPR2 &= ~( (7U << (ADC_CH_LM35 * 3))
                    | (7U << (ADC_CH_GAS  * 3))
                    | (7U << (ADC_CH_S3   * 3))
                    | (7U << (ADC_CH_S4   * 3)) );
    ADC1->SMPR2 |=  ( (ADC_SAMPLE_TIME_SEL << (ADC_CH_LM35 * 3))
                    | (ADC_SAMPLE_TIME_SEL << (ADC_CH_GAS  * 3))
                    | (ADC_SAMPLE_TIME_SEL << (ADC_CH_S3   * 3))
                    | (ADC_SAMPLE_TIME_SEL << (ADC_CH_S4   * 3)) );

    /* Number of conversions: L = ADC_NUM_CHANNELS - 1 = 3
     * SQR1 bits [23:20] */
    ADC1->SQR1 &= ~(0xFU << 20);
    ADC1->SQR1 |=  ((ADC_NUM_CHANNELS - 1U) << 20);

    /* Conversion sequence: ch0 → ch1 → ch2 → ch3
     * SQR3: 5 bits per slot, slots 1-4 (bits [0:19]) */
    ADC1->SQR3 = (ADC_CH_LM35 << 0)
               | (ADC_CH_GAS  << 5)
               | (ADC_CH_S3   << 10)
               | (ADC_CH_S4   << 15);

    /* Turn on ADC (ADON bit) and wait for stabilization */
    ADC1->CR2 |= ADC_CR2_ADON;
    small_delay(10000);   /* ~625 µs @ 16 MHz — well above Tstab */

    /* Start first conversion (SWSTART bit) */
    ADC1->CR2 |= ADC_CR2_SWSTART;
}

/*------------------------------------------------------------
 *  ADC1_DMA2_Stream0_InitStart — Public API
 *
 *  Called from main() after RCC + GPIO are configured.
 *  DMA must be initialized BEFORE ADC starts (otherwise
 *  the first DMA request may be lost).
 *------------------------------------------------------------*/
void ADC1_DMA2_Stream0_InitStart(void)
{
    DMA2_Stream0_Init();     /* configure & enable DMA first */
    ADC1_Init_Scan_DMA();    /* then start ADC conversions   */
}

/* ═══════════ DMA2 Stream 0 Transfer-Complete ISR ═══════════
 *
 * Fires every time 4 ADC samples have been transferred into
 * g_adc_buf[].  Calls into the Service Layer (greenhouse.c)
 * which processes the data and builds the SPI packet.
 *
 * Callback declared in greenhouse.h (extern linkage).
 */
extern void Greenhouse_OnAdcReady(void);

void DMA2_Stream0_IRQHandler(void)
{
    if (DMA2->LISR & DMA_LISR_TCIF0)
    {
        /* Clear TC flag (write-1-to-clear in LIFCR) */
        DMA2->LIFCR = DMA_LIFCR_CTCIF0;

        /* Process: filter → alarm → actuators → SPI packet */
        Greenhouse_OnAdcReady();
    }
}
