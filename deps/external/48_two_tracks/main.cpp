/*
 * two_tracks — Dual-read-head phase looper for the Music Thing Modular
 * Workshop Computer (RP2040).
 *
 * Records a mono audio loop to flash (IMA-ADPCM, 4 bits/sample) and plays it
 * back through two independent read heads with separately controllable
 * positions and loop lengths, creating evolving phase patterns.
 *
 * Storage engine: two_tracks_stream (forked from Goldfish by Dune Desormeaux).
 * DSP/control layer: this file.
 * Codec: adpcm.h (from MLRws / Goldfish).
 * Framework: ComputerCard.h (header-only, by Chris Johnson).
 */

#include "ComputerCard.h"
#include "two_tracks_stream.h"

#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"

/*
 * Constants
 */

static constexpr uint32_t SAMPLE_RATE       = 48000;      // Hz
static constexpr uint32_t COUNTDOWN_SAMPLES = 3 * SAMPLE_RATE;  // 3 s armed
static constexpr uint32_t BEEP_SAMPLES      = SAMPLE_RATE / 100; // 10 ms tick
static constexpr uint32_t DEBOUNCE_SAMPLES  = 100;        // record-stop debounce
static constexpr int32_t  PHASE_FRAC_BITS   = 8;          // 24.8 fixed-point
static constexpr int32_t  PHASE_ONE         = 1 << PHASE_FRAC_BITS;  // 256
static constexpr uint32_t XF_LEN            = 64;         // loop-boundary crossfade
static constexpr uint32_t XF_BRIDGE         = 32;         // post-wrap samples served from preview
static constexpr uint32_t LED_INTERVAL      = 2400;       // ~50 ms LED refresh

/*
 * cubicHermite — 4-tap Catmull-Rom interpolation.
 * Lifted from Goldfish main.cpp (by Dune Desormeaux). Coefficients carried at x2
 * scale (the acc>>1 at the end) so the 0.5 factors are avoided; clamped to int16
 * because Catmull-Rom overshoots on transients.
 */
static inline int32_t __not_in_flash_func(cubicHermite)(int32_t xm1, int32_t x0,
                                                       int32_t x1, int32_t x2,
                                                       int32_t t, int32_t F)
{
    int32_t c1 = x1 - xm1;
    int32_t c2 = 2 * xm1 - 5 * x0 + 4 * x1 - x2;
    int32_t c3 = x2 - xm1 + 3 * (x0 - x1);
    int64_t tt = t;
    int64_t acc = c3;
    acc = (acc * tt) >> F;
    acc += c2;
    acc = (acc * tt) >> F;
    acc += c1;
    acc = (acc * tt) >> F;
    int32_t res = x0 + (int32_t)(acc >> 1);
    if (res > 32767) res = 32767;
    if (res < -32768) res = -32768;
    return res;
}

/*
 * State machine: PLAY -> ARMED -> RECORD -> PLAY.
 * Z Down triggers transitions (see README "Recording Workflow").
 */
enum State { PLAY, ARMED, RECORD };

class TwoTracks : public ComputerCard
{
public:
    State state = PLAY;

    // Playback heads (filled asynchronously by core 1 from flash).
    tt_head_t headL;
    tt_head_t headR;

    // Per-head control state (core 0).
    struct HeadCtrl {
        int64_t  phase;        // 24.8 fixed-point position within [loopStart, loopEnd).
                               // 64-bit to avoid overflow on 16 MB cards where
                               // loopEnd << 8 exceeds int32_t range.
        uint32_t loopStart;    // window start (samples)
        uint32_t loopEnd;      // window end (samples)
        int32_t  speedFrac;    // 8.8 speed (256 = 1.0x)
        int16_t  last;         // hold-on-underrun sample
        // Loop-boundary crossfade state.
        int32_t  xfBridge;     // >0 = post-wrap bridge samples (served from preview)
        // Seek (pulse reset) state.
        bool     seekPending;  // request sent, waiting for seek_ready
        int32_t  seekXfCount;  // >0 = crossfading from live into seek buffer
    };
    HeadCtrl ctrl[2];

    // Cached seek buffer pointer (valid when a seek completes).
    const int16_t *seekBuf = nullptr;

    // Preview buffer cached when previews_ready (avoid re-querying each ISR).
    const int16_t *prevStart = nullptr;
    bool prevReady = false;

    uint32_t loopLength = 0;  // recorded length in samples (0 until first record)

