#include "adc_mgr.h"

/*============================================================
 *  adc_mgr.c – B? l?c Moving-Average cho 4 kênh ADC
 *
 *  M?i kênh luu ADC_FILTER_SAMPLES (m?c d?nh 8) m?u g?n nh?t
 *  trong ring buffer. GetFiltered() tr? trung bình c?ng ?
 *  gi?m nhi?u ADC dáng k?, d?c bi?t cho LM35 và gas sensor.
 *
 *  Thu?t toán O(1):
 *    - Duy trì t?ng tích lu? (g_sum[ch])
 *    - Khi thêm m?u m?i: sum -= m?u_cu, sum += m?u_m?i
 *    - GetFiltered() = sum / N  (không c?n duy?t l?i)
 *============================================================*/

/* Ring buffer: [channel][sample_index] */
static uint16_t g_ring[ADC_NUM_CHANNELS][ADC_FILTER_SAMPLES];
static uint8_t  g_idx    = 0;    /* ch? s? hi?n t?i trong ring     */
static uint8_t  g_filled = 0;    /* =1 khi ring dã d?y = 1 vòng   */

/* T?ng tích lu? cho t?ng kênh (tránh tính l?i m?i l?n d?c) */
static uint32_t g_sum[ADC_NUM_CHANNELS];

/*------------------------------------------------------------
 *  ADC_Mgr_Init – Reset toàn b? ring buffer & t?ng
 *------------------------------------------------------------*/
void ADC_Mgr_Init(void)
{
    uint8_t ch, s;
    for (ch = 0; ch < ADC_NUM_CHANNELS; ch++)
    {
        g_sum[ch] = 0;
        for (s = 0; s < ADC_FILTER_SAMPLES; s++)
            g_ring[ch][s] = 0;
    }
    g_idx    = 0;
    g_filled = 0;
}

/*------------------------------------------------------------
 *  ADC_Mgr_FeedSample – Ð?y 4 m?u ADC m?i vào ring buffer
 *
 *  G?i t? DMA2 Stream0 TC IRQ (qua Greenhouse_OnAdcReady).
 *  C?p nh?t O(1): tr? m?u cu nh?t, c?ng m?u m?i.
 *------------------------------------------------------------*/
void ADC_Mgr_FeedSample(const volatile uint16_t raw[ADC_NUM_CHANNELS])
{
    uint8_t ch;
    for (ch = 0; ch < ADC_NUM_CHANNELS; ch++)
    {
        g_sum[ch]          -= g_ring[ch][g_idx];   /* tr? m?u cu */
        g_ring[ch][g_idx]   = raw[ch];              /* ghi m?u m?i */
        g_sum[ch]          += raw[ch];              /* c?ng m?u m?i */
    }

    g_idx++;
    if (g_idx >= ADC_FILTER_SAMPLES)
    {
        g_idx    = 0;
        g_filled = 1;    /* dánh d?u ring dã d?y */
    }
}

/*------------------------------------------------------------
 *  ADC_Mgr_GetFiltered – Tr? ADC trung bình cho kênh ch
 *
 *  N?u ring chua d?y, chia cho s? m?u th?c t? (tránh chia 0).
 *------------------------------------------------------------*/
uint16_t ADC_Mgr_GetFiltered(uint8_t ch)
{
    uint8_t n;
    if (ch >= ADC_NUM_CHANNELS) return 0;
    n = g_filled ? ADC_FILTER_SAMPLES : (g_idx ? g_idx : 1);
    return (uint16_t)(g_sum[ch] / n);
}

/*------------------------------------------------------------
 *  ADC_Mgr_GetTempX10 – Nhi?t d? LM35 (don v? 0.1°C)
 *
 *  Công th?c:
 *    voltage_mV = adc_filtered × 3300 / 4095
 *    temp_x10   = voltage_mV
 *
 *  Gi?i thích: LM35 = 10 mV/°C ? 1 mV = 0.1°C
 *  Nên giá tr? mV chính là nhi?t d? × 10.
 *  Ví d?: ADC = 620 ? 620×3300/4095 ˜ 499 mV ? 49.9°C
 *------------------------------------------------------------*/
uint16_t ADC_Mgr_GetTempX10(void)
{
    uint32_t raw = ADC_Mgr_GetFiltered(ADC_IDX_LM35);
    return (uint16_t)((raw * ADC_VREF_MV) / ADC_RESOLUTION);
}

/*------------------------------------------------------------
 *  ADC_Mgr_GetGasRaw – Gas sensor ADC (dã l?c)
 *------------------------------------------------------------*/
uint16_t ADC_Mgr_GetGasRaw(void)
{
    return ADC_Mgr_GetFiltered(ADC_IDX_GAS);
}
