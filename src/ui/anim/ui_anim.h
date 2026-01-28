#ifndef UI_ANIM_H
#define UI_ANIM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_logo_start_sequence(uint32_t backlight_delay_ms,
                            uint32_t fade_time_ms);
bool ui_logo_is_done(void);

#ifdef __cplusplus
}
#endif

#endif
