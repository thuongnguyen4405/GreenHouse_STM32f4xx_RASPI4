#include "greenhouse.h"
#include "DMA_LIB.h"        /* g_adc_buf[] extern              */
#include "adc_mgr.h"        /* b? l?c moving-average           */
#include "fire_logic.h"     /* state machine + hysteresis       */
#include "actuators.h"      /* buzzer / motor control           */
#include "SPI_LIB.h"        /* SPI1_Slave_SetTxBuffer/Reset    */

/*============================================================
 *  greenhouse.c – Logic trung tâm: ADC ? Alarm ? Actuator ? SPI
 *
 *  Lu?ng x? lý (trong DMA2 TC IRQ):
 *
 *    g_adc_buf[4]  (raw from DMA)
 *         ¦
 *         ?
 *    ADC_Mgr_FeedSample()      ? d?y vào b? l?c
 *         ¦
 *         +-? ADC_Mgr_GetTempX10()  ? nhi?t d? dã l?c
 *         +-? ADC_Mgr_GetGasRaw()   ? gas dã l?c
 *                   ¦
 *                   ?
 *    FireLogic_Update()        ? state machine (hysteresis)
 *         ¦
 *         +-? FireLogic_GetState()   ? NORMAL/WARN/ALARM
 *         +-? Actuator_SetState()    ? set target cho buzzer/motor
 *         ¦
 *         ?
 *    build_packet()            ? dóng gói 16-byte SPI frame
 *         ¦
 *         ?
 *    SPI1_Slave_ResetIndex()   ? Pi d?c frame m?i t? byte 0
 *
 *  AN TOÀN ISR:
 *  DMA IRQ priority (1) > SPI IRQ priority (2), nên SPI IRQ
 *  b? block trong lúc build_packet ? không có race condition.
 *============================================================*/

volatile uint8_t g_spi_packet[PACKET_LEN];
static volatile uint8_t seq = 0;

/*------------------------------------------------------------
 *  build_packet – Ðóng gói 16-byte SPI frame
 *
 *  Byte   Field             Mô t?
 *  -----  ----------------  --------------------------
 *  [0]    MAGIC_0           0xAA (start-of-frame)
 *  [1]    MAGIC_1           0x55 (start-of-frame)
 *  [2]    SEQ               Sequence counter (0–255)
 *  [3]    STATUS            Bit-field tr?ng thái
 *  [4-5]  ADC0 (LM35)       uint16_t little-endian
 *  [6-7]  ADC1 (Gas)        uint16_t little-endian
 *  [8-9]  ADC2 (Sensor3)    uint16_t little-endian
 *  [10-11] ADC3 (Sensor4)   uint16_t little-endian
 *  [12-13] TEMP_X10         uint16_t little-endian
 *  [14]   XOR_CHECKSUM      XOR bytes [0..13]
 *  [15]   END_MARKER        0x0D (end-of-frame)
 *------------------------------------------------------------*/
static void build_packet(uint8_t status,
                          uint16_t adc[4],
                          uint16_t temp_x10)
{
    int i;
    uint8_t cs;

    /* Header */
    g_spi_packet[0]  = 0xAA;             /* magic byte 0          */
    g_spi_packet[1]  = 0x55;             /* magic byte 1          */
    g_spi_packet[2]  = seq++;            /* sequence number       */
    g_spi_packet[3]  = status;           /* status flags          */

    /* 4 kênh ADC, little-endian (low byte tru?c, high byte sau) */
    g_spi_packet[4]  = (uint8_t)(adc[0] & 0xFF);
    g_spi_packet[5]  = (uint8_t)(adc[0] >> 8);
    g_spi_packet[6]  = (uint8_t)(adc[1] & 0xFF);
    g_spi_packet[7]  = (uint8_t)(adc[1] >> 8);
    g_spi_packet[8]  = (uint8_t)(adc[2] & 0xFF);
    g_spi_packet[9]  = (uint8_t)(adc[2] >> 8);
    g_spi_packet[10] = (uint8_t)(adc[3] & 0xFF);
    g_spi_packet[11] = (uint8_t)(adc[3] >> 8);

    /* Nhi?t d? × 10 (0.1°C), little-endian */
    g_spi_packet[12] = (uint8_t)(temp_x10 & 0xFF);
    g_spi_packet[13] = (uint8_t)(temp_x10 >> 8);

    /* XOR checksum: XOR t?t c? bytes [0..13] */
    cs = 0;
    for (i = 0; i <= 13; i++)
        cs ^= g_spi_packet[i];
    g_spi_packet[14] = cs;

    /* End-of-frame marker */
    g_spi_packet[15] = 0x0D;
}

