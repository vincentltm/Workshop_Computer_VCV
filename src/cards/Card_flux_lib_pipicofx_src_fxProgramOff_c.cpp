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

// Bypass / Passthrough Program
// Knob 1 (Main): Master Volume for debugging signal chain (0 to 100%)

int16_t fxProgram3processSample(int16_t sampleIn,void*data)
{
    // Volume multiplication removed to prevent interference with mode selection
    return sampleIn;
}

static void fxProgramParam1Callback(uint16_t val,void*data)
{
    FxProgram3DataType* pData = (FxProgram3DataType*)data;
    // Map 0-4095 to 0-256 (Unity)
    pData->presetVolume.gain = val >> 4; 
    fxProgramOff.parameters[0].rawValue=val;
}

static void fxProgramParam1Display(void*data,char*res)
{
    FxProgram3DataType* pData = (FxProgram3DataType*)data;
    // Display perentage 0-100%
    int16_t pct = (pData->presetVolume.gain * 100) >> 8;
    Int16ToChar(pct, res);
    appendToString(res,"%");
}

void fxProgram3Setup(void*data)
{}

FxProgram3DataType fxProgram3data = {
    .presetVolume = {
        .gain = 256, // Default to Full Volume
        .offset = 0
    }
};

static void fxProgram3processSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void* data, volatile uint32_t* audioState)
{
    *outL = *inL;
    *outR = *inR;
}

FxProgramType fxProgramOff = {
    .name = "Bypass",
    .parameters = {
        {
            .name = "Passthrough",
            .control = 0x0,
            .rawValue = 4095,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgramParam1Display,
            .setParameter = 0
        },
        {
            .name = "Denoise Mix",
            .control = 0x1,
            .rawValue = 4095,
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
    .processSample = &fxProgram3processSample,
    .processSampleStereo = &fxProgram3processSampleStereo,
    .setup = &fxProgram3Setup,
    .reset = 0,
    .nParameters = 2,
    .isStereo = 1,
    .data = (void*)&fxProgram3data
};

} // namespace Card_Flux
