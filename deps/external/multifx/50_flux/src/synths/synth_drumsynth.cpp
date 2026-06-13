#include "SynthEngine.h"
#include "SynthCore.h"

// ============================================================================
// DRUM SYNTH — integer-only DSP (float only when converting UI params at hit)
//
// MIDI mapping (note % 12):
//   0  (36 C1): Bass  — Main knob (envelope)
//   2  (38 D1): Snare — X knob (pitch)
//   6  (42 F#1): Closed hat — Y knob (timbre)
//  10  (46 A#1): Open hat   — Y knob (timbre)
//
// Bass: sine body + band-limited click (separate short envelope), exp pitch
//       sweep, exp amp. Body boosted vs old; click heavily attenuated + LP.
// Snare: dual-tone sine partials + shaped noise, exp envelopes, X = tone/noise.
// Hats: 6× square FM-ish partials + HP noise, scaled sums, Y = metal/noise.
// ============================================================================

#define DR_FS AUDIO_BASE_RATE

// 256-point full-cycle sine Q15; index = phase >> 24, frac = (phase>>16)&0xFF
static const int16_t drumSin256[256] = {
    0, 804, 1608, 2410, 3212, 4011, 4808, 5602, 6393, 7179, 7962, 8739, 9512, 10278, 11039, 11793,
    12539, 13279, 14010, 14732, 15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403, 22005, 22594,
    23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790, 27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956,
    30273, 30571, 30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521, 32609, 32678, 32728, 32757,
    32767, 32757, 32728, 32678, 32609, 32521, 32412, 32285, 32137, 31971, 31785, 31580, 31356, 31113, 30852, 30571,
    30273, 29956, 29621, 29268, 28898, 28510, 28105, 27683, 27245, 26790, 26319, 25832, 25329, 24811, 24279, 23731,
    23170, 22594, 22005, 21403, 20787, 20159, 19519, 18868, 18204, 17530, 16846, 16151, 15446, 14732, 14010, 13279,
    12539, 11793, 11039, 10278, 9512, 8739, 7962, 7179, 6393, 5602, 4808, 4011, 3212, 2410, 1608, 804,
    0, -804, -1608, -2410, -3212, -4011, -4808, -5602, -6393, -7179, -7962, -8739, -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530, -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790, -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971, -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285, -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683, -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868, -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278, -9512, -8739, -7962, -7179, -6393, -5602, -4808, -4011, -3212, -2410, -1608, -804,
};

static inline int16_t drum_sin_u32(uint32_t ph) {
  uint32_t idx = ph >> 24;
  uint32_t frac = (ph >> 16) & 0xFFu;
  int32_t a = drumSin256[idx];
  int32_t b = drumSin256[(idx + 1u) & 0xFFu];
  return (int16_t)((a * (256 - (int32_t)frac) + b * (int32_t)frac) >> 8);
}

static inline uint32_t hz_to_phase_inc(uint32_t hz) {
  return (uint32_t)(((uint64_t)hz * 4294967296ULL) / (uint64_t)DR_FS);
}

// Q15 in 0..1 from float in [0,1]
static inline int32_t q15_from_unit(float x) {
  if (x <= 0.f)
    return 0;
  if (x >= 1.f)
    return 32767;
  return (int32_t)(x * 32768.0f);
}

static struct {
  uint32_t phase;
  uint32_t phaseInc;
  uint32_t sweepRate;
  int32_t sweepTarget;
  int32_t envVal;
  int32_t releaseRate;
  uint32_t metalPhases[6];
  uint32_t metalIncs[6];
  uint32_t noiseState;
  int32_t lastNoise;
  int32_t lastNoise2;
  uint16_t clickTimer;

  // Kick: separate click envelope + smoothed click source
  int32_t clickEnv;   // Q30
  int32_t clickLp;    // 1-pole LP for click noise (soft attack)
  int32_t clickDecay; // Q30 multiply per sample for clickEnv

  // Snare: second tone + noise envs
  uint32_t phase2;
  uint32_t phaseInc2;
  int32_t toneEnv;   // Q30
  int32_t toneDecay; // Q30 mult per sample
  int32_t noiseEnv;  // Q30
  int32_t noiseDec;  // Q30 mult per sample
  int32_t noiseLp;   // snare noise HP state
  int32_t mixToneQ15; // tone vs noise from X knob
} drState[MAX_VOICES];

