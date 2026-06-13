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

// Improved FreeVerb with Modulation
// Adds subtle LFO modulation to comb delays to reduce metallic ringing

typedef struct {
    DelayDataType delaysL[8];
    DelayDataType delaysR[8];
    FirstOrderIirType feedbackFiltersL[8];
    FirstOrderIirType feedbackFiltersR[8];
    AllpassType allpassesL[4];
    AllpassType allpassesR[4];
    GainStageDataType presetVolume;
    int16_t mix;
    int16_t freeze;
    
    // Modulation state
    uint32_t lfoPhase;
    int16_t lfoDepth;  // Amount of delay modulation (samples)
    
    // Base delay times (store originals for modulation)
    int16_t baseDelaysL[8];
    int16_t baseDelaysR[8];
} FxProgram19StereoDataType;

// Modulated comb filter read with interpolation
static inline int16_t readModulatedComb(DelayDataType* d, int16_t modOffset) {
    int32_t actualDelay = d->delayInSamples + modOffset;
    if (actualDelay < 1) actualDelay = 1;
    if (actualDelay >= (int32_t)d->delayBufferLength) actualDelay = d->delayBufferLength - 1;
    
    int32_t rdIdx = (int32_t)d->delayLinePtr - actualDelay;
    if (rdIdx < 0) rdIdx += d->delayBufferLength;
    
    return d->delayLine[rdIdx & (d->delayBufferLength - 1)];
}

// Improved comb filter with modulation
static int16_t modulatedCombProcess(int16_t input, DelayDataType* d, FirstOrderIirType* lpf, int16_t modOffset) {
    // Read with modulated delay
    int16_t delayed = readModulatedComb(d, modOffset);
    
    // Lowpass in feedback path (damping)
    int16_t filtered = firstOrderIirLowpassProcessSample(delayed, lpf);
    
    // Feedback
    int32_t fb = (filtered * d->feedback) >> 15;
    
    // Write new sample
    int32_t writeVal = input + fb;
    if (writeVal > 32767) writeVal = 32767;
    if (writeVal < -32767) writeVal = -32767;
    
    d->delayLine[d->delayLinePtr] = (int16_t)writeVal;
    d->delayLinePtr = (d->delayLinePtr + 1) & (d->delayBufferLength - 1);
    
    return delayed;
}

static void fxProgram19processSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void*data, volatile uint32_t*audioState)
{
    FxProgram19StereoDataType* pData = (FxProgram19StereoDataType*)data;
    uint8_t frozen = (pData->freeze > 2048);
    
    // === LFO for modulation ===
    pData->lfoPhase += 2500;  // ~0.6Hz modulation rate
    
    // Generate 8 phase-offset LFO values for each comb
    int16_t mods[8];
    for (int i = 0; i < 8; i++) {
        uint32_t phase = pData->lfoPhase + ((uint32_t)i * 0x20000000U);  // Spread phases
        int16_t tri = (phase >> 16) & 0x7FFF;
        if (phase & 0x80000000) tri = 32767 - tri;
        mods[i] = ((tri - 16384) * pData->lfoDepth) >> 15;  // ±lfoDepth samples
    }
    
    // Mono sum for input
    int16_t monoInput;
    if (frozen) {
        monoInput = 0;
    } else {
        monoInput = ((*inL + *inR) >> 1) >> 1;  // 50% headroom
    }
    
    // Backup/restore feedback for freeze
    int16_t savedFB = pData->delaysL[0].feedback;
    if (frozen) {
        for(int c = 0; c < 8; c++) {
            pData->delaysL[c].feedback = 32700;
            pData->delaysR[c].feedback = 32700;
        }
    }

    // === Process 8 modulated comb filters (parallel) ===
    int32_t sumL = 0;
    int32_t sumR = 0;
    
    for (int c = 0; c < 8; c++) {
        sumL += modulatedCombProcess(monoInput, &pData->delaysL[c], 
                                      &pData->feedbackFiltersL[c], mods[c]);
        sumR += modulatedCombProcess(monoInput, &pData->delaysR[c], 
                                      &pData->feedbackFiltersR[c], -mods[c]);  // Opposite mod for stereo
    }

    if (frozen) {
        for(int c = 0; c < 8; c++) {
            pData->delaysL[c].feedback = savedFB;
            pData->delaysR[c].feedback = savedFB;
        }
    }

    int16_t combOutL = clip(sumL >> 2, audioState);
    int16_t combOutR = clip(sumR >> 2, audioState);
    
    // === 4 series allpass filters for diffusion ===
    int32_t sampleOutL = combOutL;
    int32_t sampleOutR = combOutR;
    
    for (int c = 0; c < 4; c++) {
        sampleOutL = allpassProcessSample(sampleOutL, &pData->allpassesL[c], audioState);
        sampleOutR = allpassProcessSample(sampleOutR, &pData->allpassesR[c], audioState);
    }
    
    // === Output mix ===
    *outL = clip((((32767 - pData->mix) * (*inL)) >> 15) + ((pData->mix * sampleOutL) >> 14), audioState);
    *outR = clip((((32767 - pData->mix) * (*inR)) >> 15) + ((pData->mix * sampleOutR) >> 14), audioState);
    
    *outL = gainStageProcessSample(*outL, &pData->presetVolume);
    *outR = gainStageProcessSample(*outR, &pData->presetVolume);
}

