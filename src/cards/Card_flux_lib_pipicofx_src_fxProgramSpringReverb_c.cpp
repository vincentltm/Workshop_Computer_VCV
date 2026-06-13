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
#include "audio/delay.h"
#include "audio/reverbUtils.h"
#include "romfunc.h"

// "Dark Spring" - Dual Spring Physical Model

typedef struct {
    DelayDataType delay;
    AllpassType allpass;
    int16_t lpState;
} SingleSpring;

typedef struct {
    // Dispersion Chain
    AllpassType dispersionAPs[4];
    
    // Two parallel springs
    SingleSpring spring1;
    SingleSpring spring2;
    
    // Modulation
    uint32_t lfoPhase;
    int16_t lfoDepth;
    
    int16_t feedback;
    int16_t tone; 
    int16_t mix;
} SpringCoreType;

typedef struct {
    SpringCoreType coreL;
    SpringCoreType coreR;
    GainStageDataType presetVolume;
    int16_t freeze; // Freeze Parameter
} FxProgram20StereoDataType;

// Helper: One-Pole Lowpass
static inline int16_t springLowpass(int16_t sample, int16_t* state, int16_t alpha) {
    int32_t diff = (int32_t)sample - *state;
    *state = *state + ((diff * alpha) >> 15);
    return *state;
}

// Process one spring branch
static int16_t processSpringBranch(int16_t input, SingleSpring* spring, int16_t feedback, int16_t tone, int16_t modOffset, volatile uint32_t* audioState) {
    // 1. Read Delayed (Modulated)
    int32_t nominal = spring->delay.delayInSamples;
    int32_t actual = nominal + modOffset;
    if (actual < 1) actual = 1;
    
    // Manual read from circular buffer
    int32_t rdPtr = (int32_t)spring->delay.delayLinePtr - actual;
    int32_t mask = spring->delay.delayBufferLength - 1;
    int16_t delayedVal = *(spring->delay.delayLine + (rdPtr & mask));
    
    // 2. Filter (Tone/Damping)
    delayedVal = springLowpass(delayedVal, &spring->lpState, tone);
    
    // 3. Diffuse (End-of-spring reflection)
    delayedVal = allpassProcessSample(delayedVal, &spring->allpass, audioState);
    
    // 4. Feedback Injection
    int32_t fbVal = input + ((delayedVal * feedback) >> 15);
    
    // Hard clamp to prevent int16 wraparound distortion
    if (fbVal > 32767) fbVal = 32767;
    else if (fbVal < -32767) fbVal = -32767;
    
    // Write back
    addSampleToDelayline((int16_t)fbVal, &spring->delay);
    
    return delayedVal;
}

static int16_t springProcessWet(int16_t sampleIn, SpringCoreType* pData, int16_t feedbackOverride, volatile uint32_t* audioState) {
    // 1. Dispersion Chain (Creates metallic chirp)
    int16_t s = sampleIn >> 1; // Headroom
    for (int i=0; i<4; i++) {
        s = allpassProcessSample(s, &pData->dispersionAPs[i], audioState);
    }
    
    // 2. LFO for "Spring Flutter"
    pData->lfoPhase += 5000; // ~1.2Hz
    int16_t lfo = (pData->lfoPhase >> 16) & 0x7FFF;
    if (pData->lfoPhase & 0x80000000) lfo = 32767 - lfo;
    int16_t mod1 = (lfo * pData->lfoDepth) >> 15;
    int16_t mod2 = ((32767 - lfo) * pData->lfoDepth) >> 15; // Anti-phase modulation for second spring
    
    // 3. Parallel Springs
    int16_t out1 = processSpringBranch(s, &pData->spring1, feedbackOverride, pData->tone, mod1, audioState);
    int16_t out2 = processSpringBranch(s, &pData->spring2, feedbackOverride, pData->tone, mod2, audioState);
    
    // Sum
    return (out1 + out2);
}

