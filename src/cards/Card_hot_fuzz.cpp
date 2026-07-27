#define COMPUTERCARD_SAMPLE_RATE_DIV 1
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#ifndef _WIN32
#include <dlfcn.h>
#endif
#include <fstream>
#include <iostream>
#include <sstream>
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
#include <array>
#include <cmath>
#include <complex>
#include <random>
#include <functional>
#include <queue>
#include <deque>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <numeric>
#include <initializer_list>
#include "pico_mocks.h"
#include "tusb.h"
#define while(...) while((__VA_ARGS__) && !g_cancellation_requested.load(std::memory_order_relaxed))

#include "ComputerCard.h"

#define _Static_assert static_assert
namespace std { using ::sinf; using ::cosf; using ::tanf; using ::asinf; using ::acosf; using ::atanf; using ::atan2f; using ::sqrtf; using ::expf; using ::logf; using ::log10f; using ::powf; }

// DSO-local thread-local variables definition
thread_local CardGlobals* t_instance = nullptr;
thread_local bool is_core1_thread = false;
thread_local ComputerCard* ComputerCard::thisptr = nullptr;

namespace Card_HotFuzz {

    int main();

// ──────────────────────────────────────────────────────────────────────────────
// Source: main.cpp
// ──────────────────────────────────────────────────────────────────────────────

// main.cpp — Hot Fuzz: stereo fuzz + wah effects card for the Workshop Computer.
//
// Audio path (per sample, core0):
//   AudioIn -> preamp (3x) -> DC block -> fuzz (soft/hard/asym/fold) -> DC block
//          -> envelope tap (pre-fuzz) -> SVF (wah, LP) -> crossfade(dry,wet)
//          -> clamp -> AudioOut
//
// Controls:
//   Switch Up   : Fuzz + wah. Main pot = fuzz type (4 zones), X pot = drive.
//   Switch Mid  : Fuzz + wah blend. Main pot = fuzz blend, X pot = drive.
//   Switch Down : Wah mode. Main pot = manual sweep (auto-wah off) or
//                 base frequency (auto-wah on). X pot = dry/wet blend.
//   Auto-wah    : Toggled on/off by double-tapping Down (Mid→Down→Mid→Down
//                 within 0.5 s). LED 4 flashes to confirm.
//   Y pot       : Wah resonance (all modes).
//   Fuzz type   : Set by Main pot in Up mode, persists across modes.
//
// No flash persistence — settings reset to defaults on power cycle.

/* stripped ComputerCard include */
/* stripped pico include */
#include "dsp.h"
#include "svf_braids.h"
#include "braids_resources.h"
#include "resources.h"

/* stripped system include */

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

// Preamp: fixed 3x gain in Q8.8 (256 = 1x, so 768 = 3x).
static constexpr int32_t HF_PREAMP_GAIN_Q8_8 = 768;

// Envelope follower attack/release coefficients (Q15), from 31_esp's LUT.
// 10 ms attack  -> ~0.002 in Q15 -> idx ~1 (122) of ALPHA_Q15_32
// 200 ms release -> ~0.0001 in Q15. We pick a value by hand for 200ms:
//   alpha = 1 - exp(-1/(48000 * 0.2)) ~ 1.04e-4 ~ 3 in Q15.
static constexpr uint16_t HF_ENV_ATTACK_Q15  = 122;   // ~10 ms
static constexpr uint16_t HF_ENV_RELEASE_Q15 = 3;     // ~200 ms

// f-coefficient glide time constants (shift amounts, Q-shift).
// k=5 -> ~0.5 ms time constant (manual, responsive).
// k=8 -> ~5 ms time constant (auto, hides 64-sample quantisation).
static constexpr int HF_GLIDE_MANUAL_K = 5;
static constexpr int HF_GLIDE_AUTO_K    = 8;

// Control-rate: recompute cutoff target every 64 samples (~750 Hz).
static constexpr int HF_CONTROL_INTERVAL = 64;

// Auto-wah toggle: double-tap Down (Mid→Down→Mid→Down) within this window
// to toggle auto-wah on/off. 0.5 s = 24000 samples at 48 kHz.
static constexpr uint32_t HF_DOUBLETAP_WINDOW = 24000;

// LED 4 toggle confirmation flash duration (~200 ms at 100 Hz LED rate = 20 ticks).
static constexpr uint16_t HF_TOGGLE_FLASH_TICKS = 20;

// SVF input scaling: Braids SVF clips at +/-32767 (15-bit). Our audio is
// 12-bit. Scale up by <<3 (x8) on the way in and >>3 on the way out to use
// the filter's full headroom and keep SNR high.
static constexpr int HF_SVF_SHIFT = 3;

// Q15 alpha LUT for envelope follower (log-spaced 20 Hz..5 kHz), from 31_esp.
static const uint16_t ALPHA_Q15_32[32] = {
    86,102,122,146,174,208,249,297,355,423,
    505,603,719,858,1022,1218,1450,1725,2050,2435,
    2888,3421,4045,4773,5619,6597,7719,8997,10439,12048,
    13819,15738
};

// ---------------------------------------------------------------------------
// Shared state between core0 (audio) and core1 (control/LEDs)
// ---------------------------------------------------------------------------

// Volatile flags for core0 -> core1 communication (audio-rate events).
struct Core0ToCore1 {
    volatile uint8_t  fuzz_type;          // current fuzz type (0/1/2/3)
    volatile uint8_t  switch_pos;         // current switch position (0=Down,1=Mid,2=Up)
    volatile uint8_t  auto_wah;           // current auto-wah state (0/1)
    volatile uint16_t drive_level;        // for LED display (X pot value)
    volatile uint16_t blend_level;        // for LED display (fuzz/wah blend)
    volatile int16_t  sweep_position;     // for LED display (current SVF freq index)
    volatile uint16_t resonance;          // for LED display (Y pot value 0..4095)
    volatile int16_t  envelope;           // for LED display (envelope follower output)
    volatile uint16_t toggle_flash;       // LED 4 flash countdown on auto-wah toggle
};
Core0ToCore1 c0_to_c1;

// ---------------------------------------------------------------------------
// HotFuzz card
// ---------------------------------------------------------------------------

class HotFuzz : public ComputerCard {
public:
    HotFuzz()
        : ComputerCard()
    {
        // Init SVF (one cutoff shared, two independent state instances).
        svf_l_.Init();
        svf_r_.Init();
        svf_l_.set_mode(braids::SVF_MODE_LP);
        svf_r_.set_mode(braids::SVF_MODE_LP);

        // Initial filter frequency (will be overwritten on first control tick).
        svf_freq_ = hotfuzz::manual_freq_LUT[0];
        svf_l_.set_frequency(svf_freq_);
        svf_r_.set_frequency(svf_freq_);
        svf_resonance_ = 4096;  // mid-range; Y pot overrides
        svf_l_.set_resonance(svf_resonance_);
        svf_r_.set_resonance(svf_resonance_);
    }

