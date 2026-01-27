// ui_events.cpp
// Custom handlers declared in ui_events.h (SquareLine Studio 1.6.0)
// Project: HMI_ESP32_S3_7B | LVGL 8.3.11
//
// IMPORTANT:
// - Do NOT implement wrapper functions generated inside ui/screens/ui_Screen*.c
//   (e.g. ui_event_SliderFan_Slider1, ui_event_ButtonClean, ui_event_Calendar, etc.)
// - Those wrappers call the handlers declared in ui_events.h (implemented here).

#include "ui.h"
#include "lvgl.h"
#include <sys/time.h>
#include <time.h>
#include <cstring>
// App / drivers
#include "drivers/uart/uart.h"
#include "ui_sync.h"
#include "ui_modes.h"
#include "ui_modes_storage.h"
#include "pin_store.h"

// Components (topBar child access)
#include "components/ui_comp.h"
#include "components/ui_comp_topBar.h"

// Your custom helpers (C++ .cpp)
#include "app_helpers.h"
#include "ui_datetime.h"
#include "time_job.h"

extern bool s_clean_active;

// =======================================================
// Local state
// =======================================================

static lv_obj_t * s_pin_active = nullptr;  // active PIN textarea

static char g_pin_user[8] = "1234";
static char g_pin_adv[8]  = "4567";

// =======================================================
// Small helpers
// =======================================================
static void set_system_time(int Y,int Mo,int D,int H,int Mi,int S)
{
    struct tm t = {};
    t.tm_year = Y - 1900;
    t.tm_mon  = Mo - 1;
    t.tm_mday = D;
    t.tm_hour = H;
    t.tm_min  = Mi;
    t.tm_sec  = S;

    time_t epoch = mktime(&t);
    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, nullptr);
}

static inline void go_screen(lv_obj_t * scr)
{
    if (scr) lv_scr_load(scr);
}

static void pin_set_active(lv_obj_t * ta)
{
    if (s_pin_active == ta) return;

    lv_group_t * g = lv_group_get_default();

    if (s_pin_active) lv_obj_clear_state(s_pin_active, LV_STATE_FOCUSED);

    if (g) {
        lv_group_set_editing(g, false);     // suelta edición
        if (ta == NULL) lv_group_focus_freeze(g, false); // por si estaba congelado
    }

    s_pin_active = ta;

    if (!s_pin_active) {
        // ✅ suelta también el foco del group para que no apunte a un obj muerto
        if (g) lv_group_focus_obj(lv_scr_act()); // o cualquier objeto "seguro"
        return;
    }

    lv_obj_clear_state(s_pin_active, LV_STATE_DISABLED);
    lv_obj_add_state(s_pin_active, LV_STATE_FOCUSED);
    lv_textarea_set_cursor_click_pos(s_pin_active, true);

    // foco real
    if (g) {
        lv_group_focus_obj(s_pin_active);
        lv_group_set_editing(g, true);
    }
}


static inline bool is_textarea(lv_obj_t * obj)
{
    return obj && lv_obj_check_type(obj, &lv_textarea_class);
}

// ---- Settings: force placeholder "Pin" and masked input
static void settings_prepare_cb(void * /*arg*/)
{
    if (!ui_TextAreaPinSettings) return;

    lv_textarea_set_text(ui_TextAreaPinSettings, "");
    lv_textarea_set_placeholder_text(ui_TextAreaPinSettings, "Pin");

    lv_textarea_set_password_mode(ui_TextAreaPinSettings, true);
    lv_textarea_set_password_show_time(ui_TextAreaPinSettings, 0);

    pin_set_active(ui_TextAreaPinSettings);

    lv_obj_invalidate(ui_TextAreaPinSettings);
}

