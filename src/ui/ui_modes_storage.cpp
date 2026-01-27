#include "ui_modes_storage.h"
#include <Preferences.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "ui_modes.h"

// Usamos Preferences (NVS en ESP32)
static Preferences s_prefs;

// Pequeño CRC “simple” (suficiente para detectar corrupción)
static uint32_t crc32_simple(const uint8_t* data, size_t len) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) c = (c * 33u) ^ data[i];
    return c;
}

typedef struct {
    uint32_t magic;   // 'MODE'
    uint16_t ver;     // version del formato
    uint16_t count;   // cantidad de presets
    uint32_t crc;     // checksum del blob
} modes_hdr_t;

static const uint32_t kMagic = 0x4D4F4445; // "MODE"
static const uint16_t kVer   = 1;

void modes_storage_init(void)
{
    // namespace "ui_modes"
    s_prefs.begin("ui_modes", false);
}

bool modes_storage_save(void)
{
    const int count = ui_modes_presets_count();
    if (count <= 0) return false;

    const size_t blob_len = (size_t)count * sizeof(mode_cfg_t);

    const mode_cfg_t* src = ui_modes_presets();

    // Reserva buffer temporal
    mode_cfg_t* tmp = (mode_cfg_t*)malloc(blob_len);
    if (!tmp) return false;

    memcpy(tmp, src, blob_len);

    modes_hdr_t hdr{};
    hdr.magic = kMagic;
    hdr.ver   = kVer;
    hdr.count = (uint16_t)count;
    hdr.crc   = crc32_simple((const uint8_t*)tmp, blob_len);

    bool ok1 = (s_prefs.putBytes("cfg", tmp, blob_len) == blob_len);
    bool ok2 = (s_prefs.putBytes("hdr", &hdr, sizeof(hdr)) == sizeof(hdr));

    free(tmp);
    return ok1 && ok2;
}


bool modes_storage_load(void)
{
    // ✅ Primera vez: no existe nada guardado todavía
    if (!s_prefs.isKey("hdr") || !s_prefs.isKey("cfg")) return false;

    modes_hdr_t hdr{};
    if (s_prefs.getBytesLength("hdr") != sizeof(hdr)) return false;

    s_prefs.getBytes("hdr", &hdr, sizeof(hdr));
    if (hdr.magic != kMagic || hdr.ver != kVer) return false;

    const int count = ui_modes_presets_count();
    if ((int)hdr.count != count) return false;

    const size_t blob_len = (size_t)count * sizeof(mode_cfg_t);
    if (s_prefs.getBytesLength("cfg") != blob_len) return false;

    mode_cfg_t* tmp = (mode_cfg_t*)malloc(blob_len);
    if (!tmp) return false;

    s_prefs.getBytes("cfg", tmp, blob_len);

    uint32_t crc = crc32_simple((const uint8_t*)tmp, blob_len);
    if (crc != hdr.crc) {
        free(tmp);
        return false;
    }

    mode_cfg_t* dst = ui_modes_presets_mut();
    memcpy(dst, tmp, blob_len);

    free(tmp);
    return true;
}


bool modes_storage_clear(void)
{
    bool ok = true;
    ok = s_prefs.remove("cfg") && ok;
    ok = s_prefs.remove("hdr") && ok;
    return ok;
}
