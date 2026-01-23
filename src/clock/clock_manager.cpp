#include "clock_manager.h"

#include "clock_core.h"
#include "clock_nvs.h"
#include "clock_rtc.h"

#include <time.h>

void clock_manager_init()
{
    // NVS
    clock_nvs_init();   // <-- si no existe, mira el paso 2

    // RTC opcional (hoy stub)
    clock_rtc_init();
}

void clock_manager_apply_manual_time(int Y, int Mo, int D, int h24, int mi, int sec)
{
    // 1) Set hora del sistema
    clock_set_system_ymdhms(Y, Mo, D, h24, mi, sec);

    // 2) Guardar en NVS (fallback)
    time_t now = time(nullptr);
    clock_nvs_save((int64_t)now);  // <-- usa el nombre que sí existe

    // 3) Guardar en RTC si está disponible
    if (clock_rtc_is_available()) {
        clock_rtc_try_save_from_system(); // <-- usa el nombre que sí existe
    }
}
