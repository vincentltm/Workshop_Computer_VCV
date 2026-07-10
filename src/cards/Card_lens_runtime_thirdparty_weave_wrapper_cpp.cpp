// Automatically generated separate compilation wrapper
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <stdio.h>
#include <string.h>
#include <cstring>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include <setjmp.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
#include <inttypes.h>
#include <cinttypes>
#include "pico_mocks.h"
#include "tusb.h"
#define while(...) while((__VA_ARGS__) && !g_cancellation_requested.load(std::memory_order_relaxed))

#include "ComputerCard.h"

namespace Card_Lens {
#include "braids/macro_oscillator.h"
/* stripped system include */
/* stripped system include */

extern "C" {

// Delay buffer: allocated externally (from audio pool) and wired into
// DigitalOscillator via set_delay_buffer(). Sized for the largest model
// (SAW_COMB at kCombDelayLength=4096 int16s = 8192 bytes).
typedef braids::DigitalOscillator::DelayLines WeaveDelayLines;

struct WeaveState {
    int32_t value;
    // MacroOscillator without its inline delay buffer: ~700 bytes.
    // Static assert below will catch size mismatches at compile time.
    char osc_storage[900];
    // Delay lines for physical-model/comb models, from audio pool.
    // NULL if not yet wired (will be wired on first init from audio pool
    // allocation stored immediately after this struct in the nodestate block).
    WeaveDelayLines* delay_lines;
    uint8_t initialized;
    uint8_t last_trig;
    int16_t next_sample;
    uint8_t has_next_sample;
    uint8_t pending_strike;
};

// Immediately after WeaveState in the nodestate block sits the WeaveDelayLines.
// runtime_kernel_state_bytes returns sizeof(WeaveState) + sizeof(WeaveDelayLines),
// so the allocator gives us one contiguous block; we carve the pointer ourselves.
#define WEAVE_STATE_BYTES  (sizeof(WeaveState))
#define WEAVE_DELAY_BYTES  (sizeof(WeaveDelayLines))
#define WEAVE_TOTAL_BYTES  (WEAVE_STATE_BYTES + WEAVE_DELAY_BYTES)

static_assert(sizeof(braids::MacroOscillator) <= 900,
    "MacroOscillator grew past osc_storage; increase osc_storage in WeaveState");

// Block size must be even — many Braids render functions write 2 samples
// per loop iteration and will overflow a size=1 buffer.
#define WEAVE_BLOCK 2

void op_weave_init(struct WeaveState* st) {
    // Delay lines live in the bytes immediately following this struct
    // (both allocated as one block from the nodestate/audio pool).
    st->delay_lines = reinterpret_cast<WeaveDelayLines*>(
        reinterpret_cast<uint8_t*>(st) + WEAVE_STATE_BYTES);
    memset(st->delay_lines, 0, WEAVE_DELAY_BYTES);

    braids::MacroOscillator* osc = new (st->osc_storage) braids::MacroOscillator();
    osc->Init();
    osc->set_delay_buffer(st->delay_lines);

    st->initialized = 1;
    st->last_trig   = 0;
    st->has_next_sample = 0;
    st->pending_strike  = 0;
}

// Inputs (all 0..4095):
//   note    – V/OCT pitch (MIDI note * 1 + fractional)
//   timbre  – Braids timbre
//   color   – Braids color
//   model   – oscillator model: 0..4095 maps to index 0..47
//   trig    – gate/trigger; rising edge calls Strike()
void op_weave_process(struct WeaveState* st,
                      int32_t note, int32_t timbre, int32_t color,
                      int32_t model, int32_t trig)
{
    if (!st->initialized) op_weave_init(st);

    // Track trigger state on every sample to avoid missing short pulses.
    bool trigger_high = (trig > 255);
    if (trigger_high && !st->last_trig) {
        st->pending_strike = 1;
    }
    st->last_trig = (uint8_t)trigger_high;

    if (st->has_next_sample) {
        st->value = (int32_t)st->next_sample >> 4;
        st->has_next_sample = 0;
        return;
    }

    braids::MacroOscillator* osc =
        reinterpret_cast<braids::MacroOscillator*>(st->osc_storage);

    // Ensure delay buffer is always wired (survives state restore from flash).
    osc->set_delay_buffer(st->delay_lines);

    bool strike = st->pending_strike;
    if (strike) {
        osc->Strike();
        st->pending_strike = 0;
    }

    // Model: 0..4095 → 0..47
    int32_t model_idx = (model * 48) >> 12;   // equivalent to / 4096 * 48
    if (model_idx < 0)  model_idx = 0;
    if (model_idx > 47) model_idx = 47;

    // Pitch: +12 semitones to compensate for 48kHz vs Braids' native 96kHz
    int32_t braids_pitch = (note + 12) << 7;
    if (braids_pitch < 0)     braids_pitch = 0;
    if (braids_pitch > 16383) braids_pitch = 16383;

    // Timbre/color: 0..4095 → 0..32767
    int32_t t_sc = (timbre * 32767) >> 12;
    int32_t c_sc = (color  * 32767) >> 12;
    if (t_sc < 0) { t_sc = 0; }
    if (t_sc > 32767) { t_sc = 32767; }
    if (c_sc < 0) { c_sc = 0; }
    if (c_sc > 32767) { c_sc = 32767; }

    osc->set_pitch((int16_t)braids_pitch);
    osc->set_parameters((int16_t)t_sc, (int16_t)c_sc);
    osc->set_shape((braids::MacroOscillatorShape)model_idx);

    uint8_t sync_buf[WEAVE_BLOCK] = { (uint8_t)(strike ? 1 : 0), 0 };
    int16_t out_buf[WEAVE_BLOCK]  = { 0, 0 };
    osc->Render(sync_buf, out_buf, WEAVE_BLOCK);

    // 16-bit → 12-bit
    st->value = (int32_t)out_buf[0] >> 4;
    st->next_sample = out_buf[1];
    st->has_next_sample = 1;
}

// Called by runtime to size the nodestate allocation for this op.
// Returns combined size of WeaveState + WeaveDelayLines so both fit
// in one contiguous block from the audio pool (top-down arena).
size_t weave_state_total_bytes(void) {
    return WEAVE_TOTAL_BYTES;
}

}

} // namespace Card_Lens