static const uint16_t metalFreqs[6] = {205, 369, 522, 540, 812, 1165};

static void drumsynth_reset(int voiceIdx, EnginePool* /*pool*/) {
  drState[voiceIdx].phase = 0;
  drState[voiceIdx].phase2 = 0;
  drState[voiceIdx].envVal = 0;
  drState[voiceIdx].clickTimer = 0;
  drState[voiceIdx].clickEnv = 0;
  drState[voiceIdx].clickLp = 0;
  drState[voiceIdx].noiseState = (uint32_t)(voiceIdx * 0x1234567u + 1u);
  drState[voiceIdx].lastNoise = 0;
  drState[voiceIdx].lastNoise2 = 0;
  drState[voiceIdx].noiseLp = 0;
  for (int i = 0; i < 6; i++)
    drState[voiceIdx].metalPhases[i] = 0;
}

static void drumsynth_initVoice(int voiceIdx, uint8_t /*note*/, uint8_t /*velocity*/) {}

static void drumsynth_renderBlockSetup(Voice* v, const SynthParams* p, int voiceIdx,
                                       int /*count*/, EnginePool* /*pool*/, bool gateRise) {
  if (!gateRise)
    return;

  uint8_t nc = v->note % 12u;

  drState[voiceIdx].phase = 0;
  drState[voiceIdx].phase2 = 0;
  drState[voiceIdx].envVal = 1 << 30;
  drState[voiceIdx].noiseState = drState[voiceIdx].noiseState * 1664525u + 1013904223u;
  drState[voiceIdx].lastNoise = 0;
  drState[voiceIdx].lastNoise2 = 0;
  drState[voiceIdx].noiseLp = 0;
  drState[voiceIdx].clickLp = 0;

  int32_t mQ15 = q15_from_unit(p->envelope);
  int32_t xQ15 = q15_from_unit(p->pitch);
  int32_t yQ15 = q15_from_unit(p->timbre);

  if (nc == 0) {
    // ---- Kick: Main = mostly decay length. Pitch/sweep use mPitch so the left side
    // still sounds like the "right third" (sub + slow 808 glide), not a thin hit; only
    // tail length follows the raw knob (short left, boomy right).
    uint32_t mPitch = 21845u + (uint32_t)(((uint64_t)mQ15 * 10922u) >> 15); // ~2/3..1.0 of range
    if (mPitch > 32767u)
      mPitch = 32767u;

    uint32_t f0 = 118u + (uint32_t)((mPitch * 48) >> 15);
    uint32_t f1 = 36u + (uint32_t)((mPitch * 22) >> 15) + ((mPitch * 22) >> 16);
    drState[voiceIdx].phaseInc = hz_to_phase_inc(f0);
    drState[voiceIdx].sweepTarget = (int32_t)hz_to_phase_inc(f1);

    uint32_t sr = (uint32_t)(22u + (((uint32_t)(32767 - mPitch) * 340u) >> 15));
    if (sr > 65535u)
      sr = 65535u;
    drState[voiceIdx].sweepRate = sr;

    // Amp decay: raw m only — short (~45–90 ms) at left, long at right
    uint32_t tauSamp = (uint32_t)DR_FS * 48u / 1000u +
              (uint32_t)(((uint64_t)mQ15 * (uint64_t)DR_FS * 74u) >> 15) / 100u;
    uint32_t tauMin = (uint32_t)DR_FS / 32u +
                      (uint32_t)(((uint64_t)mQ15 * (uint64_t)DR_FS) >> 20);
    if (tauSamp < tauMin)
      tauSamp = tauMin;
    drState[voiceIdx].releaseRate = (int32_t)((1LL << 30) / (int64_t)tauSamp);

    // Click: ~1.2 ms active window, fast decay, low level
    drState[voiceIdx].clickTimer = (uint16_t)((DR_FS * 12) / 10000);
    if (drState[voiceIdx].clickTimer < 8)
      drState[voiceIdx].clickTimer = 8;
    drState[voiceIdx].clickEnv = 1 << 27;
    drState[voiceIdx].clickDecay = (int32_t)(3221225472LL / (int64_t)DR_FS);

  } else if (nc == 2) {
    // ---- Snare: X = tone pitch / snappiness (maps to tone vs noise) ----
    uint32_t f1 = 185u + (uint32_t)((xQ15 * 120) >> 15);
    uint32_t f2 = f1 + 140u + (uint32_t)((xQ15 * 80) >> 15);
    drState[voiceIdx].phaseInc = hz_to_phase_inc(f1);
    drState[voiceIdx].phaseInc2 = hz_to_phase_inc(f2);
    drState[voiceIdx].phase2 = 0;
    drState[voiceIdx].mixToneQ15 = 8000 + ((xQ15 * 20000) >> 15);
    if (drState[voiceIdx].mixToneQ15 > 30000)
      drState[voiceIdx].mixToneQ15 = 30000;

    drState[voiceIdx].toneEnv = 1 << 30;
    drState[voiceIdx].noiseEnv = 1 << 30;
    // Exponential decay: env *= (1 - 1/tau) per sample (Q30 multiplier)
    uint32_t tauTone = (uint32_t)DR_FS / 28u + (uint32_t)((xQ15 * (uint32_t)DR_FS) >> 17);
    if (tauTone < 180u)
      tauTone = 180u;
    uint32_t tauNoise = tauTone + (uint32_t)DR_FS / 120u + (uint32_t)((xQ15 * (uint32_t)DR_FS) >> 18);
    drState[voiceIdx].toneDecay = (int32_t)((1LL << 30) - ((1LL << 30) / (int64_t)tauTone));
    drState[voiceIdx].noiseDec = (int32_t)((1LL << 30) - ((1LL << 30) / (int64_t)tauNoise));
    drState[voiceIdx].releaseRate = 0;

  } else if (nc == 6 || nc == 10) {
    for (int i = 0; i < 6; i++) {
      drState[voiceIdx].metalPhases[i] = 0;
      uint32_t det = 32768u + (uint32_t)((yQ15 * 3277) >> 15);
      drState[voiceIdx].metalIncs[i] =
          (uint32_t)(((uint64_t)metalFreqs[i] * det * 4294967296ULL) >> 15) /
          (uint64_t)DR_FS;
    }
    if (nc == 6) {
      uint32_t tauSamp = (uint32_t)DR_FS / 35u + (uint32_t)((yQ15 * (uint32_t)DR_FS) >> 18);
      drState[voiceIdx].releaseRate = (int32_t)((1LL << 30) / (int64_t)tauSamp);
    } else {
      uint32_t tauSamp = (uint32_t)DR_FS / 8u + (uint32_t)((yQ15 * (uint32_t)DR_FS) >> 16);
      drState[voiceIdx].releaseRate = (int32_t)((1LL << 30) / (int64_t)tauSamp);
    }
  }
}

