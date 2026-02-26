#ifndef _GREENHOUSE_H_
#define _GREENHOUSE_H_

#include <stdint.h>
#include "board.h"

/*============================================================
 *  greenhouse – Logic trung tâm h? th?ng
 *
 *  Module này k?t n?i t?t c? thành ph?n:
 *    ADC data (adc_mgr) ? Alarm logic (fire_logic)
 *    ? Actuator control ? SPI packet (cho Raspberry Pi)
 *
 *  Ðu?c g?i t? DMA2 TC IRQ m?i khi 4 kênh ADC hoàn t?t scan.
 *============================================================*/

/* SPI TX buffer – chia s? v?i SPI_LIB qua SetTxBuffer() */
extern volatile uint8_t g_spi_packet[PACKET_LEN];

/* T?o frame kh?i t?o (all zeros), load vào SPI TX buffer */
void Greenhouse_InitPacket(void);

/* Callback t? DMA2 TC IRQ ? x? lý logic + build frame m?i */
void Greenhouse_OnAdcReady(void);

#endif /* _GREENHOUSE_H_ */
