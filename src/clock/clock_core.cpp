#include "clock_core.h"
#include <stdint.h>

#include <time.h>
#include <sys/time.h>
#include <Preferences.h>

static Preferences s_prefs;
static uint32_t s_last_save_ms = 0;

void clock_init()
{
  // Solo NVS. NO tocar I2C aquí.
  s_prefs.begin("clock", false);
}

bool clock_time_is_valid()
{
  return time(nullptr) > 1700000000; // ~2023
}

void clock_set_system_ymdhms(int Y,int Mo,int D,int H,int Mi,int S)
{
  struct tm t = {};
  t.tm_year = Y - 1900;
  t.tm_mon  = Mo - 1;
  t.tm_mday = D;
  t.tm_hour = H;
  t.tm_min  = Mi;
  t.tm_sec  = S;

  time_t epoch = mktime(&t);
  struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
  settimeofday(&tv, nullptr);
}

bool clock_restore_fallback()
{
  int64_t epoch = s_prefs.getLong64("epoch", -1);
  if (epoch < 0) return false;

  struct timeval tv = { .tv_sec = (time_t)epoch, .tv_usec = 0 };
  settimeofday(&tv, nullptr);

  return clock_time_is_valid();
}

void clock_save_fallback_now()
{
  if (!clock_time_is_valid()) return;
  int64_t epoch = (int64_t)time(nullptr);
  s_prefs.putLong64("epoch", epoch);
}

void clock_periodic_save_tick(uint32_t every_ms)
{
  uint32_t now = millis();
  if (now - s_last_save_ms < every_ms) return;
  s_last_save_ms = now;

  clock_save_fallback_now();
}
