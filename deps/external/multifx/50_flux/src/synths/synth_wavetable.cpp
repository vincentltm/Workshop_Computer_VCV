// ============================================================================
// Synth Engine: WAVETABLE MORPH
// Crossfades between Triangle → Saw → Square → Narrow Pulse
// ============================================================================
#include "SynthEngine.h"

static int32_t wavetable_renderSample(Voice *v, const SynthParams *p,
                                      int voiceIdx, EnginePool *pool,
                                      bool gateRise, bool activeGate) {
  v->phase += v->phaseInc;
  uint32_t phase = v->phase;

  // Generate 4 waveforms
  int32_t tri = abs((int32_t)((phase >> 16) - 32768)) - 16384;
  int32_t saw = (int32_t)(phase >> 16) - 32768;
  int32_t sqr = (phase < 0x80000000) ? 16000 : -16000;
  int32_t pul = (phase < 0x20000000) ? 16000 : -16000;

  // Morph via timbre knob (0..1 → 0..3 index)
  // Convert to Q10 for 3x range: 0..3072
  int32_t mQ10 = (int32_t)(p->timbre * 3072.0f);
  if (mQ10 < 0)
    mQ10 = 0;
  if (mQ10 > 3072)
    mQ10 = 3072;

  int32_t outA, outB;
  int32_t mixQ10; // fractional part in Q10 (0..1024)
  if (mQ10 < 1024) {
    outA = tri;
    outB = saw;
    mixQ10 = mQ10;
  } else if (mQ10 < 2048) {
    outA = saw;
    outB = sqr;
    mixQ10 = mQ10 - 1024;
  } else {
    outA = sqr;
    outB = pul;
    mixQ10 = mQ10 - 2048;
  }

  // Integer crossfade: outA + (outB - outA) * mix
  int32_t rawOsc = outA + (((outB - outA) * mixQ10) >> 10);
  return rawOsc >> 1;
}

const SynthEngine engineWavetable = {.renderSample = wavetable_renderSample,
                                     .renderBlockSetup = nullptr,
                                     .reset = nullptr,
                                     .outputGainQ8 =
                                         384, // +3dB boost (compensate for >>1)
                                     .name = "Wavetable"};
