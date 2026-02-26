#include "actuators.h"
#include "board.h"

/*============================================================
 *  actuators.c – Buzzer beep pattern + Motor ON/OFF
 *
 *  Buzzer dùng GPIO push-pull (PB0), không ph?i PWM.
 *  Pattern beep du?c t?o b?ng software timer:
 *    - Actuator_SetState() set target (g?i t? DMA IRQ)
 *    - Actuator_Tick1ms()  ch?y pattern (g?i t? SysTick 1ms)
 *
 *  Motor dùng GPIO push-pull (PB1), ON/OFF theo state.
 *
 *  Dùng BSRR thay vì ODR d? atomic set/reset (an toàn ISR).
 *============================================================*/

static FireState g_state = FIRE_STATE_NORMAL;
static uint16_t  g_tick  = 0;   /* ms counter cho buzzer pattern */

/* ----------- Low-level GPIO ----------- */

/*------------------------------------------------------------
 *  Buzzer_Set – B?t/t?t buzzer qua PB0
 *
 *  Dùng BSRR (Bit Set Reset Register):
 *    - Bit 0 (set)   : ghi 1 ? PB0 = HIGH
 *    - Bit 16 (reset): ghi 1 ? PB0 = LOW
 *  BSRR là atomic, an toàn khi g?i t? nhi?u ISR context.
 *------------------------------------------------------------*/
void Buzzer_Set(uint8_t on)
{
    if (on) GPIOB->BSRR = (1U << 0);           /* PB0 = HIGH (b?t) */
    else    GPIOB->BSRR = (1U << (0 + 16));     /* PB0 = LOW  (t?t) */
}

/*------------------------------------------------------------
 *  Motor_Set – B?t/t?t motor qua PB1
 *------------------------------------------------------------*/
void Motor_Set(uint8_t on)
{
    if (on) GPIOB->BSRR = (1U << 1);            /* PB1 = HIGH (b?t) */
    else    GPIOB->BSRR = (1U << (1 + 16));      /* PB1 = LOW  (t?t) */
}

uint8_t Buzzer_Get(void) { return (GPIOB->ODR >> 0) & 1U; }
uint8_t Motor_Get(void)  { return (GPIOB->ODR >> 1) & 1U; }

/* ----------- High-level Actuator API ----------- */

/*------------------------------------------------------------
 *  Actuator_Init – Reset state, t?t t?t c?
 *------------------------------------------------------------*/
void Actuator_Init(void)
{
    g_state = FIRE_STATE_NORMAL;
    g_tick  = 0;
    Buzzer_Set(0);
    Motor_Set(0);
}

/*------------------------------------------------------------
 *  Actuator_SetState – C?p nh?t target state
 *
 *  G?i t? Greenhouse_OnAdcReady() (DMA IRQ context).
 *  Khi state d?i ? reset tick counter d? pattern b?t d?u l?i.
 *------------------------------------------------------------*/
void Actuator_SetState(FireState st)
{
    if (st != g_state)
    {
        g_state = st;
        g_tick  = 0;    /* reset pattern timing khi d?i state */
    }
}

/*------------------------------------------------------------
 *  Actuator_Tick1ms – G?i t? SysTick_Handler m?i 1 ms
 *
 *  Ch?y buzzer beep pattern theo g_state:
 *
 *    NORMAL:
 *      Buzzer OFF, Motor OFF, counter reset.
 *
 *    WARN (beep ch?m ~1 Hz):
 *      +--ON--+              +--ON--+
 *      ¦100ms ¦   900ms OFF  ¦100ms ¦ ...
 *      +------+--------------+------+
 *      Motor OFF.
 *
 *    ALARM (beep nhanh ~10 Hz):
 *      +ON+   +ON+   +ON+
 *      ¦50¦50 ¦50¦50 ¦50¦ ...
 *      +--+---+--+---+--+
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
            Motor_Set(0);          /* WARN: chua b?t motor */
            break;

        case FIRE_STATE_ALARM:
            g_tick++;
            if (g_tick <= BUZZER_ALARM_ON_MS)
                Buzzer_Set(1);
            else
                Buzzer_Set(0);
            if (g_tick >= (uint16_t)(BUZZER_ALARM_ON_MS + BUZZER_ALARM_OFF_MS))
                g_tick = 0;
            Motor_Set(1);          /* ALARM: b?t motor (qu?t hút / bom) */
            break;

        default:
            Buzzer_Set(0);
            Motor_Set(0);
            break;
    }
}

uint8_t Actuator_IsBuzzerOn(void) { return Buzzer_Get(); }
uint8_t Actuator_IsMotorOn(void)  { return Motor_Get();  }
