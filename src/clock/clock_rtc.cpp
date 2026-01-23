#include "clock_rtc.h"

static bool rtc_ok = false;

void clock_rtc_init()
{
  // Aún NO hay DS3231 conectado -> no tocar I2C
  rtc_ok = false;
}

bool clock_rtc_is_available()
{
  return rtc_ok;
}

void clock_rtc_try_save_from_system()
{
  // stub: cuando conectes DS3231 pondremos rtc.adjust aquí.
}
