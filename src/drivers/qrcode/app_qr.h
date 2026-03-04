#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

// ============================================================
// API pública
// ============================================================

void app_qr_init(void);

void app_qr_nvs_load(void);
void app_qr_nvs_save(void);

void app_qr_refresh(void);

// Setters
void app_qr_set_page(const char *value);
void app_qr_set_serial(const char *value);

// Getters
const char *app_qr_get_page(void);
const char *app_qr_get_serial(void);

#ifdef __cplusplus
}
#endif