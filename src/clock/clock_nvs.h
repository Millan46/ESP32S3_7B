#pragma once
#include <stdbool.h>
#include "clock_manager.h"  // trae ClockDateTime

#ifdef __cplusplus
extern "C" {
#endif

void clock_nvs_init(void);
bool clock_nvs_load(ClockDateTime& out);
void clock_nvs_save(const ClockDateTime& in);

#ifdef __cplusplus
}
#endif
