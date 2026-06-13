// ============================================================================
// Sampler Engine — Extracted from SynthCore.cpp
// Unified handler for OneShot, Loop, Player, and Drums modes.
// ============================================================================
#include "SynthEngine.h"
#include <math.h>

// ============================================================================
// Per-voice block state (computed in renderBlockSetup, consumed in renderSample)
// ============================================================================
static struct {
    int32_t  speed;
    uint32_t startOffset;
    uint32_t endOffset;
    int32_t  attackRate;
    int32_t  releaseRate;
} samplerState[MAX_VOICES];

// ============================================================================
// Block Setup — compute speed, offsets, envelope rates
// ============================================================================
static void sampler_renderBlockSetup(Voice* v, const SynthParams* p, int voiceIdx,
                                      int count, EnginePool* /*pool*/, bool /*gateRise*/) {
    // Speed is pre-calculated in SynthCore.cpp (unified pitch/CV/MIDI logic).
    // v->phaseInc contains the correct playback speed (SAMPLER_SPEED_Q16 at unity).
    int32_t speed = v->phaseInc;
    if (speed == 0) speed = SAMPLER_SPEED_Q16; // Safe fallback (should not occur)

    uint32_t totalSamples = v->sampleLen;
    uint32_t startOffset = 0;
    uint32_t endOffset = totalSamples;

    // --- Offset computation per mode ---
    uint32_t timbreQ16 = (uint32_t)(p->timbre * 65536.0f);

    if (p->mode == SYNTH_MODE_SAMPLER_PLAYER) {
        // Player: timbre = Start, envelope = Length (-1..1, negative = reverse)
        startOffset = (uint32_t)(((uint64_t)timbreQ16 * totalSamples) >> 16);
        v->lastLoopStart = startOffset; 
        
        float lenVal = p->envelope;
        bool reverse = false;
        if (lenVal < 0) { reverse = true; lenVal = -lenVal; }
        
        uint32_t lenSamples = (uint32_t)(lenVal * totalSamples);
        if (lenSamples < 100) lenSamples = 100;
        
        endOffset = startOffset + lenSamples;
        if (reverse) speed = -speed;
    } else if (p->mode == SYNTH_MODE_SAMPLER_LOOP) {
        // Loop: timbre = Start Position, play to end
        startOffset = (uint32_t)(((uint64_t)timbreQ16 * totalSamples) >> 16);
        endOffset = totalSamples;
    } else {
        // OneShot / Drums: Start from exactly 0 (full transient), play to end.
        startOffset = 0;
        endOffset = totalSamples;
    }
    
    // Safety clamps
    if (endOffset > totalSamples) endOffset = totalSamples;
    if (startOffset >= endOffset) startOffset = (endOffset > 100) ? endOffset - 100 : 0;

    // Envelope rates
    int32_t attackRate = (1 << 25) * AUDIO_SAMPLE_RATE_DIV;   // ~1ms attack
    int32_t releaseRate = (1 << 20) * AUDIO_SAMPLE_RATE_DIV;  // ~30ms release default

    if (p->mode == SYNTH_MODE_SAMPLER_ONESHOT || p->mode == SYNTH_MODE_DRUMS) {
        // Map p->envelope (Main Knob) to Decay/Release Time
        float t = p->envelope;
        if (p->mode == SYNTH_MODE_DRUMS) {
            // Drums get a fast default, scaled by main knob
            releaseRate = 1 << 22; // Fast
            if (t > 0.1f) {
                float inv = 1.0f - t;
                releaseRate = (int32_t)(inv * inv * 500000.0f) + 50;
            }
        } else {
            // OneShot
            int32_t t_sq = (int32_t)(t * t * 100000.0f); 
            releaseRate = 100000 - t_sq;
            if (releaseRate < 100) releaseRate = 100; // Max sustain
            if (t > 0.98f) releaseRate = 0; // Infinite sustain
        }
    }
    
    samplerState[voiceIdx].speed = speed;
    samplerState[voiceIdx].startOffset = startOffset;
    samplerState[voiceIdx].endOffset = endOffset;
    samplerState[voiceIdx].attackRate = attackRate;
    samplerState[voiceIdx].releaseRate = releaseRate;
}

