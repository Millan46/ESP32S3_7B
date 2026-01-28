#include "pin_store.h"
#include <string.h>

// ============================================================
// Config
// ============================================================

static const char *NVS_NAMESPACE = "hmi";
static const char *KEY_USER = "pin_user";
static const char *KEY_ADV  = "pin_adv";

// Defaults si no hay nada guardado
static const char *DEF_USER = "1234";
static const char *DEF_ADV  = "4567";

static void safe_copy(char *dst, size_t dst_sz, const char *src)
{
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
}

// ============================================================
// Arduino implementation (Preferences)
// ============================================================
#if defined(ARDUINO)

#include <Preferences.h>

void pin_load(char *user_out, size_t user_sz, char *adv_out, size_t adv_sz)
{
    safe_copy(user_out, user_sz, DEF_USER);
    safe_copy(adv_out,  adv_sz,  DEF_ADV);

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, true)) {
        return; // no se pudo abrir NVS
    }

    String u = prefs.getString(KEY_USER, DEF_USER);
    String a = prefs.getString(KEY_ADV,  DEF_ADV);
    prefs.end();

    safe_copy(user_out, user_sz, u.c_str());
    safe_copy(adv_out,  adv_sz,  a.c_str());
}

void pin_save(const char *user_pin, const char *adv_pin)
{
    // Si vienen null, guarda defaults
    if (!user_pin || !*user_pin) user_pin = DEF_USER;
    if (!adv_pin  || !*adv_pin)  adv_pin  = DEF_ADV;

    Preferences prefs;
    if (!prefs.begin(NVS_NAMESPACE, false)) {
        return;
    }

    prefs.putString(KEY_USER, user_pin);
    prefs.putString(KEY_ADV,  adv_pin);
    prefs.end();
}

#else
// ============================================================
// ESP-IDF implementation (nvs_flash / nvs)
// ============================================================

#include "nvs.h"
#include "nvs_flash.h"

// Nota: normalmente nvs_flash_init() ya se llama en el arranque.
// Si no, lo intentamos una vez aquí.
static void ensure_nvs_init_once(void)
{
    static bool inited = false;
    if (inited) return;

    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    inited = true;
}

void pin_load(char *user_out, size_t user_sz, char *adv_out, size_t adv_sz)
{
    safe_copy(user_out, user_sz, DEF_USER);
    safe_copy(adv_out,  adv_sz,  DEF_ADV);

    ensure_nvs_init_once();

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return;
    }

    // USER
    {
        size_t len = 0;
        if (nvs_get_str(h, KEY_USER, NULL, &len) == ESP_OK && len > 0) {
            char tmp[16] = {0};
            if (len > sizeof(tmp)) len = sizeof(tmp);
            if (nvs_get_str(h, KEY_USER, tmp, &len) == ESP_OK) {
                safe_copy(user_out, user_sz, tmp);
            }
        }
    }

    // ADV
    {
        size_t len = 0;
        if (nvs_get_str(h, KEY_ADV, NULL, &len) == ESP_OK && len > 0) {
            char tmp[16] = {0};
            if (len > sizeof(tmp)) len = sizeof(tmp);
            if (nvs_get_str(h, KEY_ADV, tmp, &len) == ESP_OK) {
                safe_copy(adv_out, adv_sz, tmp);
            }
        }
    }

    nvs_close(h);
}

void pin_save(const char *user_pin, const char *adv_pin)
{
    if (!user_pin || !*user_pin) user_pin = DEF_USER;
    if (!adv_pin  || !*adv_pin)  adv_pin  = DEF_ADV;

    ensure_nvs_init_once();

    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }

    nvs_set_str(h, KEY_USER, user_pin);
    nvs_set_str(h, KEY_ADV,  adv_pin);
    nvs_commit(h);
    nvs_close(h);
}

#endif
