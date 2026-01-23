#pragma once
#include <stdbool.h>

void clock_manager_init();                  // init NVS + rtc stub
void clock_manager_apply_manual_time(int Y,int Mo,int D,int h24,int mi,int sec);
