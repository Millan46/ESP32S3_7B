#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ui_is_syncing(void);
void ui_set_syncing(bool v);

void ui_sync_led_level_from_stm(uint8_t pct0_100);
void ui_sync_fan_level_from_stm(uint8_t pct0_100);
void ui_sync_led_timer_from_stm(uint8_t sel0_4);
void ui_sync_fan_timer_from_stm(uint8_t sel0_4);

#ifdef __cplusplus
}
#endif
