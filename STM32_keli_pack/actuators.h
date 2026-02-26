#ifndef _ACTUATORS_H_
#define _ACTUATORS_H_

#include "stm32f4xx.h"
#include <stdint.h>
#include "fire_logic.h"

/*============================================================
 *  actuators – Ði?u khi?n Buzzer (PB0) & Motor (PB1)
 *
 *  Buzzer beep pattern theo FireState:
 *    NORMAL : t?t
 *    WARN   : beep ch?m ~1 Hz (100ms ON / 900ms OFF)
 *    ALARM  : beep nhanh ~10 Hz (50ms ON / 50ms OFF)
 *
 *  Motor (qu?t hút / bom nu?c):
 *    NORMAL : t?t
 *    WARN   : t?t (chua c?n can thi?p)
 *    ALARM  : b?t (hút khói / làm mát kh?n c?p)
 *
 *  Hardware: GPIO push-pull output, dùng BSRR d? atomic set/reset.
 *============================================================*/

/* Kh?i t?o: t?t c? buzzer & motor, reset state */
void    Actuator_Init(void);

/* Set target state (g?i t? greenhouse logic trong DMA IRQ) */
void    Actuator_SetState(FireState st);

/* Tick 1ms (g?i t? SysTick_Handler ? ch?y pattern beep) */
void    Actuator_Tick1ms(void);

/* Query tr?ng thái (cho SPI packet STATUS byte) */
uint8_t Actuator_IsBuzzerOn(void);
uint8_t Actuator_IsMotorOn(void);

/* Low-level GPIO (legacy API, tuong thích code cu) */
void    Buzzer_Set(uint8_t on);
void    Motor_Set(uint8_t on);
uint8_t Buzzer_Get(void);
uint8_t Motor_Get(void);

#endif /* _ACTUATORS_H_ */