    // Core1 entry point: LED updates only.
    void SecondCore() {
        uint32_t led_counter = 0;

        while (true) {
            // --- LED updates (~100 Hz) ---
            if (++led_counter >= 10) {  // ~1000 Hz loop / 100 Hz update = 10
                led_counter = 0;
                update_leds_();
            }

            busy_wait_us_32(1000);  // ~1 ms tick
        }
    }

    // Core0 audio ISR: called once per sample at 48 kHz.
    void ProcessSample() override {
        // --- Read inputs ---
        int16_t dry_l = AudioIn1();
        int16_t dry_r = AudioIn2();

        // Read controls (smoothed by ComputerCard in ISR already).
        uint16_t main_val = (uint16_t)KnobVal(Knob::Main);
        uint16_t x_val    = (uint16_t)KnobVal(Knob::X);
        uint16_t y_val    = (uint16_t)KnobVal(Knob::Y);

        // Switch position (0=Down, 1=Middle, 2=Up).
        ComputerCard::Switch sw = SwitchVal();
        uint8_t sw_pos = (sw == ComputerCard::Switch::Down) ? 0
                       : (sw == ComputerCard::Switch::Middle) ? 1 : 2;
        if (sw_pos != switch_pos_) {
            switch_pos_ = sw_pos;
            c0_to_c1.switch_pos = sw_pos;
        }

        // --- Double-tap Down detection (auto-wah toggle) ---
        // From Mid, press Down→release to Mid→press Down within 0.5 s
        // toggles auto-wah on/off. The Down position is momentary (springs
        // back to Mid), so this is a natural double-tap gesture.
        // 3-state machine prevents accidental re-toggle when releasing
        // after the toggle and pressing Down again to play.
        //   IDLE     → first Down press+release → WAITING
        //   WAITING  → second Down press within 0.5 s → toggle, → COOLDOWN
        //   WAITING  → timeout → IDLE
        //   COOLDOWN → any Down release → IDLE
        sample_ctr_++;
        if (sw_pos != prev_sw_pos_) {
            if (sw_pos == 0) {
                // Mid→Down transition (a Down press).
                if (dt_state_ == DT_WAITING &&
                    (sample_ctr_ - first_tap_release_time_) <= HF_DOUBLETAP_WINDOW) {
                    // Second tap within window — toggle auto-wah.
                    auto_wah_ = auto_wah_ ? 0 : 1;
                    c0_to_c1.toggle_flash = HF_TOGGLE_FLASH_TICKS;
                    dt_state_ = DT_COOLDOWN;
                }
                // If IDLE or COOLDOWN, a Down press is just playing — no action.
            } else if (prev_sw_pos_ == 0) {
                // Down→Mid transition (a Down release).
                if (dt_state_ == DT_IDLE) {
                    // First tap released — start waiting for the second.
                    dt_state_ = DT_WAITING;
                    first_tap_release_time_ = sample_ctr_;
                } else {
                    // COOLDOWN (post-toggle) or WAITING (stale) → back to IDLE.
                    dt_state_ = DT_IDLE;
                }
            }
        }
        // Timeout: if waiting too long for the second tap, cancel.
        if (dt_state_ == DT_WAITING &&
            (sample_ctr_ - first_tap_release_time_) > HF_DOUBLETAP_WINDOW) {
            dt_state_ = DT_IDLE;
        }
        prev_sw_pos_ = sw_pos;

        // CV inputs (optional modulators, per Q20).
        int16_t cv1 = CVIn1();
        int16_t cv2 = CVIn2();
        if (Connected(Input::CV2)) {
            // CV2 is -2048..2047; map to 0..4095 additively.
            int32_t add = (cv2 + 2048) >> 1;  // 0..2047
            x_val = (uint16_t)clamp_u16((uint32_t)x_val + add, 4095);
        }

        // --- Determine effect state from switch ---
        //
        // Up   : Main = fuzz type (4 zones), X = drive, full wet crossfade.
        // Mid  : Main = fuzz blend (dry/wet), X = drive, fuzz + wah.
        // Down : Main = auto-wah base freq (CCW = off), X = wah blend.

        bool fuzz_enabled;
        uint8_t active_fuzz_type;
        uint16_t xfade_ramp;
        uint16_t blend_display = 0;   // for LED: fuzz or wah blend amount

        if (sw_pos == 2) {
            // Up: fuzz + wah. Main selects fuzz type (4 equal zones).
            fuzz_enabled = true;
            fuzz_type_ = (uint8_t)(main_val >> 10);  // 0..3
            active_fuzz_type = fuzz_type_;
            xfade_ramp = 4096;  // fully wet
            blend_display = 0;   // no blend in Up mode
        } else if (sw_pos == 1) {
            // Mid: fuzz + wah blend. Main = blend (0=dry..4095=full fuzz).
            fuzz_enabled = true;
            active_fuzz_type = fuzz_type_;  // persists from Up mode
            xfade_ramp = main_val;  // 0..4095 crossfade
            blend_display = main_val;
        } else {
            // Down: wah mode. Auto-wah is toggled by double-tapping Down.
            // When auto-wah is on, Main pot = base frequency.
            // When auto-wah is off, Main pot = manual sweep (full range).
            fuzz_enabled = false;
            active_fuzz_type = fuzz_type_;
            if (auto_wah_) {
                auto_wah_base_idx_ = (uint16_t)(main_val >> 4);  // 0..255
            }
            xfade_ramp = x_val;  // wah blend: 0=dry, 4095=wet
            blend_display = x_val;
        }

        // --- Control-rate block (every 64 samples) ---
        if (++block_ctr_ >= HF_CONTROL_INTERVAL) {
            block_ctr_ = 0;

            // Debounce CV1 connection: require ~8 control blocks (~11ms)
            // of stable Connected() before latching, to filter out
            // normalisation probe transients that can briefly flip the
            // connected state and override the Main pot.
            if (Connected(Input::CV1)) {
                if (cv1_conn_counter_ < 255) cv1_conn_counter_++;
                if (cv1_conn_counter_ >= 8) cv1_connected_ = true;
            } else {
                cv1_conn_counter_ = 0;
                cv1_connected_ = false;
            }

            // Update SVF resonance from Y pot (0..4095 -> 0..32760).
            svf_resonance_ = (int16_t)(y_val * 8);  // 0..32760
            svf_l_.set_resonance(svf_resonance_);
            svf_r_.set_resonance(svf_resonance_);

            // Cutoff target:
            int16_t new_freq;
            if (auto_wah_) {
                // Auto-wah: envelope sweeps frequency upward from base.
                // Base freq = floor (resting point when quiet).
                // Envelope pushes cutoff higher as playing gets louder.
                uint16_t env_idx = (uint16_t)((uint16_t)env_ >> 4);  // 0..255
                int16_t env_freq = (int16_t)hotfuzz::env_freq_LUT[env_idx];
                int16_t base_freq = (int16_t)hotfuzz::manual_freq_LUT[auto_wah_base_idx_];
                new_freq = (env_freq > base_freq) ? env_freq : base_freq;
            } else {
                // Manual: frequency from pot or CV1, depending on mode.
                if (sw_pos == 0) {
                    // Down mode, manual wah (Main < threshold): Main pot = freq.
                    uint16_t main_idx = (uint16_t)(main_val >> 4);
                    new_freq = (int16_t)hotfuzz::manual_freq_LUT[main_idx];
                } else if (cv1_connected_) {
                    // CV1 overrides cutoff in Up/Mid modes.
                    uint16_t cv_idx = (uint16_t)((uint16_t)(cv1 + 2048) >> 4);
                    new_freq = (int16_t)hotfuzz::manual_freq_LUT[cv_idx];
                } else {
                    // Up/Mid without CV1: hold last frequency.
                    new_freq = svf_freq_;
                }
            }
            svf_freq_target_ = new_freq;
        }

        // Per-sample f glide toward target.
        int32_t glided = glide_i32(svf_freq_, svf_freq_target_,
                                   auto_wah_ ? HF_GLIDE_AUTO_K
                                             : HF_GLIDE_MANUAL_K);
        svf_freq_ = (int16_t)glided;
        svf_l_.set_frequency(svf_freq_);
        svf_r_.set_frequency(svf_freq_);

        // --- Audio path ---
        // Preamp (3x, Q8.8): (x * 768) >> 8, clamp to audio range.
        int32_t pl = ((int32_t)dry_l * HF_PREAMP_GAIN_Q8_8) >> 8;
        int32_t pr = ((int32_t)dry_r * HF_PREAMP_GAIN_Q8_8) >> 8;
        pl = clamp_audio(pl);
        pr = clamp_audio(pr);
        int16_t pre_l = (int16_t)pl;
        int16_t pre_r = (int16_t)pr;

        // Envelope follower tap (pre-fuzz, post-preamp): sum of channels.
        int16_t mono_pre = (int16_t)((pre_l + pre_r) >> 1);
        envelope_follow(mono_pre, &env_, HF_ENV_ATTACK_Q15, HF_ENV_RELEASE_Q15);

        // DC blocker #1 (post-preamp, pre-fuzz).
        pre_l = dc_block(pre_l, &dc1_l_);
        pre_r = dc_block(pre_r, &dc1_r_);

        // Fuzz stage (if enabled).
        int16_t fuzz_l = pre_l;
        int16_t fuzz_r = pre_r;
        if (fuzz_enabled) {
            fuzz_l = apply_fuzz(pre_l, active_fuzz_type, x_val);
            fuzz_r = apply_fuzz(pre_r, active_fuzz_type, x_val);
            // DC blocker #2 (post-fuzz, catches asymmetry DC).
            fuzz_l = dc_block(fuzz_l, &dc2_l_);
            fuzz_r = dc_block(fuzz_r, &dc2_r_);
        }

        // Wah stage: SVF LP, shared cutoff, independent state.
        int32_t svf_in_l = (int32_t)fuzz_l << HF_SVF_SHIFT;
        int32_t svf_in_r = (int32_t)fuzz_r << HF_SVF_SHIFT;
        int32_t svf_out_l = svf_l_.Process(svf_in_l);
        int32_t svf_out_r = svf_r_.Process(svf_in_r);
        int16_t wet_l = clamp_audio(svf_out_l >> HF_SVF_SHIFT);
        int16_t wet_r = clamp_audio(svf_out_r >> HF_SVF_SHIFT);

        // Crossfade dry <-> wet (true interpolation, bounded).
        int16_t out_l = crossfade(dry_l, wet_l, xfade_ramp);
        int16_t out_r = crossfade(dry_r, wet_r, xfade_ramp);

        // Final clamp (belt-and-suspenders) and output.
        out_l = clamp_audio(out_l);
        out_r = clamp_audio(out_r);
        AudioOut1(out_l);
        AudioOut2(out_r);

        // Publish display state for core1 LED updates.
        c0_to_c1.drive_level = x_val;
        c0_to_c1.blend_level = blend_display;
        c0_to_c1.sweep_position = svf_freq_;
        c0_to_c1.fuzz_type = fuzz_type_;
        c0_to_c1.switch_pos = switch_pos_;
        c0_to_c1.auto_wah = auto_wah_;
        c0_to_c1.resonance = y_val;
        c0_to_c1.envelope = env_;
    }

private:
    // --- Live DSP state (core0 only) ---
    braids::Svf svf_l_, svf_r_;
    int16_t svf_freq_ = 0;
    int16_t svf_freq_target_ = 0;
    int16_t svf_resonance_ = 0;
    int16_t env_ = 0;                    // envelope follower state
    int32_t dc1_l_ = 0, dc1_r_ = 0;       // DC blocker #1 (pre-fuzz)
    int32_t dc2_l_ = 0, dc2_r_ = 0;       // DC blocker #2 (post-fuzz)
    int block_ctr_ = 0;

