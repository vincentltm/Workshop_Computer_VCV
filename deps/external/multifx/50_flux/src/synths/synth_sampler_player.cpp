#include "SynthEngine.h"

// ============================================================================
// Sampler Player: Monophonic Looper
// Controls (Mapped in main.cpp):
//  - Main (p->pitch): Speed/Pitch
//  - X (p->timbre): Start Point (0..1)
//  - Y (p->envelope): Loop Length + Direction (-1..+1)
// ============================================================================

static struct {
    int32_t  speed;
    uint32_t startOffset;
    uint32_t endOffset;
} playerState[MAX_VOICES]; // Usually only voice 0 active

static void player_renderBlockSetup(Voice* v, const SynthParams* p, int voiceIdx,
                                      int count, EnginePool* /*pool*/, bool gateRise) {
    // 1. Speed (Main Knob -> p->pitch)
    // Optimization: Use SynthCore calculated speed
    int32_t speed = v->phaseInc;
    if (speed == 0) speed = 30105;

    // 2. Offsets
    // Start (Knob X -> p->timbre)
    uint32_t total = v->sampleLen;
    if (total == 0) total = 100;
    
    uint32_t startOffset = (uint32_t)(p->timbre * total);
    if (startOffset >= total) startOffset = total - 100;
    v->lastLoopStart = startOffset; // Store for reset on gate

    // Length (Knob Y -> p->envelope: -1.0 to 1.0)
    float lenVal = p->envelope;
    bool reverse = (lenVal < 0);
    if (reverse) lenVal = -lenVal;
    
    // Minimum length check
    uint32_t lenSamples = (uint32_t)(lenVal * total);
    if (lenSamples < 100) lenSamples = 100;
    
    uint32_t endOffset = startOffset + lenSamples;
    if (endOffset > total) endOffset = total;
    
    if (reverse) speed = -speed;
    
    playerState[voiceIdx].speed = speed;
    playerState[voiceIdx].startOffset = startOffset;
    playerState[voiceIdx].endOffset = endOffset;
}

static int32_t player_renderSample(Voice* v, const SynthParams* p, int voiceIdx,
                                     EnginePool* /*pool*/, bool gateRise, bool activeGate) {
    int32_t val = 0;
    int32_t speed = playerState[voiceIdx].speed;
    uint32_t start = playerState[voiceIdx].startOffset;
    uint32_t end = playerState[voiceIdx].endOffset;

    // Reset on Gate Trigger (Pulse/MIDI)
    if (gateRise) {
        if (speed < 0) v->sampleIdxQ16 = (uint64_t)end << 16;
        else v->sampleIdxQ16 = (uint64_t)start << 16;
    }

    if (!v->samplePtr || v->sampleLen == 0) return 0;
    
    uint32_t currentIdx = v->sampleIdxQ16 >> 16;
    
    // Loop Logic
    if (currentIdx >= end || currentIdx < start) {
         if (speed > 0) v->sampleIdxQ16 = (uint64_t)start << 16;
         else v->sampleIdxQ16 = (uint64_t)end << 16;
    }
    
    v->sampleIdxQ16 += speed;
    currentIdx = v->sampleIdxQ16 >> 16;

    // Decode
    if (currentIdx < v->sampleLen) {
        int16_t s1 = Mulaw_Decode(v->samplePtr[currentIdx]);
        // Simple Point sampling at high speeds? No, linear.
        int16_t s2 = s1;
        if (currentIdx + 1 < v->sampleLen) s2 = Mulaw_Decode(v->samplePtr[currentIdx + 1]);
        
        val = s1 + (((s2 - s1) * (v->sampleIdxQ16 & 0xFFFF)) >> 16);
    }
    
    // Always active, no envelope (or maybe simple de-click?)
    // Player mode is usually "Always On" drone or triggered loop.
    // Let's apply a simple 1.0 gain but smooth edges if needed?
    // User didn't specify envelope. "Main knob: pitch".
    // So no VCA?
    // Let's assume standard unity gain.
    
    return val;
}

const SynthEngine engineSamplerPlayer = {
    .renderSample = player_renderSample,
    .renderBlockSetup = player_renderBlockSetup,
    .reset = nullptr,
    .initVoice = nullptr,
    .renderBlock = nullptr,
    .outputGainQ8 = 256,
    .name = "Sampler Player"
};
