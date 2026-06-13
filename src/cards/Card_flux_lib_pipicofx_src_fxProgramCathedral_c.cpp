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
#include <cstring>
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

// Deep Space Cathedral Reverb (Based on Dattorro Plate)
// Scaled up for 96k Buffer.

#define AP_IN_1 343 
#define AP_IN_2 258
#define AP_IN_3 916
#define AP_IN_4 670

#define AP_MOD_L 1624
#define DELAY_1_L 10780 
#define AP_TANK_L 4354  
#define DELAY_2_L 9010

#define AP_MOD_R 2196
#define DELAY_1_R 10204
#define AP_TANK_R 5775 
#define DELAY_2_R 7651

#define COEFF_IN_1 24576 
#define COEFF_IN_2 24576 
#define COEFF_IN_3 20480 
#define COEFF_IN_4 20480 
#define COEFF_TANK_AP 16384 

typedef struct {
    AllpassType apIn[4];
    DelayDataType modApL; 
    DelayDataType delay1L;
    AllpassType apTankL;
    DelayDataType delay2L;
    int16_t lpStateL;
    DelayDataType modApR;
    DelayDataType delay1R;
    AllpassType apTankR;
    DelayDataType delay2R;
    int16_t lpStateR;
    uint32_t lfoPhase;
    int16_t lfoDepth;
    int16_t decay;  
    int16_t bandwidth; 
    int16_t damping; 
    int16_t mix;
    int16_t inputLpState;
    int16_t freeze; // Freeze Parameter
    GainStageDataType presetVolume;
} FxProgram24DataType;

static inline int16_t plateLowpass(int16_t sample, int16_t* state, int16_t alpha) {
    int32_t diff = (int32_t)sample - *state;
    *state = *state + ((diff * alpha) >> 15);
    return *state;
}

static int16_t processModulatedAP(int16_t x, DelayDataType* d, int16_t modOffset, int16_t g) {
    int32_t nominal = d->delayInSamples;
    int32_t actual = nominal - modOffset; 
    if (actual < 1) actual = 1;
    int32_t rdPtr = (int32_t)d->delayLinePtr - actual;
    int32_t mask = d->delayBufferLength - 1; 
    
    int16_t w_delayed = *(d->delayLine + (rdPtr & mask));
    int32_t w_n = x + ((g * w_delayed) >> 15);
    if (w_n > 32767) w_n = 32767;
    if (w_n < -32767) w_n = -32767;
    *(d->delayLine + d->delayLinePtr) = (int16_t)w_n;
    d->delayLinePtr = (d->delayLinePtr + 1) & mask;
    int32_t y = ((-g * w_n) >> 15) + w_delayed;
    return (int16_t)y;
}

