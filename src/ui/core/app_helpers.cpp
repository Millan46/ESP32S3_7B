#include "app_helpers.h"

#include "ui/ui.h"
#include "ui/components/ui_comp.h"
#include "ui/components/ui_comp_topBar.h"
#include "drivers/lvgl_port/lvgl_port.h"
#include "drivers/uart/uart.h"
#include <stdbool.h>


#define PANEL_IMG_ON   0xFBFBFB  // blanco
#define PANEL_IMG_OFF  0xD0D0D0  // gris oscuro (ajusta si quieres más oscuro)


// Ajusta el timeout aquí
static const uint32_t BL_TIMEOUT_MS = 60000;

static uint32_t s_last_activity_ms = 0;
static bool s_bl_on = true;


static lv_obj_t* topbar_find_indicator_cabin(lv_obj_t* root)
{
    if (!root) return nullptr;

    lv_obj_t* ind = ui_comp_get_child(root, UI_COMP_TOPBAR_INDICATORCABIN);
    if (ind) return ind;

    uint32_t cnt = lv_obj_get_child_cnt(root);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t* ch = lv_obj_get_child(root, i);
        ind = topbar_find_indicator_cabin(ch);
        if (ind) return ind;
    }
    return nullptr;
}


void app_cabin_indicator_set_color(uint32_t color_hex)
{
    lv_obj_t* scr = lv_scr_act();
    lv_obj_t* ind = topbar_find_indicator_cabin(scr);
    if (!ind) return;

    // Caso típico: indicador es un objeto/panel con color de fondo
    lv_obj_set_style_bg_color(ind, lv_color_hex(color_hex), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ind, LV_OPA_COVER, LV_PART_MAIN);
}


void app_send_current_levels(void)
{
    // Fan 0..100
    if (ui_SliderFan) {
        uint8_t v = (uint8_t)lv_slider_get_value(ui_SliderFan);
        if (v > 100) v = 100;
        Uart::setFanLevel(v);
    }

    // Led 0..100
    if (ui_SliderLed) {
        uint8_t v = (uint8_t)lv_slider_get_value(ui_SliderLed);
        if (v > 100) v = 100;
        Uart::setLedLevel(v);
    }

    // Private 0..1
    if (ui_SliderPrivate) {
        uint8_t v = (uint8_t)lv_slider_get_value(ui_SliderPrivate);
        if (v > 1) v = 1;
        Uart::setPrivate(v);
    }

    // Lock 0..1
    if (ui_SliderLock) {
       uint8_t v = (uint8_t)lv_slider_get_value(ui_SliderLock);
       if (v > 1) v = 1;
       Uart::setLock(v);
    }
}


static inline void bl_set(bool on)
{
    // 👇 AQUI es donde usas TU helper real / driver real:
    // Si ya tienes funciones tipo app_display_bl_on/off, úsalas aquí.
    if (on) {
        wavesahre_rgb_lcd_bl_on();
    } else {
        wavesahre_rgb_lcd_bl_off(); // si no existe, dime y lo adapto a tu pin
    }
    s_bl_on = on;
}

void app_bl_init(void)
{
    s_last_activity_ms = millis();
    s_bl_on = true;
    bl_set(true);
}

void app_bl_tick(uint32_t now_ms)
{
    // ⛔ Mientras el timer esté activo, NO apagar la pantalla
    if (app_is_timer_active()) {
        if (!s_bl_on) {
            bl_set(true);
        }
        return;
    }

    // Comportamiento normal por timeout
    if (s_bl_on && (now_ms - s_last_activity_ms >= BL_TIMEOUT_MS)) {
        bl_set(false);
    }
}


void ui_panel_update_img_recolor(lv_obj_t* panel, int value)
{
    if (!panel) return;

    uint32_t color = (value > 0) ? PANEL_IMG_ON : PANEL_IMG_OFF;

    lv_obj_set_style_bg_img_recolor(
        panel,
        lv_color_hex(color),
        LV_PART_MAIN
    );

    lv_obj_set_style_bg_img_recolor_opa(
        panel,
        LV_OPA_COVER,
        LV_PART_MAIN
    );

    lv_obj_invalidate(panel);
}

extern bool app_is_timer_active(void);

void app_bl_register_activity(uint32_t now_ms)
{
    s_last_activity_ms = now_ms;

    // Si el timer está activo, fuerza pantalla encendida
    if (app_is_timer_active()) {
        if (!s_bl_on) {
            s_bl_on = true;
            bl_set(true);
        }
        return;
    }

    // Comportamiento normal
    if (!s_bl_on) {
        s_bl_on = true;
        bl_set(true);
    }
}
