#ifndef _ADC_MGR_H_
#define _ADC_MGR_H_

#include <stdint.h>
#include "board.h"

/*============================================================
 *  adc_mgr – Qu?n lý d? li?u ADC + B? l?c Moving-Average
 *
 *  Module này nh?n d? li?u thô t? DMA buffer (g_adc_buf[]),
 *  l?c nhi?u b?ng moving average N m?u, và cung c?p API
 *  d?c giá tr? dã l?c + tính nhi?t d? LM35.
 *
 *  Lu?ng:
 *    DMA TC IRQ ? ADC_Mgr_FeedSample(g_adc_buf)
 *               ? GetFiltered() / GetTempX10() / GetGasRaw()
 *============================================================*/

/* Kh?i t?o b? l?c, reset ring buffer */
void     ADC_Mgr_Init(void);

/* Ð?y 4 m?u ADC thô m?i vào ring buffer (g?i t? DMA TC IRQ) */
void     ADC_Mgr_FeedSample(const volatile uint16_t raw[ADC_NUM_CHANNELS]);

/* Tr? giá tr? ADC trung bình (dã l?c) cho kênh ch (0..3) */
uint16_t ADC_Mgr_GetFiltered(uint8_t ch);

/* Tr? nhi?t d? LM35 × 10 (don v? 0.1°C), ví d? 325 = 32.5°C */
uint16_t ADC_Mgr_GetTempX10(void);

/* Tr? gas ADC (dã l?c) */
uint16_t ADC_Mgr_GetGasRaw(void);

#endif /* _ADC_MGR_H_ */
