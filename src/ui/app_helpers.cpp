#include "app_helpers.h"

#include "ui/ui.h"
#include "ui/components/ui_comp.h"
#include "ui/components/ui_comp_topBar.h"

#include "drivers/uart/uart.h"
#include <stdbool.h>
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
}
