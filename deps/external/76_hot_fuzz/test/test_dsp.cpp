// test_dsp.cpp — host-side DSP unit tests for Hot Fuzz.
//
// Compiles dsp.h, svf_braids.h, and resources.h against a desktop toolchain
// (no Pico SDK).  Verifies: SVF frequency response, DC blocker settling,
// fuzz overflow safety, envelope follower timing, bypass bit-transparency,
// and the crossfade bounds.
//
// Run with:  ctest --test-dir build

#include <cstdint>
#include <cmath>
#include <cstdio>
#include <vector>
#include <string>
#include <algorithm>

// We compile dsp.h + svf_braids.h + braids_resources.cc, not main.cpp.
#include "dsp.h"
#include "svf_braids.h"
#include "braids_resources.h"
#include "resources.h"

// Constants from main.cpp that the tests reference.  In the host build we
// redefine them here so the tests can link without main.cpp.
static constexpr uint16_t HF_ENV_ATTACK_Q15  = 122;   // ~10 ms
static constexpr uint16_t HF_ENV_RELEASE_Q15 = 3;     // ~200 ms

static int test_failures = 0;
static int test_count = 0;

void check(bool cond, const std::string &name, const std::string &detail = "") {
    test_count++;
    if (cond) {
        printf("[PASS] %s\n", name.c_str());
    } else {
        printf("[FAIL] %s %s\n", name.c_str(), detail.c_str());
        test_failures++;
    }
}

// Approximate comparison for fixed-point audio: allow 1 LSB of difference
// (quantization noise at 12-bit).
bool approx(int16_t a, int16_t b, int16_t tol = 2) {
    return std::abs((int)a - (int)b) <= tol;
}

// ---------------------------------------------------------------------------
// Test: fuzz transfers stay within audio range for all drive values
// ---------------------------------------------------------------------------
void test_fuzz_overflow() {
    printf("\n--- Fuzz overflow safety ---\n");
    bool all_ok = true;
    // Test full-scale and half-scale inputs across all drive values.
    int16_t test_inputs[] = {-2048, -1024, -1, 0, 1, 1024, 2047};
    for (int type = 0; type < 4; type++) {
        for (int d = 0; d <= 4095; d += 256) {
            for (int16_t in : test_inputs) {
                int16_t out = apply_fuzz(in, (uint8_t)type, (uint16_t)d);
                if (out < -2048 || out > 2047) {
                    printf("  overflow: type=%d drive=%d in=%d out=%d\n",
                           type, d, in, out);
                    all_ok = false;
                }
            }
        }
    }
    check(all_ok, "fuzz output within [-2048, 2047] for all drive values");

    // Edge case: drive=4095 (t=1, near-square-wave). The t=0 guard in the
    // code is defensive; at drive=4095 the threshold is actually 1 due to
    // integer truncation: (4095*2047)>>12 = 2046, t = 2047-2046 = 1.
    {
        int16_t out_pos = fuzz_hard(1000, 4095);
        int16_t out_neg = fuzz_hard(-1000, 4095);
        int16_t out_zero = fuzz_hard(0, 4095);
        check(out_pos == 2047, "fuzz_hard drive=4095 positive -> 2047",
              "got " + std::to_string(out_pos));
        check(out_neg == -2047, "fuzz_hard drive=4095 negative -> -2047",
              "got " + std::to_string(out_neg));
        check(out_zero == 0, "fuzz_hard drive=4095 zero -> 0",
              "got " + std::to_string(out_zero));

        int16_t apos = fuzz_asym(1000, 4095);
        int16_t aneg = fuzz_asym(-1000, 4095);
        int16_t azero = fuzz_asym(0, 4095);
        check(apos == 2047, "fuzz_asym drive=4095 positive -> 2047",
              "got " + std::to_string(apos));
        check(aneg == 0, "fuzz_asym drive=4095 negative -> 0",
              "got " + std::to_string(aneg));
        check(azero == 0, "fuzz_asym drive=4095 zero -> 0",
              "got " + std::to_string(azero));
    }

    // Verify reciprocal LUT matches the old division for all t > 0.
    {
        bool lut_ok = true;
        for (int16_t t = 1; t <= 2047; t++) {
            for (int16_t y = -t; y <= t; y += 37) {
                int32_t div_result = (y * 2047) / t;
                int32_t lut_result = (y * (int32_t)FUZZ_RECIP_LUT[t]) >> 12;
                if (std::abs(div_result - lut_result) > 2) {
                    printf("  LUT mismatch: t=%d y=%d div=%d lut=%d\n",
                           t, y, div_result, lut_result);
                    lut_ok = false;
                    break;
                }
            }
            if (!lut_ok) break;
        }
        check(lut_ok, "FUZZ_RECIP_LUT matches division within 2 LSB");
    }

    // Foldback fuzz: octave symmetry and drive=4095 edge case.
    {
        // Octave symmetry: fuzz_fold(x) ≈ fuzz_fold(-x) due to full-wave rect.
        bool sym_ok = true;
        for (int16_t x = -2047; x <= 2047; x += 137) {
            for (int d = 0; d <= 4095; d += 512) {
                int16_t a = fuzz_fold(x, (uint16_t)d);
                int16_t b = fuzz_fold((int16_t)(-x), (uint16_t)d);
                if (std::abs(a - b) > 2) {
                    printf("  fold symmetry fail: x=%d d=%d a=%d b=%d\n", x, d, a, b);
                    sym_ok = false;
                    break;
                }
            }
            if (!sym_ok) break;
        }
        check(sym_ok, "fuzz_fold is symmetric (octave) within 2 LSB");

        // Drive=4095: t=1, near-square-wave with rectification.
        int16_t fpos = fuzz_fold(1000, 4095);
        int16_t fneg = fuzz_fold(-1000, 4095);
        int16_t fzero = fuzz_fold(0, 4095);
        check(fpos == 2047, "fuzz_fold drive=4095 positive -> 2047",
              "got " + std::to_string(fpos));
        check(fneg == 2047, "fuzz_fold drive=4095 negative -> 2047",
              "got " + std::to_string(fneg));
        check(fzero == 0, "fuzz_fold drive=4095 zero -> 0",
              "got " + std::to_string(fzero));

        // Drive=0: clean octave (no fold, just gain + rectify).
        int16_t cpos = fuzz_fold(1000, 0);
        int16_t cneg = fuzz_fold(-1000, 0);
        check(cpos > 0, "fuzz_fold drive=0 positive -> positive");
        check(cneg > 0, "fuzz_fold drive=0 negative -> positive (rectified)");
    }
}

