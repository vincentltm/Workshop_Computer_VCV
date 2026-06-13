#include "SynthEngine.h"

// ============================================================================
// Sampler One Shot: Polyphonic, Chromatic, Decaying Envelope
// Controls:
//  - Main: Envelope Decay/Release
//  - X: Pitch (Chromatic)
//  - Y: Sample Select (Handled in main.cpp, passed as p->timbre? No, main.cpp sets voice->sampleIndex)
//       Wait, main.cpp handles sample selection. p->timbre might be unused or used for something else.
//       User: "Y knob (and audio r in) selects sample".
//       In main.cpp, Y knob -> Sample Select. 
//       So p->timbre is NOT the sample select here.
//       But wait, Y knob ALSO maps to p->timbre in main.cpp unless we block it.
//       Core usually ignores p->timbre for OneShot if it's select.
//       Let's assume standard behavior: p->timbre is ignored here.
// ============================================================================

static struct {
    int32_t  speed;
    uint32_t startOffset;
    uint32_t endOffset;
    int32_t  attackRate;
    int32_t  releaseRate;
} oneshotState[MAX_VOICES];

static void oneshot_renderBlockSetup(Voice* v, const SynthParams* p, int voiceIdx,
                                      int count, EnginePool* /*pool*/, bool /*gateRise*/) {
    // 1. Speed Calculation
    // Optimization: SynthCore.cpp already calculates the full modulated speed 
    // (including Pitch Knob, Audio CV, and Note Offset) and stores it in v->phaseInc.
    // We should use that to ensure modulation works.
    int32_t speed = v->phaseInc;

    // Fallback? If phaseInc is 0 (shouldn't happen), assume unity?
    if (speed == 0) speed = 30105;

    // 2. Offsets
    // Play full sample
    uint32_t startOffset = 0;
    uint32_t endOffset = v->sampleLen;
    
    // Safety
    if (endOffset > v->sampleLen) endOffset = v->sampleLen;
    if (startOffset >= endOffset) startOffset = 0;

    // 3. Envelope
    // Main Knob -> p->envelope -> Decay/Release
    // 0..1 -> Short..Long
    // Exponential curve
    float t = p->envelope;
    int32_t t_sq = (int32_t)(t * t * 100000.0f); 
    int32_t releaseRate = 100000 - t_sq;
    if (releaseRate < 100) releaseRate = 100; // Max sustain
    if (t > 0.98f) releaseRate = 0; // Infinite sustain
    
    // Attack: Slower to de-click (was 200000000/Instant)
    int32_t attackRate = 10000000; // ~100 samples (2ms)

    oneshotState[voiceIdx].speed = speed;
    oneshotState[voiceIdx].startOffset = startOffset;
    oneshotState[voiceIdx].endOffset = endOffset;
    oneshotState[voiceIdx].attackRate = attackRate;
    oneshotState[voiceIdx].releaseRate = releaseRate;
}

static int32_t oneshot_renderSample(Voice* v, const SynthParams* p, int voiceIdx,
                                     EnginePool* /*pool*/, bool gateRise, bool activeGate) {
    int32_t val = 0;
    int32_t speed = oneshotState[voiceIdx].speed;
    uint32_t startOffset = oneshotState[voiceIdx].startOffset;
    uint32_t endOffset = oneshotState[voiceIdx].endOffset;

    if (gateRise) {
        v->sampleIdxQ16 = (uint64_t)startOffset << 16;
        v->samplerEnvStage = 0; 
        v->samplerEnvVal = 0;
    }
    
    if (!v->samplePtr || v->sampleLen == 0) return 0;

    uint32_t currentIdx = v->sampleIdxQ16 >> 16;
    if (currentIdx >= endOffset) {
        v->active = false;
        return 0;
    }

    v->sampleIdxQ16 += speed;

    // Decode (Linear Interp)
    if (currentIdx < v->sampleLen) {
        int16_t s1 = Mulaw_Decode(v->samplePtr[currentIdx]);
        int16_t s2 = s1;
        if (currentIdx + 1 < v->sampleLen) {
            s2 = Mulaw_Decode(v->samplePtr[currentIdx + 1]);
        }
        int32_t frac = v->sampleIdxQ16 & 0xFFFF;
        val = s1 + (((s2 - s1) * frac) >> 16);
    }
    
    // Envelope
    if (v->samplerEnvStage == 0) { // Attack
        v->samplerEnvVal += oneshotState[voiceIdx].attackRate;
        if (v->samplerEnvVal >= (1<<30)) { v->samplerEnvVal = 1<<30; v->samplerEnvStage = 1; }
    } else { // Decay/Release (OneShot ignores Gate Release, just plays out)
        if (oneshotState[voiceIdx].releaseRate > 0) {
            v->samplerEnvVal -= oneshotState[voiceIdx].releaseRate;
            if (v->samplerEnvVal < 0) { v->samplerEnvVal = 0; v->active = false; }
        }
    }
    
    val = (val * (v->samplerEnvVal >> 16)) >> 14;
    return val;
}

const SynthEngine engineSamplerOneShot = {
    .renderSample = oneshot_renderSample,
    .renderBlockSetup = oneshot_renderBlockSetup,
    .reset = nullptr,
    .initVoice = nullptr,
    .renderBlock = nullptr,
    .outputGainQ8 = 256,
    .name = "Sampler OneShot"
};