    // Armed countdown
    uint32_t countdown = 0;
    int32_t  beepTimer = 0;
    uint32_t debounce  = 0;

    // Boot grace: suppress zDown for the first ~21ms so the knob LPF
    // (which starts from 0 and reads as Switch::Down during convergence)
    // doesn't trigger a spurious PLAY→ARMED at boot.
    uint32_t bootGrace = 1000;

    // LED sub-sampling
    uint32_t ledCounter = 0;

    // Recording GO flash timer (all LEDs on for ~200ms when RECORD starts).
    uint32_t goFlashTimer = 0;

    // Switch edge detection
    Switch lastSwitch = Switch::Up;

    // Play mode tracking (which Z position we're in within PLAY state).
    // Middle = Offset, Up = Phasing (per README controls table).
    Switch playMode = Switch::Middle;

    // Slewed knob values (>>6) to reduce audible boundary crossings.
    int32_t slewMain = 0;
    int32_t slewX = 0;
    int32_t slewY = 0;

    // ------------------------------------------------------------------
    // updatePlayMode — apply knob/CV mapping for the current Z position.
    // Called every sample in PLAY. Sets each head's loopStart/loopEnd/speedFrac.
    // ------------------------------------------------------------------
    void __not_in_flash_func(updatePlayMode)()
    {
        if (loopLength == 0) return;

        // Slew knobs (>>6 — slower than typical, per README technical note,
        // to reduce audible ADPCM boundary crossings when turning knobs).
        int32_t kMain = KnobVal(Knob::Main);
        int32_t kX    = KnobVal(Knob::X);
        int32_t kY    = KnobVal(Knob::Y);
        slewMain += (kMain - slewMain) >> 6;
        slewX    += (kX    - slewX)    >> 6;
        slewY    += (kY    - slewY)    >> 6;

        if (playMode == Switch::Middle) {
            // ---- Offset mode ----
            // Big Knob: loop window length (CCW=tiny fragment, CW=full loop).
            // ponytail: one window length for both heads — if independent window
            // lengths are needed, split into two computations.
            uint32_t windowLen = (uint32_t)((loopLength * slewMain) >> 12);
            if (windowLen < 64) windowLen = 64;
            if (windowLen > loopLength) windowLen = loopLength;

            // Knob X -> left offset, Knob Y -> right offset.
            // CV modulates multiplicatively, but only when plugged in. Unpatched
            // CV reads ~0 (bipolar), so cv+2048 = 2048 (half-scale) which would
            // halve the knob's effective range — hence the Connected() check.
            int32_t offX, offY;
            if (Connected(Input::CV1)) offX = (slewX * (CVIn1() + 2048)) >> 12;
            else                       offX = slewX;
            if (Connected(Input::CV2)) offY = (slewY * (CVIn2() + 2048)) >> 12;
            else                       offY = slewY;
            uint32_t startL = (uint32_t)((loopLength * offX) >> 12);
            uint32_t startR = (uint32_t)((loopLength * offY) >> 12);

            // Clamp window to loop: if start + window > loopLength, shift start
            // back so the full window plays (no silence gap).
            if (startL + windowLen > loopLength)
                startL = (windowLen < loopLength) ? loopLength - windowLen : 0;
            if (startR + windowLen > loopLength)
                startR = (windowLen < loopLength) ? loopLength - windowLen : 0;

            ctrl[0].loopStart = startL;
            ctrl[0].loopEnd   = startL + windowLen;
            ctrl[0].speedFrac = PHASE_ONE;
            ctrl[1].loopStart = startR;
            ctrl[1].loopEnd   = startR + windowLen;
            ctrl[1].speedFrac = PHASE_ONE;

        } else if (playMode == Switch::Up) {
            // ---- Phasing mode ----
            // Left head: full loop, 1.0x speed.
            ctrl[0].loopStart = 0;
            ctrl[0].loopEnd   = loopLength;
            ctrl[0].speedFrac = PHASE_ONE;

            // Right head: Big Knob -> loop length (CCW=short=fast phasing,
            // CW=full=no phasing). CV1 modulates multiplicatively when plugged.
            int32_t bigMod;
            if (Connected(Input::CV1)) bigMod = (slewMain * (CVIn1() + 2048)) >> 12;
            else                       bigMod = slewMain;
            if (bigMod > 4095) bigMod = 4095;
            uint32_t rLen = (uint32_t)((loopLength * bigMod) >> 12);
            if (rLen < 64) rLen = 64;
            if (rLen > loopLength) rLen = loopLength;

            // Knob X -> right window start.
            uint32_t rStart = (uint32_t)((loopLength * slewX) >> 12);
            if (rStart + rLen > loopLength) rStart = loopLength - rLen;

            ctrl[1].loopStart = rStart;
            ctrl[1].loopEnd   = rStart + rLen;

            // Knob Y -> right speed (0.9x..1.1x, centre = 1.0x).
            // CV2 modulates multiplicatively when plugged.
            int32_t yMod;
            if (Connected(Input::CV2)) yMod = (slewY * (CVIn2() + 2048)) >> 12;
            else                       yMod = slewY;
            if (yMod > 4095) yMod = 4095;
            // Map 0..4095 -> -26..+26 around 256 (1.0x) for 0.9x..1.1x range.
            int32_t speedDelta = ((yMod - 2048) * 26) >> 11;
            ctrl[1].speedFrac = PHASE_ONE + speedDelta;
        }
    }

