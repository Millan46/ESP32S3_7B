#pragma once
#include <stdint.h>
#include <stdbool.h>

void clock_nvs_init();
bool clock_nvs_load(int64_t *epoch_out);
void clock_nvs_save(int64_t epoch);
