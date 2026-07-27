// Voices of SID - dual MOS 6581 SID emulation (reSID engine)
//
// Architecture overview:
//   sid1 — voices 0+2 — AudioOut1 (left channel)
//   sid2 — voice  1   — AudioOut2 (right channel)
//
// Voice 3 (sid1 only) provides the oscillator for ring modulation
// and hard sync on voice 1. Voice 2 on sid1 and voices 1+3 on
// sid2 are muted — each chip runs only its active voices.
//
// CV and pulse inputs are passed through to the outputs:
//   CVIn1 → CVOut1, CVIn2 → CVOut2 (1:1 voltage copy)
//   PulseIn1 → PulseOut1, PulseIn2 → PulseOut2 (gate passthrough)
// This allows chaining multiple modules by patching through.
//
// Three-mode switch:
//   Up    — Sound Edit: X/Y select waveforms, Main controls decay/release
//   Mid   — Play: CV/gate control, Main=decay/release, X=resonance, Y=pulse width
//   Down  — Randomize envelope continuously with spinning LED animation
//
// CV inputs use 1V/oct scaling: 341 ADC counts = 1 octave.
// 107-note lookup table covers MIDI 0–106 (C-1–F#8) of SID frequency
// divisor values.

#include "ComputerCard.h"
#include "reSID/sid.h"
#include "pico/stdlib.h"
#include <stdlib.h>

// PAL C64 clock — the SID's internal reference for pitch and envelope timing.
// We keep an exact average clock by accumulating 985248/48000 cycles per sample
// and alternating 20/21-cycle steps as needed.
static constexpr int   SID_CLOCK_HZ      = 985248;
static constexpr int   SAMPLE_RATE_HZ    = 48000;
static constexpr float SID_CLOCK         = 985248.0f;

// Control-rate updates: 48000 / 240 = 200 Hz (every 5 ms).
// Knobs, switch, and CV are only read inside this control-rate block
// to avoid interrupt-safety issues with the audio callback.
static constexpr int   CONTROL_RATE      = 240;

// 1V/octave scaling for CV inputs.
// The computer's 12-bit ADC maps ~±6V to -2048..2047, giving
// 4096 / 12 ≈ 341 counts per volt = 1 octave.
static constexpr int   COUNTS_PER_OCTAVE = 341;

// SID frequency divisor table for MIDI notes 0–106 (C-1–F#8).
// Each entry = round(440 × 2^((midi−69)/12) × 16777216 / 985248).
// That is: Hz_to_frequency_register ÷ clock_frequency.
// Spacing follows the 12th-root-of-2 for chromatic semitone intervals.
// Values above 65535 are clamped — these are below the SID's minimum
// frequency (sub-sonic range) and produce the same pitch.
static const uint16_t note_table[107] = {
      139,   147,   156,   166,   175,   186,   197,   209,   221,   234,   248,   263,
      278,   295,   313,   331,   351,   372,   394,   417,   442,   468,   496,   526,
      557,   590,   625,   662,   702,   743,   788,   834,   884,   937,   992,  1051,
     1114,  1180,  1250,  1325,  1403,  1487,  1575,  1669,  1768,  1873,  1985,  2103,
     2228,  2360,  2500,  2649,  2807,  2973,  3150,  3338,  3536,  3746,  3969,  4205,
     4455,  4720,  5001,  5298,  5613,  5947,  6300,  6675,  7072,  7493,  7938,  8410,
     8910,  9440, 10001, 10596, 11226, 11894, 12601, 13350, 14144, 14985, 15876, 16820,
    17820, 18880, 20003, 21192, 22452, 23787, 25202, 26700, 28288, 29970, 31752, 33640,
    35641, 37760, 40005, 42384, 44904, 47574, 50403, 53401, 56576, 59940, 63504,
};

