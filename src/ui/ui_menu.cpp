#include "ui_menu.h"

#include "lvgl.h"
#include "ui.h"

#include "screens/ui_ScreenMain.h"
#include "screens/ui_ScreenSettings.h"

#include "screens/ui_ScreenOption.h"
#include "screens/ui_ScreenTimer.h"
#include "screens/ui_ScreenDate.h"
#include "screens/ui_ScreenModes.h"

static inline void nav_to(lv_obj_t *screen)
{
    if (screen) lv_scr_load(screen);
}

// ===== Main -> Settings =====
static void on_open_settings(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    nav_to(ui_ScreenSettings);
}

// ===== Option -> Timer/Date/Modes =====
static void on_option_timer(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    nav_to(ui_ScreenTimer);
}

static void on_option_date(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    nav_to(ui_ScreenDate);
}

static void on_option_modes(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    nav_to(ui_ScreenModes);
}

// ===== Option back -> Main =====
static void on_back_option_to_main(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    nav_to(ui_ScreenMain);
}

// ===== Back (Timer/Date/Modes) -> Option =====
static void on_back_to_option(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    nav_to(ui_ScreenOption);
}

// ===== Settings back -> Main =====
static void on_back_settings_to_main(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    nav_to(ui_ScreenMain);
}

void ui_menu_bind(void)
{
    static bool bound = false;
    if (bound) return;
    bound = true;

    // ===== Main =====
    // Main sí tiene Settings:
    if (ui_ButtonSettings) {
        lv_obj_add_event_cb(ui_ButtonSettings, on_open_settings, LV_EVENT_CLICKED, NULL);
    }

    // ===== Option =====
    if (ui_ButtonTimer)  lv_obj_add_event_cb(ui_ButtonTimer,  on_option_timer, LV_EVENT_CLICKED, NULL);
    if (ui_ButtonDate)   lv_obj_add_event_cb(ui_ButtonDate,   on_option_date,  LV_EVENT_CLICKED, NULL);
    if (ui_ButtonModes)  lv_obj_add_event_cb(ui_ButtonModes,  on_option_modes, LV_EVENT_CLICKED, NULL);

    if (ui_ButtonBackOption) {
        lv_obj_add_event_cb(ui_ButtonBackOption, on_back_option_to_main, LV_EVENT_CLICKED, NULL);
    }

    // ===== Back buttons =====
    if (ui_ButtonBackTimer)  lv_obj_add_event_cb(ui_ButtonBackTimer,  on_back_to_option, LV_EVENT_CLICKED, NULL);
    if (ui_ButtonBackDate)   lv_obj_add_event_cb(ui_ButtonBackDate,   on_back_to_option, LV_EVENT_CLICKED, NULL);
    if (ui_ButtonBackModes)  lv_obj_add_event_cb(ui_ButtonBackModes,  on_back_to_option, LV_EVENT_CLICKED, NULL);

    // Settings usa BackHome:
    if (ui_ButtonBackHome)   lv_obj_add_event_cb(ui_ButtonBackHome,   on_back_settings_to_main, LV_EVENT_CLICKED, NULL);
}
