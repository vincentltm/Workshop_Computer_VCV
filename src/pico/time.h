// pico/time.h — stub for VCV Rack porting
#pragma once
#include <stdint.h>
#include <chrono>

// absolute_time_t stub
typedef uint64_t absolute_time_t;

static inline absolute_time_t get_absolute_time() {
    using namespace std::chrono;
    return (uint64_t)duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

static inline uint32_t to_ms_since_boot(absolute_time_t t) {
    return (uint32_t)(t / 1000ULL);
}

static inline uint64_t to_us_since_boot(absolute_time_t t) {
    return t;
}

static inline absolute_time_t make_timeout_time_us(uint64_t us) {
    return get_absolute_time() + us;
}

static inline int64_t absolute_time_diff_us(absolute_time_t from, absolute_time_t to) {
    return (int64_t)(to - from);
}