// =============================
// UI feedback helpers (border flash)
// =============================
static void ta_flash_border(lv_obj_t *ta, uint32_t hex_rgb)
{
    if (!ta) return;

    // Aplica borde visible al textarea
    lv_obj_set_style_border_width(ta, 3, LV_PART_MAIN);
    lv_obj_set_style_border_color(ta, lv_color_hex(hex_rgb), LV_PART_MAIN);
    lv_obj_set_style_border_opa(ta, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_invalidate(ta);
}

static void ta_clear_border_cb(void *arg)
{
    lv_obj_t *ta = (lv_obj_t *)arg;
    if (!ta) return;

    // Quita el borde (regresa al estilo original)
    lv_obj_set_style_border_width(ta, 0, LV_PART_MAIN);
    lv_obj_invalidate(ta);
}

// Verde 1.2s
static void ta_ok(lv_obj_t *ta)
{
    ta_flash_border(ta, 0x00C853);
    lv_async_call(ta_clear_border_cb, ta);
    // Nota: lv_async_call se ejecuta "pronto", pero queremos delay real.
    // Para delay real usamos timer:
}

// Rojo 1.2s
static void ta_bad(lv_obj_t *ta)
{
    ta_flash_border(ta, 0xD50000);
    // delay real con timer:
}

// Timer-based clear (real delay)
static void ta_clear_border_timer(lv_timer_t *t)
{
    lv_obj_t *ta = (lv_obj_t *)t->user_data;
    if (ta) {
        lv_obj_set_style_border_width(ta, 0, LV_PART_MAIN);
        lv_obj_invalidate(ta);
    }
    lv_timer_del(t);
}

static void ta_flash_ok(lv_obj_t *ta)
{
    ta_flash_border(ta, 0x00C853);
    lv_timer_create(ta_clear_border_timer, 1200, ta);
}

static void ta_flash_bad(lv_obj_t *ta)
{
    ta_flash_border(ta, 0xD50000);
    lv_timer_create(ta_clear_border_timer, 1200, ta);
}


// ---- Pin screen: load pins and show placeholders
static void screenpin_prepare(void)
{
    pin_load(g_pin_user, sizeof(g_pin_user), g_pin_adv, sizeof(g_pin_adv));

    if (ui_TextAreaPinUser) {
        lv_textarea_set_text(ui_TextAreaPinUser, "");
        lv_textarea_set_placeholder_text(ui_TextAreaPinUser, g_pin_user);
        lv_textarea_set_password_mode(ui_TextAreaPinUser, true);
        lv_textarea_set_password_show_time(ui_TextAreaPinUser, 0);
    }

    if (ui_TextAreaPinAdvanced) {
        lv_textarea_set_text(ui_TextAreaPinAdvanced, "");
        lv_textarea_set_placeholder_text(ui_TextAreaPinAdvanced, g_pin_adv);
        lv_textarea_set_password_mode(ui_TextAreaPinAdvanced, true);
        lv_textarea_set_password_show_time(ui_TextAreaPinAdvanced, 0);
    }

    pin_set_active(ui_TextAreaPinUser);

}

// ---- PIN keypad operations
static inline void pin_clear_active(void)
{
    if (!s_pin_active) return;
    if (!lv_obj_check_type(s_pin_active, &lv_textarea_class)) return;
    lv_textarea_set_text(s_pin_active, "");
}

static inline void pin_append(char c)
{
    if (!is_textarea(s_pin_active)) return;

    const char * t = lv_textarea_get_text(s_pin_active);
    if (t && std::strlen(t) >= 6) return;  // optional length limit

    char s[2] = { c, 0 };
    lv_textarea_add_text(s_pin_active, s);
}

static inline void pin_del(void)
{
    if (!is_textarea(s_pin_active)) return;
    lv_textarea_del_char(s_pin_active);
}

static inline const char * pin_get_text(void)
{
    if (!is_textarea(s_pin_active)) return "";
    return lv_textarea_get_text(s_pin_active);
}

static bool is_digits_only(const char *s)
{
    if (!s || !*s) return false;
    for (; *s; s++) {
        if (*s < '0' || *s > '9') return false;
    }
    return true;
}

static inline void ui_wait_touch_release(void)
{
    lv_indev_t * indev = lv_indev_get_act();
    if (indev) lv_indev_wait_release(indev);
}

static inline void pin_ok(void)
{
    const char *p = pin_get_text();
    if (!p) p = "";

    // =========================
    // A) SETTINGS = LOGIN
    // =========================
    if (s_pin_active == ui_TextAreaPinSettings) {

        if (std::strcmp(p, g_pin_user) == 0) {
            ta_flash_ok(ui_TextAreaPinSettings);
            pin_clear_active();

            // ✅ importantísimo para que no quede el touch "capturado"
            pin_set_active(nullptr);
            ui_wait_touch_release();

            go_screen(ui_ScreenOption);
        }
        else if (std::strcmp(p, g_pin_adv) == 0) {
            ta_flash_ok(ui_TextAreaPinSettings);
            pin_clear_active();

            // ✅ importantísimo
            pin_set_active(nullptr);
            ui_wait_touch_release();

            go_screen(ui_ScreenAdvanced);
        }
        else {
            ta_flash_bad(ui_TextAreaPinSettings);
            pin_clear_active();
        }
        return;
    }

    // =========================
    // B) SCREENPIN = GUARDAR NUEVO PIN
    // =========================
    if (lv_scr_act() == ui_ScreenPin) {

        // validación: 4..6 dígitos
        size_t n = std::strlen(p);
        if (!is_digits_only(p) || n < 4 || n > 6) {
            if (s_pin_active) ta_flash_bad(s_pin_active);
            pin_clear_active();
            return;
        }

        // Guardar USER
        if (s_pin_active == ui_TextAreaPinUser) {
            std::strncpy(g_pin_user, p, sizeof(g_pin_user) - 1);
            g_pin_user[sizeof(g_pin_user) - 1] = '\0';

            pin_save(g_pin_user, g_pin_adv);

            ta_flash_ok(ui_TextAreaPinUser);
            lv_textarea_set_text(ui_TextAreaPinUser, "");
            lv_textarea_set_placeholder_text(ui_TextAreaPinUser, g_pin_user);

            // opcional: seguir escribiendo en USER
            pin_set_active(ui_TextAreaPinUser);
            return;
        }

        // Guardar ADV
        if (s_pin_active == ui_TextAreaPinAdvanced) {
            std::strncpy(g_pin_adv, p, sizeof(g_pin_adv) - 1);
            g_pin_adv[sizeof(g_pin_adv) - 1] = '\0';

            pin_save(g_pin_user, g_pin_adv);

            ta_flash_ok(ui_TextAreaPinAdvanced);
            lv_textarea_set_text(ui_TextAreaPinAdvanced, "");
            lv_textarea_set_placeholder_text(ui_TextAreaPinAdvanced, g_pin_adv);

            // opcional: seguir escribiendo en ADV
            pin_set_active(ui_TextAreaPinAdvanced);
            return;
        }

        if (s_pin_active) ta_flash_bad(s_pin_active);
        pin_clear_active();
        return;
    }
}

// =======================================================
// ============== Handlers from ui_events.h ==============
// =======================================================

// ---------- Preset modes ----------
void ui_event_ButtonPhone_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    apply_mode_phone();
}

