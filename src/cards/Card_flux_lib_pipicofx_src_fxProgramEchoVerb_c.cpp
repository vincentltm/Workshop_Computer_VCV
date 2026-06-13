// Automatically generated separate compilation wrapper
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
#include <algorithm>
#include <vector>
#include <string>
#include <atomic>
#include <thread>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>
#include <float.h>
#include <setjmp.h>
#include <time.h>
#include <errno.h>
#include <locale.h>
#include <inttypes.h>
#include <cinttypes>
#include "pico_mocks.h"
#include "tusb.h"
#define while(...) while((__VA_ARGS__) && !g_cancellation_requested.load(std::memory_order_relaxed))

#include "ComputerCard.h"

namespace Card_Flux {
    extern const int16_t exptable_impl[];
    extern const int16_t logtable_impl[];
#include "pipicofx/fxPrograms.h"
#include "stringFunctions.h"
#include "romfunc.h"
#include "audio/audiotools.h"
#include "audio/reverbUtils.h"
/* stripped system include */

// --- ECHO VERB ---
// Stereo Delay fed into a diffuse reverb tank with analog/tube warmth.

// Smooth 1-pole filter for parameters
static inline int32_t smoothParam(int32_t current, int32_t target, int32_t alpha) {
    return current + (((target - current) * alpha) >> 15);
}

// Lowpass filter for tone/damping
static inline int16_t lpFilter(int16_t sample, int32_t* state, int16_t alpha) {
    int32_t diff = (int32_t)sample - *state;
    *state = *state + ((diff * alpha) >> 15);
    return (int16_t)*state;
}

// Polynomial Soft Clipper for Warmth
static inline int16_t tapeSaturate(int32_t x, int32_t driveAmt) {
    // driveAmt 0..4096
    int32_t driven = x + ((x * driveAmt) >> 13);
    
    // Hard clamp before polynomial
    if (driven > 49000) driven = 49000;
    if (driven < -49000) driven = -49000;
    
    // x - x^3 / k approximation
    int32_t x_abs = (driven > 0) ? driven : -driven;
    int32_t sub = (driven * x_abs) >> 17;
    int32_t out = driven - sub;
    
    if (out > 32767) out = 32767;
    if (out < -32767) out = -32767;
    return (int16_t)out;
}

static void fxProgramProcessSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void* data, volatile uint32_t* audioState) {
    FxProgram9DataType* pData = (FxProgram9DataType*)data;
    uint8_t frozen = (pData->freeze > 2048);

    // Update Smooth Time
    int32_t targetTimeQ8 = (int32_t)pData->time << 8;
    pData->delaySmoothedTime = smoothParam(pData->delaySmoothedTime, targetTimeQ8, 100);
    
    int32_t delaySamples = pData->delaySmoothedTime >> 8;
    if (delaySamples < 1) delaySamples = 1;
    pData->delayL.delayInSamples = delaySamples;
    
    // Slight offset for right channel for width
    int32_t delaySamplesR = delaySamples + (delaySamples >> 4); 
    if (delaySamplesR > (pData->delayR.delayBufferLength - 100)) 
        delaySamplesR = pData->delayR.delayBufferLength - 100;
    pData->delayR.delayInSamples = delaySamplesR;

    // Read Delays
    int16_t tapL = getDelayedSample(&pData->delayL);
    int16_t tapR = getDelayedSample(&pData->delayR);

    // Warmth Param controls Feedback and Saturation (0..4096)
    int32_t warmth = pData->warmth;
    int32_t fbGain = 10000 + ((warmth * 18000) >> 12); // Up to nice long feedback
    int32_t driveAmt = warmth;

    if (frozen) {
        fbGain = 32767;
        driveAmt = 0; // Reduce noise buildup when frozen
    }

    // Feedback lines (Ping Pong style)
    int32_t feedL = (tapR * fbGain) >> 15;
    int32_t feedR = (tapL * fbGain) >> 15;
    
    // Damping (constant 6kHz ish)
    feedL = lpFilter((int16_t)feedL, &pData->lpStateL, 18000);
    feedR = lpFilter((int16_t)feedR, &pData->lpStateR, 18000);

    // Input mixes with feedback
    int32_t inSumL = frozen ? feedL : (((int32_t)(*inL + *inR) >> 2) + feedL);
    int32_t inSumR = frozen ? feedR : (((int32_t)(*inR + *inL) >> 2) + feedR);

    // Saturate
    int16_t writeL = tapeSaturate(inSumL, driveAmt);
    int16_t writeR = tapeSaturate(inSumR, driveAmt);

    // Write back to delay
    addSampleToDelayline(writeL, &pData->delayL);
    addSampleToDelayline(writeR, &pData->delayR);

    // --- REVERB TANK (Diffusion of the delay taps) ---
    int16_t wetL = allpassProcessSample(tapL, &pData->apL1, audioState);
    wetL = allpassProcessSample(wetL, &pData->apL2, audioState);
    
    int16_t wetR = allpassProcessSample(tapR, &pData->apR1, audioState);
    wetR = allpassProcessSample(wetR, &pData->apR2, audioState);

    // Final Mix
    int16_t mix = pData->mix;
    int16_t dryGain = 32767 - (mix >> 1); // Curve keeps more dry level
    
    int32_t outL_mixed = ((*inL * dryGain) >> 15) + ((wetL * mix) >> 12); 
    int32_t outR_mixed = ((*inR * dryGain) >> 15) + ((wetR * mix) >> 12);
    
    if (outL_mixed > 32767) outL_mixed = 32767; else if (outL_mixed < -32767) outL_mixed = -32767;
    if (outR_mixed > 32767) outR_mixed = 32767; else if (outR_mixed < -32767) outR_mixed = -32767;

    *outL = gainStageProcessSample((int16_t)outL_mixed, &pData->presetVolume);
    *outR = gainStageProcessSample((int16_t)outR_mixed, &pData->presetVolume);
}