// Waveform selection lookup — 8 curated combinations.
// Each entry is the upper nibble of the SID voice control register
// (reg 0x04 / 0x0B / 0x12).
//
// SID waveform bits:  0x80=noise, 0x40=pulse, 0x20=saw, 0x10=triangle.
// Bit 2 (0x04) = ring modulation enable (voice 1 only).
// Bit 1 (0x02) = sync enable.
// Bit 0 (0x01) = gate — added by gate_voice(), not stored here.
//
// Entry 6 (0x14 = triangle | ring) has the ring bit baked in,
// which is redundant with the ring_mod flag. Harmless — OR of
// identical bits is idempotent.
static const uint8_t waveforms[8] = {
    0x40,  // Pulse
    0x10,  // Triangle
    0x20,  // Saw
    0x60,  // Pulse + Saw
    0x30,  // Triangle + Saw
    0x50,  // Triangle + Pulse
    0x14,  // Triangle + Ring mod
    0x80,  // Noise
};

class SIDCard : public ComputerCard
{
    enum Mode { SOUND_EDIT, PLAY };

    // --- SID chips and voice routing ---
    // sid1: voices 0 (lead) and 2 (ring-mod carrier / sync source) → AudioOut1
    // sid2: voice 1 (second voice) → AudioOut2
    // Unused voices are muted in the constructor.
    SID sid1;
    SID sid2;

    Mode mode = PLAY;
    uint16_t control_phase = 0;
    uint32_t sid_cycle_accum = 0;

    // --- Waveform selections (persistent across mode switches) ---
    // Indices 0–7 into waveforms[]. Set by X/Y knobs in Sound Edit mode.
    int wave1_sel = 1;   // default = Triangle
    int wave2_sel = 1;   // default = Triangle

    // --- Envelope parameters (modified by Z-down randomization) ---
    // Attack = 0: instant attack (~2 ms). Sustain = 15: max sustain level.
    // Decay and release are driven by the Main knob (shared for both voices).
    uint8_t attack_v1      = 0;
    uint8_t sustain_v1     = 15;
    uint8_t attack_v2      = 0;
    uint8_t sustain_v2     = 15;
    bool    ring_mod       = false;   // toggled by randomization
    bool    sync_flag      = false;   // toggled by randomization

    // --- Switch state for Z-down edge detection ---
    bool prev_switch_down  = false;
    int  spin_counter      = 0;

    // --- Gate/control cache for audio-rate gate handling ---
    bool force_open_gates  = false;
    bool gate1_state       = false;
    bool gate2_state       = false;
    bool gate_ctrl_dirty   = true;
    uint8_t ctrl1_cached   = waveforms[1];
    uint8_t ctrl2_cached   = waveforms[1];

    // --- SID register helpers ---
    //
    // SID register layout per voice (voice N base address = N × 7):
    //   +0/+1 : frequency divisor  lo/hi  (16-bit)
    //   +2/+3 : pulse width        lo/hi  (12-bit)
    //   +4    : control register   (waveform bits | gate)
    //   +5    : attack (hi nibble) | decay (lo nibble)
    //   +6    : sustain (hi nibble) | release (lo nibble)
    //
    // Chip-level filter registers:
    //   0x15  : filter cutoff    lo (bits 0–2)
    //   0x16  : filter cutoff    hi (bits 0–7) → combined 11-bit value
    //   0x17  : resonance (hi nibble) | voice routing (lo nibble)
    //   0x17  lo nibble = 0x03: voices 0+1 through filter
    //   0x18  : filter mode (hi nibble) | master volume (lo nibble)
    //   0x18  set to 0x1F: low-pass mode, max volume (gain = 1.0)

    /// Convert 1V/oct CV value (-2048..2047) to a SID frequency divisor.
    /// semitones = cv × 12 / 341, then MIDI = 48 + semitones (rounded).
    /// 0V = MIDI 48 (C3). Range: ~4 octaves below, ~5 octaves above.
    /// Clamped to the 107-entry table (MIDI 0–106, C-1–F#8).
    static uint16_t cv_to_freq(int16_t cv) {
        int32_t semitones = cv * 12;
        int32_t midi = 48 + (semitones + (semitones >= 0 ? COUNTS_PER_OCTAVE / 2 : -COUNTS_PER_OCTAVE / 2)) / COUNTS_PER_OCTAVE;
        int idx = midi;
        if (idx < 0)  idx = 0;
        if (idx > 106) idx = 106;
        return note_table[idx];
    }

