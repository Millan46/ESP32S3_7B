#include <time.h>
#include <sys/time.h>
#include <Preferences.h>

static Preferences prefs;

static inline bool time_is_valid()
{
  // Umbral para saber si ya seteaste hora (1970+). Ajusta si quieres.
  return time(nullptr) > 1700000000; // ~2023
}

static inline void set_system_time_ymdhms(int Y,int Mo,int D,int H,int Mi,int S)
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

// ====== NVS fallback (si no hay RTC) ======
static inline void nvs_begin() { prefs.begin("clock", false); }
static inline void nvs_end()   { prefs.end(); }

static inline void nvs_save_now()
{
  if (!time_is_valid()) return;
  time_t now = time(nullptr);
  prefs.putLong64("epoch", (int64_t)now);
  prefs.putULong("ms", (uint32_t)millis());
}

static inline bool nvs_restore()
{
  prefs.begin("clock", true);
  int64_t saved_epoch = prefs.getLong64("epoch", -1);
  uint32_t saved_ms   = prefs.getULong("ms", 0);
  prefs.end();

  if (saved_epoch < 0) return false;

  uint32_t now_ms = millis();
  uint32_t delta_ms = (now_ms >= saved_ms) ? (now_ms - saved_ms) : 0;
  time_t restored = (time_t)saved_epoch + (time_t)(delta_ms / 1000);

  struct timeval tv = { .tv_sec = restored, .tv_usec = 0 };
  settimeofday(&tv, nullptr);
  return true;
}
