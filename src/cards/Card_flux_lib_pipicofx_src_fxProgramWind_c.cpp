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
#include "audio/audiotools.h"
#include "romfunc.h"

// FxProgram31: "WindBot" (Physical Model of a Bore Instrument)
// Uses a waveguide model with a non-linear reed interaction.
// The input signal's envelope allows "blowing" into the instrument.
// The input signal is also mixed in as "noise" excitation.

typedef struct {
    int16_t* delayLine;
    int32_t length;
    int32_t writePtr;
    int32_t lpState; 
    int32_t dcBlockState;
} BoreType;

typedef struct {
    int16_t* buffer;
    
    BoreType boreL;
    BoreType boreR;
    
    // Parameters
    int16_t mix;
    int16_t tune;  
    int16_t tone;  // Lip/Reed stiffness or Filter
    int32_t pressureEnv;
    
    int16_t lastTuneParam;
    
    GainStageDataType presetVolume;
} FxProgram31DataType;

static FxProgram31DataType progData;

static void updateTuning(FxProgram31DataType* d) {
    // Map Tune 0..4095 to Lengths
    
    int32_t baseLen = 40 + ((d->tune * 600) >> 12); // Range 40..640 samples
    
    d->boreL.length = baseLen;
    // Stereo: Slight Detune (Unison) for "Thick" sound instead of Interval
    d->boreR.length = baseLen + (baseLen >> 6); // ~1.5% longer
    
    // Safety
    if (d->boreL.length < 10) d->boreL.length = 10;
    if (d->boreR.length < 10) d->boreR.length = 10;
}

static inline int16_t reedTable(int16_t diff, int16_t stiffness) {
    // Cubic Polynomial: y = x - x^3
    // Creates soft clipping and negative resistance slope for oscillation.
    
    int32_t x = diff;
    
    // Stiffness controls "Embouchure". 
    // High stiffness -> attenuates input to non-linearity (harder to blow)
    // Scale x down by stiffness?
    // stiffness 0..32k.
    // effective_x = x * (1.0 - stiff/64k)?
    // Let's just use x directly but clip strictly.
    
    // Calc x^3
    // Q15 math: (x*x)/32768
    int32_t x2 = (x * x) >> 15;
    if (x < 0) x2 = -x2; // Preserve sign for scaling? No x*x is positive.
    // x^3 = x2 * x
    int32_t x3 = (x2 * x) >> 15;
    
    // Result
    int32_t out = x - x3;
    
    // Input is limited to +/- 16k in calling function to keep cubic stable
    // (If x > 1.0, x - x^3 goes negative fast).
    // We want the region where x < 1.0.
    
    return (int16_t)out;
}

static int16_t processBore(BoreType* b, int16_t pressure, int16_t noise, int16_t stiffness) {
    // Waveguide loop
    // Read from Delay (Bore reflection)
    int32_t rIdx = b->writePtr - b->length;
    while(rIdx < 0) rIdx += 4096;
    
    int16_t boreOut = b->delayLine[rIdx];
    
    // Filter Reflection (Viscothermal loss + Bell loss)
    // Loss factor: 0.98 (~32100)
    int32_t lossy = (boreOut * 32100) >> 15;
    
    // Lowpass (Tone)
    b->lpState += ((lossy - b->lpState) * 5000) >> 15; 
    int16_t reflected = b->lpState;
    
    // DC Block
    b->dcBlockState += (reflected - b->dcBlockState) >> 9;
    reflected -= b->dcBlockState;
    
    // Reed Junction
    // Scale inputs to prevent overdrive of cubic function
    // Optimal range for X is +/- 16000 (0.5 in Q15) to sit in sigmoid "sweet spot"
    int16_t diff = (pressure >> 1) - (reflected >> 0) + (noise >> 4); 
    
    // Stiffness adds a bias or attenuation?
    // Let's use stiffness to attenuate the diff (harder reed)
    int16_t stiffScale = 32767 - (stiffness >> 1); // 1.0 .. 0.5
    diff = (diff * stiffScale) >> 15;
    
    // Hard clamp Input to cubic to avoid instability
    if (diff > 20000) diff = 20000;
    if (diff < -20000) diff = -20000;
    
    int16_t reedOut = reedTable(diff, stiffness);
    
    // Incoming Wave = Pressure + Reed Reflection
    int16_t forward = (pressure >> 1) + reedOut;
    
    // Soft Limit Bore Energy
    if (forward > 24000) forward = 24000;
    if (forward < -24000) forward = -24000;
    
    // Write
    b->delayLine[b->writePtr] = forward;
    b->writePtr++;
    if(b->writePtr >= 4096) b->writePtr = 0;
    
    return boreOut; 
}