    /// Write a 16-bit frequency divisor to voice vnum on sid.
    /// vnum is the byte offset, not an index multiplier (e.g. 0 or 14 for voice 3).
    static void write_freq(SID& sid, int vnum, uint16_t fn) {
        sid.write(0x00 + vnum, fn & 0xFF);
        sid.write(0x01 + vnum, (fn >> 8) & 0xFF);
    }

    /// Set gate on/off for voice vnum (0–2).
    /// ctrl contains waveform bits (upper nibble); we OR in the gate bit (0x01)
    /// when on, or mask it out when off. This preserves the waveform selection
    /// while only toggling the gate.
    static void gate_voice(SID& sid, int vnum, bool on, uint8_t ctrl) {
        sid.write(0x04 + vnum * 7, on ? (ctrl | 0x01) : (ctrl & 0xFE));
    }

    /// Write filter cutoff (0–2047) to the SID's 11-bit cutoff register pair.
    static void set_filter_cutoff(SID& sid, int cutoff) {
        sid.write(0x15, cutoff & 0x07);
        sid.write(0x16, cutoff >> 3);
    }

    /// Write attack/decay for one voice. SID reg +5: upper nibble=attack, lower=decay.
    void write_ad(SID& sid, int vnum, uint8_t attack, uint8_t decay) {
        uint8_t reg = 0x05 + vnum * 7;
        sid.write(reg, (attack << 4) | (decay & 0x0F));
    }

    /// Write sustain/release for one voice. SID reg +6: upper nibble=sustain, lower=release.
    void write_sr(SID& sid, int vnum, uint8_t sustain, uint8_t release) {
        uint8_t reg = 0x06 + vnum * 7;
        sid.write(reg, (sustain << 4) | (release & 0x0F));
    }

    /// Convenience wrapper: write all four ADSR registers for one voice.
    void write_voice_ad_sr(SID& sid, int vnum, uint8_t atk, uint8_t dec, uint8_t sus, uint8_t rel) {
        write_ad(sid, vnum, atk, dec);
        write_sr(sid, vnum, sus, rel);
    }

    /// Read Main knob and map to shared decay/release time (0–15).
    /// k >> 8 maps the 12-bit knob (0–4095) onto 16 linear steps.
    /// Attack and sustain come from the per-voice member variables,
    /// which are randomized by Z-down hold.
    void apply_main_knob() {
        int32_t k = KnobVal(Knob::Main);
        int dr     = k >> 8;

        // sid1 voice 0 (lead) and sid2 voice 1 (second voice)
        write_voice_ad_sr(sid1, 0, attack_v1, dr, sustain_v1, dr);
        write_voice_ad_sr(sid2, 1, attack_v2, dr, sustain_v2, dr);
    }

    /// Configure voice 3 on sid1 as the ring-mod/sync oscillator.
    /// Uses voice 1's frequency with a triangle waveform.
    /// Voice 3 is enabled when the ring-mod or sync bit is set in the
    /// voice 1 control register — this covers both the randomization
    /// flags AND waveform table entry 6 (0x14 = triangle | ring).
    void configure_voice3(uint16_t freq, uint8_t ctrl1) {
        bool use_voice3 = (ctrl1 & 0x06) != 0;
        write_freq(sid1, 14, freq);
        sid1.write(0x12, use_voice3 ? 0x11 : 0x00);
    }

    /// Spinning dot animation on all 6 LEDs while Z is held down.
    /// Advances one position every ~32 control ticks (~160 ms for a full rotation).
    void update_spin_animation() {
        spin_counter++;
        int pos = (spin_counter >> 5) % 6;
        for (int i = 0; i < 6; i++) LedOn(i, i == pos);
    }

