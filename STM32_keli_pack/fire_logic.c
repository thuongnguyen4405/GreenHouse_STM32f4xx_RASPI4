#include "fire_logic.h"
#include "board.h"

/*============================================================
 *  fire_logic.c – State machine v?i hysteresis
 *
 *  Nguyên lý hysteresis:
 *    - Ngu?ng ON  (chuy?n lên) > Ngu?ng OFF (chuy?n xu?ng)
 *    - Kho?ng cách gi?a ON và OFF = "vùng ch?t" (dead zone)
 *    - Khi giá tr? dao d?ng trong vùng ch?t ? state gi? nguyên
 *    ? Ch?ng hi?n tu?ng relay chattering / flickering
 *
 *  Ví d? temperature:
 *    WARN_ON  = 350 (35.0°C),  WARN_OFF  = 330 (33.0°C)
 *    ALARM_ON = 500 (50.0°C),  ALARM_OFF = 450 (45.0°C)
 *    ? N?u dang NORMAL, ph?i = 35.0°C m?i lên WARN
 *    ? N?u dang WARN, ph?i = 33.0°C m?i xu?ng NORMAL
 *============================================================*/

static FireState g_temp_state;
static FireState g_gas_state;

/*------------------------------------------------------------
 *  FireLogic_Init
 *------------------------------------------------------------*/
void FireLogic_Init(void)
{
    g_temp_state = FIRE_STATE_NORMAL;
    g_gas_state  = FIRE_STATE_NORMAL;
}

/*------------------------------------------------------------
 *  update_one – C?p nh?t state cho 1 sensor
 *
 *  Tham s?:
 *    cur       : state hi?n t?i
 *    val       : giá tr? do du?c
 *    warn_on   : ngu?ng vào WARN  (t? NORMAL)
 *    warn_off  : ngu?ng thoát WARN (v? NORMAL)
 *    alarm_on  : ngu?ng vào ALARM (t? WARN)
 *    alarm_off : ngu?ng thoát ALARM (v? WARN)
 *
 *  Luu ý: T? ALARM ch? xu?ng WARN (không nh?y th?ng NORMAL)
 *  ? Ph?i qua WARN r?i m?i NORMAL (nguyên t?c an toàn)
 *------------------------------------------------------------*/
static FireState update_one(FireState cur,
                            uint16_t val,
                            uint16_t warn_on,  uint16_t warn_off,
                            uint16_t alarm_on, uint16_t alarm_off)
{
    switch (cur)
    {
        case FIRE_STATE_NORMAL:
            if (val >= alarm_on)  return FIRE_STATE_ALARM;
            if (val >= warn_on)   return FIRE_STATE_WARN;
            break;

        case FIRE_STATE_WARN:
            if (val >= alarm_on)  return FIRE_STATE_ALARM;
            if (val <= warn_off)  return FIRE_STATE_NORMAL;
            /* N?u val n?m gi?a warn_off và alarm_on ? gi? WARN */
            break;

        case FIRE_STATE_ALARM:
            if (val <= alarm_off) return FIRE_STATE_WARN;
            /* Ch? xu?ng WARN, không nh?y th?ng NORMAL */
            break;

        default:
            return FIRE_STATE_NORMAL;
    }
    return cur;  /* gi? nguyên state */
}

/*------------------------------------------------------------
 *  FireLogic_Update – G?i m?i chu k? ADC
 *
 *  temp_x10 : nhi?t d? × 10 (0.1°C), t? ADC_Mgr_GetTempX10()
 *  gas_raw  : giá tr? gas ADC dã l?c, t? ADC_Mgr_GetGasRaw()
 *------------------------------------------------------------*/
void FireLogic_Update(uint16_t temp_x10, uint16_t gas_raw)
{
    g_temp_state = update_one(g_temp_state, temp_x10,
                              TEMP_WARN_ON_X10,  TEMP_WARN_OFF_X10,
                              TEMP_ALARM_ON_X10, TEMP_ALARM_OFF_X10);

    g_gas_state  = update_one(g_gas_state, gas_raw,
                              GAS_WARN_ON_ADC,  GAS_WARN_OFF_ADC,
                              GAS_ALARM_ON_ADC, GAS_ALARM_OFF_ADC);
}

/*------------------------------------------------------------
 *  Getters
 *------------------------------------------------------------*/
FireState FireLogic_GetState(void)
{
    /* State t?ng = max(temp, gas) ? m?c nghiêm tr?ng nh?t */
    return (g_temp_state >= g_gas_state) ? g_temp_state : g_gas_state;
}

FireState FireLogic_GetTempState(void) { return g_temp_state; }
FireState FireLogic_GetGasState(void)  { return g_gas_state;  }
