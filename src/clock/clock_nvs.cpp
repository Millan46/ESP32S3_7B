#include "clock_nvs.h"
#include <Preferences.h>

static Preferences s_prefs;

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

void clock_nvs_init()
{
    s_prefs.begin("clock", false);
}

bool clock_nvs_load(ClockDateTime& out)
{
    // Si nunca se guardó, no hay nada que cargar
    if (!s_prefs.isKey("Y")) return false;

    ClockDateTime t;
    t.y  = s_prefs.getInt("Y",  -1);
    t.mo = s_prefs.getInt("Mo", -1);
    t.d  = s_prefs.getInt("D",  -1);
    t.h  = s_prefs.getInt("H",  -1);
    t.mi = s_prefs.getInt("Mi", -1);
    t.s  = s_prefs.getInt("S",  -1);

    if (!dt_valid(t)) return false;

    out = t;
    return true;
}


void clock_nvs_save(const ClockDateTime& in)
{
    if (!dt_valid(in)) return;

    s_prefs.putInt("Y",  in.y);
    s_prefs.putInt("Mo", in.mo);
    s_prefs.putInt("D",  in.d);
    s_prefs.putInt("H",  in.h);
    s_prefs.putInt("Mi", in.mi);
    s_prefs.putInt("S",  in.s);
}
