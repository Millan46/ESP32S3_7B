#include <Arduino.h>
#include "ui_anim.h"
#include "ui/ui.h"
#include "drivers/rgb_lcd_port/rgb_lcd_port.h"
#include "drivers/io_extension/io_extension.h"
#include "lvgl.h"
#include "drivers/lvgl_port/lvgl_port.h"


// ---------- helpers ----------
static void set_opa_cb(void * obj, int32_t v)
{
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, LV_PART_MAIN);
}

static void fade_ready_cb(lv_anim_t * a)
{
    if (lvgl_port_lock(-1)) {
        lv_scr_load(ui_ScreenMain);
        lvgl_port_unlock();
    }
}

// ---------- animación logo ----------
static void logo_fade_in(uint32_t fade_time_ms)
{
    if(!ui_ImageLogo) return;

    lv_obj_set_style_opa(ui_ImageLogo, LV_OPA_0, LV_PART_MAIN);

    lv_anim_t anim;
    lv_anim_init(&anim);
    lv_anim_set_var(&anim, ui_ImageLogo);
    lv_anim_set_exec_cb(&anim, set_opa_cb);
    lv_anim_set_values(&anim, LV_OPA_0, LV_OPA_COVER);
    lv_anim_set_time(&anim, fade_time_ms);
    lv_anim_set_path_cb(&anim, lv_anim_path_ease_in_out);
    lv_anim_set_ready_cb(&anim, fade_ready_cb);
    lv_anim_start(&anim);
}

// ---------- timer ----------
static void backlight_timer_cb(lv_timer_t * t)
{
    wavesahre_rgb_lcd_bl_on();
    logo_fade_in((uint32_t)t->user_data);
    lv_timer_del(t);
}

// ---------- API pública ----------
void ui_logo_start_sequence(uint32_t backlight_delay_ms,
                            uint32_t fade_time_ms)
{
    // Garantiza estado inicial
    wavesahre_rgb_lcd_bl_off();

    // Logo invisible
    if(ui_ImageLogo) {
        lv_obj_set_style_opa(ui_ImageLogo, LV_OPA_0, LV_PART_MAIN);
    }

    // Timer: BL ON -> fade
    lv_timer_t * t = lv_timer_create(backlight_timer_cb,
                                     backlight_delay_ms,
                                     (void*)fade_time_ms);
    lv_timer_set_repeat_count(t, 1);
}