    // --- CV1 connection debounce ---
    uint8_t cv1_conn_counter_ = 0;
    bool cv1_connected_ = false;

    // --- Settings (defaults, no persistence) ---
    uint8_t fuzz_type_ = 0;               // 0=soft, 1=hard, 2=asym, 3=fold
    uint8_t switch_pos_ = 1;              // Middle
    uint8_t auto_wah_ = 0;
    uint16_t auto_wah_base_idx_ = 0;      // LUT index for auto-wah base freq

    // --- Double-tap Down detection (auto-wah toggle) ---
    enum DoubleTapState : uint8_t {
        DT_IDLE,      // no tap activity
        DT_WAITING,   // first tap complete, waiting for second within 0.5 s
        DT_COOLDOWN,  // toggle just happened; next release goes to IDLE
    };
    DoubleTapState dt_state_ = DT_IDLE;
    uint32_t sample_ctr_ = 0;             // global sample counter
    uint8_t prev_sw_pos_ = 1;             // switch pos from previous sample
    uint32_t first_tap_release_time_ = 0; // sample count when first tap released

    // --- Blink pattern state (core1 only) ---
    uint16_t blink_tick_ = 0;
    static constexpr int BLINK_SLOW_HALF = 50;   // ~1 Hz
    static constexpr int BLINK_FAST_HALF = 12;   // ~4 Hz