void ui_event_ButtonWork_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    apply_mode_work();
}

void ui_event_ButtonRelax_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    apply_mode_relax();
}

void ui_event_FanSlider_ValueChanged(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    if (ui_is_syncing()) return;

    ui_modes_deselect_all();

    uint8_t v = (uint8_t)lv_slider_get_value(ui_SliderFan);
    if (v > 100) v = 100;
    ui_panel_update_img_recolor(ui_PanelFan, v);
    Uart::setFanLevel(v);
}

void ui_event_LightSlider_ValueChanged(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    if (ui_is_syncing()) return;

    ui_modes_deselect_all();

    uint8_t v = (uint8_t)lv_slider_get_value(ui_SliderLed);
    if (v > 100) v = 100;
    ui_panel_update_img_recolor(ui_PanelLed, v);
    Uart::setLedLevel(v);
}

void ui_event_PrivateSlider_ValueChanged(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    if (ui_is_syncing()) return;

    ui_modes_deselect_all();

    uint8_t v = (uint8_t)lv_slider_get_value(ui_SliderPrivate); // 0/1
    if (v > 1) v = 1;
    ui_panel_update_img_recolor(ui_PanelPrivate, v);
    Uart::setPrivate(v);
}


// ---------- Main -> Settings ----------
void ui_event_ButtonSettings_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    // ✅ suelta cualquier TA del ScreenPin ANTES de irte
    pin_set_active(NULL);

    go_screen(ui_ScreenSettings);

    // ✅ cuando la screen ya esté cargada, activa el TA de settings
    lv_async_call(settings_prepare_cb, NULL);
}