// --- Parameter Callbacks ---

static void paramMixCallback(uint16_t val, void* data) {
    FxProgram9DataType* pData = (FxProgram9DataType*)data;
    pData->mix = val; // 0..4095
    fxProgramEchoVerb.parameters[0].rawValue = val;
}
static void paramMixDisplay(void* data, char* res) {
    FxProgram9DataType* pData = (FxProgram9DataType*)data;
    uint16_t v = (pData->mix * 100) >> 12;
    UInt8ToChar((uint8_t)v, res);
    appendToString(res, "%");
}

static void paramTimeCallback(uint16_t val, void* data) {
    FxProgram9DataType* pData = (FxProgram9DataType*)data;
    
    int16_t diff = (int16_t)val - (int16_t)pData->lastTimeKnob;
    if (diff < 0) diff = -diff;
    if (diff < 4) return; // Hysteresis
    pData->lastTimeKnob = val;

    // Max 1 sec delay (48000 or 24000 samples depending on AUDIO_BASE_RATE)
    // Scale 0..4095 to 0..AUDIO_BASE_RATE
    int32_t maxDelay = AUDIO_BASE_RATE; 
    int32_t minDelay = 100;
    int32_t target = minDelay + (((int32_t)val * (maxDelay - minDelay)) >> 12);
    
    pData->time = (int16_t)target;
    if (pData->delaySmoothedTime == 0) pData->delaySmoothedTime = target << 8;
    fxProgramEchoVerb.parameters[1].rawValue = val;
}
static void paramTimeDisplay(void* data, char* res) {
    FxProgram9DataType* pData = (FxProgram9DataType*)data;
    int16_t dval = pData->time / (AUDIO_BASE_RATE / 1000);
    Int16ToChar(dval, res);
    appendToString(res, "ms");
}

static void paramWarmthCallback(uint16_t val, void* data) {
    FxProgram9DataType* pData = (FxProgram9DataType*)data;
    pData->warmth = val; 
    fxProgramEchoVerb.parameters[2].rawValue = val;
}
static void paramWarmthDisplay(void* data, char* res) {
    FxProgram9DataType* pData = (FxProgram9DataType*)data;
    uint16_t v = (pData->warmth * 100) >> 12;
    UInt8ToChar((uint8_t)v, res);
    appendToString(res, "%");
}

static void paramFreezeCallback(uint16_t val, void* data) {
    FxProgram9DataType* pData = (FxProgram9DataType*)data;
    pData->freeze = val;
}

static void paramVolumeCallback(uint16_t val, void* data) {
    FxProgram9DataType* pData = (FxProgram9DataType*)data;
    pData->presetVolume.gain = val >> 2;
    fxProgramEchoVerb.parameters[4].rawValue = val;
}
static void paramVolumeDisplay(void* data, char* res) {
    FxProgram9DataType* pData = (FxProgram9DataType*)data;
    uint16_t v = pData->presetVolume.gain * 100 / 1024;
    UInt8ToChar((uint8_t)v, res);
    appendToString(res, "%");
}

