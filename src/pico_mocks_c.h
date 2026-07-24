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
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* Compiler decorators */
#define __not_in_flash_func(x) x
#define __not_in_flash(x)
#define __in_flash(group)
#define __time_critical_func(x) x

#ifdef __APPLE__
#define __attribute__(x)
#endif

/* Memory barrier / DMB mock */
#ifdef __dmb
#undef __dmb
#endif
#define __dmb() __asm__ volatile("" : : : "memory")

/* Interrupt mock stubs — no-ops on host */
static inline uint32_t save_and_disable_interrupts(void) { return 0; }
static inline void restore_interrupts(uint32_t state) { (void)state; }
static inline unsigned int get_core_num(void) { return 0; }
static inline uint32_t time_us_32(void) { return 0; }
static inline uint64_t time_us_64(void) { return 0; }
static inline void sleep_ms(uint32_t ms) { (void)ms; }
static inline void sleep_us(uint64_t us) { (void)us; }
static inline uint32_t busy_wait_us_32(uint32_t us) { (void)us; return 0; }

/* Spinlock stubs for host compilation */
typedef uint32_t spin_lock_t;
static inline spin_lock_t* spin_lock_init(unsigned int i) { (void)i; static spin_lock_t l; return &l; }
static inline uint32_t spin_lock_blocking(spin_lock_t* lock) { (void)lock; return 0; }
static inline void spin_unlock(spin_lock_t* lock, uint32_t num) { (void)lock; (void)num; }

/* Time and absolute_time_t stubs */
typedef uint64_t absolute_time_t;
static inline absolute_time_t get_absolute_time(void) { return 0; }
static inline uint64_t to_us_since_boot(absolute_time_t t) { return t; }
static inline uint32_t to_ms_since_boot(absolute_time_t t) { return (uint32_t)(t / 1000); }
static inline absolute_time_t from_us_since_boot(uint64_t us) { return us; }

/* TinyUSB CDC stubs for host compilation */
static inline bool tud_cdc_connected(void) { return false; }
static inline uint32_t tud_cdc_write(const void *buffer, uint32_t bufsize) { (void)buffer; (void)bufsize; return 0; }
static inline uint32_t tud_cdc_write_flush(void) { return 0; }
static inline uint32_t tud_cdc_read(void *buffer, uint32_t bufsize) { (void)buffer; (void)bufsize; return 0; }
static inline uint32_t tud_cdc_available(void) { return 0; }

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

/* Avoid collision with C library clock(void) function in <time.h> for C-mode card source files */
#define clock lua_clock_bytecode_blob