// ---------- CLEAN toggle ----------
void ui_event_ButtonClean_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    apply_mode_clean();
    update_cabin_indicator();  // 🔥 actualiza indicador al instant

}


// ---------- Date screen ----------


void ui_event_ButtonSaveDate_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    int Y, Mo, D, h24, mi;
    ui_datetime_get_values(&Y, &Mo, &D, &h24, &mi);

    uint16_t sel = lv_dropdown_get_selected(ui_DropdownFormat);
    uint8_t fmt = (sel == 1) ? 24 : 12;  // según tu dropdown

    g_pending_time.Y   = Y;
    g_pending_time.Mo  = Mo;
    g_pending_time.D   = D;
    g_pending_time.h24 = h24;
    g_pending_time.mi  = mi;
    g_pending_time.sec = 0;
    g_pending_time.fmt = fmt;

    g_time_save_pending = true;


    // preview inmediato
    ui_datetime_refresh_preview_label();
}



void ui_event_ButtonBackDate_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenOption);
}

// ---------- Settings PIN focus/defocus ----------
void ui_event_PinSettings_Focused(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_FOCUSED &&
        lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    // engancha el keypad al TA de Settings
    pin_set_active(ui_TextAreaPinSettings);

    // opcional: asegura cursor por click
    lv_textarea_set_cursor_click_pos(ui_TextAreaPinSettings, true);

    // si tienes teclado en pantalla:
    // lv_keyboard_set_textarea(ui_Keyboard, ui_TextAreaPinSettings);
}

void ui_event_PinSettings_Defocused(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_DEFOCUSED) return;

    // ⚠️ NO llames pin_set_active(NULL) aquí.
    // En cambios de screen LVGL dispara DEFOCUSED y eso te apaga el editing del group.
}


// ---------- Back from Settings to Main ----------
void ui_event_ButtonBackHome_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenMain);
}

// ---------- PIN keypad buttons ----------
void ui_event_Button1_Clicked(lv_event_t * e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) pin_append('1'); }
void ui_event_Button2_Clicked(lv_event_t * e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) pin_append('2'); }
void ui_event_Button3_Clicked(lv_event_t * e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) pin_append('3'); }
void ui_event_Button4_Clicked(lv_event_t * e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) pin_append('4'); }
void ui_event_Button5_Clicked(lv_event_t * e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) pin_append('5'); }
void ui_event_Button6_Clicked(lv_event_t * e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) pin_append('6'); }
void ui_event_Button7_Clicked(lv_event_t * e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) pin_append('7'); }
void ui_event_Button8_Clicked(lv_event_t * e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) pin_append('8'); }
void ui_event_Button9_Clicked(lv_event_t * e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) pin_append('9'); }
void ui_event_Button0_Clicked(lv_event_t * e){ if(lv_event_get_code(e)==LV_EVENT_CLICKED) pin_append('0'); }

void ui_event_ButtonDel_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    pin_del();
}

void ui_event_ButtonOk_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    pin_ok();
}

// ---------- Timer ----------
void ui_event_ButtonSaveTimer_clicked(lv_event_t * e)
{
    (void)e;
    // optional: commit action
}