    // --- LED updates (called from core1 ~100 Hz) ---
    void update_leds_() {
        uint8_t sw = c0_to_c1.switch_pos;
        uint8_t ft = c0_to_c1.fuzz_type;
        uint8_t aw = c0_to_c1.auto_wah;
        uint16_t drive = c0_to_c1.drive_level;
        uint16_t blend = c0_to_c1.blend_level;
        int16_t sweep = c0_to_c1.sweep_position;
        uint16_t res = c0_to_c1.resonance;
        int16_t env = c0_to_c1.envelope;

        blink_tick_++;

        // ---- Top row (0, 1): drive + blend ----
        // LED 0: drive level (Up/Mid) or off (Down)
        // LED 1: blend amount (Mid/Down) or off (Up)
        if (sw == 2) {  // Up: fuzz + wah
            LedBrightness(0, drive);
            LedBrightness(1, 0);     // no blend in Up mode
        } else if (sw == 1) {  // Mid: fuzz blend
            LedBrightness(0, drive);
            LedBrightness(1, blend); // Main pot = fuzz blend
        } else {  // Down: wah blend
            LedBrightness(0, 0);     // no drive in Down mode
            LedBrightness(1, blend); // X pot = wah blend
        }

        // ---- Middle row (2, 3): wah state ----
        // LED 2: sweep position (proportional to cutoff frequency)
        // LED 3: resonance (proportional to Y pot)
        {
            int32_t sp = sweep;
            int32_t bright = ((sp - 9519) * 3990) >> 12;
            if (bright < 0) bright = 0;
            if (bright > 4095) bright = 4095;
            LedBrightness(2, (uint16_t)bright);
        }
        LedBrightness(3, res);

        // ---- Bottom row (4, 5): envelope + fuzz type ----
        // LED 4: envelope level (auto-wah) or off (manual).
        //        Brief full-brightness flash on auto-wah toggle.
        // LED 5: fuzz type pattern (always shows current type)
        uint16_t flash = c0_to_c1.toggle_flash;
        if (flash > 0) {
            c0_to_c1.toggle_flash = flash - 1;
            LedBrightness(4, 4095);
        } else if (aw) {
            int32_t env_u = env < 0 ? -env : env;
            int32_t env_led = env_u << 1;
            if (env_led > 4095) env_led = 4095;
            LedBrightness(4, (uint16_t)env_led);
        } else {
            LedBrightness(4, 0);
        }

        // LED 5: fuzz type pattern (always visible)
        uint16_t led5;
        switch (ft) {
            case FUZZ_SOFT:  led5 = 2048; break;
            case FUZZ_HARD:  led5 = 4095; break;
            case FUZZ_ASYM:  led5 = ((blink_tick_ / BLINK_SLOW_HALF) & 1) ? 4095 : 0; break;
            case FUZZ_FOLD:  led5 = ((blink_tick_ / BLINK_FAST_HALF) & 1) ? 4095 : 0; break;
            default:         led5 = 2048; break;
        }
        LedBrightness(5, led5);
    }
};

// ---------------------------------------------------------------------------
// Entry points
// ---------------------------------------------------------------------------

HotFuzz *g_hotfuzz = nullptr;

void second_core_main() {
    if (g_hotfuzz) g_hotfuzz->SecondCore();
}

int main() {
    set_sys_clock_khz(144000, true);  // 144 MHz = 3x48 kHz, ADC-friendly

    static HotFuzz card;
    g_hotfuzz = &card;

    // Enable normalisation probe (must be before Run()).
    card.EnableNormalisationProbe();

    // Launch core1 for LED updates.
    multicore_launch_core1(second_core_main);

    // Run audio on core0 (blocking).
    card.Run();
    return 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// Source: braids_resources.cc
// ──────────────────────────────────────────────────────────────────────────────

// Braids SVF lookup tables — implementation.
// Vendored from releases/10_twists/src/braids/resources.cc, keeping only the
// three SVF tables to avoid pulling the full 11k-line resources file.
//
// Original: Copyright 2012 Emilie Gillet, MIT license.

#include "braids_resources.h"

namespace braids {

// 257 entries; the final 128 saturate at 25078 (Nyquist clamp).
const uint16_t lut_svf_cutoff[] = {
      17,     18,     19,     20,
      22,     23,     24,     26,
      27,     29,     31,     33,
      35,     37,     39,     41,
      44,     46,     49,     52,
      55,     58,     62,     66,
      70,     74,     78,     83,
      88,     93,     99,    105,
     111,    117,    124,    132,
     140,    148,    157,    166,
     176,    187,    198,    210,
     222,    235,    249,    264,
     280,    297,    314,    333,
     353,    374,    396,    420,
     445,    471,    499,    529,
     561,    594,    629,    667,
     706,    748,    793,    840,
     890,    943,    999,   1059,
    1122,   1188,   1259,   1334,
    1413,   1497,   1586,   1681,
    1781,   1886,   1999,   2117,
    2243,   2377,   2518,   2668,
    2826,   2994,   3172,   3361,
    3560,   3772,   3996,   4233,
    4485,   4751,   5033,   5332,
    5648,   5983,   6337,   6713,
    7111,   7532,   7978,   8449,
    8949,   9477,  10037,  10628,
   11254,  11916,  12616,  13356,
   14138,  14964,  15837,  16758,
   17730,  18756,  19837,  20975,
   22174,  23435,  24761,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,  25078,  25078,  25078,
   25078,
};

const uint16_t lut_svf_damp[] = {
   65534,  49213,  46125,  44055,
   42453,  41129,  39991,  38988,
   38086,  37266,  36512,  35812,
   35158,  34544,  33965,  33416,
   32893,  32395,  31918,  31460,
   31021,  30597,  30188,  29793,
   29411,  29041,  28681,  28332,
   27992,  27661,  27339,  27024,
   26717,  26418,  26125,  25838,
   25558,  25283,  25014,  24750,
   24491,  24236,  23987,  23742,
   23501,  23264,  23031,  22802,
   22577,  22355,  22136,  21921,
   21708,  21499,  21293,  21089,
   20889,  20691,  20495,  20302,
   20112,  19924,  19738,  19555,
   19373,  19194,  19017,  18842,
   18668,  18497,  18327,  18160,
   17994,  17830,  17667,  17506,
   17347,  17189,  17033,  16878,
   16725,  16573,  16423,  16274,
   16126,  15980,  15834,  15691,
   15548,  15407,  15266,  15127,
   14989,  14853,  14717,  14582,
   14449,  14316,  14185,  14054,
   13925,  13796,  13669,  13542,
   13416,  13291,  13167,  13044,
   12922,  12801,  12680,  12561,
   12442,  12324,  12206,  12090,
   11974,  11859,  11744,  11631,
   11518,  11406,  11294,  11183,
   11073,  10964,  10855,  10747,
   10639,  10532,  10426,  10321,
   10215,  10111,  10007,   9904,
    9801,   9699,   9597,   9496,
    9396,   9296,   9196,   9097,
    8999,   8901,   8804,   8707,
    8610,   8514,   8419,   8324,
    8230,   8136,   8042,   7949,
    7856,   7764,   7672,   7581,
    7490,   7400,   7309,   7220,
    7131,   7042,   6953,   6865,
    6778,   6690,   6604,   6517,
    6431,   6345,   6260,   6175,
    6090,   6006,   5922,   5839,
    5755,   5673,   5590,   5508,
    5426,   5345,   5263,   5183,
    5102,   5022,   4942,   4862,
    4783,   4704,   4626,   4547,
    4469,   4391,   4314,   4237,
    4160,   4083,   4007,   3931,
    3855,   3780,   3705,   3630,
    3555,   3481,   3407,   3333,
    3259,   3186,   3113,   3040,
    2968,   2895,   2823,   2752,
    2680,   2609,   2538,   2467,
    2396,   2326,   2256,   2186,
    2116,   2047,   1978,   1909,
    1840,   1771,   1703,   1635,
    1567,   1500,   1432,   1365,
    1298,   1231,   1164,   1098,
    1032,    966,    900,    834,
     769,    704,    639,    574,
     510,    445,    381,    317,
     253,
};

const uint16_t lut_svf_scale[] = {
   32767,  28395,  27490,  26866,
   26373,  25958,  25596,  25273,
   24979,  24709,  24458,  24222,
   24000,  23790,  23589,  23398,
   23214,  23037,  22867,  22703,
   22544,  22389,  22239,  22093,
   21951,  21812,  21677,  21544,
   21415,  21288,  21163,  21041,
   20922,  20804,  20688,  20574,
   20462,  20352,  20243,  20136,
   20031,  19927,  19824,  19722,
   19622,  19523,  19425,  19328,
   19232,  19137,  19043,  18951,
   18859,  18768,  18677,  18588,
   18499,  18411,  18324,  18238,
   18152,  18067,  17983,  17899,
   17815,  17733,  17651,  17569,
   17488,  17408,  17328,  17249,
   17170,  17091,  17013,  16935,
   16858,  16781,  16705,  16629,
   16553,  16478,  16403,  16328,
   16254,  16180,  16106,  16033,
   15960,  15887,  15815,  15743,
   15671,  15599,  15528,  15456,
   15386,  15315,  15244,  15174,
   15104,  15034,  14964,  14895,
   14826,  14756,  14687,  14619,
   14550,  14482,  14413,  14345,
   14277,  14209,  14141,  14074,
   14006,  13939,  13871,  13804,
   13737,  13670,  13603,  13536,
   13469,  13402,  13336,  13269,
   13202,  13136,  13070,  13003,
   12937,  12870,  12804,  12738,
   12672,  12605,  12539,  12473,
   12407,  12341,  12274,  12208,
   12142,  12076,  12010,  11943,
   11877,  11811,  11744,  11678,
   11611,  11545,  11478,  11412,
   11345,  11278,  11211,  11144,
   11077,  11010,  10943,  10876,
   10808,  10741,  10673,  10605,
   10538,  10470,  10401,  10333,
   10265,  10196,  10127,  10058,
    9989,   9920,   9850,   9780,
    9710,   9640,   9570,   9499,
    9429,   9357,   9286,   9215,
    9143,   9071,   8998,   8925,
    8852,   8779,   8705,   8631,
    8557,   8482,   8407,   8332,
    8256,   8179,   8103,   8025,
    7948,   7870,   7791,   7712,
    7632,   7552,   7471,   7390,
    7308,   7225,   7142,   7058,
    6973,   6888,   6801,   6714,
    6626,   6538,   6448,   6358,
    6266,   6173,   6080,   5985,
    5889,   5791,   5692,   5592,
    5491,   5388,   5283,   5176,
    5067,   4957,   4844,   4729,
    4612,   4491,   4368,   4242,
    4112,   3979,   3841,   3698,
    3550,   3397,   3236,   3068,
    2890,   2701,   2499,   2280,
    2038,
};

}  // namespace braids

} // namespace Card_HotFuzz

extern "C" {
    __attribute__((weak)) void tuh_midi_mount_cb(uint8_t, uint8_t, uint8_t, uint8_t, uint16_t) {}
    __attribute__((weak)) void tuh_midi_rx_cb(uint8_t, uint32_t) {}
    __attribute__((weak)) void tuh_midi_umount_cb(uint8_t, uint8_t) {}
    void set_thread_globals(CardGlobals* inst) {
        t_instance = inst;
        if (inst) {
            if (!inst->card_ptr && ComputerCard::thisptr) {
                inst->card_ptr = ComputerCard::thisptr;
            }
            ComputerCard::thisptr = inst->card_ptr;
        }
    }
    void set_core1_thread(bool is_core1) {
        is_core1_thread = is_core1;
    }
    void run_card() {
        is_core1_thread = false;
        try {
            Card_HotFuzz::main();
        } catch (const ThreadExitException& e) {
            // Thread terminated safely
        }
    }
}
