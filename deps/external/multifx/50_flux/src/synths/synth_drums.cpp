#include "SynthEngine.h"

// ============================================================================
// Sampler Drums: Polyphonic, Multi-Sample Trigger
// Controls:
//  - Main: Envelope Decay
//  - X: Pitch (Global Speed)
//  - Y: Bank Select (Handled in main.cpp)
// ============================================================================

static struct {
    int32_t  speed;
    int32_t  releaseRate;
} drumsState[MAX_VOICES];

static void drums_renderBlockSetup(Voice* v, const SynthParams* p, int voiceIdx,
                                      int count, EnginePool* /*pool*/, bool /*gateRise*/) {
    // 1. Speed
    // Optimization: Use SynthCore calculated speed
    int32_t speed = v->phaseInc;
    if (speed == 0) speed = SAMPLER_SPEED_Q16; // Correct unity speed for current sample rate

    // 2. Envelope (Decay)
    // Main Knob -> p->envelope
    float t = p->envelope;
    int32_t releaseRate = 1 << 22; // Default Fast
    // Map t (0..1) to Decay Time
    // t=0 -> Fast Decay (Short hit)
    // t=1 -> Long Decay (Full sample)
    if (t > 0.1f) {
        // Longer release
        float inv = 1.0f - t;
        releaseRate = (int32_t)(inv * inv * 500000.0f) + 50; // Curve
    }
    
    drumsState[voiceIdx].speed = speed;
    drumsState[voiceIdx].releaseRate = releaseRate;
}

static int32_t drums_renderSample(Voice* v, const SynthParams* p, int voiceIdx,
                                     EnginePool* /*pool*/, bool gateRise, bool activeGate) {
    int32_t val = 0;
    int32_t speed = drumsState[voiceIdx].speed;

    if (gateRise) {
        v->sampleIdxQ16 = 0;
        v->samplerEnvStage = 0; 
        v->samplerEnvVal = 0;
    }
    
    if (!v->samplePtr || v->sampleLen == 0) return 0;
    
    uint32_t currentIdx = v->sampleIdxQ16 >> 16;
    if (currentIdx >= v->sampleLen) {
        v->active = false;
        return 0;
    }

    v->sampleIdxQ16 += speed;

    // Decode
    if (currentIdx < v->sampleLen) {
        int16_t s1 = Mulaw_Decode(v->samplePtr[currentIdx]);
        int16_t s2 = s1;
        if (currentIdx + 1 < v->sampleLen) s2 = Mulaw_Decode(v->samplePtr[currentIdx + 1]);
        val = s1 + (((s2 - s1) * (v->sampleIdxQ16 & 0xFFFF)) >> 16);
    }
    
    // Envelope (AR)
    if (v->samplerEnvStage == 0) { // Attack
        v->samplerEnvVal += (1<<25); // Instant Attack
        if (v->samplerEnvVal >= (1<<30)) { v->samplerEnvVal = 1<<30; v->samplerEnvStage = 1; }
    } else { // Decay
        v->samplerEnvVal -= drumsState[voiceIdx].releaseRate;
        if (v->samplerEnvVal < 0) { v->samplerEnvVal = 0; v->active = false; }
    }
    
    val = (val * (v->samplerEnvVal >> 16)) >> 14;
    return val;
}

const SynthEngine engineSamplerDrums = {
    .renderSample = drums_renderSample,
    .renderBlockSetup = drums_renderBlockSetup,
    .reset = nullptr,
    .initVoice = nullptr,
    .renderBlock = nullptr,
    .outputGainQ8 = 256,
    .name = "Drums"
};
