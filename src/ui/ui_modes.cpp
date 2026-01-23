// ui_modes.cpp

#include "ui_modes.h"

#include "ui/ui.h"
#include "ui_sync.h"
#include "drivers/uart/uart.h"
#include "drivers/lvgl_port/lvgl_port.h"
#include "lvgl.h"
#include "app_helpers.h"
#include "ui/screens/ui_ScreenControl.h"   // ui_SliderLed / ui_SliderFan
#include "components/ui_comp.h"
#include "components/ui_comp_topBar.h"

// ===== Valores predefinidos por modo (0..100) =====
typedef enum {
    MODE_DEFAULT = 0,   // interno (no expuesto en ui_mode_t)
    MODE_PHONE,
    MODE_WORK,
    MODE_RELAX,
    MODE_CLEAN,
    MODE_COUNT
} mode_id_t;

// ----------------------------------------------------


// Presets editables (RAM)
// ----------------------------------------------------
static mode_cfg_t s_mode_cfg[MODE_COUNT] = {
    /* DEFAULT */ { 50,  50, 0 },
    /* PHONE   */ { 44,  18, 0 },
    /* WORK    */ {100, 100, 1 },
    /* RELAX   */ { 29,  10, 0 },
    /* CLEAN   */ {100, 100, 0 },
};
mode_cfg_t* ui_modes_presets_mut(void) { return s_mode_cfg; }
const mode_cfg_t* ui_modes_presets(void) { return s_mode_cfg; }
int ui_modes_presets_count(void) { return (int)MODE_COUNT; }

// (Opcional) recordar último modo normal (para toggle de CLEAN)
static ui_mode_t s_last_normal_mode = UI_MODE_PHONE;
bool s_clean_active = false;
static ui_mode_t s_mode_before_clean = UI_MODE_DEFAULT;
// ----------------------------------------------------
// Mapeo explícito ui_mode_t -> mode_id_t
// Evita bugs por el MODE_DEFAULT extra.
// ----------------------------------------------------
static const mode_id_t ui_to_mode[UI_MODE_CLEAN + 1] = {
    [UI_MODE_DEFAULT] = MODE_DEFAULT,
    [UI_MODE_PHONE] = MODE_PHONE,
    [UI_MODE_WORK]  = MODE_WORK,
    [UI_MODE_RELAX] = MODE_RELAX,
    [UI_MODE_CLEAN] = MODE_CLEAN,
};

static inline uint8_t clamp_u8(uint8_t v, uint8_t maxv) { return (v > maxv) ? maxv : v; }

static inline bool ui_mode_valid(ui_mode_t m) {
    return (m >= UI_MODE_DEFAULT && m <= UI_MODE_CLEAN);
}

static inline mode_id_t mode_from_ui(ui_mode_t m) {
    if (!ui_mode_valid(m)) return MODE_DEFAULT;
    return ui_to_mode[m];
}

// ----------------------------------------------------
// Presets API (para tu screen Modes)
// Recomendado: usar ui_mode_t (PHONE/WORK/RELAX/CLEAN)
// ----------------------------------------------------
mode_cfg_t ui_modes_get_preset_ui(ui_mode_t mode)
{
    mode_id_t m = mode_from_ui(mode);
    return s_mode_cfg[m]; // copia
}

void ui_modes_set_preset_ui(ui_mode_t mode, uint8_t led, uint8_t fan, uint8_t priv)
{
    mode_id_t m = mode_from_ui(mode);

    s_mode_cfg[m].led  = clamp_u8(led, 100);
    s_mode_cfg[m].fan  = clamp_u8(fan, 100);
    s_mode_cfg[m].priv = priv ? 1 : 0;

    // Aquí luego enganchas tu "save to NVS" si quieres guardar persistente.
    // modes_storage_save();
}

// ----------------------------------------------------
// Compatibilidad con tu API anterior por int (si la usabas)
// 0..3 => se interpreta como ui_mode_t (PHONE..CLEAN)
// otros => cae a DEFAULT
// ----------------------------------------------------
mode_cfg_t ui_modes_get_preset(int mode_index)
{
    if (mode_index >= (int)UI_MODE_PHONE && mode_index <= (int)UI_MODE_CLEAN) {
        return ui_modes_get_preset_ui((ui_mode_t)mode_index);
    }
    return s_mode_cfg[MODE_DEFAULT];
}

