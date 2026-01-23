#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct PendingTime {
  int Y, Mo, D, h24, mi, sec;
  uint8_t fmt;
} PendingTime;

extern volatile bool g_time_save_pending;
extern PendingTime g_pending_time;