static void fxProgram20processSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void*data, volatile uint32_t* audioStatePtr)
{
    FxProgram20StereoDataType* pData = (FxProgram20StereoDataType*)data;
    uint8_t frozen = (pData->freeze > 2048);
    
    int16_t driveL, driveR;
    if (frozen) {
        driveL = 0; driveR = 0;
    } else {
        driveL = (*inL + (*inR >> 2));
        driveR = (*inR + (*inL >> 2));
    }
    
    int16_t fbL = frozen ? 32767 : pData->coreL.feedback;
    int16_t fbR = frozen ? 32767 : pData->coreR.feedback;
    
    int16_t wetL = springProcessWet(driveL, &pData->coreL, fbL, audioStatePtr);
    int16_t wetR = springProcessWet(driveR, &pData->coreR, fbR, audioStatePtr);
    
    int16_t mix = pData->coreL.mix;
    *outL = clip((((32767 - mix) * (*inL)) >> 15) + ((mix * wetL) >> 14), audioStatePtr);
    *outR = clip((((32767 - mix) * (*inR)) >> 15) + ((mix * wetR) >> 14), audioStatePtr);
    
    *outL = gainStageProcessSample(*outL, &pData->presetVolume);
    *outR = gainStageProcessSample(*outR, &pData->presetVolume);
}

// Parameters
static void fxProgramParam1Callback(uint16_t val,void*data) // Mix
{
    FxProgram20StereoDataType* pData = (FxProgram20StereoDataType*)data;
    int16_t wVal = val << 3;
    pData->coreL.mix = wVal;
    pData->coreR.mix = wVal;
}
static void fxProgramParam1Display(void*data,char*res)
{
    FxProgram20StereoDataType* pData = (FxProgram20StereoDataType*)data;
    Int16ToChar(pData->coreL.mix/328,res);
    appendToString(res,"%");
}

static void fxProgramParam2Callback(uint16_t val,void*data) // Decay
{
    FxProgram20StereoDataType* pData = (FxProgram20StereoDataType*)data;
    int16_t fb = val << 3;
    if (fb > 27000) fb = 27000; // Safety limit (was 30000) to prevent explosion
    
    pData->coreL.feedback = fb;
    pData->coreR.feedback = fb;
}
static void fxProgramParam2Display(void*data,char*res)
{
    FxProgram20StereoDataType* pData = (FxProgram20StereoDataType*)data;
    Int16ToChar(pData->coreL.feedback / 328, res);
}

static void fxProgramParam3Callback(uint16_t val,void*data) // Tone
{
    FxProgram20StereoDataType* pData = (FxProgram20StereoDataType*)data;
    // Map 0-4095 to 1000-32000
    int16_t t = 1000 + ((val * 31000) >> 12);
    pData->coreL.tone = t;
    pData->coreR.tone = t;
}
static void fxProgramParam3Display(void*data,char*res)
{
    FxProgram20StereoDataType* pData = (FxProgram20StereoDataType*)data;
    if (pData->coreL.tone < 8000) appendToString(res, "Dark");
    else if (pData->coreL.tone < 20000) appendToString(res, "Norm");
    else appendToString(res, "Bright");
}

static void fxProgramParamFreezeCallback(uint16_t val, void* data) {
    ((FxProgram20StereoDataType*)data)->freeze = val;
}

static void fxProgramPresetVolumeCallback(uint16_t val,void*data)
{
    ((FxProgram20StereoDataType*)data)->presetVolume.gain = val >> 2; 
}
static void fxProgramPresetVolumeDisplay(void*data,char*res) {
    FxProgram20StereoDataType* pData = (FxProgram20StereoDataType*)data;
    decimalInt16ToChar(pData->presetVolume.gain*39,res,2);
    for (uint8_t c=0;c<PARAMETER_NAME_MAXLEN-1;c++) {
        if(*(res+c)==0) { *(res+c)='%'; *(res+c+1)=0; break; }
    }
}