static int32_t render_hat(int voiceIdx, int32_t yQ15) {
  int32_t sum = 0;
  for (int i = 0; i < 6; i++) {
    drState[voiceIdx].metalPhases[i] += drState[voiceIdx].metalIncs[i];
    int32_t sq = (drState[voiceIdx].metalPhases[i] < 0x80000000u) ? 5200 : -5200;
    sum += sq;
  }
  sum = (sum * 3) >> 4;

  drState[voiceIdx].noiseState = drState[voiceIdx].noiseState * 1664525u + 1013904223u;
  int32_t noise = (int32_t)(drState[voiceIdx].noiseState >> 16) - 32768;

  int32_t hp1 = noise - drState[voiceIdx].lastNoise;
  drState[voiceIdx].lastNoise = noise;
  int32_t hp2 = hp1 - drState[voiceIdx].lastNoise2;
  drState[voiceIdx].lastNoise2 = hp1;

  int32_t metal = (sum * (32768 - yQ15)) >> 15;
  int32_t nmix = (hp2 * yQ15) >> 15;
  int32_t out = metal + ((nmix * 3) >> 2);
  if (out > 28000)
    out = 28000 + ((out - 28000) >> 2);
  if (out < -28000)
    out = -28000 + ((out + 28000) >> 2);
  return out;
}