    // ------------------------------------------------------------------
    // playHead — read one interpolated sample from a head, with loop-boundary
    // crossfade handled. Returns a 16-bit sample.
    // ------------------------------------------------------------------
    int16_t __not_in_flash_func(playHead)(uint32_t h)
    {
        HeadCtrl &c = ctrl[h];
        tt_head_t &head = (h == 0) ? headL : headR;

        if (loopLength == 0) return 0;

        // If a post-wrap bridge is active, serve straight from the loop-start
        // preview buffer while the head re-seeks to the new position.
        if (c.xfBridge > 0) {
            c.xfBridge--;
            if (prevReady && prevStart) {
                uint32_t bi = XF_BRIDGE - (uint32_t)c.xfBridge - 1u;
                if (bi < TT_PREVIEW_LEN) return prevStart[bi];
            }
            return c.last;
        }

        int32_t idx  = (int32_t)(c.phase >> PHASE_FRAC_BITS);
        int32_t frac = (int32_t)(c.phase & (PHASE_ONE - 1));

        // Wrap idx into [loopStart, loopEnd).
        uint32_t len = c.loopEnd - c.loopStart;
        if (len == 0) return 0;
        int32_t rel = idx - (int32_t)c.loopStart;
        while (rel < 0) rel += (int32_t)len;
        while (rel >= (int32_t)len) rel -= (int32_t)len;
        uint32_t s0 = c.loopStart + (uint32_t)rel;

        // Four taps for cubic Hermite (wrap within the loop window).
        uint32_t sm1 = (s0 == c.loopStart) ? c.loopEnd - 1 : s0 - 1;
        uint32_t s1  = (s0 + 1 == c.loopEnd) ? c.loopStart : s0 + 1;
        uint32_t s2  = (s0 + 2 >= c.loopEnd) ? c.loopStart + ((s0 + 2) - c.loopEnd) : s0 + 2;

        int32_t xm1 = tt_stream_head_read(&head, sm1);
        int32_t x0  = tt_stream_head_read(&head, s0);
        int32_t x1  = tt_stream_head_read(&head, s1);
        int32_t x2  = tt_stream_head_read(&head, s2);

        int32_t out = cubicHermite(xm1, x0, x1, x2, frac, PHASE_FRAC_BITS);

        // Loop-boundary crossfade: over the last XF_LEN samples BEFORE the wrap,
        // blend the live (fading out) signal with the loop-start preview (fading
        // in). This matches Goldfish main.cpp:752-759. Trigger when the read
        // position enters the final XF_LEN samples of the loop window.
        if (prevReady && prevStart && len > XF_LEN &&
            s0 >= c.loopEnd - XF_LEN) {
            uint32_t p = s0 - (c.loopEnd - XF_LEN);  // 0..XF_LEN-1
            int32_t preview = prevStart[p];
            int32_t ng = (int32_t)p + 1;
            int32_t og = (int32_t)XF_LEN - ng;
            out = (out * og + preview * ng) / (int32_t)XF_LEN;
        }

        // Advance the phase. Speed is 8.8 fixed (256 = 1.0x). Use 64-bit to avoid
        // overflow on 16 MB cards (loopEnd << 8 can exceed int32_t range).
        c.phase += c.speedFrac;
        int64_t loopEndPhase = (int64_t)c.loopEnd << PHASE_FRAC_BITS;
        if ((int64_t)c.phase >= loopEndPhase) {
            // Wrap: subtract the loop length, serve XF_BRIDGE samples from the
            // loop-start preview while the head re-seeks to the new position.
            c.phase -= (int64_t)len << PHASE_FRAC_BITS;
            c.xfBridge = XF_BRIDGE;
        }

        c.last = (int16_t)out;
        return (int16_t)out;
    }

