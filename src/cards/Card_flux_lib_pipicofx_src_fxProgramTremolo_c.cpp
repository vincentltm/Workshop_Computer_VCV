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
#include "audio/audiotools.h"
#include "audio/phaseDistortedSineSquare.h"
#include "stringFunctions.h"
#include "romfunc.h"

int16_t fxProgram18processSample(int16_t sampleIn,void*data)
{
    FxProgram18DataType* pData= (FxProgram18DataType*)data;
    sampleIn = gainStageProcessSample(sampleIn,&pData->presetVolume);
    return tremoloProcessSample(sampleIn,&pData->tremolo);
}

static void fxProgramParam1Callback(uint16_t val,void*data) // Rate
{
    FxProgram18DataType* pData= (FxProgram18DataType*)data;
    float rateVal = ((float)val)/200.f + 0.2f;
    phaseDistortedSineSquareSetFrequency(rateVal,&pData->tremolo.modulator);
    fxProgramTremolo.parameters[0].rawValue = val;
}

static void fxProgramParam1Display(void*data,char*res)
{
    FxProgram18DataType* pData= (FxProgram18DataType*)data;
    float f = phaseDistortedSineSquareGetFrequency(&pData->tremolo.modulator)*8.0f;
    uint16_t intf = (uint16_t)float2int(f);
    fixedPointInt16ToChar(res,intf,3);
    appendToString(res," Hz");

}

static void fxProgramParam2Callback(uint16_t val,void*data) // Depth
{
    FxProgram18DataType* pData= (FxProgram18DataType*)data;
    pData->tremolo.depth = val << 3;
    fxProgramTremolo.parameters[1].rawValue = val;
}

static void fxProgramParam2Display(void*data,char*res)
{
    FxProgram18DataType* pData= (FxProgram18DataType*)data;
    int16_t depth = pData->tremolo.depth;
    Int16ToChar(depth/328,res);
    appendToString(res,"%");
}


static void fxProgramParam3Callback(uint16_t val,void*data) // Shape
{
    FxProgram18DataType* pData= (FxProgram18DataType*)data;
    pData->tremolo.modulator.squareRatio = val >> 4;
    fxProgramTremolo.parameters[2].rawValue = val;
}

static void fxProgramParam3Display(void*data,char*res)
{
    FxProgram18DataType* pData= (FxProgram18DataType*)data;
    Int16ToChar(pData->tremolo.modulator.squareRatio,res);
}

static void fxProgramParam4Callback(uint16_t val,void*data) // Pulse Width
{
    FxProgram18DataType* pData= (FxProgram18DataType*)data;
    pData->tremolo.modulator.pulseWidth = ((int16_t)val - 2048) << 4;
    phaseDistortedSineSquarePulseWidth(pData->tremolo.modulator.pulseWidth,&pData->tremolo.modulator);
    fxProgramTremolo.parameters[3].rawValue = val;
}

static void fxProgramParam4Display(void*data,char*res)
{
    FxProgram18DataType* pData= (FxProgram18DataType*)data;
    Int16ToChar(pData->tremolo.modulator.pulseWidth,res);
}

static void fxProgramPresetVolumeCallback(uint16_t val,void*data)
{
    FxProgram18DataType* pData = (FxProgram18DataType*)data;
    pData->presetVolume.gain = val >> 2; // 0 to 1024
    fxProgramTremolo.parameters[4].rawValue=val;
}

static void fxProgramPresetVolumeDisplay(void*data,char*res)
{
    FxProgram18DataType* pData = (FxProgram18DataType*)data;
    int16_t dVal;
    dVal = pData->presetVolume.gain*39; // percent with two decimal points
    decimalInt16ToChar(dVal,res,2);
    for (uint8_t c=0;c<PARAMETER_NAME_MAXLEN-1;c++)
    {
        if(*(res+c)==0)
        {
            *(res+c)='%';
            *(res+c+1)=(char)0;
            break;
        }
    }
}

FxProgram18DataType fxProgram18data=
{
    .tremolo = {
        .depth = 24000,
        .currentLfoVal = 0,
        .modulator = {
            .squareRatio = 0,
            .phaseIncrement = 131,
            .phaseIncrementCorrection1 = 0,
            .currentPhase = 0,
            .pulseWidth = 0
        }
    },
    .presetVolume = {
        .gain = 0xff,
        .offset = 0
    }
};

void fxProgram18Setup(void*data)
{
    FxProgram18DataType * pData = (FxProgram18DataType*)data;
    // CRITICAL: Clear memory to avoid uninitialized noise
    char* b = (char*)pData;
    for(int i=0; i<sizeof(FxProgram18DataType); i++) b[i] = 0;
    
    initTremolo(&pData->tremolo);
    
    // RESTORE DEFAULTS
    pData->presetVolume.gain = 0x100; // Unity Gain approx
    pData->tremolo.depth = 24000;
    // Set sane default speed (approx 3Hz)
    float rateVal = 0.5f; 
    phaseDistortedSineSquareSetFrequency(rateVal, &pData->tremolo.modulator);
}

FxProgramType fxProgramTremolo = {
    .name = "Tremolo",
    .parameters = {
        {
            .name = "Depth",
            .control = 0,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgramParam2Display,
            .setParameter = &fxProgramParam2Callback
        },
        {
            .name = "Rate",
            .control = 1,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgramParam1Display,
            .setParameter = &fxProgramParam1Callback
        },
        {
            .name = "Shape",
            .control = 2,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgramParam3Display,
            .setParameter = &fxProgramParam3Callback
        },
        {
            .name = "PulseWidth",
            .control = 0xFF,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgramParam4Display,
            .setParameter = &fxProgramParam4Callback
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
    .processSample = &fxProgram18processSample,
    .setup = &fxProgram18Setup,
    .reset = 0,
    .nParameters = 5,
    .data = (void*)&fxProgram18data
};

} // namespace Card_Flux
