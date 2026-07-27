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

#include "ComputerCard.h"
#include "pico/multicore.h"
#include "dsp.h"
#include "svf_braids.h"
#include "braids_resources.h"
#include "resources.h"

#include <stdint.h>

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

            busy_wait_us(1000);  // ~1 ms tick
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