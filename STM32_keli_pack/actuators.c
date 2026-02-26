#include "actuators.h"
#include "board.h"

/*============================================================
 *  actuators.c – Buzzer beep pattern + Motor ON/OFF
 *
 *  Buzzer dùng GPIO push-pull (PB0), không phải PWM.
 *  Pattern beep được tạo bằng software timer:
 *    - Actuator_SetState() set target (gọi từ DMA IRQ)
 *    - Actuator_Tick1ms()  chạy pattern (gọi từ SysTick 1ms)
 *
 *  Motor dùng GPIO push-pull (PB1), ON/OFF theo state.
 *
 *  Dùng BSRR thay vì ODR để atomic set/reset (an toàn ISR).
 *============================================================*/

static FireState g_state = FIRE_STATE_NORMAL;
static uint16_t  g_tick  = 0;   /* ms counter cho buzzer pattern */

/* ═══════════ Low-level GPIO ═══════════ */

/*------------------------------------------------------------
 *  Buzzer_Set – Bật/tắt buzzer qua PB0
 *
 *  Dùng BSRR (Bit Set Reset Register):
 *    - Bit 0 (set)   : ghi 1 → PB0 = HIGH
 *    - Bit 16 (reset): ghi 1 → PB0 = LOW
 *  BSRR là atomic, an toàn khi gọi từ nhiều ISR context.
 *------------------------------------------------------------*/
void Buzzer_Set(uint8_t on)
{
    if (on) GPIOB->BSRR = (1U << 0);           /* PB0 = HIGH (bật) */
    else    GPIOB->BSRR = (1U << (0 + 16));     /* PB0 = LOW  (tắt) */
}

/*------------------------------------------------------------
 *  Motor_Set – Bật/tắt motor qua PB1
 *------------------------------------------------------------*/
void Motor_Set(uint8_t on)
{
    if (on) GPIOB->BSRR = (1U << 1);            /* PB1 = HIGH (bật) */
    else    GPIOB->BSRR = (1U << (1 + 16));      /* PB1 = LOW  (tắt) */
}

uint8_t Buzzer_Get(void) { return (GPIOB->ODR >> 0) & 1U; }
uint8_t Motor_Get(void)  { return (GPIOB->ODR >> 1) & 1U; }

/* ═══════════ High-level Actuator API ═══════════ */

/*------------------------------------------------------------
 *  Actuator_Init – Reset state, tắt tất cả
 *------------------------------------------------------------*/
void Actuator_Init(void)
{
    g_state = FIRE_STATE_NORMAL;
    g_tick  = 0;
    Buzzer_Set(0);
    Motor_Set(0);
}

/*------------------------------------------------------------
 *  Actuator_SetState – Cập nhật target state
 *
 *  Gọi từ Greenhouse_OnAdcReady() (DMA IRQ context).
 *  Khi state đổi → reset tick counter để pattern bắt đầu lại.
 *------------------------------------------------------------*/
void Actuator_SetState(FireState st)
{
    if (st != g_state)
    {
        g_state = st;
        g_tick  = 0;    /* reset pattern timing khi đổi state */
    }
}

/*------------------------------------------------------------
 *  Actuator_Tick1ms – Gọi từ SysTick_Handler mỗi 1 ms
 *
 *  Chạy buzzer beep pattern theo g_state:
 *
 *    NORMAL:
 *      Buzzer OFF, Motor OFF, counter reset.
 *
 *    WARN (beep chậm ~1 Hz):
 *      ┌──ON──┐              ┌──ON──┐
 *      │100ms │   900ms OFF  │100ms │ ...
 *      └──────┘──────────────└──────┘
 *      Motor OFF.
 *
 *    ALARM (beep nhanh ~10 Hz):
 *      ┌ON┐   ┌ON┐   ┌ON┐
 *      │50│50 │50│50 │50│ ...
 *      └──┘───└──┘───└──┘
 *      Motor ON.
 *------------------------------------------------------------*/
void Actuator_Tick1ms(void)
{
    switch (g_state)
    {
        case FIRE_STATE_NORMAL:
            Buzzer_Set(0);
            Motor_Set(0);
            g_tick = 0;
            break;

        case FIRE_STATE_WARN:
            g_tick++;
            if (g_tick <= BUZZER_WARN_ON_MS)
                Buzzer_Set(1);
            else
                Buzzer_Set(0);
            if (g_tick >= (uint16_t)(BUZZER_WARN_ON_MS + BUZZER_WARN_OFF_MS))
                g_tick = 0;
            Motor_Set(0);          /* WARN: chưa bật motor */
            break;

        case FIRE_STATE_ALARM:
            g_tick++;
            if (g_tick <= BUZZER_ALARM_ON_MS)
                Buzzer_Set(1);
            else
                Buzzer_Set(0);
            if (g_tick >= (uint16_t)(BUZZER_ALARM_ON_MS + BUZZER_ALARM_OFF_MS))
                g_tick = 0;
            Motor_Set(1);          /* ALARM: bật motor (quạt hút / bơm) */
            break;

        default:
            Buzzer_Set(0);
            Motor_Set(0);
            break;
    }
}

uint8_t Actuator_IsBuzzerOn(void) { return Buzzer_Get(); }
uint8_t Actuator_IsMotorOn(void)  { return Motor_Get();  }