    // ------------------------------------------------------------------
    // ProcessSample — per-sample ISR at 48 kHz
    // ------------------------------------------------------------------
    virtual void __not_in_flash_func(ProcessSample)()
    {
        // Switch transition detection (Z Down triggers state changes).
        Switch sw = SwitchVal();
        bool swChanged = (sw != lastSwitch);
        bool zDown = swChanged && sw == Switch::Down && bootGrace == 0;
        lastSwitch = sw;

        // Debounce counter for record-stop.
        if (debounce > 0) debounce--;

        // Boot grace countdown.
        if (bootGrace > 0) bootGrace--;

        int32_t outL = 0;
        int32_t outR = 0;

        switch (state)
        {
        case PLAY:
        {
            // Cache preview buffers once ready (avoid per-ISR re-query).
            if (!prevReady && tt_stream_previews_ready()) {
                prevStart = tt_stream_preview_start();
                prevReady = true;
            }

            // PLAY → ARMED on Z Down — MUST run before the loopLength==0
            // early break, otherwise you can never arm the first recording
            // from a fresh boot (boot deadlock: silent PLAY forever).
            if (zDown && debounce == 0) {
                state = ARMED;
                countdown = COUNTDOWN_SAMPLES;
                beepTimer = 0;  // first tick fires at 2s boundary
                // All 6 LEDs on for the countdown (rows turn off bottom→top).
                LedOn(0); LedOn(1); LedOn(2); LedOn(3); LedOn(4); LedOn(5);
                CVOut1(0);
                CVOut2(0);
                break;
            }

            // If no loop recorded yet, show a dim ready indicator and output
            // silence. This makes PLAY-with-no-loop visibly alive rather than
            // invisibly silent (helps confirm the firmware booted correctly).
            if (loopLength == 0) {
                LedBrightness(0, 512);  // dim: alive, waiting for first record
                break;
            }

            // Mode switching on Z change within PLAY.
            Switch swNow = SwitchVal();
            if (swNow != playMode && (swNow == Switch::Middle || swNow == Switch::Up)) {
                playMode = swNow;
                // Set the new mode's loop windows FIRST, then re-init phases
                // to the new loopStart. If phases are set before updatePlayMode,
                // they use the OLD mode's loopStart (e.g. Offset's Y-noon
                // position = loopLength/2), leaving heads out of phase.
                updatePlayMode();
                ctrl[0].phase = (int64_t)ctrl[0].loopStart << PHASE_FRAC_BITS;
                ctrl[1].phase = (int64_t)ctrl[1].loopStart << PHASE_FRAC_BITS;
                ctrl[0].xfBridge = 0;
                ctrl[1].xfBridge = 0;
                // Invalidate preview cache (loop windows may change).
                prevReady = false;
                tt_stream_request_previews(loopLength);
            } else {
                // Apply knob/CV mapping for the current mode (every sample).
                updatePlayMode();
            }

            // Pulse reset: Pulse 1 resets left head, Pulse 2 resets right head.
            // Request a seek to the loop start; crossfade from live into the
            // pre-decoded seek buffer once ready, then snap phase.
            // ponytail: single shared seek buffer — if both pulses fire within
            // the seek latency, the second overwrites the first's target. Add
            // per-head seek buffers in two_tracks_stream.c if simultaneous
            // resets are needed.
            if (PulseIn1RisingEdge()) {
                tt_stream_request_seek(ctrl[0].loopStart);
                ctrl[0].seekPending = true;
            }
            if (PulseIn2RisingEdge()) {
                tt_stream_request_seek(ctrl[1].loopStart);
                ctrl[1].seekPending = true;
            }

            // Service pending seeks: when the seek buffer is ready, arm the
            // crossfade countdown. The actual blend is applied to the head
            // outputs after playHead() returns (below).
            for (uint32_t h = 0; h < 2; h++) {
                if (ctrl[h].seekPending && tt_stream_seek_ready()) {
                    seekBuf = tt_stream_seek_buf();
                    ctrl[h].seekPending = false;
                    ctrl[h].seekXfCount = XF_LEN;
                }
            }

            int16_t sL = playHead(0);
            int16_t sR = playHead(1);

            // Apply seek crossfade to the head outputs.
            if (ctrl[0].seekXfCount > 0 && seekBuf) {
                uint32_t p = XF_LEN - (uint32_t)ctrl[0].seekXfCount;
                int32_t ng = (int32_t)p + 1;
                int32_t og = (int32_t)XF_LEN - ng;
                sL = (int16_t)((sL * og + seekBuf[p] * ng) / (int32_t)XF_LEN);
                ctrl[0].seekXfCount--;
                if (ctrl[0].seekXfCount == 0) {
                    // Snap phase to seek target and re-init the head.
                    ctrl[0].phase = (int64_t)ctrl[0].loopStart << PHASE_FRAC_BITS;
                    tt_stream_head_init(&headL, 0);
                }
            }
            if (ctrl[1].seekXfCount > 0 && seekBuf) {
                uint32_t p = XF_LEN - (uint32_t)ctrl[1].seekXfCount;
                int32_t ng = (int32_t)p + 1;
                int32_t og = (int32_t)XF_LEN - ng;
                sR = (int16_t)((sR * og + seekBuf[p] * ng) / (int32_t)XF_LEN);
                ctrl[1].seekXfCount--;
                if (ctrl[1].seekXfCount == 0) {
                    ctrl[1].phase = (int64_t)ctrl[1].loopStart << PHASE_FRAC_BITS;
                    tt_stream_head_init(&headR, 0);
                }
            }

            outL = sL;
            outR = sR;

            // LED feedback (sub-sampled ~50ms via ledCounter).
            ledCounter++;
            if (ledCounter >= LED_INTERVAL) {
                ledCounter = 0;
                // LED 0/1: output level meters.
                int32_t absL = sL < 0 ? -sL : sL;
                int32_t absR = sR < 0 ? -sR : sR;
                LedBrightness(0, (uint16_t)(absL * 2));  // scale to 0..4095
                LedBrightness(1, (uint16_t)(absR * 2));

                if (playMode == Switch::Middle) {
                    // Offset mode: LED 2 = left offset, LED 3 = right offset,
                    // LED 4/5 = position within loop window (bright=start, dark=end).
                    uint32_t startL = (loopLength > 0)
                        ? (uint32_t)(((uint64_t)ctrl[0].loopStart * 4095) / loopLength) : 0;
                    uint32_t startR = (loopLength > 0)
                        ? (uint32_t)(((uint64_t)ctrl[1].loopStart * 4095) / loopLength) : 0;
                    LedBrightness(2, (uint16_t)startL);
                    LedBrightness(3, (uint16_t)startR);
                    // Position relative to the head's window, not the full loop.
                    uint32_t winL = ctrl[0].loopEnd - ctrl[0].loopStart;
                    uint32_t winR = ctrl[1].loopEnd - ctrl[1].loopStart;
                    uint32_t relL = (uint32_t)(ctrl[0].phase >> PHASE_FRAC_BITS) - ctrl[0].loopStart;
                    uint32_t relR = (uint32_t)(ctrl[1].phase >> PHASE_FRAC_BITS) - ctrl[1].loopStart;
                    if (winL > 0) { if (relL >= winL) relL %= winL; }
                    if (winR > 0) { if (relR >= winR) relR %= winR; }
                    uint32_t posL = (winL > 0) ? (relL * 4095) / winL : 0;
                    uint32_t posR = (winR > 0) ? (relR * 4095) / winR : 0;
                    LedBrightness(4, (uint16_t)(4095 - posL));
                    LedBrightness(5, (uint16_t)(4095 - posR));
                } else {
                    // Phasing mode: LED 2 = right window start (X),
                    // LED 3 = right speed deviation, LED 4/5 = position within window.
                    uint32_t rStart = (loopLength > 0)
                        ? (uint32_t)(((uint64_t)ctrl[1].loopStart * 4095) / loopLength) : 0;
                    LedBrightness(2, (uint16_t)rStart);
                    int32_t spdDev = (ctrl[1].speedFrac - PHASE_ONE);
                    int32_t spdAbs = spdDev < 0 ? -spdDev : spdDev;
                    LedBrightness(3, (uint16_t)(spdAbs * 16));
                    uint32_t winL = ctrl[0].loopEnd - ctrl[0].loopStart;
                    uint32_t winR = ctrl[1].loopEnd - ctrl[1].loopStart;
                    uint32_t relL = (uint32_t)(ctrl[0].phase >> PHASE_FRAC_BITS) - ctrl[0].loopStart;
                    uint32_t relR = (uint32_t)(ctrl[1].phase >> PHASE_FRAC_BITS) - ctrl[1].loopStart;
                    if (winL > 0) { if (relL >= winL) relL %= winL; }
                    if (winR > 0) { if (relR >= winR) relR %= winR; }
                    uint32_t posL = (winL > 0) ? (relL * 4095) / winL : 0;
                    uint32_t posR = (winR > 0) ? (relR * 4095) / winR : 0;
                    LedBrightness(4, (uint16_t)(4095 - posL));
                    LedBrightness(5, (uint16_t)(4095 - posR));
                }
            }

            // CV outputs: phase position within each head's loop window.
            // Maps 0..winLen to -2048..+2047 so window start ≈ -6V, window end ≈ +6V.
            if (loopLength > 0) {
                uint32_t winL = ctrl[0].loopEnd - ctrl[0].loopStart;
                uint32_t winR = ctrl[1].loopEnd - ctrl[1].loopStart;
                uint32_t relL = (uint32_t)(ctrl[0].phase >> PHASE_FRAC_BITS) - ctrl[0].loopStart;
                uint32_t relR = (uint32_t)(ctrl[1].phase >> PHASE_FRAC_BITS) - ctrl[1].loopStart;
                if (winL > 0) { if (relL >= winL) relL %= winL; }
                if (winR > 0) { if (relR >= winR) relR %= winR; }
                int32_t posL = (winL > 0) ? (int32_t)((relL * 4095) / winL) - 2048 : -2048;
                int32_t posR = (winR > 0) ? (int32_t)((relR * 4095) / winR) - 2048 : -2048;
                if (posL > 2047) posL = 2047;
                if (posL < -2048) posL = -2048;
                if (posR > 2047) posR = 2047;
                if (posR < -2048) posR = -2048;
                CVOut1((int16_t)posL);
                CVOut2((int16_t)posR);
            }
            break;
        }

        case ARMED:
        {
            // Monitor passthrough during countdown — boost <<4 to match the
            // 16-bit level that ADPCM encodes and playback decodes.
            int32_t in = AudioIn1();
            outL = in << 4;
            outR = in << 4;

            // Z Up cancels ARMED → PLAY (re-init at loop start). Z Down restarts
            // the countdown (handled below). Z Middle does nothing.
            if (swChanged && sw == Switch::Up) {
                state = PLAY;
                debounce = DEBOUNCE_SAMPLES;
                if (loopLength > 0) {
                    // Re-init heads at loop start, landing in the mode matching
                    // the current Z position (Up = Phasing).
                    tt_stream_head_init(&headL, 0);
                    tt_stream_head_init(&headR, 0);
                    playMode = sw;
                    updatePlayMode();
                    ctrl[0].phase = (int64_t)ctrl[0].loopStart << PHASE_FRAC_BITS;
                    ctrl[1].phase = (int64_t)ctrl[1].loopStart << PHASE_FRAC_BITS;
                    ctrl[0].xfBridge = 0;
                    ctrl[1].xfBridge = 0;
                    ctrl[0].seekPending = false;
                    ctrl[1].seekPending = false;
                    ctrl[0].seekXfCount = 0;
                    ctrl[1].seekXfCount = 0;
                    prevReady = false;
                    tt_stream_request_previews(loopLength);
                }
                LedOff(0); LedOff(1); LedOff(2); LedOff(3); LedOff(4); LedOff(5);
                break;
            }

            // Re-pressing Z Down during countdown restarts it.
            if (zDown) {
                countdown = COUNTDOWN_SAMPLES;
                beepTimer = BEEP_SAMPLES;
                // Re-light all 6 LEDs for the countdown.
                LedOn(0); LedOn(1); LedOn(2); LedOn(3); LedOn(4); LedOn(5);
            }

            // Countdown ticks: audible 10ms beep at ~3s, ~2s, and ~0s remaining.
            // The first two fire on 1s boundary crossings; the third fires at
            // countdown expiry (the start of recording). At each tick, turn off
            // the next row of LEDs (bottom → middle → top), so the visual
            // countdown shrinks from 6 LEDs to 0.
            uint32_t prevCountdown = countdown;
            if (countdown > 0) countdown--;
            uint32_t curBoundary = countdown / SAMPLE_RATE;
            uint32_t prevBoundary = prevCountdown / SAMPLE_RATE;
            if (countdown > 0 && curBoundary != prevBoundary) {
                beepTimer = BEEP_SAMPLES;
                // curBoundary = seconds-remaining floor after this tick (2, 1, 0).
                // Turn off rows: tick 1 (curBoundary=2) → bottom (4,5),
                //                tick 2 (curBoundary=1) → middle (2,3).
                if (curBoundary == 2) { LedOff(4); LedOff(5); }
                else if (curBoundary == 1) { LedOff(2); LedOff(3); }
            }

            // Beep: a short square blip on the left output.
            if (beepTimer > 0) {
                // Square wave blip at ~2kHz (24 samples period at 48kHz).
                // Alternate polarity every 12 samples for a click-like tone.
                int32_t beep = ((beepTimer / 12) & 1) ? 1024 : -1024;
                outL = beep;
                outR = beep;
                beepTimer--;
            }

            // Countdown expiry → RECORD.
            if (countdown == 0 && prevCountdown > 0) {
                // Final tick (no boundary-cross beep since countdown hits 0).
                beepTimer = BEEP_SAMPLES;
                // Turn off the top row — all LEDs dark.
                LedOff(0); LedOff(1);
                state = RECORD;
                tt_stream_record_start();
                // "GO" signal: flash all 6 LEDs for ~200ms.
                goFlashTimer = SAMPLE_RATE / 5;  // 200ms
                LedOn(0); LedOn(1); LedOn(2); LedOn(3); LedOn(4); LedOn(5);
            }
            break;
        }

        case RECORD:
        {
            // Record the mono input: scale 12-bit to 16-bit for ADPCM quality
            // (per MLRws mlr.c:1006).
            int32_t in = AudioIn1();
            int16_t sample16 = (int16_t)(in << 4);
            tt_stream_record_sample(sample16);

            // Monitor passthrough: boost <<4 to match playback level.
            outL = in << 4;
            outR = in << 4;

            // GO flash: all 6 LEDs on for ~200ms when RECORD starts.
            if (goFlashTimer > 0) {
                goFlashTimer--;
                // LEDs already on from ARMED expiry; just keep them.
                if (goFlashTimer == 0) {
                    LedOff(0); LedOff(1); LedOff(2);
                    LedOff(3); LedOff(4); LedOff(5);
                }
            } else {
                // Audio level meter: 3-tier smooth VU across all 6 LEDs.
                //   bottom (4,5): responds to any signal
                //   middle (2,3): lights at ~1/6 of full range
                //   top (0,1):    lights at ~1/2 of full range
                // Sub-sampled at LED_INTERVAL to reduce flicker.
                ledCounter++;
                if (ledCounter >= LED_INTERVAL) {
                    ledCounter = 0;
                    int32_t absIn = in < 0 ? -in : in;  // 0..2048
                    // Bottom row: always some response to signal.
                    uint16_t bot = (uint16_t)((absIn * 4) > 4095 ? 4095 : absIn * 4);
                    // Middle row: threshold ~341 (1/6 of 2048).
                    uint16_t mid = (absIn > 341)
                        ? (uint16_t)(((absIn - 341) * 4) > 4095 ? 4095 : (absIn - 341) * 4)
                        : 0;
                    // Top row: threshold ~1024 (1/2 of 2048).
                    uint16_t top = (absIn > 1024)
                        ? (uint16_t)(((absIn - 1024) * 4) > 4095 ? 4095 : (absIn - 1024) * 4)
                        : 0;
                    LedBrightness(0, top);
                    LedBrightness(1, top);
                    LedBrightness(2, mid);
                    LedBrightness(3, mid);
                    LedBrightness(4, bot);
                    LedBrightness(5, bot);
                }
            }

            // CV outputs during RECORD: fill position + recording length so far.
            uint32_t written = tt_stream_write_index();
            uint32_t cap = tt_stream_capacity_samples();
            uint16_t fillBright = (cap > 0) ? (uint16_t)(((uint64_t)written * 4095) / cap) : 0;
            CVOut1((int16_t)((fillBright >> 1) - 2048));
            uint32_t recLen = tt_stream_write_index();
            int32_t recCv = (cap > 0)
                ? (int32_t)(((uint64_t)recLen * 4095) / cap) - 2048 : -2048;
            if (recCv > 2047) recCv = 2047;
            if (recCv < -2048) recCv = -2048;
            CVOut2((int16_t)recCv);

            // Stop on Z Down (with debounce) or when buffer full.
            bool bufferFull = (tt_stream_write_index() >= tt_stream_capacity_samples());
            if ((zDown && debounce == 0) || bufferFull) {
                tt_stream_record_stop();
                loopLength = tt_stream_recorded_samples();
                // Enter PLAY: request previews for the new loop, init heads.
                tt_stream_request_previews(loopLength);
                tt_stream_head_init(&headL, 0);
                tt_stream_head_init(&headR, 0);
                ctrl[0].phase = 0;
                ctrl[1].phase = 0;
                ctrl[0].loopStart = 0;
                ctrl[0].loopEnd = loopLength;
                ctrl[1].loopStart = 0;
                ctrl[1].loopEnd = loopLength;
                ctrl[0].speedFrac = PHASE_ONE;
                ctrl[1].speedFrac = PHASE_ONE;
                ctrl[0].seekPending = false;
                ctrl[1].seekPending = false;
                ctrl[0].seekXfCount = 0;
                ctrl[1].seekXfCount = 0;
                state = PLAY;
                // Consume the Z Down edge so the PLAY→ARMED transition doesn't
                // fire on the same sample (would re-arm recording instantly).
                zDown = false;
                // Set play mode to current Z position (Middle=Offset, Up=Phasing).
                playMode = (sw == Switch::Up) ? Switch::Up : Switch::Middle;
                debounce = DEBOUNCE_SAMPLES;
                // Clear record LEDs.
                LedOff(0); LedOff(1); LedOff(2); LedOff(3); LedOff(4); LedOff(5);
            }
            break;
        }
        }

        // Clip outputs to 12-bit signed range and send.
        if (outL > 2047)  outL = 2047;
        if (outL < -2048) outL = -2048;
        if (outR > 2047)  outR = 2047;
        if (outR < -2048) outR = -2048;
        AudioOut1((int16_t)outL);
        AudioOut2((int16_t)outR);
    }
};