// ============================================================================
// Per-Sample Render — gate, decode, envelope, completion
// Returns fully processed val (not rawOsc — sampler bypasses shared VCA)
// ============================================================================
static int32_t sampler_renderSample(Voice* v, const SynthParams* p, int voiceIdx,
                                     EnginePool* /*pool*/, bool gateRise, bool activeGate) {
    int32_t val = 0;
    int32_t speed = samplerState[voiceIdx].speed;
    uint32_t startOffset = samplerState[voiceIdx].startOffset;
    uint32_t endOffset = samplerState[voiceIdx].endOffset;
    int32_t attackRate = samplerState[voiceIdx].attackRate;
    int32_t releaseRate = samplerState[voiceIdx].releaseRate;

    // --- Gate trigger ---
    if (gateRise) {
        if (p->mode == SYNTH_MODE_SAMPLER_PLAYER) {
             if (speed < 0) v->sampleIdxQ16 = (uint64_t)endOffset << 16;
             else v->sampleIdxQ16 = (uint64_t)startOffset << 16;
        } else {
             v->sampleIdxQ16 = (uint64_t)startOffset << 16;
        }
        v->samplerEnvStage = 0; 
        v->samplerEnvVal = 0;
    }
    
    if (!v->samplePtr || v->sampleLen == 0) {
        v->samplerEnvVal = 0;
        return 0;
    }

    uint32_t currentIdx = v->sampleIdxQ16 >> 16;
    
    // --- Boundary check ---
    if (currentIdx >= endOffset || (currentIdx < startOffset && currentIdx < 0xF0000000)) { 
        if (p->mode == SYNTH_MODE_SAMPLER_PLAYER) {
            // Loop: wrap back
            if (speed < 0) v->sampleIdxQ16 = (uint64_t)endOffset << 16;
            else v->sampleIdxQ16 = (uint64_t)startOffset << 16;
        } else {
            val = 0;
        }
    }

    // Advance position
    v->sampleIdxQ16 += speed;
    currentIdx = v->sampleIdxQ16 >> 16;
        
    // --- Loop mode ---
    if (p->mode == SYNTH_MODE_SAMPLER_LOOP && activeGate) {
        if (currentIdx >= v->loopEndIdx) {
            currentIdx = v->loopStartIdx;
            v->sampleIdxQ16 = (uint64_t)v->loopStartIdx << 16;
        }
    }
    
    // --- μ-Law decode with linear interpolation ---
    if (currentIdx < v->sampleLen) {
        int16_t s1 = Mulaw_Decode(v->samplePtr[currentIdx]);
        int16_t s2 = s1;
        if (currentIdx + 1 < v->sampleLen) {
            s2 = Mulaw_Decode(v->samplePtr[currentIdx + 1]);
        }
        int32_t frac = v->sampleIdxQ16 & 0xFFFF;
        val = s1 + (((s2 - s1) * frac) >> 16);
    }
    
    // --- Envelope (Attack / Sustain / Release) ---
    if (v->samplerEnvStage == 0) {
        v->samplerEnvVal += attackRate;
        if (v->samplerEnvVal >= (1<<30)) { v->samplerEnvVal = 1<<30; v->samplerEnvStage = 1; }
    } else if (v->samplerEnvStage == 1) {
        v->samplerEnvVal = 1<<30;
        if (p->mode == SYNTH_MODE_DRUMS || p->mode == SYNTH_MODE_SAMPLER_ONESHOT) {
            v->samplerEnvStage = 2; // Immediate decay for triggers
        } else if (!activeGate) {
            v->samplerEnvStage = 2;
        }
    } else {
        v->samplerEnvVal -= releaseRate;
        if (v->samplerEnvVal < 0) v->samplerEnvVal = 0;
    }
    val = (val * (v->samplerEnvVal >> 16)) >> 14;

    // Boost + soft clip
    val = val << 2; 
    if (val > 32000) val = 32000;
    if (val < -32000) val = -32000;

    // --- Completion check ---
    bool samplerFinished = ((v->sampleIdxQ16 >> 16) >= endOffset);
    if (p->mode == SYNTH_MODE_SAMPLER_ONESHOT || p->mode == SYNTH_MODE_DRUMS) {
        if (samplerFinished || (v->samplerEnvStage >= 2 && v->samplerEnvVal == 0)) v->active = false;
    } else if (p->mode == SYNTH_MODE_SAMPLER_LOOP) {
        if (!activeGate && v->samplerEnvVal == 0) v->active = false;
    }

    return val;
}

// ============================================================================
// Engine Definition
// ============================================================================
const SynthEngine engineSampler = {
    .renderSample = sampler_renderSample,
    .renderBlockSetup = sampler_renderBlockSetup,
    .reset = nullptr,
    .initVoice = nullptr,
    .renderBlock = nullptr,
    .outputGainQ8 = 256,
    .name = "Sampler"
};
