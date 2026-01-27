/*****************************************************************************
 * | File         :   ds3231.cpp
 * | Author       :   (adaptado para tu framework estilo Waveshare)
 * | Function     :   DS3231 RTC control via I2C interface
 * | Info         :
 * |                 Driver para leer/escribir hora/fecha y temperatura
 * |                 usando las funciones DEV_I2C_* existentes.
 * ----------------
 * | This version :   V1.0
 * | Date         :   2026-01-27
 *
 ******************************************************************************/

#include "clock_rtc.h"


// Objeto global (igual que tu IO_EXTENSION)
ds3231_obj_t DS3231;

/* =======================
   Helpers BCD
   ======================= */
static inline uint8_t bcd2bin(uint8_t v)
{
    return (uint8_t)((v >> 4) * 10 + (v & 0x0F));
}

static inline uint8_t bin2bcd(uint8_t v)
{
    return (uint8_t)(((v / 10) << 4) | (v % 10));
}

/* =======================
   API
   ======================= */

/**
 * @brief Inicializa el DS3231 en el bus I2C.
 *
 * - Setea dirección esclavo (0x68 por defecto).
 * - Opcional: limpia flags de estado si el reloj viene de fábrica o se reseteó.
 */
void DS3231_Init(void)
{
    DEV_I2C_Set_Slave_Addr(&DS3231.addr, DS3231_ADDR);

    // Lee STATUS para ver si OSF está activo (Oscillator Stop Flag)
    uint8_t status = 0;
    DEV_I2C_Read_Nbyte(DS3231.addr, DS3231_REG_STATUS, &status, 1);

    // Si el oscilador se detuvo en algún momento, lo marcamos internamente.
    DS3231.osf = ((status & DS3231_STAT_OSF) != 0);

    // Si quieres limpiar OSF automáticamente:
    // (Muchos proyectos lo limpian después de setear hora válida)
    // status &= ~DS3231_STAT_OSF;
    // uint8_t data[2] = {DS3231_REG_STATUS, status};
    // DEV_I2C_Write_Nbyte(DS3231.addr, data, 2);
}

/**
 * @brief Lee hora/fecha del DS3231.
 * @param t Puntero a struct destino.
 * @return 0 OK, -1 error básico (puntero nulo)
 *
 * Nota: Lee formato 24h (si el DS3231 estuviera en 12h, lo convierte).
 */
int DS3231_GetTime(ds3231_time_t *t)
{
    if (!t) return -1;

    uint8_t raw[7] = {0};
    DEV_I2C_Read_Nbyte(DS3231.addr, DS3231_REG_SECONDS, raw, 7);

    uint8_t sec = raw[0] & 0x7F;
    uint8_t min = raw[1] & 0x7F;

    // Hours: bit6 = 12/24, si 12h: bit5=PM, bits[4:0]=hour
    uint8_t hr_raw = raw[2];
    uint8_t hour = 0;

    if (hr_raw & 0x40) {
        // 12h mode
        uint8_t hr12 = bcd2bin(hr_raw & 0x1F);
        bool pm = (hr_raw & 0x20) != 0;

        // convierte a 24h
        if (hr12 == 12) hour = pm ? 12 : 0;
        else            hour = pm ? (uint8_t)(hr12 + 12) : hr12;
    } else {
        // 24h mode: bits[5:0]
        hour = bcd2bin(hr_raw & 0x3F);
    }

    uint8_t day  = raw[3] & 0x07;     // 1..7
    uint8_t date = raw[4] & 0x3F;     // 1..31
    uint8_t mon  = raw[5] & 0x1F;     // 1..12 (ignora century)
    uint8_t year = raw[6];            // 0..99

    t->second = bcd2bin(sec);
    t->minute = bcd2bin(min);
    t->hour   = hour;

    t->wday   = bcd2bin(day);
    t->day    = bcd2bin(date);
    t->month  = bcd2bin(mon);
    t->year   = (uint16_t)(2000 + bcd2bin(year)); // típico DS3231

    return 0;
}

/**
 * @brief Escribe hora/fecha al DS3231 (modo 24h).
 * @param t Struct con valores (año esperado: 2000..2099 recomendado)
 * @return 0 OK, -1 error básico (puntero nulo)
 */
int DS3231_SetTime(const ds3231_time_t *t)
{
    if (!t) return -1;

    uint16_t y = t->year;
    if (y < 2000) y = 2000;
    if (y > 2099) y = 2099;

    uint8_t yy = (uint8_t)(y - 2000);

    // Armamos bloque de escritura desde 0x00
    // segundos, minutos, horas(24h), day(1..7), date, month, year
    uint8_t data[8];
    data[0] = DS3231_REG_SECONDS;
    data[1] = bin2bcd((uint8_t)(t->second % 60));
    data[2] = bin2bcd((uint8_t)(t->minute % 60));
    data[3] = bin2bcd((uint8_t)(t->hour   % 24));  // 24h mode (bit6=0)
    data[4] = bin2bcd((uint8_t)(t->wday   ? t->wday : 1)); // si viene 0, usa 1
    data[5] = bin2bcd((uint8_t)(t->day    ? t->day  : 1));
    data[6] = bin2bcd((uint8_t)(t->month  ? t->month: 1)); // sin century bit
    data[7] = bin2bcd(yy);

    DEV_I2C_Write_Nbyte(DS3231.addr, data, sizeof(data));

    // Limpia OSF (recomendado después de setear hora válida)
    uint8_t status = 0;
    DEV_I2C_Read_Nbyte(DS3231.addr, DS3231_REG_STATUS, &status, 1);
    status &= (uint8_t)~DS3231_STAT_OSF;
    uint8_t stw[2] = {DS3231_REG_STATUS, status};
    DEV_I2C_Write_Nbyte(DS3231.addr, stw, 2);

    DS3231.osf = false;

    return 0;
}

