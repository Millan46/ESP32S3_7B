#pragma once
#include <stdbool.h>

void clock_rtc_init();
bool clock_rtc_is_available();
void clock_rtc_try_save_from_system();
