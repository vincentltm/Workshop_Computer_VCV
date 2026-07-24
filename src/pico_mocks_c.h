/*
 * pico_mocks_c.h — C-compatible subset of pico_mocks.h for .c source wrappers.
 *
 * pico_mocks.h requires C++ (uses std::atomic, std::thread, namespaces, etc.).
 * .c source wrappers are compiled with $(CC) -std=gnu99, so they cannot include
 * pico_mocks.h directly. This header provides only the macros and stub
 * declarations that C sources actually use.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* Compiler decorators */
#define __not_in_flash_func(x) x
#define __not_in_flash(x)
#define __in_flash(group)

#ifndef __APPLE__
/* On Apple, __attribute__ is left as-is; the mach-o section warning from
   pico_mocks.h applies only to the C++ build so we skip it here. */
#define __time_critical_func(x) x
#endif

/* Memory barrier / DMB mock */
#ifdef __dmb
#undef __dmb
#endif
#define __dmb() __asm__ volatile("" : : : "memory")

/* Interrupt mock stubs — no-ops on host */
static inline uint32_t save_and_disable_interrupts(void) { return 0; }
static inline void restore_interrupts(uint32_t state) { (void)state; }

/* Flash / XIP layout constants */
#define XIP_BASE              0x10000000u
#define FLASH_PAGE_SIZE       256u
#define FLASH_SECTOR_SIZE     4096u
#define PICO_FLASH_SIZE_BYTES (2u * 1024u * 1024u)

/* _Static_assert compat (C11 already has it; guard for pre-C11) */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  /* _Static_assert is already a keyword */
#elif !defined(_Static_assert)
  #define _Static_assert(cond, msg) typedef char _sa_check_[(cond) ? 1 : -1]
#endif

/* Hardware write-masked (used by some cards) */
static inline void hw_write_masked(volatile uint32_t *addr, uint32_t values, uint32_t write_mask) {
    *addr = (*addr & ~write_mask) | (values & write_mask);
}

/* u_int16_t alias (some BSD-style code uses this) */
#ifndef u_int16_t
typedef uint16_t u_int16_t;
#endif
