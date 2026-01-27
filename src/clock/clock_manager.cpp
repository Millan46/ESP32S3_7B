#include "clock_manager.h"
#include "clock_nvs.h"
#include "clock_rtc.h"

static ClockDateTime s_now{2026, 1, 1, 0, 0, 0};
static bool s_valid = false;

static bool dt_valid(const ClockDateTime& t)
{
    if (t.y < 2020 || t.y > 2099) return false;
    if (t.mo < 1 || t.mo > 12) return false;
    if (t.d < 1 || t.d > 31) return false;
    if (t.h < 0 || t.h > 23) return false;
    if (t.mi < 0 || t.mi > 59) return false;
    if (t.s < 0 || t.s > 59) return false;
    return true;
}

void clock_manager_init()
{
    clock_nvs_init();
    clock_rtc_init();

    // 1) RTC primero
    ClockDateTime t;
    if (clock_rtc_is_available() &&
        !clock_rtc_lost_power() &&
        clock_rtc_read(t) &&
        dt_valid(t))
    {
        s_now = t;
        s_valid = true;
        clock_nvs_save(s_now);
        return;
    }

    // 2) NVS fallback
    if (clock_nvs_load(t) && dt_valid(t)) {
        s_now = t;
        s_valid = true;
        return;
    }

    // 3) inválido
    s_valid = false;
}

bool clock_manager_get_now(ClockDateTime& out)
{
    out = s_now;
    return s_valid;
}

bool clock_manager_sync_from_rtc()
{
    if (!clock_rtc_is_available() || clock_rtc_lost_power()) return false;

    ClockDateTime t;
    if (!clock_rtc_read(t)) return false;
    if (!dt_valid(t)) return false;

    s_now = t;
    s_valid = true;
    clock_nvs_save(s_now);
    return true;
}

void clock_manager_apply_manual_time(int Y, int Mo, int D, int h24, int mi, int sec)
{
    ClockDateTime t{Y, Mo, D, h24, mi, sec};
    if (!dt_valid(t)) return;

    s_now = t;
    s_valid = true;

    // guarda en RTC
    if (clock_rtc_is_available()) {
        (void)clock_rtc_write(s_now);
    }

    // guarda fallback
    clock_nvs_save(s_now);
}
static bool is_leap_year(int y)
{
    return ((y % 4) == 0 && (y % 100) != 0) || ((y % 400) == 0);
}

static int days_in_month(int y, int m) // m 1..12
{
    static const int d[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m == 2) return d[1] + (is_leap_year(y) ? 1 : 0);
    return d[m - 1];
}

// Llamar cada 1 segundo desde loop/timer
void clock_manager_tick_1s(void)
{
    if (!s_valid) return;

    s_now.s++;
    if (s_now.s >= 60) { s_now.s = 0; s_now.mi++; }
    if (s_now.mi >= 60) { s_now.mi = 0; s_now.h++; }
    if (s_now.h >= 24)  { s_now.h = 0; s_now.d++; }

    int maxd = days_in_month(s_now.y, s_now.mo);
    if (s_now.d > maxd) { s_now.d = 1; s_now.mo++; }
    if (s_now.mo > 12)  { s_now.mo = 1; s_now.y++; }
}
