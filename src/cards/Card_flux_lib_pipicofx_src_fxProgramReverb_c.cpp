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

// Stereo data type
typedef struct {
    ReverbType reverbL;
    ReverbType reverbR;
    int16_t reverbTime;
    GainStageDataType presetVolume;
    int16_t freeze; // Added freeze member
} FxProgram10StereoDataType;

// Mono processing (legacy)
static int16_t fxProgram10processSample(int16_t sampleIn,void*data)
{
    FxProgram10DataType* pData= (FxProgram10DataType*)data;
    sampleIn = reverbProcessSample(sampleIn,&pData->reverb);
    sampleIn = gainStageProcessSample(sampleIn,&pData->presetVolume);
    return sampleIn;
}

// Stereo processing
static void fxProgram10processSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void*data, volatile uint32_t*audioState)
{
    FxProgram10StereoDataType* pData = (FxProgram10StereoDataType*)data;
    uint8_t frozen = (pData->freeze > 2048); // Check if frozen
    
    // Mono Sum Drive
    int16_t monoInput;
    if (frozen) { // Mute input if frozen
        monoInput = 0;
    } else {
        monoInput = (*inL + *inR) >> 1;
    }
    
    // Backup feedback values and set to 32767 if frozen
    int16_t backupL[4], backupR[4];
    if (frozen) {
        for(int i=0; i<4; i++) {
            backupL[i] = pData->reverbL.feedbackValues[i];
            backupR[i] = pData->reverbR.feedbackValues[i];
            pData->reverbL.feedbackValues[i] = 32700; // Almost 1.0 for stability
            pData->reverbR.feedbackValues[i] = 32700;
        }
    }

    // Process Wet Signal (Mono In -> Stereo Out)
    int16_t wetL = reverbProcessSampleWet(monoInput, &pData->reverbL);
    int16_t wetR = reverbProcessSampleWet(monoInput, &pData->reverbR);
    
    // Restore feedback values if frozen
    if (frozen) {
        for(int i=0; i<4; i++) {
            pData->reverbL.feedbackValues[i] = backupL[i];
            pData->reverbR.feedbackValues[i] = backupR[i];
        }
    }

    // Reconstruct Output: Dry Stereo + Wet Stereo
    // Note: Reverb mix is stored in the reverb struct, but we need to apply it manually 
    // since we bypassed the internal mixing.
    // However, pData->reverbL.mix might not be directly accessible or used this way in standard reverb?
    // Let's assume the mix parameter behaves like a wet/dry balance.
    // Actually reverbProcessSampleWet returns PURE wet.
    // Standard reverb struct has a 'mix' field.
    
    int16_t mix;
    // We can't easily access pData->reverbL.mix from here if it's private or not updated? 
    // But wait, fxProgramParam sets it? "setReverbTime" doesn't set mix.
    // The mix is not part of ReverbType in the struct definition in header?
    // Header says: int16_t mix;
    mix = pData->reverbL.mix;
    
    *outL = (((32767 - mix) * (*inL)) >> 15) + ((mix * wetL) >> 15);
    *outR = (((32767 - mix) * (*inR)) >> 15) + ((mix * wetR) >> 15);
    
    // Apply volume
    *outL = gainStageProcessSample(*outL, &pData->presetVolume);
    *outR = gainStageProcessSample(*outR, &pData->presetVolume);
}

static void fxProgramParam1Callback(uint16_t val,void*data) // reverb time
{
    FxProgram10StereoDataType* pData= (FxProgram10StereoDataType*)data;
    pData->reverbTime = (((uint32_t)val*1900)>>12) + 100;
    setReverbTime(pData->reverbTime,&pData->reverbL);
    setReverbTime(pData->reverbTime,&pData->reverbR);
    fxProgramReverb.parameters[1].rawValue = val;
}

static void fxProgramParam1Display(void*data,char*res)
{
    FxProgram10StereoDataType* pData = (FxProgram10StereoDataType*)data;
    Int16ToChar(pData->reverbTime,res);
    for (uint8_t c=0;c<PARAMETER_NAME_MAXLEN-1;c++)
    {
        if(*(res+c)==0)
        {
            *(res+c)='m';
            *(res+c+1)='s';
            *(res+c+2)=(char)0;
            break;
        }
    }
}

static void fxProgramParam2Callback(uint16_t val,void*data) // Mix
{
    FxProgram10StereoDataType* pData= (FxProgram10StereoDataType*)data;
    int16_t wVal = val << 3;
    pData->reverbL.mix = wVal;
    pData->reverbR.mix = wVal;
    fxProgramReverb.parameters[0].rawValue = val;
}

static void fxProgramParam2Display(void*data,char*res)
{
    FxProgram10StereoDataType* pData= (FxProgram10StereoDataType*)data;
    int16_t mixpercent = (int16_t)(pData->reverbL.mix/328);
    Int16ToChar(mixpercent,res);
    appendToString(res,"%");

}