void ui_modes_set_preset(int mode_index, uint8_t led, uint8_t fan, uint8_t priv)
{
    if (mode_index >= (int)UI_MODE_PHONE && mode_index <= (int)UI_MODE_CLEAN) {
        ui_modes_set_preset_ui((ui_mode_t)mode_index, led, fan, priv);
        return;
    }
    // si te pasan algo inválido, no tocar nada (o podrías escribir DEFAULT si quieres)
}

// ---------- Estilo panel seleccionado ----------
static lv_style_t s_mode_selected_style;
static bool s_mode_style_inited = false;

static void mode_style_init_once()
{
    if (s_mode_style_inited) return;
    s_mode_style_inited = true;

    lv_style_init(&s_mode_selected_style);
    lv_style_set_bg_color(&s_mode_selected_style, lv_color_hex(0xbbbbbb));
    lv_style_set_bg_opa(&s_mode_selected_style, LV_OPA_COVER);

    // Aplica el estilo solo cuando el panel está CHECKED
    if (ui_PanelPhone) lv_obj_add_style(ui_PanelPhone, &s_mode_selected_style, LV_STATE_CHECKED);
    if (ui_PanelWork)  lv_obj_add_style(ui_PanelWork,  &s_mode_selected_style, LV_STATE_CHECKED);
    if (ui_PanelRelax) lv_obj_add_style(ui_PanelRelax, &s_mode_selected_style, LV_STATE_CHECKED);
    if (ui_PanelClean) lv_obj_add_style(ui_PanelClean, &s_mode_selected_style, LV_STATE_CHECKED);
}

static void deselect_all_mode_buttons()
{
    if (ui_ButtonPhone) lv_obj_clear_state(ui_ButtonPhone, LV_STATE_CHECKED);
    if (ui_ButtonWork)  lv_obj_clear_state(ui_ButtonWork,  LV_STATE_CHECKED);
    if (ui_ButtonRelax) lv_obj_clear_state(ui_ButtonRelax, LV_STATE_CHECKED);
    if (ui_ButtonClean) lv_obj_clear_state(ui_ButtonClean, LV_STATE_CHECKED);

    if (ui_PanelPhone) lv_obj_clear_state(ui_PanelPhone, LV_STATE_CHECKED);
    if (ui_PanelWork)  lv_obj_clear_state(ui_PanelWork,  LV_STATE_CHECKED);
    if (ui_PanelRelax) lv_obj_clear_state(ui_PanelRelax, LV_STATE_CHECKED);
    if (ui_PanelClean) lv_obj_clear_state(ui_PanelClean,  LV_STATE_CHECKED);
}
void ui_modes_deselect_all(void)
{
    // Asume que ya estás en contexto LVGL o con lock tomado
    deselect_all_mode_buttons();
}

