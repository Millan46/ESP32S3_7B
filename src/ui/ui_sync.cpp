#include "ui_sync.h"

#include "lvgl.h"
#include "ui.h"
#include "core/app_helpers.h"

// Traemos los nombres reales de objetos de SquareLine
#include "screens/ui_ScreenControl.h"
#include "screens/ui_ScreenTimer.h"

// ==================================================
// Protección anti-loop (ESP32 <-> STM32)
// ==================================================
static volatile bool s_sync_ui = false;

bool ui_is_syncing(void) { return s_sync_ui; }
void ui_set_syncing(bool v) { s_sync_ui = v; }

// ==================================================
// Mensajes async hacia LVGL
// ==================================================
typedef struct {
  uint8_t type;
  uint8_t value;
} UiSyncMsg;

enum : uint8_t {
  UI_SYNC_LED_PCT = 1,     // 0..100
  UI_SYNC_FAN_PCT,         // 0..100
  UI_SYNC_LED_TIMER,       // 0..4 (ajusta si cambias TIME_LIST_COUNT)
  UI_SYNC_FAN_TIMER,       // 0..4
};

// ==================================================
// Callback ejecutado EN CONTEXTO LVGL
// ==================================================
static void ui_sync_apply_cb(void *p)
{
  UiSyncMsg *m = (UiSyncMsg *)p;
  ui_set_syncing(true);

  switch (m->type) {

    // LED slider: ui_SliderLed (0..100)
    case UI_SYNC_LED_PCT: {
      uint8_t v = m->value;
      if (v > 100) v = 100;

      if (ui_SliderLed) {
        // Asegura rango por si SquareLine quedó 0..5
        lv_slider_set_range(ui_SliderLed, 0, 100);
        lv_slider_set_value(ui_SliderLed, (int)v, LV_ANIM_OFF);
        ui_panel_update_img_recolor(ui_PanelLed, v);

        // Dispara tu handler para que refresque labels sin loop (por s_sync_ui)
        lv_event_send(ui_SliderLed, LV_EVENT_VALUE_CHANGED, NULL);
      }
    } break;

    // FAN slider: ui_SliderFan (0..100)
    case UI_SYNC_FAN_PCT: {
      uint8_t v = m->value;
      if (v > 100) v = 100;

      if (ui_SliderFan) {
        lv_slider_set_range(ui_SliderFan, 0, 100);
        lv_slider_set_value(ui_SliderFan, (int)v, LV_ANIM_OFF);
        ui_panel_update_img_recolor(ui_PanelFan, v);
        lv_event_send(ui_SliderFan, LV_EVENT_VALUE_CHANGED, NULL);
      }
    } break;

    // LED timer roller: ui_RollerLed (0..4)
    case UI_SYNC_LED_TIMER: {
      uint8_t v = m->value;
      if (v > 4) v = 4;

      if (ui_RollerLed) {
        lv_roller_set_selected(ui_RollerLed, (uint16_t)v, LV_ANIM_OFF);
        lv_event_send(ui_RollerLed, LV_EVENT_VALUE_CHANGED, NULL);
      }
    } break;

    // FAN timer roller: ui_RollerFan (0..4)
    case UI_SYNC_FAN_TIMER: {
      uint8_t v = m->value;
      if (v > 4) v = 4;

      if (ui_RollerFan) {
        lv_roller_set_selected(ui_RollerFan, (uint16_t)v, LV_ANIM_OFF);
        lv_event_send(ui_RollerFan, LV_EVENT_VALUE_CHANGED, NULL);
      }
    } break;

    default:
      break;
  }

  ui_set_syncing(false);
  delete m;
}

// ==================================================
// API pública (safe desde loop/UART)
// Mantiene los nombres viejos para no romper main.cpp
// ==================================================
void ui_sync_led_level_from_stm(uint8_t pct0_100)
{
  if (pct0_100 > 100) pct0_100 = 100;
  UiSyncMsg *m = new UiSyncMsg{UI_SYNC_LED_PCT, pct0_100};
  lv_async_call(ui_sync_apply_cb, m);
}

void ui_sync_fan_level_from_stm(uint8_t pct0_100)
{
  if (pct0_100 > 100) pct0_100 = 100;
  UiSyncMsg *m = new UiSyncMsg{UI_SYNC_FAN_PCT, pct0_100};
  lv_async_call(ui_sync_apply_cb, m);
}

void ui_sync_led_timer_from_stm(uint8_t sel0_4)
{
  UiSyncMsg *m = new UiSyncMsg{UI_SYNC_LED_TIMER, sel0_4};
  lv_async_call(ui_sync_apply_cb, m);
}

void ui_sync_fan_timer_from_stm(uint8_t sel0_4)
{
  UiSyncMsg *m = new UiSyncMsg{UI_SYNC_FAN_TIMER, sel0_4};
  lv_async_call(ui_sync_apply_cb, m);
}