static void fxProgram24processSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void*data, volatile uint32_t* audioStatePtr)
{
    FxProgram24DataType* pData = (FxProgram24DataType*)data;
    uint8_t frozen = (pData->freeze > 2048);

    int16_t input = (*inL + *inR) >> 1;
    input = plateLowpass(input, &pData->inputLpState, pData->bandwidth);
    input = input >> 1; // 6dB headroom like Plate
    
    // FREEZE: Mute Input
    if (frozen) input = 0;

    for(int i=0; i<4; i++) {
        input = allpassProcessSample(input, &pData->apIn[i], audioStatePtr);
    }
    
    int16_t tankOutL = getDelayedSample(&pData->delay2L);
    int16_t tankOutR = getDelayedSample(&pData->delay2R);
    
    pData->lfoPhase += 4096; 
    int16_t modBase = (pData->lfoPhase >> 16) & 0x7FFF;
    if (pData->lfoPhase & 0x80000000) modBase = 32767 - modBase;
    int16_t modL = (modBase * pData->lfoDepth) >> 15;
    int16_t modR = ((32767 - modBase) * pData->lfoDepth) >> 15; 

    // FREEZE: Force Decay but keep slightly under unity for stability
    int16_t decayVal = frozen ? 32767 : pData->decay;

    // --- Left Tank ---
    int32_t inLeft = input + ((decayVal * tankOutR) >> 15);
    if (inLeft > 32767) inLeft = 32767; else if (inLeft < -32767) inLeft = -32767;

    int16_t sL = processModulatedAP((int16_t)inLeft, &pData->modApL, modL, 16384); 
    addSampleToDelayline(sL, &pData->delay1L);
    sL = getDelayedSample(&pData->delay1L);
    
    if (!frozen) sL = plateLowpass(sL, &pData->lpStateL, pData->damping);
    
    sL = allpassProcessSample(sL, &pData->apTankL, audioStatePtr);
    addSampleToDelayline(sL, &pData->delay2L);

    // --- Right Tank ---
    int32_t inRight = input + ((decayVal * tankOutL) >> 15);
    if (inRight > 32767) inRight = 32767; else if (inRight < -32767) inRight = -32767;

    int16_t sR = processModulatedAP((int16_t)inRight, &pData->modApR, modR, 16384);
    addSampleToDelayline(sR, &pData->delay1R);
    sR = getDelayedSample(&pData->delay1R);
    
    if (!frozen) sR = plateLowpass(sR, &pData->lpStateR, pData->damping);
    
    sR = allpassProcessSample(sR, &pData->apTankR, audioStatePtr);
    addSampleToDelayline(sR, &pData->delay2R);

    // Mix Dry/Wet (Tap end of chain)
    int16_t wetL = sL; 
    int16_t wetR = sR;

    int16_t mix = pData->mix;
    *outL = clip((((32767 - mix) * (*inL)) >> 15) + ((mix * wetL) >> 14), audioStatePtr);
    *outR = clip((((32767 - mix) * (*inR)) >> 15) + ((mix * wetR) >> 14), audioStatePtr);
    
    *outL = gainStageProcessSample(*outL, &pData->presetVolume);
    *outR = gainStageProcessSample(*outR, &pData->presetVolume);
}

static void fxProgram24Setup(void* data) {
    FxProgram24DataType* pData = (FxProgram24DataType*)data;
    int16_t* ptr = getDelayMemoryPointer();
    
    // Input APs (1k samples each)
    int16_t apLens[4] = {AP_IN_1, AP_IN_2, AP_IN_3, AP_IN_4};
    int16_t apCoeffs[4] = {COEFF_IN_1, COEFF_IN_2, COEFF_IN_3, COEFF_IN_4};
    for(int i=0; i<4; i++) {
        pData->apIn[i].delayLineIn = ptr;
        pData->apIn[i].delayLineOut = ptr + 512;
        pData->apIn[i].bufferSize = 511; // 512 In + 512 Out
        pData->apIn[i].delayInSamples = apLens[i];
        pData->apIn[i].coefficient = apCoeffs[i];
        pData->apIn[i].delayPtr = 0;
        pData->apIn[i].oldValues = 0;
        for(int k=0; k<1024; k++) *(ptr+k)=0;
        ptr += 1024; 
    }

    // -- Left Tank --
    // Memory budget: 88500 total.
    // 4x input APs = 4096, modApL/R = 2x4096 = 8192,
    // delay1L/R = 2x11000 = 22000, apTankL/R = 2x16384 = 32768,
    // delay2L = 9500, delay2R = 8000
    // Total = 4096+8192+22000+32768+9500+8000 = 84556 (within 88500)
    initDelay(&pData->modApL, ptr, 4096);  // POT >= AP_MOD_L(1624), required for bitmask in processModulatedAP
    pData->modApL.delayInSamples = AP_MOD_L;
    ptr += 4096;
    
    initDelay(&pData->delay1L, ptr, 11000); 
    pData->delay1L.delayInSamples = DELAY_1_L;
    ptr += 11000;  // was ptr+=12000, wasting 1000 samples
    
    pData->apTankL.delayLineIn = ptr;
    pData->apTankL.delayLineOut = ptr + 8192; 
    pData->apTankL.bufferSize = 8191; // 8k In + 8k Out
    pData->apTankL.delayInSamples = AP_TANK_L; 
    pData->apTankL.coefficient = COEFF_TANK_AP;
    pData->apTankL.delayPtr = 0;
    pData->apTankL.oldValues = 0;
    for(int k=0;k<16384;k++) *(ptr+k)=0;
    ptr += 16384; 
    
    initDelay(&pData->delay2L, ptr, 9500); 
    pData->delay2L.delayInSamples = DELAY_2_L;
    ptr += 9500;   // was ptr+=12000, wasting 2500 samples

    // -- Right Tank --
    initDelay(&pData->modApR, ptr, 4096);  // POT >= AP_MOD_R(2196), required for bitmask in processModulatedAP
    pData->modApR.delayInSamples = AP_MOD_R;
    ptr += 4096;

    initDelay(&pData->delay1R, ptr, 11000);
    pData->delay1R.delayInSamples = DELAY_1_R;
    ptr += 11000;  // was ptr+=12000
    
    pData->apTankR.delayLineIn = ptr;
    pData->apTankR.delayLineOut = ptr + 8192; 
    pData->apTankR.bufferSize = 8191;
    pData->apTankR.delayInSamples = AP_TANK_R;
    pData->apTankR.coefficient = COEFF_TANK_AP;
    pData->apTankR.delayPtr = 0;
    pData->apTankR.oldValues = 0;
    for(int k=0;k<16384;k++) *(ptr+k)=0;
    ptr += 16384; 
    
    initDelay(&pData->delay2R, ptr, 8000); 
    pData->delay2R.delayInSamples = DELAY_2_R;
    ptr += 8000;   // was ptr+=12000, wasting 4000 samples
    
    pData->freeze = 0;
}