// Init Helper
void initSingleSpring(SingleSpring* spr, int16_t** ptr, int16_t delayLen, int16_t apLen) {
    initDelay(&spr->delay, *ptr, 2048);
    spr->delay.delayInSamples = delayLen;
    for(int k=0; k<2048; k++) *(*ptr + k) = 0;
    *ptr += 2048;
    
    spr->allpass.delayLineIn = *ptr;
    spr->allpass.delayLineOut = *ptr + 512;
    spr->allpass.bufferSize = 511;
    spr->allpass.delayInSamples = apLen;
    spr->allpass.coefficient = 19660; // 0.6
    spr->allpass.delayPtr = 0;
    spr->allpass.oldValues = 0;
    
    for(int k=0; k<1024; k++) *(*ptr + k) = 0;
    *ptr += 1024;
    
    spr->lpState = 0;
}

void initSpringCore(SpringCoreType* pData, int16_t** memBase, uint8_t channelIndex) {
    int16_t* ptr = *memBase;
    int16_t spread = (channelIndex == 1) ? 29 : 0;

    // 1. Dispersion Chain
    // Buffer must be >= max delay time. With spread=29, AP[3] = 499+29 = 528
    // so we need bufferSize=1023 (next POT-1 above 528). Each AP = 1024 In + 1024 Out = 2048.
    int16_t dispLens[4] = {101, 233, 367, 499}; 
    for(int i=0; i<4; i++) {
        pData->dispersionAPs[i].delayLineIn = ptr;
        pData->dispersionAPs[i].delayLineOut = ptr + 1024;
        pData->dispersionAPs[i].bufferSize = 1023;
        pData->dispersionAPs[i].delayInSamples = dispLens[i] + spread;
        pData->dispersionAPs[i].coefficient = 24576; 
        pData->dispersionAPs[i].delayPtr = 0;
        pData->dispersionAPs[i].oldValues = 0;
        for(int k=0;k<2048;k++) *(ptr+k)=0;
        ptr += 2048;
    }
    
    // 2. Dual Springs
    initSingleSpring(&pData->spring1, &ptr, 1392 + spread, 211);
    initSingleSpring(&pData->spring2, &ptr, 1776 - spread, 263);

    pData->feedback = 0;
    pData->tone = 16000;
    pData->lfoPhase = 0;
    pData->lfoDepth = 60; 
    pData->mix = 16384;
    
    *memBase = ptr;
}

static void fxProgram20SetupStereo(void*data) {
    FxProgram20StereoDataType* pData = (FxProgram20StereoDataType*)data;
    int16_t* mem = getDelayMemoryPointer();
    
    initSpringCore(&pData->coreL, &mem, 0);
    initSpringCore(&pData->coreR, &mem, 1);
    pData->freeze = 0;
}

FxProgram20StereoDataType fxProgram20data = {
    .coreL.mix = 16384,
    .coreR.mix = 16384,
    .presetVolume.gain = 0x100,
    .freeze = 0
};

FxProgramType fxProgramSpringReverb = {
    .name = "Spring Reverb",
    .nParameters = 5,
    .parameters = {
        {
            .name="Mix", .control=0, .increment=1, .rawValue=0,
            .setParameter=fxProgramParam1Callback, .getParameterDisplay=fxProgramParam1Display
        },
        {
            .name="Decay", .control=1, .increment=1, .rawValue=0,
            .setParameter=fxProgramParam2Callback, .getParameterDisplay=fxProgramParam2Display
        },
        {
            .name="Tone", .control=2, .increment=1, .rawValue=0,
            .setParameter=fxProgramParam3Callback, .getParameterDisplay=fxProgramParam3Display
        },
        {
            .name="Freeze", .control=0xff, .increment=1, .rawValue=0,
            .setParameter=fxProgramParamFreezeCallback, .getParameterDisplay=0
        },
        {
            .name="Volume", .control=0xff, .increment=1, .rawValue=0x3ff,
            .setParameter=fxProgramPresetVolumeCallback, .getParameterDisplay=fxProgramPresetVolumeDisplay
        }
    },
    .processSampleStereo = &fxProgram20processSampleStereo,
    .setup = &fxProgram20SetupStereo,
    .isStereo = 1,
    .data = (void*)&fxProgram20data
};

} // namespace Card_Flux
