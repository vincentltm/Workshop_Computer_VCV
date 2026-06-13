#include "SynthEngine.h"

// ============================================================================
// Sampler Loop: Polyphonic, Chromatic, Gated Looping
// Controls:
//  - Main: Envelope Release (Standard ADSR)
//  - X: Pitch (Chromatic)
//  - Y: Sample Select
// ============================================================================

static struct {
    int32_t  speed;
    int32_t  releaseRate;
} loopState[MAX_VOICES];

static void loop_renderBlockSetup(Voice* v, const SynthParams* p, int voiceIdx,
                                      int count, EnginePool* /*pool*/, bool /*gateRise*/) {
    // 1. Speed (Chromatic)
    // Optimization: Use SynthCore calculated speed (v->phaseInc)
    int32_t speed = v->phaseInc;
    if (speed == 0) speed = 30105;

    // 2. Envelope
    // Main Knob -> Release Time
    float t = p->envelope;
    int32_t releaseRate;
    if (t < 0.3f) releaseRate = 1 << 20; // Fast
    else releaseRate = (int32_t)((1.0f - t) * (1 << 20)) + 1000;

    loopState[voiceIdx].speed = speed;
    loopState[voiceIdx].releaseRate = releaseRate;
}

static int32_t loop_renderSample(Voice* v, const SynthParams* p, int voiceIdx,
                                     EnginePool* /*pool*/, bool gateRise, bool activeGate) {
    int32_t val = 0;
    int32_t speed = loopState[voiceIdx].speed;

    if (gateRise) {
        v->sampleIdxQ16 = (uint64_t)v->loopStartIdx << 16;
        v->samplerEnvStage = 0; 
        v->samplerEnvVal = 0;
    }
    
    if (!v->samplePtr || v->sampleLen == 0) return 0;

    // Loop Logic
    uint32_t currentIdx = v->sampleIdxQ16 >> 16;
    if (currentIdx >= v->loopEndIdx) {
        currentIdx = v->loopStartIdx;
        v->sampleIdxQ16 = (uint64_t)v->loopStartIdx << 16;
    }

    v->sampleIdxQ16 += speed;

    // Decode
    if (currentIdx < v->sampleLen) {
        int16_t s1 = Mulaw_Decode(v->samplePtr[currentIdx]);
        int16_t s2 = s1;
        if (currentIdx + 1 < v->sampleLen) {
             s2 = Mulaw_Decode(v->samplePtr[currentIdx + 1]);
        }
        val = s1 + (((s2 - s1) * (v->sampleIdxQ16 & 0xFFFF)) >> 16);
    }
    
    // Envelope (ASR)
    if (v->samplerEnvStage == 0) { // Attack
        v->samplerEnvVal += 10000000; // ~2ms Attack (was 1<<25 / ~0.5ms)
        if (v->samplerEnvVal >= (1<<30)) { v->samplerEnvVal = 1<<30; v->samplerEnvStage = 1; }
    } else if (v->samplerEnvStage == 1) { // Sustain
        v->samplerEnvVal = 1<<30;
        if (!activeGate) v->samplerEnvStage = 2;
    } else { // Release
        v->samplerEnvVal -= loopState[voiceIdx].releaseRate;
        if (v->samplerEnvVal < 0) { v->samplerEnvVal = 0; v->active = false; }
    }
    
    val = (val * (v->samplerEnvVal >> 16)) >> 14;
    return val;
}

const SynthEngine engineSamplerLoop = {
    .renderSample = loop_renderSample,
    .renderBlockSetup = loop_renderBlockSetup,
    .reset = nullptr,
    .initVoice = nullptr,
    .renderBlock = nullptr,
    .outputGainQ8 = 256,
    .name = "Sampler Loop"
};