    /// Randomize envelope, waveforms, and effects.
    /// Called continuously while Z is held down. Changes are written to the
    /// SID registers immediately so they're audible live.
    /// Envelope ranges are constrained to musical values:
    ///   attack 0–4    (snappy,  ~2–16ms)
    ///   sustain 8–15  (always audible, minimum 50% level)
    /// Waveforms and ring-mod have ~25% chance each; sync is 10% (rare).
    void randomize_patch() {
        attack_v1  = rand() % 5;
        sustain_v1 = 8 + (rand() % 8);
        attack_v2  = rand() % 5;
        sustain_v2 = 8 + (rand() % 8);
        wave1_sel  = rand() % 8;
        wave2_sel  = rand() % 8;
        ring_mod   = (rand() % 4) == 0;
        sync_flag  = (rand() % 10) == 0;
    }

    /// Read X and Y knobs in Sound Edit mode to select waveforms.
    /// >> 9 divides the 12-bit knob range into 8 bands of 512 steps each.
    /// Only updates wave1_sel/wave2_sel when the knob crosses a band boundary.
    void read_waveforms() {
        int v1 = (KnobVal(Knob::X) >> 9) & 7;
        int v2 = (KnobVal(Knob::Y) >> 9) & 7;
        if (v1 != wave1_sel || v2 != wave2_sel) {
            wave1_sel = v1;
            wave2_sel = v2;
        }
    }

    /// Build the SID control register byte for voice 1.
    /// Starts with the waveform byte from the table, then ORs in
    /// ring-mod (0x04) and sync (0x02) bits if enabled by randomization.
    uint8_t build_ctrl1() const {
        uint8_t c = waveforms[wave1_sel];
        if (ring_mod)  c |= 0x04;
        if (sync_flag) c |= 0x02;
        return c;
    }

    /// Build the SID control register byte for voice 2.
    /// Voice 2 does not support ring mod or sync (those are voice 1 features
    /// in the SID hardware), so only the waveform table entry is used.
    uint8_t build_ctrl2() const {
        return waveforms[wave2_sel];
    }

public:
    SIDCard() {
        // Non-deterministic randomization across power cycles.
        srand((unsigned int)to_us_since_boot(get_absolute_time()));

        // --- Initialize SID chips ---
        // Sampling parameters must be configured for reSID timing.
        // Audio here is pulled via clock() + output_filtered().
        // MOS6581 model has the classic aggressive analog filter character.
        // SID_CLOCK must be float — reSID API requirement.

        sid1.reset();
        sid1.set_chip_model(MOS6581);
        sid1.set_sampling_parameters(SID_CLOCK, SAMPLE_INTERPOLATE, 48000.0f);
        sid1.enable_filter(true);

        sid2.reset();
        sid2.set_chip_model(MOS6581);
        sid2.set_sampling_parameters(SID_CLOCK, SAMPLE_INTERPOLATE, 48000.0f);
        sid2.enable_filter(true);

        // --- Voice routing via mute ---
        // sid1: voices 0 and 2 active, voice 1 muted
        // sid2: voice 1 active, voices 0 and 2 muted
        sid1.mute(1, true);
        sid2.mute(0, true);
        sid2.mute(2, true);

        // --- Set initial frequencies ---
        // Write a default pitch (note_table index ~0) to voices 1+2 on both
        // chips. These get overwritten by cv_to_freq() as soon as
        // ProcessSample runs, so the values are arbitrary.
        for (int i = 0; i < 2; i++) {
            sid1.write(0x00 + i * 7, 0x00);
            sid1.write(0x01 + i * 7, 0x11);
            sid2.write(0x00 + i * 7, 0x00);
            sid2.write(0x01 + i * 7, 0x11);
        }

        // --- Default filter state ---
        // Cutoff    = 2047 (fully open, no high-frequency attenuation)
        // Resonance = 0 (no resonance emphasis)
        // Routing   = 0x03 (voices 0+1 through filter, voice 2 bypasses)
        // Mode/Vol  = 0x1F (low-pass mode, maximum output volume)
        sid1.write(0x15, 0x07);
        sid1.write(0x16, 0xFF);
        sid1.write(0x17, 0x03);
        sid1.write(0x18, 0x1F);
        sid2.write(0x15, 0x07);
        sid2.write(0x16, 0xFF);
        sid2.write(0x17, 0x03);
        sid2.write(0x18, 0x1F);

        // Enable cable detection on input jacks.
        // Unconnected CV inputs read 0 rather than floating values.
        EnableNormalisationProbe();
    }

