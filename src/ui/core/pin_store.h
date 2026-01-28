#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Carga PINs guardados. Si no existen, deja los defaults actuales en los buffers.
// user_out / adv_out: buffers destino
// user_sz / adv_sz: tamaño de buffer
void pin_load(char *user_out, size_t user_sz, char *adv_out, size_t adv_sz);

// Guarda ambos PINs de forma persistente (NVS).
// Debes pasar strings terminados en '\0'
void pin_save(const char *user_pin, const char *adv_pin);

#ifdef __cplusplus
}
#endif
