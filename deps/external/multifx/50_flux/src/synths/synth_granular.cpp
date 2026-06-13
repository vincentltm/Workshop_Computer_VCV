// ============================================================================
// Synth Engine: GRANULAR CLOUD
// Spawns grains into a global pool; rendering done by RenderGlobalGrains.
// This engine only handles spawn logic per-voice.
// ============================================================================
#include "SynthEngine.h"

static void granular_blockSetup(Voice* v, const SynthParams* p, int voiceIdx,
                                int count, EnginePool* pool, bool gateRise) {
    // Granular spawn logic runs once per block per voice
    bool grainGate = v->gate || p->drone;

    if (!grainGate) return;

    if (globalGrainSpawnTimer > (uint32_t)count) {
        globalGrainSpawnTimer -= count;
    } else {
        globalGrainSpawnTimer = 0;
    }

    float dens = 0.5f + (p->envelope * 1.5f); // 0.5 .. 2.0 (overlap factor)

    if (globalGrainSpawnTimer == 0) {
        // Find free grain
        int freeIdx = -1;
        for (int g = 0; g < MAX_GRAINS; g++) {
            if (!globalGrains[g].active) { freeIdx = g; break; }
        }

        if (freeIdx >= 0) {
            Grain* g = &globalGrains[freeIdx];
            g->active = true;
            g->phase = 0;

            v->noiseState = v->noiseState * 1664525 + 1013904223;
            int32_t rnd = (v->noiseState >> 16);

            float posParam = p->timbre;
            float mainParam = p->envelope;

            // Speed: sample-domain increment
            // Speed: sample-domain increment (Fixed-point 2^x)
            int32_t noteDiffQ16 = ((int32_t)v->note - 60) << 16;
            uint64_t noteRatioQ30 = precise_exp2_q30(noteDiffQ16 / 12);
            uint32_t baseInc = (uint32_t)((30105ULL * noteRatioQ30) >> 30);

            // Pitch mod from Knob X (Fixed-point)
            uint32_t finalIncQ16 = baseInc << 16; // Shift up to support fractional mod
            if (!v->isMidi) {
                 // pitchMod = 0.5f + (p->pitch * 1.5f);
                 uint32_t p_q16 = (uint32_t)(p->pitch * 65536.0f);
                 uint32_t mod_q16 = 32768 + ((p_q16 * 98304) >> 16); 
                 finalIncQ16 = (uint32_t)(((uint64_t)finalIncQ16 * mod_q16) >> 16);
            }

            // Random pitch jitter (spray)
            if (mainParam > 0.01f) {
                // Approximate spray with integer math
                int32_t sprayRange = (int32_t)(mainParam * 32768.0f);
                int32_t jitter = ((rnd & 0xFFFF) * sprayRange) >> 16; // 0..sprayRange
                jitter -= (sprayRange >> 1); // center
                finalIncQ16 = (uint32_t)((int64_t)finalIncQ16 + (((int64_t)finalIncQ16 * jitter) >> 16));
            }
            g->inc = finalIncQ16 >> 16;

            // Length & Density
            uint32_t baseLen = 1000 + (uint32_t)(dens * (float)AUDIO_BASE_RATE);
            uint32_t ln = baseLen + (rnd & 511);
            g->length = ln << 16;

            // Pre-calc window scale
            uint32_t half = ln >> 1;
            if (half < 1) half = 1;
            g->windowScale = 65536 / half;

            // Position
            if (v->sampleLen > 100) {
                int32_t center = (int32_t)(posParam * (float)v->sampleLen);
                int32_t jitter = (int32_t)(mainParam * (float)v->sampleLen * 0.05f);
                if (jitter < 0) jitter = 0; // Guard: float→int truncation safety
                int32_t pos = center + (jitter > 0 ? (rnd % jitter) : 0);
                if (pos < 0) pos = 0;
                if (pos >= (int)v->sampleLen - 1) pos = (int)v->sampleLen - 2;
                g->startPos = (uint32_t)pos;
            } else {
                g->active = false;
            }

            g->windowPeak = 32768;
            g->amp = (int32_t)(p->volume * 65536.0f);
        }

        // Density timer
        uint32_t lastLen = (uint32_t)((float)AUDIO_BASE_RATE * 0.1f) + (uint32_t)((0.2f + p->envelope * 0.8f) * (float)AUDIO_BASE_RATE * 0.9f);
        uint32_t interval = lastLen / (uint32_t)(dens * 4.0f);
        if (interval < 100) interval = 100;
        globalGrainSpawnTimer = interval;
    }
}

static int32_t granular_renderSample(Voice* v, const SynthParams* p, int voiceIdx,
                                     EnginePool* pool, bool gateRise, bool activeGate) {
    // Granular audio is rendered globally in RenderGlobalGrains,
    // not per-voice. Return silence here.
    return 0;
}

static void granular_reset(int voiceIdx, EnginePool* pool) {
    for (int g = 0; g < MAX_GRAINS; g++) {
        globalGrains[g].active = false;
    }
    globalGrainSpawnTimer = 0;
}

const SynthEngine engineGranular = {
    .renderSample = granular_renderSample,
    .renderBlockSetup = granular_blockSetup,
    .reset = granular_reset,
    .outputGainQ8 = 256,   // Unity (granular has its own volume control)
    .name = "Granular"
};
