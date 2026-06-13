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
#include "audio/gainstage.h"
#include "audio/audiotools.h"

static int16_t fxProgram2processSample(int16_t sampleIn,void*data)
{
    FxProgram2DataType* pData = (FxProgram2DataType*)data;
    sampleIn >>= 1;
    sampleIn = simpleChorusProcessSample(sampleIn,&pData->chorusData);
    return gainStageProcessSample(sampleIn,&pData->presetVolume);
}

// Stereo Data Type
typedef struct {
    SimpleChorusType chorusL;
    SimpleChorusType chorusR;
    GainStageDataType presetVolume;
} FxProgram2StereoDataType;

// Stereo Processing
static void fxProgram2processSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void*data, volatile uint32_t*audioState)
{
    FxProgram2StereoDataType* pData = (FxProgram2StereoDataType*)data;
    
    // Process L and R independently (Result is wide stereo due to LFO phase offset)
    int16_t sampleL = *inL >> 1;
    int16_t sampleR = *inR >> 1;
    
    *outL = simpleChorusProcessSample(sampleL, &pData->chorusL);
    *outR = simpleChorusProcessSample(sampleR, &pData->chorusR);
    
    *outL = gainStageProcessSample(*outL, &pData->presetVolume);
    *outR = gainStageProcessSample(*outR, &pData->presetVolume);
}

static void fxProgram2Param1Callback(uint16_t val,void*data) // frequency
{
    FxProgram2StereoDataType* pData = (FxProgram2StereoDataType*)data;
    // map 0 - 4095 to 1 1000
    val = ((val*250) >> 10) + 1;
    simpleChorusSetFrequency(val,&pData->chorusL);
    simpleChorusSetFrequency(val,&pData->chorusR);
    fxProgramVibChorus.parameters[0].rawValue = val;
}

static void fxProgram2Param1Display(void*data,char*res)
{
    FxProgram2StereoDataType* pData = (FxProgram2StereoDataType*)data;
    decimalInt16ToChar(pData->chorusL.frequency,res,2);
    appendToString(res," Hz");
}

static void fxProgram2Param2Callback(uint16_t val,void*data) // depth
{
    FxProgram2StereoDataType* pData = (FxProgram2StereoDataType*)data;
    // map to 0 to 255
    val >>= 4;
    pData->chorusL.depth = (uint8_t)val;
    pData->chorusR.depth = (uint8_t)val;
    fxProgramVibChorus.parameters[1].rawValue = val;
}

static void fxProgram2Param2Display(void*data,char*res)
{
    int16_t dVal;
    FxProgram2StereoDataType* fData=(FxProgram2StereoDataType*)data;
    dVal = (fData->chorusL.depth*100) >> 8;
    Int16ToChar(dVal,res);
    appendToString(res,"%");
}

static void fxProgram2Param3Callback(uint16_t val,void*data) // mix
{
    FxProgram2StereoDataType* pData = (FxProgram2StereoDataType*)data;
    // map to 0 to 255
    val >>= 4;
    pData->chorusL.mix = (uint8_t)val;
    pData->chorusR.mix = (uint8_t)val;
    fxProgramVibChorus.parameters[2].rawValue = val;
}

static void fxProgram2Param3Display(void*data,char*res)
{
    int16_t dVal;
    FxProgram2StereoDataType* fData = (FxProgram2StereoDataType*)data;
    dVal = (fData->chorusL.mix*100) >> 8;
    Int16ToChar(dVal,res);
    appendToString(res,"%");
}

static void fxProgramPresetVolumeCallback(uint16_t val,void*data)
{
    FxProgram2StereoDataType* pData = (FxProgram2StereoDataType*)data;
    pData->presetVolume.gain = val >> 2; // 0 to 1024
    fxProgramVibChorus.parameters[3].rawValue=val;
}

static void fxProgramPresetVolumeDisplay(void*data,char*res)
{
    FxProgram2StereoDataType* pData = (FxProgram2StereoDataType*)data;
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


static void fxProgram2SetupStereo(void*data)
{
    FxProgram2StereoDataType* pData = (FxProgram2StereoDataType*)data;
    initSimpleChorus(&pData->chorusL);
    
    // Setup Right Channel
    initSimpleChorus(&pData->chorusR);
    // Offset Memory (Buffer size is 2048)
    pData->chorusR.delayBuffer = (int16_t*)(getDelayMemoryPointer()) + 2048; 
    
    // Invert LFO Phase for Right Channel (Ascending vs Descending)
    pData->chorusR.lfoQuadrant = 1; 
    pData->chorusR.lfoVal = 0;
}

FxProgram2StereoDataType fxProgram2data = {
    .chorusL = {
        .mix = 128,
        .frequency = 500,
        .depth = 10
    },
    .chorusR = {
        .mix = 128,
        .frequency = 500,
        .depth = 10
    },
    .presetVolume = {
        .gain=0xFF,
        .offset=0
    }
};

FxProgramType fxProgramVibChorus = {
    .name = "Vibrato/Chorus",
    .parameters = {
        {
            .name = "Mix            ",
            .control = 0,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgram2Param3Display,
            .setParameter = &fxProgram2Param3Callback
        },
        {
            .name = "Frequency      ",
            .control = 1,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgram2Param1Display,
            .setParameter = &fxProgram2Param1Callback
        },
        {
            .name = "Depth          ",
            .control = 2,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = &fxProgram2Param2Display,
            .setParameter = &fxProgram2Param2Callback
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
        }
    },
    .processSample = &fxProgram2processSample,
    .processSampleStereo = &fxProgram2processSampleStereo,
    .setup = &fxProgram2SetupStereo,
    .reset = 0,
    .nParameters = 4,
    .isStereo = 1,
    .data = (void*)&fxProgram2data
};

} // namespace Card_Flux
