/*****************************************************************************
 * | File         :   clock_rtc.h
 * | Function     :   DS3231 RTC control via I2C interface + wrapper clock_rtc_*
 ******************************************************************************/

#ifndef __CLOCK_RTC_H
#define __CLOCK_RTC_H

#include <stdint.h>
#include <stdbool.h>

#include "drivers/i2c/i2c.h"
#include "clock_manager.h"   // <-- aquí vive ClockDateTime (debes definirlo ahí)

/* =======================
 * DS3231 I2C Address
 * ======================= */
#define DS3231_ADDR                0x68

/* =======================
 * DS3231 Register Map
 * ======================= */
#define DS3231_REG_SECONDS         0x00
#define DS3231_REG_MINUTES         0x01
#define DS3231_REG_HOURS           0x02
#define DS3231_REG_DAY             0x03
#define DS3231_REG_DATE            0x04
#define DS3231_REG_MONTH           0x05
#define DS3231_REG_YEAR            0x06

#define DS3231_REG_CONTROL         0x0E
#define DS3231_REG_STATUS          0x0F
#define DS3231_REG_AGING           0x10
#define DS3231_REG_TEMP_MSB        0x11
#define DS3231_REG_TEMP_LSB        0x12

/* CONTROL bits */
#define DS3231_CTRL_EOSC           (1 << 7)
#define DS3231_CTRL_BBSQW          (1 << 6)
#define DS3231_CTRL_CONV           (1 << 5)
#define DS3231_CTRL_RS2            (1 << 4)
#define DS3231_CTRL_RS1            (1 << 3)
#define DS3231_CTRL_INTCN          (1 << 2)
#define DS3231_CTRL_A2IE           (1 << 1)
#define DS3231_CTRL_A1IE           (1 << 0)

/* STATUS bits */
#define DS3231_STAT_OSF            (1 << 7)
#define DS3231_STAT_EN32KHZ        (1 << 3)
#define DS3231_STAT_BSY            (1 << 2)
#define DS3231_STAT_A2F            (1 << 1)
#define DS3231_STAT_A1F            (1 << 0)

/* =======================
 * Structs (DS3231)
 * ======================= */
typedef struct _ds3231_time_t {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  wday;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} ds3231_time_t;

typedef struct _ds3231_obj_t {
    i2c_master_dev_handle_t addr;
    bool osf;
} ds3231_obj_t;

extern ds3231_obj_t DS3231;

/* =======================
 * Driver DS3231 API
 * ======================= */
#ifdef __cplusplus
extern "C" {
#endif

void DS3231_Init(void);
int  DS3231_GetTime(ds3231_time_t *t);
int  DS3231_SetTime(const ds3231_time_t *t);
int  DS3231_GetTemperatureC(float *out_c);
void DS3231_Enable32kHz(uint8_t enable);
void DS3231_SetSquareWave(uint8_t rate);
uint8_t DS3231_HadOscillatorStop(void);

/* =======================
 * Wrapper API para clock_manager
 * (Estas son las que tu manager llama)
 * ======================= */
void clock_rtc_init(void);
bool clock_rtc_is_available(void);
bool clock_rtc_lost_power(void);
bool clock_rtc_read(ClockDateTime& out);
bool clock_rtc_write(const ClockDateTime& in);

#ifdef __cplusplus
}
#endif

#endif // __CLOCK_RTC_H
