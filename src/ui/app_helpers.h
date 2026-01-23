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

// Envía por UART los valores actuales del UI
// Fan/Led 0..100, Private 0..1
void app_send_current_levels(void);

#ifdef __cplusplus
}
#endif
