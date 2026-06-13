// ============================================================================
// Synth Engine: STRINGS (Karplus-Strong Physical Modeling)
// Pluck + Bow modes, uses EnginePool KS delay lines
// ============================================================================
#include "SynthEngine.h"

static int32_t strings_renderSample(Voice *v, const SynthParams *p,
                                    int voiceIdx, EnginePool *pool,
                                    bool gateRise, bool activeGate) {
  if (voiceIdx >= 4)
    return 0; // Only 4 KS slots in pool

  // Frequency → delay line length
  int32_t len = 0;
  if (v->phaseInc > 0)
    len = (uint32_t)(4294967295U / v->phaseInc);
  else
    len = 760;
  if (len > 760)
    len = 760;

  // Timbre as Q15 (0..32768)
  int32_t timbreQ15 = (int32_t)(p->timbre * 32768.0f);
  if (timbreQ15 < 0)
    timbreQ15 = 0;
  if (timbreQ15 > 32768)
    timbreQ15 = 32768;

  // Pluck on gate rise
  if (gateRise) {
    // Timbre > 0.5 reduces pluck amplitude (bowing mode)
    int32_t pluckAmplitude = 32768;
    if (timbreQ15 > 16384) {
      // pluckScale = 1.0 - ((timbre - 0.5) * 2.0)
      // In Q15: 32768 - ((timbreQ15 - 16384) * 2)
      pluckAmplitude = 32768 - ((timbreQ15 - 16384) << 1);
      if (pluckAmplitude < 0)
        pluckAmplitude = 0;
    }
    for (int i = 0; i < len; i++) {
      v->noiseState = v->noiseState * 1664525 + 1013904223;
      int32_t noise = (int32_t)(v->noiseState >> 16);
      pool->eng.ks.delayLine[voiceIdx][i] =
          (int16_t)((noise * pluckAmplitude) >> 15);
    }
    pool->eng.ks.ptr[voiceIdx] = 0;
  }

  // Read current and next sample
  int16_t s1 = pool->eng.ks.delayLine[voiceIdx][pool->eng.ks.ptr[voiceIdx]];
  int nextPtr = pool->eng.ks.ptr[voiceIdx] + 1;
  if (nextPtr >= len)
    nextPtr = 0;
  int16_t s2 = pool->eng.ks.delayLine[voiceIdx][nextPtr];

  int32_t filtered = (s1 + s2) >> 1; // Basic KS averaging filter

  // Stiffness (Y Knob 0-50%): Extra LPF damping
  if (timbreQ15 < 16384) {
    // stiffness = 1.0 - (timbre * 2.0)  → Q15: 32768 - (timbreQ15 * 2)
    int32_t stiffQ15 = 32768 - (timbreQ15 << 1);
    // blend = stiffness * 16384 → stiffQ15 >> 1
    int32_t blend = stiffQ15 >> 1;
    int32_t prev = pool->eng.ks.delayLine[voiceIdx][pool->eng.ks.ptr[voiceIdx]];
    filtered = filtered + (((prev - filtered) * blend) >> 14);
  }

  // Feedback (Decay) — sqrt curve for longer sustain earlier on knob
  // envQ15 = envelope * 32768
  int32_t envQ15 = (int32_t)(p->envelope * 32768.0f);
  if (envQ15 < 0)
    envQ15 = 0;
  if (envQ15 > 32768)
    envQ15 = 32768;
  int32_t sqrtEnvQ15 = fast_isqrt_q15(envQ15);
  // feedback = 0.96 + sqrt(env) * 0.038
  // Q15: fbQ15 = 31457 + (sqrtEnvQ15 * 1245) >> 15
  // 0.96 * 32768 = 31457, 0.038 * 32768 = 1245
  int32_t fbQ15 = 31457 + ((sqrtEnvQ15 * 1245) >> 15);
  if (fbQ15 > 32700)
    fbQ15 = 32700; // safety clamp
  filtered = (filtered * fbQ15) >> 15;

  // Bowing (Y > 50%): continuous noise excitation while gated
  if (v->gate && timbreQ15 > 16384) {
    v->noiseState = v->noiseState * 1664525 + 1013904223;
    int32_t noise = (int16_t)(v->noiseState >> 16);
    // bowNorm = (timbre - 0.5) * 2.0 → Q15: (timbreQ15 - 16384) * 2
    int32_t bowQ15 = (timbreQ15 - 16384) << 1;
    if (bowQ15 > 32768)
      bowQ15 = 32768;
    // bowAmount = bowNorm * 24000 → (bowQ15 * 24000) >> 15
    int32_t bowAmount = (bowQ15 * AUDIO_BASE_RATE) >> 15;
    filtered += (noise * bowAmount) >> 15;
  }

  // Hard clip
  if (filtered > 32000)
    filtered = 32000;
  if (filtered < -32000)
    filtered = -32000;

  // Write back to delay line
  pool->eng.ks.delayLine[voiceIdx][pool->eng.ks.ptr[voiceIdx]] = filtered;
  pool->eng.ks.ptr[voiceIdx]++;
  if (pool->eng.ks.ptr[voiceIdx] >= len)
    pool->eng.ks.ptr[voiceIdx] = 0;

  return filtered;
}

static void strings_reset(int voiceIdx, EnginePool *pool) {
  if (voiceIdx >= 0 && voiceIdx < 4) {
    for (int i = 0; i < 760; i++)
      pool->eng.ks.delayLine[voiceIdx][i] = 0;
    pool->eng.ks.ptr[voiceIdx] = 0;
  }
}

const SynthEngine engineStrings = {.renderSample = strings_renderSample,
                                   .renderBlockSetup = nullptr,
                                   .reset = strings_reset,
                                   .outputGainQ8 =
                                       180, // -3dB (strings are naturally loud)
                                   .name = "Strings"};
