#include <Arduino.h>

#include "drivers/lvgl_port/lvgl_port.h"
#include "ui/ui.h"
#include "anim/ui_anim.h"

#include "drivers/uart/uart.h"
#include "drivers/uart/protocol.h"
#include "ui/core/app_helpers.h"
#include "ui/ui_sync.h"
#include "ui/ui_menu.h" 
#include "ui/ui_datetime.h"
#include "ui/ui_modes.h"
#include "ui/ui_modes_storage.h"
#include "clock/time_job.h"
#include "clock/clock_manager.h"
#include "clock/clock_rtc.h"
volatile bool pir_event = false;
volatile bool ir_event  = false;

// -------- Handshake STM32 --------
static bool stm_ready = false;
static uint32_t last_sync_ms = 0;
static uint32_t last_rx_ms = 0;
static const uint32_t SYNC_PERIOD_MS = 500;
static const uint32_t LINK_TIMEOUT_MS  = 1500;  // si 1.5s sin RX, considero caído


void print_time(const char* tag, const ClockDateTime& t) {
  Serial.printf("%s %04d-%02d-%02d %02d:%02d:%02d\n",
                tag, t.y,t.mo,t.d,t.h,t.mi,t.s);
}

// -------- Setup / Loop --------

void setup()
{
    Serial.begin(115200);
    delay(1500);
    Serial.println("\nBOOT: start");
    Serial.flush();

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
    wavesahre_rgb_lcd_bl_off();   // lo más pronto posible

    ESP_ERROR_CHECK(lvgl_port_init(panel_handle, tp_handle));
     // ✅ Init clock (RTC + NVS)
    Serial.println("BOOT: before clock_manager_init");
    Serial.flush();
    clock_manager_init();
    Serial.println("BOOT: after clock_manager_init");
    Serial.flush();

    ClockDateTime m{};
    if (clock_manager_get_now(m)) print_time("MANAGER:", m);
    else Serial.println("MANAGER: invalid");

    ClockDateTime r{};
    if (clock_rtc_read(r)) print_time("RTC:", r);
    else Serial.println("RTC: read failed");
    

    ClockDateTime t;
    if (clock_manager_get_now(t)) {
        Serial.printf("NOW: %04d-%02d-%02d %02d:%02d:%02d\n", t.y,t.mo,t.d,t.h,t.mi,t.s);
    } else {
        Serial.println("Clock invalid (RTC lost power and no NVS)");
    }
      Serial.flush();
    
    if (lvgl_port_lock(-1)) {

        // fuerza fondo negro inmediato
        lv_obj_t * scr = lv_scr_act();
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        lv_refr_now(lv_disp_get_default());

        ui_init();

        // ====== LOGO ======
        lv_scr_load(ui_ScreenLogo);
        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(lv_disp_get_default());

        ui_logo_start_sequence(500, 3000);

        while (!ui_logo_is_done()) {
            lv_timer_handler();
            delay(5);
        }

        // ====== TU FLUJO NORMAL ======
        modes_storage_init();
        modes_storage_load();
        apply_mode_safe(UI_MODE_DEFAULT);

        ui_menu_bind();

        // Timer del UI (TopBar/Preview)
        ui_datetime_start_timer();

        // SIEMPRE ir a ScreenDate al encender
        lv_scr_load(ui_ScreenDate);

        ui_datetime_init_controls();
        ui_datetime_set_editing(true);

        // ✅ Re-sync desde RTC al entrar a ScreenDate (opcional, recomendado)
        clock_manager_sync_from_rtc();

        // ✅ Cargar hora real (RTC o NVS) al "current" del UI
        ClockDateTime dt;
        if (clock_manager_get_now(dt)) {
            ui_datetime_set_current(dt.y, dt.mo, dt.d, dt.h, dt.mi, dt.s);
        } else {
            // fallback si todavía no hay hora válida
            ui_datetime_set_current(2026, 1, 1, 0, 0, 0);
        }

        // ✅ NO usa time()
        ui_datetime_load_from_current_to_controls();

        lv_obj_invalidate(lv_scr_act());
        lv_refr_now(lv_disp_get_default());

        lvgl_port_unlock();
    }

    // Backlight manager normal (timeout, activity, etc.)
    app_bl_init();
}



// ===== Indicador por IR/CLEAN =====
static bool s_ir_active = false;
static uint32_t s_ir_last_ms = 0;
static bool s_timer_active = false;  // recibido por UART

// Ajusta: cuánto tiempo se queda ROJO después de un EVT_IR (si no tienes IR_OFF)
#define IR_HOLD_MS 2000

// Usa tu flag real de clean (si está en otro archivo, decláralo extern)
extern bool s_clean_active;
bool app_is_timer_active(void)
{
    return s_timer_active;
}
// Lógica única de color
void update_cabin_indicator(void)
{
    if (s_clean_active) {
        // Prioridad máxima
        app_cabin_indicator_set_color(0xFFC107); // AMARILLO (CLEAN)
    }
    else if (s_ir_active) {
        // IR activo fuerza ROJO
        app_cabin_indicator_set_color(0xFF0000); // ROJO (IR)
    }
    else if (s_timer_active) {
        // Timer activo y IR desactivado → ROJO
        app_cabin_indicator_set_color(0xFF0000); // ROJO (TIMER)
    }
    else {
        // Timer OFF e IR OFF → VERDE
        app_cabin_indicator_set_color(0x00FF00); // VERDE
    }
}


void loop()
{
    uint32_t now = millis();
    
    // ===== ⏱️ TICK DE 1 SEGUNDO (RELOJ MAESTRO) =====
    static uint32_t last_1s = 0;
    if (now - last_1s >= 1000) {
        last_1s += 1000;
        clock_manager_tick_1s();
    }
    // ==============================================

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
            case Uart::EVT_TIMER_ACTIVE:
                s_timer_active = (p.value != 0);
                //app_bl_register_activity(now); // ✅
                update_cabin_indicator();
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
    //app_bl_tick(now);
  
    // 5) Tick LVGL
    delay(5);
    if (lvgl_port_lock(0)) {
        time_job_tick();
        lvgl_port_unlock();
    }
}
