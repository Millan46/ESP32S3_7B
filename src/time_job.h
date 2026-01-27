#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PendingTime {
  int Y, Mo, D, h24, mi, sec;
  uint8_t fmt;
} PendingTime;

extern volatile bool g_time_save_pending;
extern PendingTime g_pending_time;

// 👇 FALTA ESTO
void time_job_tick(void);

#ifdef __cplusplus
}
#endif
