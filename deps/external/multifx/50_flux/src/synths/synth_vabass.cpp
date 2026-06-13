// ============================================================================
// Synth Engine: VA LEAD/BASS
// Monophonic (voice 0), dual-oscillator + sub, ladder-style 4-pole LPF
//
// Controls:
//   Main (envelope): Decay time (0=instant, 1=very long)
//   X (pitch):       Standard pitch ±2 octaves
//   Y (timbre):      Filter cutoff (exponential curve) + envelope mod depth
//   filterCutoff:    Resonance (from switch-down page)
//
// State aliasing (to avoid new RAM cost):
//   v->phase         = Osc1 saw phase
//   vabassSubPhase[] = Osc2 saw phase (detuned)
//   vabassFilterEnv[]= Sub-osc phase (repurposed)
//   vabassSvfBand[]  = LP pole 0  (reuse SVF band slot)
//   vabassGlidePitch[]= LP pole 1 (reuse glide slot)
//   Pool memory not used — the 4-pole chain uses existing 2 extern slots
//   for poles 0+1; poles 2+3 sit in local statics here (2×4×int32 = 32B)
// ============================================================================
#include "SynthEngine.h"

// Only 2 extra pole arrays (32 bytes total), poles 0+1 aliased to externals
static int32_t lp2[MAX_VOICES] = {0};
static int32_t lp3[MAX_VOICES] = {0};
// Filter envelope: stored in modalBandL1 (free when not in Modal mode)
// We declare our own static int32 with __attribute__((section)) to avoid
// any clash, using the smallest possible allocation:
static int32_t fEnvVal[MAX_VOICES]  = {0}; // current filter env level
static int32_t fEnvRate[MAX_VOICES] = {0}; // decay rate

static void valead_reset(int voiceIdx, EnginePool * /*pool*/) {
    if (voiceIdx < 0 || voiceIdx >= MAX_VOICES) return;
    vabassSubPhase[voiceIdx]  = 0;
    vabassFilterEnv[voiceIdx] = 0;
    vabassSvfBand[voiceIdx]   = 0;
    vabassGlidePitch[voiceIdx]= 0;
    lp2[voiceIdx] = lp3[voiceIdx] = 0;
    fEnvVal[voiceIdx] = fEnvRate[voiceIdx] = 0;
}