// Aplica a STM32 + refleja en sliders del ScreenControl
static void apply_mode_common(lv_obj_t *imgbtn_to_check,
                             lv_obj_t *panel_to_check,
                             uint8_t led_pct,
                             uint8_t fan_pct,
                             uint8_t private_on,
                             bool clean_on)
{
    mode_style_init_once();

    led_pct = clamp_u8(led_pct, 100);
    fan_pct = clamp_u8(fan_pct, 100);
    private_on = private_on ? 1 : 0;

    deselect_all_mode_buttons();

    if (imgbtn_to_check) lv_obj_add_state(imgbtn_to_check, LV_STATE_CHECKED);
    if (panel_to_check)  lv_obj_add_state(panel_to_check,  LV_STATE_CHECKED);

    // 1) UART STM32 (niveles)
    Uart::setLedLevel(led_pct);
    Uart::setFanLevel(fan_pct);
    Uart::setPrivate(private_on);

    // 2) UART CLEAN ON/OFF (explícito aquí para que no quede “colgado”)
    Uart::send(Uart::CMD_CLEAN, clean_on ? 1 : 0);

    // 3) UI sliders (screen control)
    ui_set_syncing(true);

    if (ui_SliderLed) {
        lv_slider_set_range(ui_SliderLed, 0, 100);
        lv_slider_set_value(ui_SliderLed, (int)led_pct, LV_ANIM_OFF);
        ui_panel_update_img_recolor(ui_PanelLed, led_pct);
        lv_event_send(ui_SliderLed, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_SliderFan) {
        lv_slider_set_range(ui_SliderFan, 0, 100);
        lv_slider_set_value(ui_SliderFan, (int)fan_pct, LV_ANIM_OFF);
        ui_panel_update_img_recolor(ui_PanelFan, fan_pct);
        lv_event_send(ui_SliderFan, LV_EVENT_VALUE_CHANGED, NULL);
    }

    if (ui_SliderPrivate) {
        lv_slider_set_range(ui_SliderPrivate, 0, 1);
        lv_slider_set_value(ui_SliderPrivate, (int)private_on, LV_ANIM_OFF);
        ui_panel_update_img_recolor(ui_PanelPrivate, private_on);
        lv_event_send(ui_SliderPrivate, LV_EVENT_VALUE_CHANGED, NULL);
    }

    ui_set_syncing(false);
}

static void apply_mode_from_preset(mode_id_t m, lv_obj_t *btn, lv_obj_t *panel)
{
    // si es modo normal, recuerda último
    if (m == MODE_PHONE) s_last_normal_mode = UI_MODE_PHONE;
    if (m == MODE_WORK)  s_last_normal_mode = UI_MODE_WORK;
    if (m == MODE_RELAX) s_last_normal_mode = UI_MODE_RELAX;

    const mode_cfg_t *c = &s_mode_cfg[m];
    const bool clean_on = (m == MODE_CLEAN);

    apply_mode_common(btn, panel, c->led, c->fan, c->priv, clean_on);
}

// ----------------------------------------------------
// API pública: aplicar modos
// ----------------------------------------------------

void apply_mode_default(void)
{
    s_clean_active = false;
    apply_mode_from_preset(MODE_DEFAULT, NULL, NULL); // sin botón/panel
}


void apply_mode_phone(void)
{
    s_clean_active = false;
    apply_mode_from_preset(MODE_PHONE, ui_ButtonPhone, ui_PanelPhone);
}

void apply_mode_work(void)
{
    s_clean_active = false;
    apply_mode_from_preset(MODE_WORK, ui_ButtonWork, ui_PanelWork);
}

void apply_mode_relax(void)
{
    s_clean_active = false;
    apply_mode_from_preset(MODE_RELAX, ui_ButtonRelax, ui_PanelRelax);
}

// CLEAN como toggle: si ya está ON, vuelve al último modo normal.
void apply_mode_clean(void)
{
    if (s_clean_active) {
        // apagar CLEAN y volver al último modo normal
        s_clean_active = false;

        switch (s_last_normal_mode) {
            case UI_MODE_PHONE: apply_mode_phone(); break;
            case UI_MODE_WORK:  apply_mode_work();  break;
            case UI_MODE_RELAX: apply_mode_relax(); break;
            default:            apply_mode_phone(); break;
        }
        return;
    }

    // activar CLEAN
    s_clean_active = true;
    apply_mode_from_preset(MODE_CLEAN, ui_ButtonClean, ui_PanelClean);
}

// ----------------------------------------------------
// apply_mode_safe (con lock LVGL)
// ----------------------------------------------------
void apply_mode_safe(ui_mode_t mode)
{
    // 🔒 Si CLEAN está activo, ignora cambios de modo (excepto CLEAN)
    if (s_clean_active && mode != UI_MODE_CLEAN) {
        return;
    }

    if (!lvgl_port_lock(50)) return;

    switch (mode) {
        case UI_MODE_DEFAULT: apply_mode_default(); break;
        case UI_MODE_PHONE:   apply_mode_phone();   break;
        case UI_MODE_WORK:    apply_mode_work();    break;
        case UI_MODE_RELAX:   apply_mode_relax();   break;
        case UI_MODE_CLEAN:   apply_mode_clean();   break;
        default: break;
    }

    lvgl_port_unlock();
}