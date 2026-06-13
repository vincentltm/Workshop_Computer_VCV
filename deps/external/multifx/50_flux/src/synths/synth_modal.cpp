// ============================================================================
// Synth Engine: MODAL RESONATOR (Bells / Metallic Percussion)
// Noise impulse → two parallel SVF bandpass filters
// ============================================================================
#include "SynthEngine.h"

static int32_t modal_renderSample(Voice *v, const SynthParams *p, int voiceIdx,
                                  EnginePool *pool, bool gateRise,
                                  bool activeGate) {
  // Impulse on gate rise
  if (gateRise) {
    modalImpulseEnv[voiceIdx] = 65536;
    modalBandL1[voiceIdx] = 0;
    modalBandB1[voiceIdx] = 0;
    modalBandL2[voiceIdx] = 0;
    modalBandB2[voiceIdx] = 0;
  }

  // Impulse decay (~33ms)
  modalImpulseEnv[voiceIdx] -= (2000 * AUDIO_SAMPLE_RATE_DIV);
  if (modalImpulseEnv[voiceIdx] < 0)
    modalImpulseEnv[voiceIdx] = 0;

  // Q Factor / Decay (Main Knob)
  // sqrt curve: 0% = infinite bell, 100% = wood
  // Convert envelope to Q15, then use integer sqrt
  int32_t envQ15 = (int32_t)(p->envelope * 32768.0f);
  if (envQ15 < 0)
    envQ15 = 0;
  if (envQ15 > 32768)
    envQ15 = 32768;
  int32_t sqrtEnvQ15 = fast_isqrt_q15(envQ15);
  // invEnv = 1.0 - sqrtEnv → Q15: 32768 - sqrtEnvQ15
  int32_t invQ15 = 32768 - sqrtEnvQ15;
  if (invQ15 < 0)
    invQ15 = 0;
  // dampQ15 = invEnv^2 → (invQ15 * invQ15) >> 15
  int32_t dampQ15 = (invQ15 * invQ15) >> 15;
  int32_t q = 1 + ((dampQ15 * 4000) >> 15);

  // Excitation noise
  v->noiseState = v->noiseState * 1664525 + 1013904223;
  int32_t noise = (int32_t)v->noiseState >> 4;
  int32_t exciter = (noise * (modalImpulseEnv[voiceIdx] >> 6)) >> 15;
  if (exciter > q)
    exciter = q; // Dynamic gain compensation

  // Frequency coefficients
  int32_t f1 = ((int32_t)(v->phaseInc >> 14) * 25600) >> 15;
  if (f1 > 32000)
    f1 = 32000;

  // F2 = F1 × ratio (Y Knob: 1.0× to 4.0×)
  // timbreQ12 = timbre * 4096
  int32_t timbreQ12 = (int32_t)(p->timbre * 4096.0f);
  if (timbreQ12 < 0)
    timbreQ12 = 0;
  if (timbreQ12 > 4096)
    timbreQ12 = 4096;
  int32_t ratioQ12 = 4096 + (timbreQ12 * 3);
  int32_t f2 = (f1 * ratioQ12) >> 12;
  if (f2 > 32000)
    f2 = 32000;

  // === Filter 1 (SVF Bandpass) ===
  int32_t l1 = modalBandL1[voiceIdx];
  int32_t b1 = modalBandB1[voiceIdx];
  int32_t qb1 = (q * b1 + 16384) >> 15;
  int32_t h1 = exciter - l1 - qb1;
  b1 += (f1 * h1 + 16384) >> 15;
  l1 += (f1 * b1 + 16384) >> 15;
  l1 -= (l1 >> 20); // Weak DC leak
  if (b1 > 60000)
    b1 = 60000;
  else if (b1 < -60000)
    b1 = -60000;
  if (l1 > 60000)
    l1 = 60000;
  else if (l1 < -60000)
    l1 = -60000;
  modalBandL1[voiceIdx] = l1;
  modalBandB1[voiceIdx] = b1;

  // === Filter 2 (SVF Bandpass) ===
  int32_t l2 = modalBandL2[voiceIdx];
  int32_t b2 = modalBandB2[voiceIdx];
  int32_t qb2 = (q * b2 + 16384) >> 15;
  int32_t h2 = exciter - l2 - qb2;
  b2 += (f2 * h2 + 16384) >> 15;
  l2 += (f2 * b2 + 16384) >> 15;
  l2 -= (l2 >> 20);
  if (b2 > 60000)
    b2 = 60000;
  else if (b2 < -60000)
    b2 = -60000;
  if (l2 > 60000)
    l2 = 60000;
  else if (l2 < -60000)
    l2 = -60000;
  modalBandL2[voiceIdx] = l2;
  modalBandB2[voiceIdx] = b2;

  // Output sum with soft clip
  int32_t rawOsc = (b1 + b2) >> 1;
  if (rawOsc > 28000)
    rawOsc = 28000 + ((rawOsc - 28000) >> 2);
  else if (rawOsc < -28000)
    rawOsc = -28000 + ((rawOsc + 28000) >> 2);

  return rawOsc;
}

static void modal_reset(int voiceIdx, EnginePool *pool) {
  if (voiceIdx >= 0 && voiceIdx < MAX_VOICES) {
    modalBandL1[voiceIdx] = 0;
    modalBandB1[voiceIdx] = 0;
    modalBandL2[voiceIdx] = 0;
    modalBandB2[voiceIdx] = 0;
    modalImpulseEnv[voiceIdx] = 0;
  }
}

const SynthEngine engineModal = {
    .renderSample = modal_renderSample,
    .renderBlockSetup = nullptr,
    .reset = modal_reset,
    .outputGainQ8 = 220, // ~-1.3dB (slightly hot due to resonance)
    .name = "Modal"};