// Parameter callbacks
static void fxProgramParameter1Callback(uint16_t val, void*data) // Size (Decay)
{
    FxProgram19StereoDataType* pData = (FxProgram19StereoDataType*)data;
    int32_t feedback = (((val << 3) * 9175) >> 15) + 22938;  // 0.7 to 0.98
    for (int c = 0; c < 8; c++) {
        pData->delaysL[c].feedback = (int16_t)feedback;
        pData->delaysR[c].feedback = (int16_t)feedback;
    }
    fxProgramFreeVerb.parameters[1].rawValue = val;
}

static void fxProgramParameter1Display(void*data, char*res)
{
    FxProgram19StereoDataType* pData = (FxProgram19StereoDataType*)data;
    float ffbk = int2float(pData->delaysL[0].feedback) / 32767.0f;
    int16_t t60 = 0;
    if (ffbk > 0.0000305f) {
        t60 = (int16_t)float2int(-589.03004f / fln(ffbk));
    }
    Int16ToChar(t60, res);
    appendToString(res, " ms");
}

static void fxProgramParameter2Callback(uint16_t val, void*data) // Damping
{
    FxProgram19StereoDataType* pData = (FxProgram19StereoDataType*)data;
    for (int c = 0; c < 8; c++) {
        pData->feedbackFiltersL[c].alpha = 32767 - (val << 3);
        pData->feedbackFiltersR[c].alpha = 32767 - (val << 3);
    }
    fxProgramFreeVerb.parameters[2].rawValue = val;
}

static void fxProgramParameter2Display(void*data, char*res)
{
    FxProgram19StereoDataType* pData = (FxProgram19StereoDataType*)data;
    Int16ToChar(pData->feedbackFiltersL[0].alpha / 328, res);
    appendToString(res, "%");
}

static void fxProgramParam3Callback(uint16_t val, void*data) // Mix
{
    FxProgram19StereoDataType* pData = (FxProgram19StereoDataType*)data;
    pData->mix = (val << 3);
    fxProgramFreeVerb.parameters[0].rawValue = val;
}

static void fxProgramParam3Display(void*data, char*res)
{
    FxProgram19StereoDataType* pData = (FxProgram19StereoDataType*)data;
    Int16ToChar(pData->mix / 328, res);
    appendToString(res, "%");
}

static void fxProgramPresetVolumeCallback(uint16_t val, void*data)
{
    FxProgram19StereoDataType* pData = (FxProgram19StereoDataType*)data;
    pData->presetVolume.gain = val >> 2;
    fxProgramFreeVerb.parameters[4].rawValue = val;
}

static void fxProgramParamFreezeCallback(uint16_t val, void*data)
{
    ((FxProgram19StereoDataType*)data)->freeze = val;
}

static void fxProgramPresetVolumeDisplay(void*data, char*res)
{
    FxProgram19StereoDataType* pData = (FxProgram19StereoDataType*)data;
    decimalInt16ToChar(pData->presetVolume.gain * 39, res, 2);
    appendToString(res, "%");
}

// Base delay times (48kHz, prime-ish for richness)
static const int16_t combDelaysL[8] = {1695, 1760, 1623, 1548, 1390, 1476, 1293, 1215};
static const int16_t combDelaysR[8] = {1718, 1783, 1646, 1571, 1413, 1499, 1316, 1238};

