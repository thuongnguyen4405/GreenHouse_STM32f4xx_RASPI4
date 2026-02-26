#ifndef _FIRE_LOGIC_H_
#define _FIRE_LOGIC_H_

#include <stdint.h>

/*============================================================
 *  fire_logic – State machine c?nh báo cháy / khí gas
 *
 *  3 tr?ng thái: NORMAL ? WARN ? ALARM
 *  Có hysteresis (ngu?ng ON ? ngu?ng OFF) d? ch?ng nh?p nháy.
 *
 *  So d? chuy?n d?i:
 *    NORMAL -[val = WARN_ON]--? WARN -[val = ALARM_ON]--? ALARM
 *      ?                         ¦                          ¦
 *      +--[val = WARN_OFF]------+                          ¦
 *                                ?                          ¦
 *                                +--[val = ALARM_OFF]------+
 *
 *  Temperature và Gas du?c dánh giá Ð?C L?P.
 *  State cu?i = max(temp_state, gas_state).
 *============================================================*/

typedef enum {
    FIRE_STATE_NORMAL = 0,   /* An toàn                      */
    FIRE_STATE_WARN   = 1,   /* C?nh báo (buzzer beep ch?m)  */
    FIRE_STATE_ALARM  = 2    /* Báo d?ng (beep nhanh + motor) */
} FireState;

/* Kh?i t?o: c? temp & gas v? NORMAL */
void      FireLogic_Init(void);

/* C?p nh?t v?i giá tr? m?i (g?i m?i chu k? ADC) */
void      FireLogic_Update(uint16_t temp_x10, uint16_t gas_raw);

/* Tr? state t?ng h?p = max(temp, gas) */
FireState FireLogic_GetState(void);

/* Tr? state riêng t?ng sensor (d? hi?n th? chi ti?t trên GUI) */
FireState FireLogic_GetTempState(void);
FireState FireLogic_GetGasState(void);

#endif /* _FIRE_LOGIC_H_ */
