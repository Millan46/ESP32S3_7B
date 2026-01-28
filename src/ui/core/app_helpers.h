#pragma once
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Cambia el color del indicador Cabin (topbar component)
void app_cabin_indicator_set_color(uint32_t hex_rgb);
void app_cabin_indicator_refresh();  
void update_cabin_indicator(void);

void app_bl_init(void);
void app_bl_register_activity(uint32_t now_ms);
void app_bl_tick(uint32_t now_ms);

// Envía por UART los valores actuales del UI
// Fan/Led 0..100, Private 0..1
void app_send_current_levels(void);

void ui_panel_update_img_recolor(lv_obj_t* panel, int value);
bool app_is_timer_active(void);


#ifdef __cplusplus
}
#endif