/*------------------------------------------------------------
 *  Greenhouse_InitPacket – T?o frame kh?i t?o (data = 0)
 *
 *  G?i 1 l?n t? main() TRU?C khi b?t ADC.
 *  Ð?m b?o SPI luôn có frame h?p l? d? g?i cho Pi,
 *  k? c? khi Pi poll tru?c khi ADC có data d?u tiên.
 *------------------------------------------------------------*/
void Greenhouse_InitPacket(void)
{
    uint16_t zeros[4] = {0, 0, 0, 0};
    build_packet(0, zeros, 0);
    SPI1_Slave_SetTxBuffer(g_spi_packet, PACKET_LEN);
}

/*------------------------------------------------------------
 *  Greenhouse_OnAdcReady – Callback t? DMA2 Stream0 TC IRQ
 *
 *  Ðây là hàm ch?y trong ISR context (DMA IRQ priority 1).
 *  Ph?i th?c thi nhanh, không blocking.
 *
 *  Lu?ng:
 *    1. Feed 4 m?u ADC thô vào b? l?c moving-average
 *    2. Ð?c temperature & gas dã l?c
 *    3. C?p nh?t fire logic state machine (hysteresis)
 *    4. Set target state cho actuator (pattern ch?y trong SysTick)
 *    5. Build STATUS byte cho SPI frame
 *    6. L?y 4 giá tr? ADC dã l?c
 *    7. Build SPI packet 16 bytes
 *    8. Reset SPI TX index ? Pi d?c frame m?i t? byte 0
 *------------------------------------------------------------*/
void Greenhouse_OnAdcReady(void)
{
    uint16_t temp_x10;
    uint16_t gas_raw;
    FireState st;
    uint8_t  gas_flag, temp_flag;
    uint8_t  status;
    uint16_t adc[4];
    uint8_t  ch;

    /* 1. Ð?y m?u ADC thô vào b? l?c */
    ADC_Mgr_FeedSample(g_adc_buf);

    /* 2. Ð?c giá tr? dã l?c */
    temp_x10 = ADC_Mgr_GetTempX10();    /* 0.1°C, ví d? 325 = 32.5°C */
    gas_raw  = ADC_Mgr_GetGasRaw();     /* raw ADC 0–4095            */

    /* 3. C?p nh?t state machine (có hysteresis ch?ng nh?p nháy) */
    FireLogic_Update(temp_x10, gas_raw);
    st = FireLogic_GetState();           /* NORMAL / WARN / ALARM     */

    /* 4. Set actuator target state
     *    (buzzer beep pattern ch?y trong SysTick_Handler m?i 1ms) */
    Actuator_SetState(st);

    /* 5. Build STATUS byte
     *    Bit 0: Buzzer dang ON?
     *    Bit 1: Motor dang ON?
     *    Bit 2: Gas alarm flag (WARN ho?c ALARM)
     *    Bit 3: Temperature alarm flag (WARN ho?c ALARM) */
    gas_flag  = (FireLogic_GetGasState()  >= FIRE_STATE_WARN) ? 1U : 0U;
    temp_flag = (FireLogic_GetTempState() >= FIRE_STATE_WARN) ? 1U : 0U;

    status = 0;
    status |= (Actuator_IsBuzzerOn() ? 1U : 0U) << 0;
    status |= (Actuator_IsMotorOn()  ? 1U : 0U) << 1;
    status |= gas_flag  << 2;
    status |= temp_flag << 3;

    /* 6. L?y 4 giá tr? ADC dã l?c (cho payload) */
    for (ch = 0; ch < 4; ch++)
        adc[ch] = ADC_Mgr_GetFiltered(ch);

    /* 7. Ðóng gói SPI frame 16 bytes */
    build_packet(status, adc, temp_x10);

    /* 8. Reset SPI TX pointer ? l?n poll ti?p theo Pi nh?n frame m?i */
    SPI1_Slave_ResetIndex();
}