static int32_t drumsynth_renderSample(Voice* v, const SynthParams* p, int voiceIdx,
                                      EnginePool* /*pool*/, bool /*gateRise*/, bool /*activeGate*/) {
  if (drState[voiceIdx].envVal <= 0) {
    v->active = false;
    return 0;
  }

  int32_t val = 0;
  uint8_t nc = v->note % 12u;

  if (nc == 0) {
    int32_t curr = (int32_t)drState[voiceIdx].phaseInc;
    int32_t tgt = drState[voiceIdx].sweepTarget;
    curr += ((tgt - curr) * (int32_t)drState[voiceIdx].sweepRate) >> 16;
    drState[voiceIdx].phaseInc = (uint32_t)curr;
    drState[voiceIdx].phase += drState[voiceIdx].phaseInc;

    int32_t body = drum_sin_u32(drState[voiceIdx].phase);
    body = (body * 24000) >> 15;

    if (body > 12000)
      body = 12000 + ((body - 12000) >> 1);
    else if (body < -12000)
      body = -12000 + ((body + 12000) >> 1);

    drState[voiceIdx].noiseState = drState[voiceIdx].noiseState * 1664525u + 1013904223u;
    int32_t nraw = (int32_t)(drState[voiceIdx].noiseState >> 16) - 32768;
    drState[voiceIdx].clickLp += ((nraw - drState[voiceIdx].clickLp) * 12000) >> 15;
    int32_t clickSamp = (drState[voiceIdx].clickLp * 3) >> 2;

    int32_t clickMix = 0;
    if (drState[voiceIdx].clickTimer > 0) {
      clickMix = (int32_t)(((int64_t)clickSamp * drState[voiceIdx].clickEnv) >> 30);
      drState[voiceIdx].clickEnv =
          (int32_t)(((int64_t)drState[voiceIdx].clickEnv * drState[voiceIdx].clickDecay) >> 30);
      drState[voiceIdx].clickTimer--;
    }

    val = body + (clickMix >> 2);

    drState[voiceIdx].envVal -= (int32_t)(((int64_t)drState[voiceIdx].envVal *
                                           drState[voiceIdx].releaseRate) >>
                                          30);
    if (drState[voiceIdx].envVal < 0)
      drState[voiceIdx].envVal = 0;

  } else if (nc == 2) {
    drState[voiceIdx].phase += drState[voiceIdx].phaseInc;
    drState[voiceIdx].phase2 += drState[voiceIdx].phaseInc2;

    int32_t t1 = drum_sin_u32(drState[voiceIdx].phase);
    int32_t t2 = drum_sin_u32(drState[voiceIdx].phase2);
    int32_t tone = ((t1 * 10) >> 4) + ((t2 * 6) >> 4);
    tone = (tone * (drState[voiceIdx].toneEnv >> 16)) >> 14;
    drState[voiceIdx].toneEnv =
        (int32_t)(((int64_t)drState[voiceIdx].toneEnv * drState[voiceIdx].toneDecay) >> 30);

    drState[voiceIdx].noiseState = drState[voiceIdx].noiseState * 1664525u + 1013904223u;
    int32_t nraw = (int32_t)(drState[voiceIdx].noiseState >> 16) - 32768;
    drState[voiceIdx].noiseLp += ((nraw - drState[voiceIdx].noiseLp) * 20000) >> 15;
    int32_t ns = nraw - drState[voiceIdx].noiseLp;
    ns = (ns * (drState[voiceIdx].noiseEnv >> 16)) >> 14;
    drState[voiceIdx].noiseEnv =
        (int32_t)(((int64_t)drState[voiceIdx].noiseEnv * drState[voiceIdx].noiseDec) >> 30);

    int32_t mt = drState[voiceIdx].mixToneQ15;
    val = ((tone * mt) * 2 + (ns * (32768 - mt))) >> 16;
    if (drState[voiceIdx].toneEnv < (1 << 22) && drState[voiceIdx].noiseEnv < (1 << 22))
      drState[voiceIdx].envVal = 0;

  } else if (nc == 6 || nc == 10) {
    int32_t yQ = q15_from_unit(p->timbre);
    val = render_hat(voiceIdx, yQ);
    drState[voiceIdx].envVal -= (int32_t)(((int64_t)drState[voiceIdx].envVal *
                                           drState[voiceIdx].releaseRate) >>
                                          30);
    if (drState[voiceIdx].envVal < 0)
      drState[voiceIdx].envVal = 0;
  }

  if (nc != 2)
    val = (val * (drState[voiceIdx].envVal >> 16)) >> 14;
  return val;
}

const SynthEngine engineDrumSynth = {
    .renderSample = drumsynth_renderSample,
    .renderBlockSetup = drumsynth_renderBlockSetup,
    .reset = drumsynth_reset,
    .initVoice = drumsynth_initVoice,
    .renderBlock = nullptr,
    .outputGainQ8 = 288,
    .name = "DrumSynth",
};