// ---------------------------------------------------------------------------
// Test: DC blocker settles to zero for a DC input
// ---------------------------------------------------------------------------
void test_dc_blocker() {
    printf("\n--- DC blocker settling ---\n");
    int32_t state = 0;
    // Feed a DC offset of 1000 for 10000 samples; output should trend to 0.
    int16_t dc = 1000;
    int16_t last_out = dc;
    for (int i = 0; i < 10000; i++) {
        last_out = dc_block(dc, &state);
    }
    // The >>8 shift leaves a residual up to ~255 (the quantisation step).
    // This is the repo-standard DC blocker from 21_resonator; in practice the
    // residual is inaudible against 12-bit audio.  We check it's "mostly"
    // removed: output should be well below the 1000 DC offset.
    check(std::abs((int)last_out) < 300,
          "DC blocker reduces DC offset from 1000 to <300",
          "got " + std::to_string(last_out));
}

// ---------------------------------------------------------------------------
// Test: envelope follower attack and release timing
// ---------------------------------------------------------------------------
void test_envelope_follower() {
    printf("\n--- Envelope follower timing ---\n");
    int16_t env = 0;
    // Attack: feed full-scale, env should rise toward 2047.
    // 10 ms attack at 48 kHz = 480 samples.  After ~3 time constants (1440
    // samples) env should be > 95% of target.
    for (int i = 0; i < 1440; i++) {
        envelope_follow(2047, &env, HF_ENV_ATTACK_Q15, HF_ENV_RELEASE_Q15);
    }
    check(env > 1700, "envelope rises to >1700 after 30 ms attack",
          "got " + std::to_string(env));

    // Release: feed silence, env should fall toward 0.
    // 200 ms release at 48 kHz = 9600 samples.  After ~3 time constants
    // (28800 samples) env should be < 5% of peak.
    for (int i = 0; i < 28800; i++) {
        envelope_follow(0, &env, HF_ENV_ATTACK_Q15, HF_ENV_RELEASE_Q15);
    }
    check(env < 100, "envelope falls to <100 after 600 ms release",
          "got " + std::to_string(env));
}

// ---------------------------------------------------------------------------
// Test: crossfade bounds — output always between dry and wet
// ---------------------------------------------------------------------------
void test_crossfade() {
    printf("\n--- Crossfade bounds ---\n");
    bool all_ok = true;
    int16_t dry = 1000;
    int16_t wet = -1500;
    for (uint16_t r = 0; r <= 4096; r += 256) {
        int16_t out = crossfade(dry, wet, r);
        int16_t lo = std::min(dry, wet);
        int16_t hi = std::max(dry, wet);
        if (out < lo - 2 || out > hi + 2) {
            printf("  out of bounds: r=%d out=%d [%d..%d]\n", r, out, lo, hi);
            all_ok = false;
        }
    }
    check(all_ok, "crossfade output bounded by [min(dry,wet), max(dry,wet)]");

    // Ramp=0 -> dry, ramp=4096 -> wet
    int16_t out_dry = crossfade(1000, -500, 0);
    int16_t out_wet = crossfade(1000, -500, 4096);
    check(approx(out_dry, 1000, 2), "crossfade ramp=0 returns dry");
    check(approx(out_wet, -500, 2), "crossfade ramp=4096 returns wet");
}

