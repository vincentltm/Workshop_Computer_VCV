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

static int16_t fxProgram5processSample(int16_t sampleIn,void*data)
{
    FxProgram5DataType* pData= (FxProgram5DataType*)data;
    sampleIn = bitCrusherProcessSample(sampleIn,&pData->bitcrusher);
    sampleIn = gainStageProcessSample(sampleIn,&pData->presetVolume);
    return sampleIn;
}

static void fxProgram5Param1Callback(uint16_t val,void*data) // set bit mask
{
    FxProgram5DataType* pData= (FxProgram5DataType*)data;
    uint16_t resolution;
    resolution = (4096 - val) >> 8;
    setBitMask((uint8_t)resolution,&pData->bitcrusher);
    fxProgramMonsterCrusher.parameters[0].rawValue = val;
}

static void fxProgram5Param1Display(void*data,char*res)
{
    uint16_t resolution;
    FxProgram5DataType *  fData = (FxProgram5DataType*)data;
    
    resolution = fData->bitcrusher.bitmask;
    uint8_t nbits = (4096-fxProgramMonsterCrusher.parameters[0].rawValue) >> 8;
    UInt8ToChar(nbits,res);
    appendToString(res,"-bits");
}

static void fxProgramPresetVolumeCallback(uint16_t val,void*data)
{
    FxProgram5DataType* pData = (FxProgram5DataType*)data;
    pData->presetVolume.gain = val >> 2; // 0 to 1024
    fxProgramMonsterCrusher.parameters[1].rawValue=val;
}

static void fxProgramPresetVolumeDisplay(void*data,char*res)
{
    FxProgram5DataType* pData = (FxProgram5DataType*)data;
    uint16_t dVal;
    dVal = pData->presetVolume.gain*39; // percent with two decimal points
    decimalInt16ToChar((int16_t)dVal,res,2);
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

void fxProgram5Setup(void*data)
{

}

FxProgram5DataType fxProgram5data = {
    .bitcrusher = {
        .bitmask = 0x8000
    },
    .presetVolume = {
        .gain = 0xFF,
        .offset = 0
    }
};

FxProgramType fxProgramMonsterCrusher = {
    .name = "Monstercrusher",
    .parameters = {
        {
            .name = "Resolution",
            .control = 0,
            .rawValue = 0,
            .increment = 256,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgram5Param1Display,
            .setParameter = &fxProgram5Param1Callback
        },
        {
            .name = "Volume",
            .control = 0xff,
            .rawValue = 0x3FF,
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
    .processSample = &fxProgram5processSample,
    .setup = &fxProgram5Setup,
    .reset = 0,
    .nParameters = 2,
    .data = (void*)&fxProgram5data
};
} // namespace Card_Flux
