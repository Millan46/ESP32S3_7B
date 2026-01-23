#include "clock_nvs.h"
#include <Preferences.h>

static Preferences s_prefs;

void clock_nvs_init()
{
    s_prefs.begin("clock", false);
}

bool clock_nvs_load(int64_t *epoch_out)
{
    int64_t v = s_prefs.getLong64("epoch", -1);
    if (v < 0) return false;
    if (epoch_out) *epoch_out = v;
    return true;
}

void clock_nvs_save(int64_t epoch)
{
    s_prefs.putLong64("epoch", epoch);
}
