#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Llama en setup/arranque (una vez)
void modes_storage_init(void);

// Carga presets desde NVS a s_mode_cfg (si no existe, deja defaults)
bool modes_storage_load(void);

// Guarda presets actuales (s_mode_cfg) a NVS
bool modes_storage_save(void);

// Borra presets guardados (opcional: volver a defaults en próximo boot)
bool modes_storage_clear(void);

#ifdef __cplusplus
}
#endif