static int32_t valead_renderSample(Voice *v, const SynthParams *p, int voiceIdx,
                                   EnginePool * /*pool*/, bool gateRise,
                                   bool activeGate) {
    uint32_t baseInc = v->phaseInc;

    // Tiny analog drift
    v->noiseState = v->noiseState * 1664525u + 1013904223u;
    int32_t drift = (int32_t)(v->noiseState >> 22);

    // Osc1: Main Saw
    v->phase += baseInc + (uint32_t)drift;
    int32_t saw1 = (int32_t)(v->phase >> 16) - 32768;

    // Osc2: Detuned Saw (~4 cents)
    uint32_t detune = (baseInc * 152u) >> 16;
    vabassSubPhase[voiceIdx] += baseInc + detune - (uint32_t)drift;
    int32_t saw2 = (int32_t)(vabassSubPhase[voiceIdx] >> 16) - 32768;

    // Sub: Square, -1 oct
    uint32_t subPh = (uint32_t)vabassFilterEnv[voiceIdx];
    subPh += baseInc >> 1;
    vabassFilterEnv[voiceIdx] = (int32_t)subPh;
    int32_t subSq = (subPh >> 31) ? 32000 : -32000;

    // Mix — give the sub a bigger share for a fatter low-end
    int32_t oscMix = (saw1 >> 1) + (saw2 >> 1) + (subSq >> 1);

    // Soft drive
    if (oscMix >  24000) oscMix =  24000 + ((oscMix - 24000) >> 2);
    if (oscMix < -24000) oscMix = -24000 + ((oscMix + 24000) >> 2);

    // Filter envelope (exponential decay)
    if (gateRise) {
        fEnvVal[voiceIdx] = 1 << 29;
        // Map p->envelope (0.0 to 1.0) to an exponential shift (fastest=8 to slowest=16)
        fEnvRate[voiceIdx] = 8 + (int32_t)(p->envelope * 8.0f);
    }
    if (!activeGate) {
        // Fast decay on note off
        fEnvVal[voiceIdx] -= (fEnvVal[voiceIdx] >> 8);
    } else {
        fEnvVal[voiceIdx] -= (fEnvVal[voiceIdx] >> fEnvRate[voiceIdx]);
    }

    // 4-pole ladder filter
    int32_t timbreQ15 = (int32_t)(p->timbre * 32768.0f);
    if (timbreQ15 < 0)     timbreQ15 = 0;
    if (timbreQ15 > 32768) timbreQ15 = 32768;
    int32_t cn2 = (timbreQ15 * timbreQ15) >> 15;
    int32_t cn3 = (cn2      * timbreQ15) >> 15;
    int32_t baseCutoff = 80 + ((cn3 * 17000) >> 15);

    // Dynamic envelope amount: less env sweep when cutoff is high
    int32_t envAmt = 14000 - ((timbreQ15 * 12000) >> 15);
    if (envAmt < 1000) envAmt = 1000;
    int32_t envMod = (int32_t)(((int64_t)fEnvVal[voiceIdx] * envAmt) >> 29);
    
    int32_t cutoff = baseCutoff + envMod;
    if (cutoff > 18000) cutoff = 18000;
    if (cutoff < 80)    cutoff = 80;

    int32_t resQ15 = (int32_t)(p->filterCutoff * 32768.0f);
    if (resQ15 < 0)     resQ15 = 0;
    if (resQ15 > 32768) resQ15 = 32768;
    int32_t feedback = (resQ15 * 28000) >> 15;

    int32_t f_q15 = (int32_t)(cutoff * 2.0f * 3.14159f * 32768.0f / (float)AUDIO_BASE_RATE);
    if (f_q15 > 20000) f_q15 = 20000;
    if (f_q15 < 10)    f_q15 = 10;

    // Uses: vabassSvfBand = lp0, vabassGlidePitch = lp1, lp2[], lp3[]
    int32_t lp0ref = vabassSvfBand[voiceIdx];
    int32_t lp1ref = (int32_t)vabassGlidePitch[voiceIdx];

    int32_t fb  = (int32_t)(((int64_t)lp3[voiceIdx] * feedback) >> 15);
    int32_t inp = (oscMix >> 1) - fb;

    lp0ref    += (int32_t)(((int64_t)f_q15 * (inp     - lp0ref)) >> 15);
    lp1ref    += (int32_t)(((int64_t)f_q15 * (lp0ref  - lp1ref)) >> 15);
    lp2[voiceIdx] += (int32_t)(((int64_t)f_q15 * (lp1ref - lp2[voiceIdx])) >> 15);
    lp3[voiceIdx] += (int32_t)(((int64_t)f_q15 * (lp2[voiceIdx] - lp3[voiceIdx])) >> 15);

    // Clip
    if (lp0ref >  32700) lp0ref =  32700;
    if (lp0ref < -32700) lp0ref = -32700;
    if (lp1ref >  32700) lp1ref =  32700;
    if (lp1ref < -32700) lp1ref = -32700;
    if (lp2[voiceIdx] >  32700) lp2[voiceIdx] =  32700;
    if (lp2[voiceIdx] < -32700) lp2[voiceIdx] = -32700;
    if (lp3[voiceIdx] >  32700) lp3[voiceIdx] =  32700;
    if (lp3[voiceIdx] < -32700) lp3[voiceIdx] = -32700;

    vabassSvfBand[voiceIdx]    = lp0ref;
    vabassGlidePitch[voiceIdx] = (uint32_t)lp1ref;

    int32_t out = lp3[voiceIdx];
    if (out >  28000) out =  28000 + ((out - 28000) >> 2);
    if (out < -28000) out = -28000 + ((out + 28000) >> 2);
    if (out >  32000) out =  32000;
    if (out < -32000) out = -32000;
    return out;
}

const SynthEngine engineVABass = {
    .renderSample     = valead_renderSample,
    .renderBlockSetup = nullptr,
    .reset            = valead_reset,
    .initVoice        = nullptr,
    .renderBlock      = nullptr,
    .outputGainQ8     = 768,
    .name             = "Virtual Analog"
};
