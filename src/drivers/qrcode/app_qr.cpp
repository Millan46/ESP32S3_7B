#include "app_qr.h"

#include <string.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_err.h"

#include <lvgl.h>
#include <lv_qrcode.h>
#include "ui.h"

// ============================================================
// Config
// ============================================================

#define NVS_NAMESPACE "qr"
#define KEY_PAGE      "page"
#define KEY_SERIAL    "serial"

#define BUF_SIZE 128
#define QR_SIZE  250

// ============================================================
// Private data (NO expuestos)
// ============================================================

static char s_page[BUF_SIZE]   = {0};
static char s_serial[BUF_SIZE] = {0};

static lv_obj_t *qr_page   = NULL;
static lv_obj_t *qr_serial = NULL;

// ============================================================
// Helpers
// ============================================================

static void safe_copy(char *dst, size_t dst_sz, const char *src)
{
    if (!dst || dst_sz == 0) return;
    if (!src) src = "";
    strncpy(dst, src, dst_sz - 1);
    dst[dst_sz - 1] = '\0';
}

// ============================================================
// Init
// ============================================================

void app_qr_init(void)
{
    static bool initialized = false;
    if (initialized) return;

    esp_err_t err = nvs_flash_init();

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    initialized = true;
}

// ============================================================
// Getters / Setters
// ============================================================

void app_qr_set_page(const char *value)
{
    safe_copy(s_page, BUF_SIZE, value);
}

void app_qr_set_serial(const char *value)
{
    safe_copy(s_serial, BUF_SIZE, value);
}

const char *app_qr_get_page(void)
{
    return s_page;
}

const char *app_qr_get_serial(void)
{
    return s_serial;
}

// ============================================================
// NVS Load
// ============================================================

void app_qr_nvs_load(void)
{
    nvs_handle_t handle;

    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
        return;

    size_t len;

    len = BUF_SIZE;
    nvs_get_str(handle, KEY_PAGE, s_page, &len);

    len = BUF_SIZE;
    nvs_get_str(handle, KEY_SERIAL, s_serial, &len);

    nvs_close(handle);
}

// ============================================================
// NVS Save
// ============================================================

void app_qr_nvs_save(void)
{
    nvs_handle_t handle;

    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK)
        return;

    nvs_set_str(handle, KEY_PAGE, s_page);
    nvs_set_str(handle, KEY_SERIAL, s_serial);

    nvs_commit(handle);
    nvs_close(handle);
}

// ============================================================
// QR Render
// ============================================================

static void qr_create_if_needed(void)
{
    if (!qr_page && ui_QRPageHolder)
    {
        qr_page = lv_qrcode_create(ui_QRPageHolder,
                                   QR_SIZE,
                                   lv_color_black(),
                                   lv_color_white());
        lv_obj_center(qr_page);
    }

    if (!qr_serial && ui_QRSerialHolder)
    {
        qr_serial = lv_qrcode_create(ui_QRSerialHolder,
                                     QR_SIZE,
                                     lv_color_black(),
                                     lv_color_white());
        lv_obj_center(qr_serial);
    }
}

void app_qr_refresh(void)
{
    qr_create_if_needed();

    const char *page   = (s_page[0])   ? s_page   : "EMPTY";
    const char *serial = (s_serial[0]) ? s_serial : "EMPTY";

    if (qr_page)
        lv_qrcode_update(qr_page, page, strlen(page));

    if (qr_serial)
        lv_qrcode_update(qr_serial, serial, strlen(serial));
}