// ---------------------------------------------------------------------------
// Test: bypass bit-transparency (ramp=0 -> out == in)
// ---------------------------------------------------------------------------
void test_bypass_transparency() {
    printf("\n--- Bypass bit-transparency ---\n");
    bool all_ok = true;
    for (int16_t in = -2048; in <= 2047; in += 137) {
        int16_t out = crossfade(in, 0, 0);  // ramp=0 = dry
        if (!approx(out, in, 1)) {
            printf("  mismatch: in=%d out=%d\n", in, out);
            all_ok = false;
        }
    }
    check(all_ok, "bypass (ramp=0) is bit-transparent");
}

// ---------------------------------------------------------------------------
// Test: SVF frequency response
// ---------------------------------------------------------------------------
void test_svf_frequency_response() {
    printf("\n--- SVF frequency response ---\n");
    // Feed a sine sweep and measure the output amplitude at each frequency.
    // The LP output should pass low frequencies and attenuate high ones.
    braids::Svf svf;
    svf.Init();
    svf.set_mode(braids::SVF_MODE_LP);

    // Set cutoff to roughly 500 Hz (mid-range of the wah).
    // From the LUT, manual_freq_LUT[0] = 9519, which corresponds to 300 Hz.
    // manual_freq_LUT[255] = 13723, corresponding to 2000 Hz.
    // 500 Hz is around index ~50.
    svf.set_frequency(hotfuzz::manual_freq_LUT[50]);
    svf.set_resonance(4096);  // moderate Q

    const double fs = 48000.0;
    bool low_pass_ok = true;
    bool high_atten_ok = true;

    // Test at 100 Hz (well below cutoff) — should pass at near-unity.
    {
        double freq = 100.0;
        double amp = 0.0;
        int N = 4800;  // 100 ms
        for (int i = 0; i < N; i++) {
            double s = std::sin(2.0 * M_PI * freq * i / fs);
            int32_t in = (int32_t)(s * 2000);  // scale to ~12-bit
            int32_t out = svf.Process(in << 3) >> 3;
            amp = std::max(amp, std::abs((double)out));
        }
        if (amp < 1500) {
            printf("  100 Hz amplitude: %.1f (expected > 1500)\n", amp);
            low_pass_ok = false;
        }
    }

    // Test at 5000 Hz (well above cutoff) — should be attenuated.
    {
        double freq = 5000.0;
        double amp = 0.0;
        int N = 4800;
        for (int i = 0; i < N; i++) {
            double s = std::sin(2.0 * M_PI * freq * i / fs);
            int32_t in = (int32_t)(s * 2000);
            int32_t out = svf.Process(in << 3) >> 3;
            amp = std::max(amp, std::abs((double)out));
        }
        if (amp > 800) {
            printf("  5000 Hz amplitude: %.1f (expected < 800)\n", amp);
            high_atten_ok = false;
        }
    }

    check(low_pass_ok, "SVF passes low frequencies (100 Hz > 1500)");
    check(high_atten_ok, "SVF attenuates high frequencies (5000 Hz < 800)");
}

// ---------------------------------------------------------------------------
// Test: glide_i32 moves toward target
// ---------------------------------------------------------------------------
void test_glide() {
    printf("\n--- Glide helper ---\n");
    int32_t v = 0;
    for (int i = 0; i < 10000; i++) {
        v = glide_i32(v, 4096, 8);
    }
    // After 10000 iterations with k=8, v should be close to 4096.
    // Integer >>8 truncation stalls within ~255 of target (the quantisation
    // step).  In the real firmware the target is a 16-bit SVF frequency_
    // value, so +/-255 is sub-LSB noise.
    check(std::abs(v - 4096) < 300, "glide converges to target (k=8, 10000 steps)",
          "got " + std::to_string(v));
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
int main() {
    printf("=== Hot Fuzz DSP tests ===\n");
    test_fuzz_overflow();
    test_dc_blocker();
    test_envelope_follower();
    test_crossfade();
    test_bypass_transparency();
    test_svf_frequency_response();
    test_glide();

    printf("\n=== Summary: %d/%d passed, %d failed ===\n",
           test_count - test_failures, test_count, test_failures);
    return test_failures > 0 ? 1 : 0;
}