void ui_event_RollerFanTimer_Ready(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_READY) return;
    if (ui_is_syncing()) return;

    uint16_t idx = lv_roller_get_selected(lv_event_get_target(e));
    if (idx > 4) idx = 4;
    Uart::setFanTimer(idx);
}

void ui_event_RollerFanTimer_Change(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    if (ui_is_syncing()) return;

    uint16_t idx = lv_roller_get_selected(lv_event_get_target(e));
    if (idx > 4) idx = 4;
    Uart::setFanTimer(idx);
}

void ui_event_RollerLedTimer_Ready(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_READY) return;
    if (ui_is_syncing()) return;

    uint16_t idx = lv_roller_get_selected(lv_event_get_target(e));
    if (idx > 4) idx = 4;
    Uart::setLedTimer(idx);
}

void ui_event_RollerLedTimer_Change(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    if (ui_is_syncing()) return;

    uint16_t idx = lv_roller_get_selected(lv_event_get_target(e));
    if (idx > 4) idx = 4;
    Uart::setLedTimer(idx);
}

void ui_event_ButtonBackTimer_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenOption);
}

// ---------- Option screen navigation ----------
void ui_event_ButtonTimer_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenTimer);
}

void ui_event_ButtonDate_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    go_screen(ui_ScreenDate);

    // Lee dropdown 12/24 y aplica al RollerH
    uint16_t sel = lv_dropdown_get_selected(ui_DropdownFormat);
    ui_datetime_set_format_24h(sel == 1);

    // Mostrar/ocultar AM/PM según formato (opcional pero recomendado)
    if (ui_DropdownAmPm) {
        if (sel == 1) lv_obj_add_flag(ui_DropdownAmPm, LV_OBJ_FLAG_HIDDEN);
        else          lv_obj_clear_flag(ui_DropdownAmPm, LV_OBJ_FLAG_HIDDEN);
    }

    ui_datetime_refresh_days_keep_selection();
    ui_datetime_refresh_preview_label();
}


void ui_event_ButtonModes_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenModes);
}

void ui_event_ButtonBackOption_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenMain);
}


void ui_event_ButtonBackPin_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenMain);
}

void ui_event_ButtonPin_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenPin);
    screenpin_prepare();
}

// ---------- Info / Advanced navigation ----------
void ui_event_ButtonInfo_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenInfo);
}

void ui_event_ButtonBackAdvanced_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenSettings);
}

// ---------- Modes screen (stubs ready for expansion) ----------
void ui_event_RollerModes_Ready(lv_event_t * e)   { (void)e; }
void ui_event_RollerModes_Change(lv_event_t * e)
{
    (void)e;
    if (!ui_RollerModes) return;

    int idx = lv_roller_get_selected(ui_RollerModes);
    if (idx < (int)UI_MODE_DEFAULT) idx = (int)UI_MODE_DEFAULT;
    if (idx > (int)UI_MODE_CLEAN)   idx = (int)UI_MODE_CLEAN;

    ui_mode_t mode = (ui_mode_t)idx;
    mode_cfg_t p = ui_modes_get_preset_ui(mode);

    ui_set_syncing(true);
    if (ui_SliderModesLed)     lv_slider_set_value(ui_SliderModesLed, p.led, LV_ANIM_OFF);
    if (ui_SliderModesFan)     lv_slider_set_value(ui_SliderModesFan, p.fan, LV_ANIM_OFF);
    if (ui_SliderModesPrivate) lv_slider_set_value(ui_SliderModesPrivate, p.priv, LV_ANIM_OFF);
    ui_set_syncing(false);

    // ✅ Fuerza actualización de labels (llamando tus handlers)
lv_event_send(ui_SliderModesLed,     LV_EVENT_VALUE_CHANGED, NULL);
lv_event_send(ui_SliderModesFan,     LV_EVENT_VALUE_CHANGED, NULL);
lv_event_send(ui_SliderModesPrivate, LV_EVENT_VALUE_CHANGED, NULL);
}


