#include <Arduino.h>

#include "drivers/lvgl_port/lvgl_port.h"
#include "ui/ui.h"
#include "ui_anim.h"

#include "drivers/uart/uart.h"
#include "drivers/uart/protocol.h"
#include "ui/app_helpers.h"
#include "ui_sync.h"
#include "ui/ui_menu.h"   // <- usa este include si ui_menu.h está en /include
#include "ui/ui_datetime.h"
#include "ui/ui_modes.h"
#include "ui/ui_modes_storage.h"
#include "time_job.h"
#include "clock/clock_manager.h"
volatile bool pir_event = false;
volatile bool ir_event  = false;

// -------- Handshake STM32 --------
static bool stm_ready = false;
static uint32_t last_sync_ms = 0;
static uint32_t last_rx_ms = 0;
static const uint32_t SYNC_PERIOD_MS = 500;
static const uint32_t LINK_TIMEOUT_MS  = 1500;  // si 1.5s sin RX, considero caído
static inline void uart_handshake_tick()
{
    if (stm_ready) return;

    uint32_t now = millis();
    if (now - last_sync_ms >= SYNC_PERIOD_MS) {
        last_sync_ms = now;
        Uart::send(Uart::CMD_SYNC, 0);
    }
}
static void process_time_save_job()
{
  if (!g_time_save_pending) return;

  PendingTime t = g_pending_time;
  g_time_save_pending = false;

  clock_manager_apply_manual_time(t.Y, t.Mo, t.D, t.h24, t.mi, t.sec);

  if (lvgl_port_lock(0)) {
    ui_datetime_set_format_24h(t.fmt == 24);   // ✅ APLICA fmt
    ui_datetime_set_editing(false);
    ui_datetime_load_from_system_to_controls();
    lvgl_port_unlock();
  }
}

void setup()
{
    Serial.begin(115200);

    // UART hacia STM32
    Uart::begin(115200);
    delay(50);

    // Handshake STM32
    last_sync_ms = millis();
    Uart::send(Uart::CMD_SYNC, 0);

    static esp_lcd_panel_handle_t panel_handle = NULL;
    static esp_lcd_touch_handle_t tp_handle    = NULL;

    tp_handle    = touch_gt911_init();
    panel_handle = waveshare_esp32_s3_rgb_lcd_init();

    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle));

    if (lvgl_port_lock(-1)) {

        ui_init();

        modes_storage_init();
        modes_storage_load();
        apply_mode_safe(UI_MODE_DEFAULT);

        ui_menu_bind();

        clock_manager_init();

        // ✅ Timer del UI (TopBar/Preview)
        ui_datetime_start_timer();

        // ✅ SIEMPRE ir a ScreenDate al encender
        lv_scr_load(ui_ScreenDate);

        ui_datetime_init_controls();
        ui_datetime_set_editing(true);

         // tu reloj interno default (o el que tengas guardado)
        ui_datetime_set_current(2026, 1, 1, 0, 0, 0);

        // ✅ NO usa time()
        ui_datetime_load_from_current_to_controls();


        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(lv_disp_get_default());

        lvgl_port_unlock();
    }

    wavesahre_rgb_lcd_bl_on();
}

// ===== Indicador por IR/CLEAN =====
static bool s_ir_active = false;
static uint32_t s_ir_last_ms = 0;

// Ajusta: cuánto tiempo se queda ROJO después de un EVT_IR (si no tienes IR_OFF)
#define IR_HOLD_MS 2000

// Usa tu flag real de clean (si está en otro archivo, decláralo extern)
extern bool s_clean_active;

// Lógica única de color
void update_cabin_indicator(void)
{
    if (s_clean_active) {
        app_cabin_indicator_set_color(0xFFC107); // AMARILLO (CLEAN)
    } else if (s_ir_active) {
        app_cabin_indicator_set_color(0xFF0000); // ROJO (IR)
    } else {
        app_cabin_indicator_set_color(0x00FF00); // VERDE (normal)
    }
}


void loop()
{
    uint32_t now = millis();

    // 1) Si estaba listo pero el STM32 se apagó -> marco desconectado
    if (stm_ready && (now - last_rx_ms > LINK_TIMEOUT_MS)) {
        stm_ready = false;
    }

    // 2) Mientras no esté listo, reintento SYNC periódicamente
    if (!stm_ready && (now - last_sync_ms > SYNC_PERIOD_MS)) {
        last_sync_ms = now;
        Uart::send(Uart::CMD_SYNC, 0);
    }

    // 3) Leer UART (no bloqueante)
    Uart::Packet p;
    while (Uart::read(p)) {
        last_rx_ms = millis(); // marca actividad RX

        switch (p.cmd) {
            case Uart::EVT_READY:
                stm_ready = true;
                // opcional debug:
                // Serial.println("✅ STM32 READY");
                break;

            case Uart::EVT_PIR:
                pir_event = true;
                break;

            case Uart::EVT_IR:
                s_ir_active = (p.value != 0);

                if (lvgl_port_lock(0)) {
                   update_cabin_indicator();
                   lvgl_port_unlock();
                }
                break;
            case Uart::EVT_LED_SYNC:
                ui_sync_led_level_from_stm(p.value);
                break;

            case Uart::EVT_FAN_SYNC:
                ui_sync_fan_level_from_stm(p.value);
                break;

            case Uart::EVT_LED_TIMER_SYNC:
                ui_sync_led_timer_from_stm(p.value);
                break;

            case Uart::EVT_FAN_TIMER_SYNC:
                ui_sync_fan_timer_from_stm(p.value);
                break;

            default:
                break;
        }
    }

    // 4) Procesar eventos PIR/IR dentro de LVGL lock
    if (pir_event || ir_event) {
        if (lvgl_port_lock(0)) {
            if (pir_event) {
                pir_event = false;
            }
            if (ir_event) {
                ir_event = false;
            }
            lvgl_port_unlock();
        }
    }

    // 5) Tick LVGL
    delay(5);
    if (lvgl_port_lock(0)) {
        process_time_save_job();
        lv_timer_handler();   
        lvgl_port_unlock();
    }
}
