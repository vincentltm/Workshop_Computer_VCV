// ============================================================================
// Synth Engine: NOISE (White Noise via LCG)
// ============================================================================
#include "SynthEngine.h"

static int32_t noise_renderSample(Voice* v, const SynthParams* p, int voiceIdx,
                                  EnginePool* pool, bool gateRise, bool activeGate) {
    v->noiseState = v->noiseState * 1664525 + 1013904223;
    return (int32_t)((int16_t)(v->noiseState >> 16)) >> 2;
}

const SynthEngine engineNoise = {
    .renderSample = noise_renderSample,
    .renderBlockSetup = nullptr,
    .reset = nullptr,
    .outputGainQ8 = 512,   // +6dB boost (noise is quiet at >>2)
    .name = "Noise"
};
