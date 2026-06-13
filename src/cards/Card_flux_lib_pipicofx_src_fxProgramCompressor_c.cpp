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
/* stripped system include */
#include "pipicofx/fxPrograms.h"
#include "stringFunctions.h"
#include "romfunc.h"
#include "fastExpLog.h"

static int16_t fxProgramProcessSample(int16_t sampleIn,void*data)
{
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    switch(pData->compressorType)
    {
        case 0:
            sampleIn = compressorProcessSample(sampleIn,&pData->compressor);
            break;
        case 1:
            sampleIn = compressor2ProcessSample(sampleIn,&pData->compressor);
            break;
        case 2:
            sampleIn = compressor3ProcessSample(sampleIn,&pData->compressor);
            break;
    }
    sampleIn = gainStageProcessSample(sampleIn,&pData->presetVolume);
    return sampleIn;
}

static void fxProgramP1Callback(uint16_t val,void*data) 
{
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    pData->compressor.avgLowpass.alphaRising = (1 << 15) - 2 - (val >> 6);
    fxProgramCompressor.parameters[3].rawValue = val;
}

static void fxProgramP1Display(void*data,char*res)
{
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    float attackFloat;
    float t60;
    int32_t ival;
    int16_t i16val;
    attackFloat = int2float(pData->compressor.avgLowpass.alphaRising)/32767.0f;
    t60=-0.143911568f/fln(attackFloat); // -3*ln(10)/(ln(attack)*f_sample)*1000., result in t60 in ms
    ival = float2int(t60);
    i16val = (int16_t)ival;
    Int16ToChar(i16val,res);
    appendToString(res," ms");
}

static void fxProgramP2Callback(uint16_t val,void*data) 
{
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    pData->compressor.avgLowpass.alphaFalling = (1 << 15) - 2 - (val >> 6);
    fxProgramCompressor.parameters[4].rawValue = val;
}

static void fxProgramP2Display(void*data,char*res)
{
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    float releaseFloat;
    float t60;
    int32_t ival;
    int16_t i16val;
    releaseFloat = int2float(pData->compressor.avgLowpass.alphaFalling)/32767.0f;
    t60=-0.143911568f/fln(releaseFloat); // -3*ln(10)/(ln(release)*f_sample)*1000., result in t60 in ms
    ival = float2int(t60);
    i16val = (int16_t)ival;
    Int16ToChar(i16val,res);
    appendToString(res," ms");
}

static void fxProgramP3Callback(uint16_t val,void*data) 
{
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    uint16_t enumVal = (val >> 9) + 1;
    if (enumVal > 5)
    {
        enumVal=5;
    }
    fxProgramCompressor.parameters[1].rawValue = val;
    pData->compressor.gainFunction.gainReduction = enumVal;
}

static void fxProgramP3Display(void*data,char*res)
{
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    const char* dstrings[5];
    dstrings[0]="1:2            ";
    dstrings[1]="1:4            ";
    dstrings[2]="1:8            ";
    dstrings[3]="1:16           ";
    dstrings[4]="1:Inf          ";

    for(uint8_t c=0;c<16;c++)
    {
        *(res+c)=*(dstrings[pData->compressor.gainFunction.gainReduction-1] + c);
    }
}

static void fxProgramP4Callback(uint16_t val,void*data) 
{
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    pData->compressor.gainFunction.threshhold = val << 3;
    fxProgramCompressor.parameters[0].rawValue = val;
}

static void fxProgramP4Display(void*data,char*res)
{
    int16_t dbval;
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    dbval = asDb(pData->compressor.gainFunction.threshhold);
    decimalInt16ToChar(dbval,res,1);
    appendToString(res," dB");
}


static void fxProgramP5Callback(uint16_t val,void*data) 
{
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    pData->presetVolume.gain = val;
    fxProgramCompressor.parameters[2].rawValue = val;
}

static void fxProgramP5Display(void*data,char*res)
{
    FxProgram8DataType* pData = (FxProgram8DataType*)data;
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

static void fxProgramP6Callback(uint16_t val,void*data) 
{
    uint8_t intermVal;
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    intermVal = val >> 10;
    if (intermVal == 3)
    {
        intermVal = 2;
    }
    pData->compressorType = intermVal;
    fxProgramCompressor.parameters[5].rawValue = val;
}

static void fxProgramP6Display(void*data,char*res)
{
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    *res=0;
    switch (pData->compressorType)
    {
        case 0:
            appendToString(res,"Dirty");
            break;
        case 1:
            appendToString(res,"Snappy");
            break;
        case 2:
            appendToString(res,"Pumpy");
            break;
    }
}

FxProgram8DataType fxProgram8Data =
{
    .compressor = {
        .gainFunction = {
            .threshhold = 32767,
            .gainReduction = 1
        },
        .avgLowpass = {
            .oldVal = 0,
            .oldXVal = 0,
            .alphaRising = 32703,
            .alphaFalling = 32703
        },
        .currentAvg = 0
    },
    .presetVolume = {
        .gain = 0xff,
        .offset = 0
    }
};

static void fxProgramReset(void*data)
{
    FxProgram8DataType * pData=(FxProgram8DataType*)data;
    compressorReset(&pData->compressor);
} 

FxProgramType fxProgramCompressor = {
    .name = "Compressor",
    .parameters = {
        {
            .name = "Threshhold     ",
            .control = 0x0,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgramP4Display,
            .setParameter = &fxProgramP4Callback
        },
        {
            .name = "Ratio          ",
            .control = 1,
            .rawValue = 0,
            .increment = 512,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgramP3Display,
            .setParameter = &fxProgramP3Callback
        },
        {
            .name = "Makeup Gain    ",
            .control = 2,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgramP5Display,
            .setParameter = &fxProgramP5Callback
        },
        {
            .name = "Attack         ",
            .control = 0xff,
            .rawValue = 4095,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgramP1Display,
            .setParameter = &fxProgramP1Callback
        },
        {
            .name = "Release        ",
            .control = 0xff,
            .rawValue = 4095,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgramP2Display,
            .setParameter = &fxProgramP2Callback
        },
        {
            .name = "Flavor        ",
            .control = 0xff,
            .rawValue = 0,
            .increment = 1024,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgramP6Display,
            .setParameter = &fxProgramP6Callback
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
    .processSample = &fxProgramProcessSample,
    .setup = 0,
    .reset = &fxProgramReset,
    .nParameters = 6,
    .data = (void*)&fxProgram8Data
};

} // namespace Card_Flux
