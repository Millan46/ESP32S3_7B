#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ClockDateTime {
    int y;
    int mo;
    int d;
    int h;
    int mi;
    int s;
} ClockDateTime;

void clock_manager_init(void);

bool clock_manager_get_now(ClockDateTime& out);
bool clock_manager_sync_from_rtc(void);

void clock_manager_apply_manual_time(int Y, int Mo, int D, int h24, int mi, int sec);

// ✅ nuevo: avanza el reloj 1s
void clock_manager_tick_1s(void);

#ifdef __cplusplus
}
#endif