// New Freeze callback
static void fxProgramParamFreezeCallback(uint16_t val, void* data) {
    FxProgram10StereoDataType* pData = (FxProgram10StereoDataType*)data;
    pData->freeze = val;
    fxProgramReverb.parameters[3].rawValue = val; 
}

static void fxProgramParam3Callback(uint16_t val,void*data) // Parameter
{
    FxProgram10StereoDataType* pData= (FxProgram10StereoDataType*)data;
    uint8_t newParamNr = (val >> 10);
    if (newParamNr >= 4) newParamNr = 3;
    
    if (newParamNr != pData->reverbL.paramNr) {
        pData->reverbL.paramNr = newParamNr;
        pData->reverbR.paramNr = newParamNr;
        int16_t* delayMem = getDelayMemoryPointer();
        // Clear 48k block to stop old signal
        for(int i=0; i<48000; i++) delayMem[i] = 0;
        initReverbExtended(&pData->reverbL, pData->reverbTime, delayMem, 4096);
        initReverbExtended(&pData->reverbR, pData->reverbTime, delayMem + 24000, 4096); 
    }
    fxProgramReverb.parameters[2].rawValue = val;
}

static void fxProgramParam3Display(void*data,char*res)
{
    FxProgram10StereoDataType* pData = (FxProgram10StereoDataType*)data;
    *res=0;
    appendToString(res,getReverbParameterSetName(&pData->reverbL));
}


static void fxProgramPresetVolumeCallback(uint16_t val,void*data)
{
    FxProgram10StereoDataType* pData = (FxProgram10StereoDataType*)data;
    pData->presetVolume.gain = val >> 2;
    fxProgramReverb.parameters[4].rawValue=val; 
}

static void fxProgramPresetVolumeDisplay(void*data,char*res)
{
    FxProgram10StereoDataType* pData = (FxProgram10StereoDataType*)data;
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

static void fxProgram10SetupStereo(void*data)
{
    FxProgram10StereoDataType* pData= (FxProgram10StereoDataType*)data;
    // Standard Reverb: Uses initReverb which allocates 16k chunks.
    // L: 0..16k.
    // R: 16k..32k. (Offset by 32768 to vary sound?)
    // Actually, original initReverb grabbed memory sequentially.
    // But initReverb in reverb.c RESETs the getDelayMemoryPointer? No.
    // getDelayMemoryPointer returns base.
    // `initReverb` uses fixed offsets. L and R would collide if we don't offset manually!
    
    int16_t* delayMem = getDelayMemoryPointer();
    // Clear 48k block (L and R combined)
    for(int i=0; i<48000; i++) delayMem[i] = 0;
    
    initReverbExtended(&pData->reverbL, pData->reverbTime, delayMem, 4096);
    initReverbExtended(&pData->reverbR, pData->reverbTime, delayMem + 24000, 4096); 
    
    pData->reverbL.paramNr = 0;
    pData->reverbR.paramNr = 0;
    pData->freeze = 0; 
}

FxProgram10StereoDataType fxProgram10data=
{
    .reverbTime=300,
    .reverbL.paramNr=0,
    .reverbL.mix = 16384,
    .reverbR.paramNr=0,
    .reverbR.mix = 16384,
    .presetVolume = {
        .gain = 0xff,
        .offset = 0
    },
    .freeze = 0 // Initialize freeze state in data struct
};

FxProgramType fxProgramReverb = {
    .name = "Reverb",
    .nParameters=5, // Updated to 5 parameters
    .parameters = {
        {
            .name="Mix",
            .control=0,
            .increment=1,
            .rawValue=0,
            .setParameter=fxProgramParam2Callback,
            .getParameterValue=0,
            .getParameterDisplay=fxProgramParam2Display
        },
        {
            .name="Time",
            .control=1,
            .increment=1,
            .rawValue=0,
            .setParameter=fxProgramParam1Callback,
            .getParameterValue=0,
            .getParameterDisplay=fxProgramParam1Display
        },
        {
            .name="Param",
            .control=2,
            .increment=1,
            .rawValue=0,
            .setParameter=fxProgramParam3Callback,
            .getParameterValue=0,
            .getParameterDisplay=fxProgramParam3Display
        },
        {
            .name="Freeze",
            .control=0xff,
            .increment=1,
            .rawValue=0,
            .setParameter=fxProgramParamFreezeCallback,
            .getParameterValue=0,
            .getParameterDisplay=0
        },
        {
            .name="Volume",
            .control=0xff,
            .increment=1,
            .rawValue=0x3ff,
            .setParameter=fxProgramPresetVolumeCallback,
            .getParameterValue=0,
            .getParameterDisplay=fxProgramPresetVolumeDisplay
        },
    },
    .processSample = 0,
    .processSampleStereo = &fxProgram10processSampleStereo,
    .setup = &fxProgram10SetupStereo,
    .reset = 0,
    .isStereo = 1,
    .data = (void*)&fxProgram10data
};
} // namespace Card_Flux