void ui_event_ModesFanSlider_ValueChanged(lv_event_t * e)      { (void)e; }
void ui_event_ModesLightSlider_ValueChanged(lv_event_t * e)    { (void)e; }
void ui_event_ModesPrivateSlider_ValueChanged(lv_event_t * e)  { (void)e; }

void ui_event_ButtonSaveModes_Clicked(lv_event_t * e) { (void)e; 
    
    int idx = lv_roller_get_selected(ui_RollerModes);
    if (idx < (int)UI_MODE_DEFAULT) idx = (int)UI_MODE_DEFAULT;
    if (idx > (int)UI_MODE_CLEAN)   idx = (int)UI_MODE_CLEAN;

    ui_mode_t mode = (ui_mode_t)idx;

    uint8_t led  = (uint8_t)lv_slider_get_value(ui_SliderModesLed);
    uint8_t fan  = (uint8_t)lv_slider_get_value(ui_SliderModesFan);
    uint8_t priv = (uint8_t)lv_slider_get_value(ui_SliderModesPrivate);

    ui_modes_set_preset_ui(mode, led, fan, priv);

    // ✅ Guardar persistente
    modes_storage_save();
}

void ui_event_ButtonBackModes_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenOption);
}

// ---------- Back buttons for other screens ----------
void ui_event_ButtonBackControl_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenMain);
}

void ui_event_ButtonBackInfo_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    go_screen(ui_ScreenMain);
}

// =============================
// SETTINGS - PIN textarea focus
// =============================

void ui_event_TextAreaPinSettings_Focused(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_FOCUSED) return;

    // 🔑 El keypad ahora escribe en Settings
    pin_set_active(ui_TextAreaPinSettings);

    // Asegura focus visual
    lv_obj_add_state(ui_TextAreaPinSettings, LV_STATE_FOCUSED);
}

void ui_event_TextAreaPinSettings_Defocused(lv_event_t * e)
{
    if (lv_event_get_code(e) != LV_EVENT_DEFOCUSED) return;

}

void ui_event_RollerAno_ValueChanged(lv_event_t * e)
{
    (void)e;
    ui_datetime_refresh_days_keep_selection();
    ui_datetime_refresh_preview_label();
}

void ui_event_RollerMes_ValueChanged(lv_event_t * e)
{
    (void)e;
    ui_datetime_refresh_days_keep_selection();
    ui_datetime_refresh_preview_label();
}

void ui_event_RollerDia_ValueChanged(lv_event_t * e) { (void)e; ui_datetime_refresh_preview_label(); }
void ui_event_RollerH_ValueChanged(lv_event_t * e)   { (void)e; ui_datetime_refresh_preview_label(); }
void ui_event_RollerM_ValueChanged(lv_event_t * e)   { (void)e; ui_datetime_refresh_preview_label(); }

void ui_event_DropdownFormat_ValueChanged(lv_event_t *e)
{
    uint16_t sel = lv_dropdown_get_selected(lv_event_get_target(e));
    ui_datetime_set_format_24h(sel == 1);
    ui_datetime_refresh_preview_label();
}


void ui_event_DropdownAmPm_ValueChanged(lv_event_t *e)
{
    uint16_t sel = lv_dropdown_get_selected(lv_event_get_target(e));
    ui_datetime_set_pm(sel == 1);   // 0=AM, 1=PM
    ui_datetime_refresh_preview_label();
}

void ui_event_IR_Enable(lv_event_t * e)
{
    if(lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;

    lv_obj_t * sw = lv_event_get_target(e);
    bool ir_on = lv_obj_has_state(sw, LV_STATE_CHECKED);

    // UART IR ON / OFF
    Uart::send(Uart::CMD_IR_ENABLE, ir_on ? 1 : 0);
}
void ui_event_PinUser_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        pin_set_active(ui_TextAreaPinUser);
}

void ui_event_PinAdvanced_Clicked(lv_event_t * e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        pin_set_active(ui_TextAreaPinAdvanced);
}
