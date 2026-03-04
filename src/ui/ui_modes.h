#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =====================================================
// Estado global (CLEAN toggle)
// =====================================================
extern bool s_clean_active;

// =====================================================
// Modos disponibles (UI)
// =====================================================
typedef enum {
    UI_MODE_DEFAULT = 0,
    UI_MODE_PHONE   = 1,
    UI_MODE_WORK    = 2,
    UI_MODE_RELAX   = 3,
    UI_MODE_CLEAN   = 4,
    UI_MODE_COUNT
} ui_mode_t;

// =====================================================
// Preset (lo que guardas por modo)
// =====================================================
typedef struct {
    uint8_t led;   // 0..100
    uint8_t fan;   // 0..100
    uint8_t priv;  // 0..1
} mode_cfg_t;

// =====================================================
// Aplicar modos
// =====================================================
void apply_mode_default(void);
void apply_mode_phone(void);
void apply_mode_work(void);
void apply_mode_relax(void);
void apply_mode_clean(void);

// Quita selección visual en la UI
void ui_modes_deselect_all(void);

// Wrapper seguro (con lock LVGL)
void apply_mode_safe(ui_mode_t mode);

// =====================================================
// Presets (para screen Modes)
// =====================================================
mode_cfg_t ui_modes_get_preset_ui(ui_mode_t mode);
void ui_modes_set_preset_ui(ui_mode_t mode,
                            uint8_t led,
                            uint8_t fan,
                            uint8_t priv);

// Compatibilidad con versión antigua (int)
// 0..4 => DEFAULT..CLEAN
mode_cfg_t ui_modes_get_preset(int mode_index);
void ui_modes_set_preset(int mode_index,
                         uint8_t led,
                         uint8_t fan,
                         uint8_t priv);

// =====================================================
// Acceso a presets (storage/NVS)
// =====================================================
mode_cfg_t* ui_modes_presets_mut(void);
const mode_cfg_t* ui_modes_presets(void);
int ui_modes_presets_count(void);

#ifdef __cplusplus
}
#endif