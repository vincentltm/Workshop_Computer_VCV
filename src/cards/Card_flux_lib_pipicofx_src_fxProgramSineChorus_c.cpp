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

#ifndef FLOAT_AUDIO
static int16_t fxProgramprocessSample(int16_t sampleIn,void*data)
{
    FxProgram11DataType* pData = (FxProgram11DataType*)data;
    sampleIn >>= 1;
    sampleIn = gainStageProcessSample(sampleIn,&pData->presetVolume);
    return sineChorusInterpolatedProcessSample(sampleIn,&pData->sineChorus);
}
#else
static float fxProgramprocessSample(float sampleIn,void*data)
{
    FxProgram11DataType* pData = (FxProgram11DataType*)data;
    return sineChorusProcessSample(sampleIn,&pData->sineChorus);
}
#endif

// Stereo Data Type
typedef struct {
    SineChorusType sineChorusL;
    SineChorusType sineChorusR;
    GainStageDataType presetVolume;
} FxProgram11StereoDataType;

// Stereo Processing
static void fxProgramprocessSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void*data, volatile uint32_t*audioState)
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    
    int16_t sampleL = *inL >> 1;
    int16_t sampleR = *inR >> 1;
    
    sampleL = gainStageProcessSample(sampleL, &pData->presetVolume);
    sampleR = gainStageProcessSample(sampleR, &pData->presetVolume);
    
    *outL = sineChorusInterpolatedProcessSample(sampleL, &pData->sineChorusL);
    *outR = sineChorusInterpolatedProcessSample(sampleR, &pData->sineChorusR);
}

static void fxProgramParam1Callback(uint16_t val,void*data) // frequency
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    // map 0 - 4095 to 1 1000
    fxProgramSineChorus.parameters[1].rawValue = val;
    val = ((val*250) >> 10) + 1;
    sineChorusSetFrequency(val,&pData->sineChorusL);
    sineChorusSetFrequency(val,&pData->sineChorusR);
}

static void fxProgramParam1Display(void*data,char*res)
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    decimalInt16ToChar(pData->sineChorusL.frequency,res,2);
    appendToString(res," Hz");
}

static void fxProgramParam2Callback(uint16_t val,void*data) // depth
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    fxProgramSineChorus.parameters[2].rawValue = val;
    // map to 0 to 255
    val >>= 4;
    pData->sineChorusL.depth = (uint8_t)val;
    pData->sineChorusR.depth = (uint8_t)val;
}

static void fxProgramParam2Display(void*data,char*res)
{
    int16_t dVal;
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    dVal = pData->sineChorusL.depth; 
    Int16ToChar(dVal,res);
}

static void fxProgramParam3Callback(uint16_t val,void*data) // mix
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    pData->sineChorusL.mix = val << 3;
    pData->sineChorusR.mix = val << 3;
    fxProgramSineChorus.parameters[0].rawValue = val;
}

static void fxProgramParam3Display(void*data,char*res)
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    Int16ToChar(pData->sineChorusL.mix/328,res);
    appendToString(res,"%");
}

static void fxProgramParam4Callback(uint16_t val,void*data)
{
    fxProgramSineChorus.parameters[3].rawValue = val;
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    pData->sineChorusL.offset = 4 + (val >> 2);
    pData->sineChorusR.offset = 4 + (val >> 2);
}

static void fxProgramParam4Display(void*data,char*res)
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    uint16_t msValue;
    msValue = (uint16_t)(((uint32_t)pData->sineChorusL.offset*213) >> 10);
    decimalInt16ToChar((int16_t)msValue,res,1);
    appendToString(res, " ms");
}

static void fxProgramParam5Callback(uint16_t val,void*data)
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    pData->sineChorusL.feedback = val << 3;
    pData->sineChorusR.feedback = val << 3;
    fxProgramSineChorus.parameters[4].rawValue = val;
}

static void fxProgramParam5Display(void*data,char*res)
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    Int16ToChar(pData->sineChorusL.feedback/328,res);
    appendToString(res,"%");
}

