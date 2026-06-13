// hardware/clocks.h — stub for VCV Rack porting
#pragma once
#include <stdint.h>

// Clock IDs (Pico SDK enum subset)
typedef enum {
    clk_gpout0 = 0,
    clk_gpout1,
    clk_gpout2,
    clk_gpout3,
    clk_ref,
    clk_sys,
    clk_peri,
    clk_usb,
    clk_adc,
    clk_rtc,
    CLK_COUNT
} clock_index;

// Returns simulated 48kHz host clock frequency in Hz
static inline uint32_t clock_get_hz(clock_index clk) {
    (void)clk;
    return 48000000U; // 48 MHz nominal
}

static inline bool set_sys_clock_khz(uint32_t freq_khz, bool required) {
    (void)freq_khz; (void)required;
    return true; // No-op in VCV Rack
}