/**
 * @brief Lee temperatura interna del DS3231 en °C.
 * @param out_c Puntero a float destino.
 * @return 0 OK, -1 error básico (puntero nulo)
 *
 * Formato: MSB = entero con signo, LSB bits[7:6]=.00,.25,.50,.75
 */
int DS3231_GetTemperatureC(float *out_c)
{
    if (!out_c) return -1;

    uint8_t msb = 0, lsb = 0;
    DEV_I2C_Read_Nbyte(DS3231.addr, DS3231_REG_TEMP_MSB, &msb, 1);
    DEV_I2C_Read_Nbyte(DS3231.addr, DS3231_REG_TEMP_LSB, &lsb, 1);

    int8_t ti = (int8_t)msb;
    float frac = ((lsb >> 6) & 0x03) * 0.25f;

    *out_c = (float)ti + (ti < 0 ? -frac : frac); // respeta signo
    return 0;
}

/**
 * @brief Habilita o deshabilita salida 32kHz del DS3231 (pin 32K).
 */
void DS3231_Enable32kHz(uint8_t enable)
{
    uint8_t status = 0;
    DEV_I2C_Read_Nbyte(DS3231.addr, DS3231_REG_STATUS, &status, 1);

    if (enable) status |= DS3231_STAT_EN32KHZ;
    else        status &= (uint8_t)~DS3231_STAT_EN32KHZ;

    uint8_t data[2] = {DS3231_REG_STATUS, status};
    DEV_I2C_Write_Nbyte(DS3231.addr, data, 2);
}

/**
 * @brief Configura SQW en modo square-wave (no interrupciones) y selecciona frecuencia.
 * @param rate 0=1Hz, 1=1.024kHz, 2=4.096kHz, 3=8.192kHz
 *
 * Nota: pone INTCN=0 para que salga square wave por SQW/INT.
 */
void DS3231_SetSquareWave(uint8_t rate)
{
    rate &= 0x03;

    uint8_t ctrl = 0;
    DEV_I2C_Read_Nbyte(DS3231.addr, DS3231_REG_CONTROL, &ctrl, 1);

    // INTCN=0 => square wave
    ctrl &= (uint8_t)~DS3231_CTRL_INTCN;

    // RS2/RS1 bits
    ctrl &= (uint8_t)~(DS3231_CTRL_RS2 | DS3231_CTRL_RS1);
    if (rate & 0x01) ctrl |= DS3231_CTRL_RS1;
    if (rate & 0x02) ctrl |= DS3231_CTRL_RS2;

    uint8_t data[2] = {DS3231_REG_CONTROL, ctrl};
    DEV_I2C_Write_Nbyte(DS3231.addr, data, 2);
}

/**
 * @brief Devuelve si el OSF estaba activo al inicializar (indicador de que el reloj perdió tiempo).
 * @return 1 si OSF=1 (perdió el oscilador), 0 si no.
 */
uint8_t DS3231_HadOscillatorStop(void)
{
    return DS3231.osf ? 1 : 0;
}
// ============================================================================
// Wrapper API (lo que usa clock_manager)
// ============================================================================

static bool s_rtc_available = true; // con ESP_ERROR_CHECK, si falla el I2C se reinicia

void clock_rtc_init(void)
{
    DS3231_Init();
    s_rtc_available = true;
}

bool clock_rtc_is_available(void)
{
    return s_rtc_available;
}

bool clock_rtc_lost_power(void)
{
    return (DS3231_HadOscillatorStop() != 0);
}

static void ds_to_clock(const ds3231_time_t& ds, ClockDateTime& out)
{
    out.y  = (int)ds.year;
    out.mo = (int)ds.month;
    out.d  = (int)ds.day;
    out.h  = (int)ds.hour;
    out.mi = (int)ds.minute;
    out.s  = (int)ds.second;
}

static void clock_to_ds(const ClockDateTime& in, ds3231_time_t& ds)
{
    ds.year   = (uint16_t)in.y;
    ds.month  = (uint8_t)in.mo;
    ds.day    = (uint8_t)in.d;
    ds.hour   = (uint8_t)in.h;
    ds.minute = (uint8_t)in.mi;
    ds.second = (uint8_t)in.s;

    // Si no usas weekday, pon 1. (Si quieres cálculo real, te lo paso.)
    ds.wday = 1;
}

bool clock_rtc_read(ClockDateTime& out)
{
    ds3231_time_t ds{};
    if (DS3231_GetTime(&ds) != 0) return false;

    ds_to_clock(ds, out);
    return true;
}

bool clock_rtc_write(const ClockDateTime& in)
{
    ds3231_time_t ds{};
    clock_to_ds(in, ds);

    return (DS3231_SetTime(&ds) == 0);
}
