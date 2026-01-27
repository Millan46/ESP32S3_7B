#include "time_job.h"

#include "clock/clock_manager.h"
#include "drivers/lvgl_port/lvgl_port.h"
#include "ui/ui_datetime.h"

volatile bool g_time_save_pending = false;
PendingTime g_pending_time = {0};

void time_job_tick(void)
{
    if (!g_time_save_pending) return;

    PendingTime t = g_pending_time;
    g_time_save_pending = false;

    // Aplica al manager (RTC + NVS)
    clock_manager_apply_manual_time(t.Y, t.Mo, t.D, t.h24, t.mi, t.sec);

    // Trae el tiempo final
    ClockDateTime dt;
    bool ok = clock_manager_get_now(dt);

    if (lvgl_port_lock(0)) {
        ui_datetime_set_format_24h(t.fmt == 24);
        ui_datetime_set_editing(false);

        if (ok) ui_datetime_set_current(dt.y, dt.mo, dt.d, dt.h, dt.mi, dt.s);
        else    ui_datetime_set_current(t.Y, t.Mo, t.D, t.h24, t.mi, t.sec);

        ui_datetime_load_from_current_to_controls();
        ui_datetime_refresh_preview_label();
        lvgl_port_unlock();
    }
}
