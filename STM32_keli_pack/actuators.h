#ifndef _ACTUATORS_H_
#define _ACTUATORS_H_

#include "stm32f4xx.h"
#include <stdint.h>
#include "fire_logic.h"

/*============================================================
 *  actuators – Điều khiển Buzzer (PB0) & Motor (PB1)
 *
 *  Buzzer beep pattern theo FireState:
 *    NORMAL : tắt
 *    WARN   : beep chậm ~1 Hz (100ms ON / 900ms OFF)
 *    ALARM  : beep nhanh ~10 Hz (50ms ON / 50ms OFF)
 *
 *  Motor (quạt hút / bơm nước):
 *    NORMAL : tắt
 *    WARN   : tắt (chưa cần can thiệp)
 *    ALARM  : bật (hút khói / làm mát khẩn cấp)
 *
 *  Hardware: GPIO push-pull output, dùng BSRR để atomic set/reset.
 *============================================================*/

/* Khởi tạo: tắt cả buzzer & motor, reset state */
void    Actuator_Init(void);

/* Set target state (gọi từ greenhouse logic trong DMA IRQ) */
void    Actuator_SetState(FireState st);

/* Tick 1ms (gọi từ SysTick_Handler → chạy pattern beep) */
void    Actuator_Tick1ms(void);

/* Query trạng thái (cho SPI packet STATUS byte) */
uint8_t Actuator_IsBuzzerOn(void);
uint8_t Actuator_IsMotorOn(void);

/* Low-level GPIO (legacy API, tương thích code cũ) */
void    Buzzer_Set(uint8_t on);
void    Motor_Set(uint8_t on);
uint8_t Buzzer_Get(void);
uint8_t Motor_Get(void);

#endif /* _ACTUATORS_H_ */