    virtual void ProcessSample();

    /// Clamp reSID output to the 12-bit signed range expected by AudioOut.
    /// reSID produces values in a wider range; the ×2 gain stage before
    /// clamping compensates for the inherently low output levels.
    static int16_t clamp_output(int val) {
        if (val > 2047) return 2047;
        if (val < -2048) return -2048;
        return (int16_t)val;
    }
};

void SIDCard::ProcessSample()
{
    // Exact SID timing with fractional-cycle accumulation.
    sid_cycle_accum += SID_CLOCK_HZ;
    int cycles_this_sample = sid_cycle_accum / SAMPLE_RATE_HZ;
    sid_cycle_accum -= cycles_this_sample * SAMPLE_RATE_HZ;

    // --- Audio generation (runs every sample at 48 kHz) ---
    // Each SID is clocked by cycles_this_sample cycles, producing
    // one output sample. The ×2 gain compensates for reSID's
    // naturally low output levels. clamp_output ensures the result
    // stays within the DAC's 12-bit signed range.

    sid1.clock(cycles_this_sample);
    int16_t out1 = clamp_output(sid1.output_filtered() * 2);

    sid2.clock(cycles_this_sample);
    int16_t out2 = clamp_output(sid2.output_filtered() * 2);

    AudioOut1(out1);
    AudioOut2(out2);

    // Pass through CV and pulse inputs to outputs.
    // CV In → CV Out: 1:1 voltage copy for chaining modules.
    // Pulse In → Pulse Out: gate passthrough.
    CVOut1(CVIn1());
    CVOut2(CVIn2());
    PulseOut1(PulseIn1());
    PulseOut2(PulseIn2());

    // Audio-rate gate handling for low latency and reliable short triggers.
    bool g1 = force_open_gates ? true : PulseIn1();
    bool g2 = force_open_gates ? true : PulseIn2();
    if (gate_ctrl_dirty || g1 != gate1_state) {
        gate_voice(sid1, 0, g1, ctrl1_cached);
        gate1_state = g1;
    }
    if (gate_ctrl_dirty || g2 != gate2_state) {
        gate_voice(sid2, 1, g2, ctrl2_cached);
        gate2_state = g2;
    }
    gate_ctrl_dirty = false;

    // --- Control-rate processing (runs at 200 Hz) ---
    // Knobs, CV, and switch are only read in this sub-sampled block
    // to maintain interrupt safety with the audio callback.
    if (++control_phase >= CONTROL_RATE) {
        control_phase = 0;

        Switch sw = SwitchVal();
        bool down = (sw == Switch::Down);

        // ═══ Z Down (held): continuous randomization ═══
        // On every control-rate tick while Z is held, we randomize
        // envelope parameters and immediately write them to the SID
        // registers so changes are audible in real time.
        // A spinning dot animation plays on all 6 LEDs.
        if (down) {
            force_open_gates = true;
            update_spin_animation();
            randomize_patch();

            // Apply the current Main knob decay/release to both voices.
            int32_t k = KnobVal(Knob::Main);
            int dr = k >> 8;
            write_voice_ad_sr(sid1, 0, attack_v1, dr, sustain_v1, dr);
            write_voice_ad_sr(sid2, 1, attack_v2, dr, sustain_v2, dr);

            ctrl1_cached = build_ctrl1();
            ctrl2_cached = build_ctrl2();
            gate_ctrl_dirty = true;

            uint16_t f1 = cv_to_freq(CVIn1());
            uint16_t f2 = cv_to_freq(CVIn2());
            write_freq(sid1, 0, f1);
            write_freq(sid2, 7, f2);

            configure_voice3(f1, ctrl1_cached);

            prev_switch_down = true;
            return;   // skip normal mode processing while held
        }

        // Detect Z release — just reset the flag.
        // No special action needed; the next tick runs in the current
        // switch position with whatever parameters are live.
        if (!down && prev_switch_down) {
            prev_switch_down = false;
        }

        // ═══ Z Up: Sound Edit mode ═══
        // X/Y knobs select waveforms for voice 1 and voice 2.
        // Gates are always open (drone/tuning mode).
        // LED layout: top row (0–2) = V1 waveform index,
        //             bottom row (3–5) = V2 waveform index.
        if (sw == Switch::Up) {
            mode = SOUND_EDIT;
            force_open_gates = true;

            read_waveforms();

            apply_main_knob();

            // In Sound Edit mode, X/Y are repurposed for waveform selection.
            // Write a default pulse width (50% duty cycle) and zero resonance
            // so pulse waveforms are audible and the sound is consistent when
            // switching to Play mode.
            sid1.write(0x17, 0x03);
            sid2.write(0x17, 0x03);
            uint16_t pw_default = 2048;
            sid1.write(0x02, pw_default & 0xFF);
            sid1.write(0x03, (pw_default >> 8) & 0x0F);
            sid2.write(0x09, pw_default & 0xFF);
            sid2.write(0x0A, (pw_default >> 8) & 0x0F);

            ctrl1_cached = build_ctrl1();
            ctrl2_cached = build_ctrl2();
            gate_ctrl_dirty = true;

            uint16_t f1 = cv_to_freq(CVIn1());
            uint16_t f2 = cv_to_freq(CVIn2());
            write_freq(sid1, 0, f1);
            write_freq(sid2, 7, f2);

            configure_voice3(f1, ctrl1_cached);

            LedOn(0, (wave1_sel >> 0) & 1);
            LedOn(1, (wave1_sel >> 1) & 1);
            LedOn(2, (wave1_sel >> 2) & 1);
            LedOn(3, (wave2_sel >> 0) & 1);
            LedOn(4, (wave2_sel >> 1) & 1);
            LedOn(5, (wave2_sel >> 2) & 1);
        } else {
            // ═══ Z Middle: Play mode ═══
            // Main = decay/release time
            // X    = filter resonance (0–15)
            // Y    = pulse width (shared by both voices)
            // Pulse1 gates voice 1, Pulse2 gates voice 2.
            // LED layout: top row (0–1) = gate indicators,
            //             bottom row (3–5) = V1 waveform index.

            mode = PLAY;
            force_open_gates = false;

            ctrl1_cached = build_ctrl1();
            ctrl2_cached = build_ctrl2();
            gate_ctrl_dirty = true;

            apply_main_knob();

            int32_t res = KnobVal(Knob::X) >> 8;
            if (res > 15) res = 15;
            sid1.write(0x17, (res << 4) | 0x03);
            sid2.write(0x17, (res << 4) | 0x03);

            uint16_t pw = KnobVal(Knob::Y) & 0x0FFF;
            if (pw < 40) pw = 40;
            sid1.write(0x02, pw & 0xFF);
            sid1.write(0x03, (pw >> 8) & 0x0F);
            sid2.write(0x09, pw & 0xFF);
            sid2.write(0x0A, (pw >> 8) & 0x0F);

            uint16_t f1 = cv_to_freq(CVIn1());
            uint16_t f2 = cv_to_freq(CVIn2());
            write_freq(sid1, 0, f1);
            write_freq(sid2, 7, f2);

            configure_voice3(f1, ctrl1_cached);

            LedOn(0, gate1_state);
            LedOn(1, gate2_state);
            LedOn(2, 0);
            LedOn(3, (wave1_sel >> 0) & 1);
            LedOn(4, (wave1_sel >> 1) & 1);
            LedOn(5, (wave1_sel >> 2) & 1);
        }
    }
}

int main()
{
    // 200 MHz gives headroom for the dual reSID engine while staying
    // within the Pico SDK's supported clock range (up to 200 MHz).
    set_sys_clock_khz(200000, true);
    SIDCard sidCard;
    sidCard.Run();
}
