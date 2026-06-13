// pico.h — stub for VCV Rack porting
#pragma once
#include <stdint.h>
#include <stddef.h>

// Common Pico types and defines
typedef unsigned int uint;

#ifndef __not_in_flash_func
#define __not_in_flash_func(x) x
#endif

#ifndef __time_critical_func
#define __time_critical_func(x) x
#endif

#define __force_inline __attribute__((always_inline)) inline

// Pico unique board ID (stub)
typedef struct { uint8_t id[8]; } pico_unique_board_id_t;
static inline void pico_get_unique_board_id(pico_unique_board_id_t *id) {
    if (id) for (int i = 0; i < 8; i++) id->id[i] = (uint8_t)i;
}