// --- Setup ---

static void fxProgramSetup(void* data) {
    FxProgram9DataType* pData = (FxProgram9DataType*)data;
    
    // Reset state
    pData->lpStateL = 0;
    pData->lpStateR = 0;
    pData->delaySmoothedTime = 0;
    pData->lastTimeKnob = 0;

    int16_t* ptr = getDelayMemoryPointer();
    
    // Allocate Delays
    int32_t delayMax = AUDIO_BASE_RATE * 2; // Allocate 2 seconds buffer space combined
    if (delayMax > DELAY_LINE_LENGTH - 4000) delayMax = DELAY_LINE_LENGTH - 4000;
    
    initDelay(&pData->delayL, ptr, delayMax / 2);
    ptr += delayMax / 2;
    
    initDelay(&pData->delayR, ptr, delayMax / 2);
    ptr += delayMax / 2;
    
    // Allocate Allpasses for Reverb tank (bufferSize must be power-of-2 minus 1)
    pData->apL1.delayLineIn = ptr; pData->apL1.delayLineOut = ptr + 512;
    pData->apL1.bufferSize = 511; pData->apL1.delayInSamples = 346; pData->apL1.coefficient = 19660; pData->apL1.delayPtr=0; pData->apL1.oldValues=0;
    for(int i=0; i<1024; i++) *(ptr+i)=0; ptr += 1024;

    pData->apL2.delayLineIn = ptr; pData->apL2.delayLineOut = ptr + 128;
    pData->apL2.bufferSize = 127; pData->apL2.delayInSamples = 112; pData->apL2.coefficient = 19660; pData->apL2.delayPtr=0; pData->apL2.oldValues=0;
    for(int i=0; i<256; i++) *(ptr+i)=0; ptr += 256;

    pData->apR1.delayLineIn = ptr; pData->apR1.delayLineOut = ptr + 512;
    pData->apR1.bufferSize = 511; pData->apR1.delayInSamples = 378; pData->apR1.coefficient = 19660; pData->apR1.delayPtr=0; pData->apR1.oldValues=0;
    for(int i=0; i<1024; i++) *(ptr+i)=0; ptr += 1024;

    pData->apR2.delayLineIn = ptr; pData->apR2.delayLineOut = ptr + 128;
    pData->apR2.bufferSize = 127; pData->apR2.delayInSamples = 126; pData->apR2.coefficient = 19660; pData->apR2.delayPtr=0; pData->apR2.oldValues=0;
    for(int i=0; i<256; i++) *(ptr+i)=0; ptr += 256;

    // Apply defaults
    pData->mix = 1000;
    pData->time = 400 * (AUDIO_BASE_RATE / 1000);
    pData->warmth = 2000;
    pData->presetVolume.gain = 0x300 >> 2;
    pData->freeze = 0;
}

static FxProgram9DataType fxProgram9Data;

FxProgramType fxProgramEchoVerb = {
    .name = "Echo Verb",
    .nParameters = 5,
    .parameters = {
        {
            .name = "Mix",
            .control = 2, // Knob Main 
            .increment = 64,
            .rawValue = 1000,
            .setParameter = paramMixCallback,
            .getParameterDisplay = paramMixDisplay
        },
        {
            .name = "Time",
            .control = 0, // Knob X
            .increment = 64,
            .rawValue = 2000,
            .setParameter = paramTimeCallback,
            .getParameterDisplay = paramTimeDisplay
        },
        {
            .name = "Warm Wash",
            .control = 1, // Knob Y
            .increment = 64,
            .rawValue = 2000,
            .setParameter = paramWarmthCallback,
            .getParameterDisplay = paramWarmthDisplay
        },
        {
            .name = "Freeze",
            .control = 0xFF,
            .increment = 2048,
            .rawValue = 0,
            .setParameter = paramFreezeCallback,
            .getParameterDisplay = NULL
        },
        {
            .name = "Level",
            .control = 0xFF,
            .increment = 32,
            .rawValue = 0x300,
            .setParameter = paramVolumeCallback,
            .getParameterDisplay = paramVolumeDisplay
        }
    },
    .processSampleStereo = fxProgramProcessSampleStereo,
    .setup = fxProgramSetup,
    .reset = NULL,
    .isStereo = 1,
    .data = (void*)&fxProgram9Data
};
} // namespace Card_Flux