static void fxProgram31ProcessStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void* data, volatile uint32_t* audioState) {
    FxProgram31DataType* d = (FxProgram31DataType*)data;
    
    if (d->tune != d->lastTuneParam) {
        updateTuning(d);
        d->lastTuneParam = d->tune;
    }
    
    // Input is Breath Pressure
    int16_t mono = (*inL + *inR) >> 1;
    int32_t absIn = mono > 0 ? mono : -mono;
    
    // Envelope Follower for Pressure
    // Attack fast, Release slow?
    if (absIn > d->pressureEnv) d->pressureEnv = absIn;
    else d->pressureEnv -= (d->pressureEnv >> 9); // Release
    
    int16_t pressure = (int16_t)d->pressureEnv;
    // Scale pressure by input volume?
    // It is inherent.
    
    int16_t stiffness = d->tone;
    
    // Process Stereo Bores
    int16_t wL = processBore(&d->boreL, pressure, *inL, stiffness);
    int16_t wR = processBore(&d->boreR, pressure, *inR, stiffness);
    
    // Mix
    int16_t mix = d->mix << 3;
    *outL = ((*inL * (32767-mix)) >> 15) + ((wL * mix) >> 15);
    *outR = ((*inR * (32767-mix)) >> 15) + ((wR * mix) >> 15);
    
    *outL = gainStageProcessSample(*outL, &d->presetVolume);
    *outR = gainStageProcessSample(*outR, &d->presetVolume);
}

static void fxProgram31Setup(void* data) {
    FxProgram31DataType* d = (FxProgram31DataType*)data;
    d->buffer = getDelayMemoryPointer(); 
    
    for(int i=0; i<32768; i++) d->buffer[i] = 0;
    
    d->boreL.delayLine = d->buffer;
    d->boreR.delayLine = d->buffer + 4096;
    
    d->boreL.writePtr = 0; d->boreL.lpState = 0; d->boreL.dcBlockState = 0;
    d->boreR.writePtr = 0; d->boreR.lpState = 0; d->boreR.dcBlockState = 0;
    
    d->mix = 16384;
    d->tune = 2048;
    d->tone = 5000;
    d->pressureEnv = 0;
    d->lastTuneParam = -1;
    d->presetVolume.gain = 256;
    
    updateTuning(d);
}

// Params
static void fxParamMix(uint16_t val, void* data) { ((FxProgram31DataType*)data)->mix = val; }
static void fxDisplayMix(void* data, char* res) { Int16ToChar(((FxProgram31DataType*)data)->mix/41, res); appendToString(res, "%"); }

static void fxParamTune(uint16_t val, void* data) { ((FxProgram31DataType*)data)->tune = val; }
static void fxDisplayTune(void* data, char* res) { Int16ToChar(((FxProgram31DataType*)data)->tune/41, res); appendToString(res, "%"); }

static void fxParamTone(uint16_t val, void* data) { ((FxProgram31DataType*)data)->tone = val; }
static void fxDisplayTone(void* data, char* res) { Int16ToChar(((FxProgram31DataType*)data)->tone/41, res); appendToString(res, "%"); }

static void fxParamVol(uint16_t val, void* data) { ((FxProgram31DataType*)data)->presetVolume.gain = val >> 2; }
static void fxDisplayVol(void* data, char* res) { decimalInt16ToChar(((FxProgram31DataType*)data)->presetVolume.gain*39,res,2); appendToString(res, "%"); }

FxProgramType fxProgramWind = {
    .name = "Wind",
    .parameters = {
        {
            .name = "Mix",
            .control = 0,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplayMix,
            .setParameter = fxParamMix
        },
        {
            .name = "Tune",
            .control = 1,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplayTune,
            .setParameter = fxParamTune
        },
        {
            .name = "Stiff",
            .control = 2,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplayTone,
            .setParameter = fxParamTone
        },
        {
            .name = "Volume",
            .control = 0xff,
            .rawValue = 0x400,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplayVol,
            .setParameter = fxParamVol
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
    .processSampleStereo = fxProgram31ProcessStereo,
    .setup = fxProgram31Setup,
    .nParameters = 4,
    .isStereo = 1,
    .data = &progData
};

} // namespace Card_Flux