FxProgram24DataType fxProgram24data = {
    .lfoDepth = 300,
    .decay = 16000,
    .bandwidth = 32000,
    .damping = 10000,
    .mix = 16384,
    .freeze = 0,
    .presetVolume = { .gain = 0xFF }
};

// Callbacks
static void fxProgram24Param1Callback(uint16_t val,void*data) { ((FxProgram24DataType*)data)->mix = val << 3; }
static void fxProgram24Param1Display(void*data,char*res) { Int16ToChar(((FxProgram24DataType*)data)->mix/328,res); appendToString(res,"%"); }

static void fxProgram24Param2Callback(uint16_t val,void*data) {
    int16_t d = val << 3;
    if (d > 32000) d = 32000;
    ((FxProgram24DataType*)data)->decay = d; 
}
static void fxProgram24Param2Display(void*data,char*res) { Int16ToChar(((FxProgram24DataType*)data)->decay/328,res); }

static void fxProgram24Param3Callback(uint16_t val,void*data) { 
    int16_t bw = 500 + ((val * 32267) >> 12);
    ((FxProgram24DataType*)data)->bandwidth = bw; 
}
static void fxProgram24Param3Display(void*data,char*res) { Int16ToChar(((FxProgram24DataType*)data)->bandwidth/328,res); }

static void fxProgram24ParamFreezeCallback(uint16_t val,void*data) { ((FxProgram24DataType*)data)->freeze = val; }

static void fxProgram24VolumeCallback(uint16_t val,void*data) { ((FxProgram24DataType*)data)->presetVolume.gain = val >> 2; }
static void fxProgram24VolumeDisplay(void*data,char*res) { decimalInt16ToChar(((FxProgram24DataType*)data)->presetVolume.gain*39,res,2); appendToString(res,"%"); }

FxProgramType fxProgramCathedral = {
    .name = "Deep Cathedral",
    .parameters = {
        {
            .name = "Mix",
            .control = 0,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgram24Param1Display,
            .setParameter = fxProgram24Param1Callback
        },
        {
            .name = "Decay",
            .control = 1,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgram24Param2Display,
            .setParameter = fxProgram24Param2Callback
        },
        {
            .name = "Tone",
            .control = 2,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgram24Param3Display,
            .setParameter = fxProgram24Param3Callback
        },
        {
            .name = "Freeze",
            .control = 0xff,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = 0,
            .setParameter = fxProgram24ParamFreezeCallback
        },
        {
            .name = "Volume",
            .control = 0xff,
            .rawValue = 0x3ff,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgram24VolumeDisplay,
            .setParameter = fxProgram24VolumeCallback
        },
        {
            .name = "",
            .control = 255,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = 0,
            .setParameter = 0
        },
        {
            .name = "",
            .control = 255,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = 0,
            .setParameter = 0
        },
        {
            .name = "",
            .control = 255,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = 0,
            .setParameter = 0
        }
    },
    .processSampleStereo = &fxProgram24processSampleStereo,
    .setup = &fxProgram24Setup,
    .nParameters = 5,
    .isStereo = 1,
    .data = (void*)&fxProgram24data
};

} // namespace Card_Flux