static TwoTracks tt;
static tt_head_t *s_headL = nullptr;
static tt_head_t *s_headR = nullptr;

// Core 1 entry: spin the flash I/O task forever (fills head rings, flushes
// recorded pages, services previews/seeks). Mirrors Goldfish.
static void __not_in_flash_func(core1_entry)()
{
    while (true) {
        tt_stream_io_task();
    }
}

int main()
{
    // Overclock to 192 MHz (multiple of 48 kHz audio clock). Voltage must be
    // raised before the switch for stability, matching MLRws/Goldfish.
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(192000, true);

    // Boot signal: sweep all 6 LEDs briefly so we know the firmware started.
    // Visible even if init hangs later (it shouldn't, but this confirms boot).
    // LedOn/LedOff are protected, so drive the LED PWM GPIOs directly (10-15).
    // The ComputerCard constructor (run before main) already initialised PWM.
    for (int i = 0; i < 6; i++) pwm_set_gpio_level(10 + i, 65535);
    sleep_ms(200);
    for (int i = 0; i < 6; i++) pwm_set_gpio_level(10 + i, 0);
    sleep_ms(100);

    tt.EnableNormalisationProbe();

    // Flash layout compute — single-core, before core 1 is launched.
    // (No JEDEC probe: uses PICO_FLASH_SIZE_BYTES, set via CMake.)
    tt_stream_init();

    // If a recording was persisted from a previous boot, load it into the
    // playback state so the card boots straight into PLAY (not ARMED).
    // switchVal isn't valid yet (ISR hasn't started), so default playMode to
    // Middle (Offset); the first ProcessSample will switch to Up (Phasing)
    // if the Z is actually in the Up position.
    {
        uint32_t rec = tt_stream_recorded_samples();
        if (rec > 0) {
            tt.loopLength = rec;
            for (int i = 0; i < 2; i++) {
                tt.ctrl[i].loopStart  = 0;
                tt.ctrl[i].loopEnd    = rec;
                tt.ctrl[i].speedFrac  = PHASE_ONE;
                tt.ctrl[i].phase      = 0;
                tt.ctrl[i].xfBridge   = 0;
                tt.ctrl[i].seekPending = false;
                tt.ctrl[i].seekXfCount = 0;
            }
            tt.playMode = ComputerCard::Switch::Middle;
            // Request previews before core 1 launches; core 1 picks this up
            // on its first io_task iteration.
            tt_stream_request_previews(rec);
        }
    }

    // Register the heads so core 1 knows which rings to refill.
    s_headL = &tt.headL;
    s_headR = &tt.headR;
    tt_stream_head_init(s_headL, 0);
    tt_stream_head_init(s_headR, 0);
    tt_stream_set_heads(s_headL, s_headR);

    multicore_launch_core1(core1_entry);

    tt.Run();
    return 0;
}
