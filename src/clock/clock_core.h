#pragma once
#include <stdint.h>
#include <stdbool.h>

void clock_init();

bool clock_time_is_valid();
void clock_set_system_ymdhms(int Y,int Mo,int D,int H,int Mi,int S);

bool clock_restore_fallback();
void clock_save_fallback_now();

void clock_periodic_save_tick(uint32_t every_ms);