static void fxProgramPresetVolumeCallback(uint16_t val,void*data)
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    pData->presetVolume.gain = val >> 2; // 0 to 1024
    fxProgramSineChorus.parameters[5].rawValue=val;
}

static void fxProgramPresetVolumeDisplay(void*data,char*res)
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
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

static void fxProgramSetupStereo(void*data)
{
    FxProgram11StereoDataType* pData = (FxProgram11StereoDataType*)data;
    
    // Init Left - manually
    int16_t* delayMem = getDelayMemoryPointer();
    pData->sineChorusL.delayBuffer = delayMem;
    for(uint16_t c=0;c<SINE_CHORUS_DELAY_SIZE;c++) *(delayMem + c)=0;
    pData->sineChorusL.lfoVal=0;
    pData->sineChorusL.lfoValOld=0;
    pData->sineChorusL.offset = 48;
    pData->sineChorusL.feedback=0;
    sineChorusSetFrequency(100,&pData->sineChorusL);
    pData->sineChorusL.lfoPhase = 0;
    
    // Init Right
    pData->sineChorusR.delayBuffer = delayMem + 4096; // Offset by 4096 samples
    for(uint16_t c=0;c<SINE_CHORUS_DELAY_SIZE;c++) *(delayMem + 4096 + c)=0;
    pData->sineChorusR.lfoVal=0;
    pData->sineChorusR.lfoValOld=0;
    pData->sineChorusR.offset = 48;
    pData->sineChorusR.feedback=0;
    sineChorusSetFrequency(100,&pData->sineChorusR);
    pData->sineChorusR.lfoPhase = 0x80000000; // 180 degree phase shift
}

FxProgram11StereoDataType fxProgram11data = {
    .sineChorusL = {
        .mix = 16384,
        .frequency = 500,
        .depth = 10,
        .feedback = 0,
        .offset = 49,
    },
    .sineChorusR = {
        .mix = 16384,
        .frequency = 500,
        .depth = 10,
        .feedback = 0,
        .offset = 49,
    },
    .presetVolume = {
        .gain=0xff,
        .offset=0
    }
};

FxProgramType fxProgramSineChorus = {
    .name = "Sine Chorus",
    .nParameters=6,
    .processSample = &fxProgramprocessSample,
    .processSampleStereo = &fxProgramprocessSampleStereo,
    .parameters = {
        {
            .name = "Blend         ",
            .control=0,
            .increment=1,
            .rawValue=0,
            .getParameterDisplay=&fxProgramParam3Display,
            .getParameterValue=0,
            .setParameter=&fxProgramParam3Callback
        },
        {
            .name = "Frequency      ",
            .control=1,
            .increment = 1,
            .rawValue=0,
            .getParameterDisplay=&fxProgramParam1Display,
            .getParameterValue=0,
            .setParameter=&fxProgramParam1Callback
        },
        {
            .name = "Depth          ",
            .control=2,
            .increment = 1,
            .rawValue=0,
            .getParameterDisplay=&fxProgramParam2Display,
            .getParameterValue=0,
            .setParameter=&fxProgramParam2Callback
        },
        {
            .name = "Offset         ",
            .control=0xFF,
            .increment=1,
            .rawValue=0,
            .getParameterDisplay=&fxProgramParam4Display,
            .getParameterValue=0,
            .setParameter=&fxProgramParam4Callback
        },
        {
            .name = "Feedback       ",
            .control=0xFF,
            .increment=1,
            .rawValue=0,
            .getParameterDisplay=&fxProgramParam5Display,
            .getParameterValue=0,
            .setParameter=&fxProgramParam5Callback
        },
        {
            .name="Volume",
            .control=0xff,
            .increment=1,
            .rawValue=0x3FF,
            .setParameter=fxProgramPresetVolumeCallback,
            .getParameterValue=0,
            .getParameterDisplay=fxProgramPresetVolumeDisplay
        }

    },
    .setup = &fxProgramSetupStereo,
    .reset = 0,
    .isStereo = 1,
    .data = (void*)&fxProgram11data
};

} // namespace Card_Flux