FxProgram19StereoDataType fxProgram19data = {
    .delaysL = {
        {.delayInSamples = 1695, .delayBufferLength = 2048},
        {.delayInSamples = 1760, .delayBufferLength = 2048},
        {.delayInSamples = 1623, .delayBufferLength = 2048},
        {.delayInSamples = 1548, .delayBufferLength = 2048},
        {.delayInSamples = 1390, .delayBufferLength = 2048},
        {.delayInSamples = 1476, .delayBufferLength = 2048},
        {.delayInSamples = 1293, .delayBufferLength = 2048},
        {.delayInSamples = 1215, .delayBufferLength = 2048}
    },
    .delaysR = {
        {.delayInSamples = 1718, .delayBufferLength = 2048},
        {.delayInSamples = 1783, .delayBufferLength = 2048},
        {.delayInSamples = 1646, .delayBufferLength = 2048},
        {.delayInSamples = 1571, .delayBufferLength = 2048},
        {.delayInSamples = 1413, .delayBufferLength = 2048},
        {.delayInSamples = 1499, .delayBufferLength = 2048},
        {.delayInSamples = 1316, .delayBufferLength = 2048},
        {.delayInSamples = 1238, .delayBufferLength = 2048}
    },
    .feedbackFiltersL = {
        {.alpha = 6554}, {.alpha = 6554}, {.alpha = 6554}, {.alpha = 6554},
        {.alpha = 6554}, {.alpha = 6554}, {.alpha = 6554}, {.alpha = 6554}
    },
    .feedbackFiltersR = {
        {.alpha = 6554}, {.alpha = 6554}, {.alpha = 6554}, {.alpha = 6554},
        {.alpha = 6554}, {.alpha = 6554}, {.alpha = 6554}, {.alpha = 6554}
    },
    .allpassesL = {
        {.delayInSamples = 245, .coefficient = 16384, .bufferSize = 1023},
        {.delayInSamples = 605, .coefficient = 16384, .bufferSize = 1023},
        {.delayInSamples = 480, .coefficient = 16384, .bufferSize = 1023},
        {.delayInSamples = 371, .coefficient = 16384, .bufferSize = 1023}
    },
    .allpassesR = {
        {.delayInSamples = 268, .coefficient = 16384, .bufferSize = 1023},
        {.delayInSamples = 628, .coefficient = 16384, .bufferSize = 1023},
        {.delayInSamples = 503, .coefficient = 16384, .bufferSize = 1023},
        {.delayInSamples = 394, .coefficient = 16384, .bufferSize = 1023}
    },
    .presetVolume = {
        .gain = 0xff
    },
    .mix = 16384,
    .freeze = 0,
    .lfoPhase = 0,
    .lfoDepth = 15  // ±15 samples modulation (subtle but effective)
};

void fxProgram19Setup(void*data)
{
    int16_t* delayMemPtr = getDelayMemoryPointer();
    FxProgram19StereoDataType* pData = (FxProgram19StereoDataType*)data;
    
    // Clear all delay memory
    for (int i = 0; i < 50000; i++) delayMemPtr[i] = 0;
    
    // Initialize left channel comb filters
    for (int c = 0; c < 8; c++) {
        initDelay(&pData->delaysL[c], delayMemPtr + c * 2048, 2048);
        pData->delaysL[c].delayInSamples = combDelaysL[c];
        pData->delaysL[c].feedback = 22938;
        pData->baseDelaysL[c] = combDelaysL[c];
    }
    
    // Initialize right channel comb filters
    for (int c = 0; c < 8; c++) {
        initDelay(&pData->delaysR[c], delayMemPtr + (8 + c) * 2048, 2048);
        pData->delaysR[c].delayInSamples = combDelaysR[c];
        pData->delaysR[c].feedback = 22938;
        pData->baseDelaysR[c] = combDelaysR[c];
    }
    
    // Initialize allpass filters
    int16_t* ptr = delayMemPtr + 16 * 2048;
    for (int c = 0; c < 4; c++) {
        pData->allpassesL[c].delayLineIn = ptr;
        pData->allpassesL[c].delayLineOut = ptr + 1024;
        pData->allpassesL[c].bufferSize = 1023;
        pData->allpassesL[c].coefficient = 16384;
        pData->allpassesL[c].delayPtr = 0;
        pData->allpassesL[c].oldValues = 0;
        for (int k = 0; k < 2048; k++) ptr[k] = 0;
        ptr += 2048;
    }
    
    for (int c = 0; c < 4; c++) {
        pData->allpassesR[c].delayLineIn = ptr;
        pData->allpassesR[c].delayLineOut = ptr + 1024;
        pData->allpassesR[c].bufferSize = 1023;
        pData->allpassesR[c].coefficient = 16384;
        pData->allpassesR[c].delayPtr = 0;
        pData->allpassesR[c].oldValues = 0;
        for (int k = 0; k < 2048; k++) ptr[k] = 0;
        ptr += 2048;
    }
    
    pData->lfoPhase = 0;
    pData->lfoDepth = 15;
    pData->freeze = 0;
}

FxProgramType fxProgramFreeVerb = {
    .name = "FreeVerb",
    .parameters = {
        {
            .name = "Mix",
            .control = 0,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgramParam3Display,
            .setParameter = fxProgramParam3Callback
        },
        {
            .name = "Decay",
            .control = 1,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgramParameter1Display,
            .setParameter = fxProgramParameter1Callback
        },
        {
            .name = "Damping",
            .control = 2,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgramParameter2Display,
            .setParameter = fxProgramParameter2Callback
        },
        {
            .name = "Freeze",
            .control = 0xff,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = 0,
            .setParameter = fxProgramParamFreezeCallback
        },
        {
            .name = "Volume",
            .control = 0xff,
            .rawValue = 0x3ff,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgramPresetVolumeDisplay,
            .setParameter = fxProgramPresetVolumeCallback
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
    .processSample = 0,
    .processSampleStereo = fxProgram19processSampleStereo,
    .setup = fxProgram19Setup,
    .reset = 0,
    .nParameters = 5,
    .isStereo = 1,
    .data = (void*)&fxProgram19data
};
} // namespace Card_Flux
