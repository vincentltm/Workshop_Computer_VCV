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
#include "romfunc.h"

// Stereo data type
typedef struct {
    int16_t mixL;
    int16_t mixR;
    Reverb3Type reverbL;
    Reverb3Type reverbR;
    GainStageDataType presetVolume;
    int16_t freeze;
} FxProgram13StereoDataType;

// Stereo processing
static void fxProgram13processSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void*data, volatile uint32_t*audioState)
{
    FxProgram13StereoDataType* pData = (FxProgram13StereoDataType*)data;
    uint8_t frozen = (pData->freeze > 2048);
    int16_t monoInput = frozen ? 0 : ((*inL + *inR) >> 1);
    int16_t savedFBL = pData->reverbL.delay.feedback;
    int16_t savedFBR = pData->reverbR.delay.feedback;
    if (frozen) { pData->reverbL.delay.feedback = 32767; pData->reverbR.delay.feedback = 32767; }
    int16_t reverbL = reverb3processSample(monoInput, &pData->reverbL);
    int16_t reverbR = reverb3processSample(monoInput, &pData->reverbR);
    if (frozen) { pData->reverbL.delay.feedback = savedFBL; pData->reverbR.delay.feedback = savedFBR; }
    *outL = (((0x7FFF - pData->mixL) * (*inL)) >> 15) + ((pData->mixL * reverbL) >> 17);
    *outR = (((0x7FFF - pData->mixR) * (*inR)) >> 15) + ((pData->mixR * reverbR) >> 17);
    *outL = gainStageProcessSample(*outL, &pData->presetVolume);
    *outR = gainStageProcessSample(*outR, &pData->presetVolume);
}

static void fxProgramParam1Callback(uint16_t val,void*data) {
    FxProgram13StereoDataType* pData= (FxProgram13StereoDataType*)data;
    pData->mixL = (val << 3); pData->mixR = (val << 3);
    fxProgramReverb3.parameters[0].rawValue = val;
}
static void fxProgramParam1Display(void*data,char*res) {
    Int16ToChar(((FxProgram13StereoDataType*)data)->mixL/328,res);
    appendToString(res,"%");
}
static void fxProgramParam2Callback(uint16_t val,void*data) {
    FxProgram13StereoDataType* pData= (FxProgram13StereoDataType*)data;
    int16_t intermVal = val << 3; if (intermVal > 0x7FFD) intermVal = 0x7FFD;
    pData->reverbL.delay.feedback = intermVal; pData->reverbR.delay.feedback = intermVal;
    fxProgramReverb3.parameters[1].rawValue = val;
}
static void fxProgramParam2Display(void*data,char*res) {
    FxProgram13StereoDataType* pData = (FxProgram13StereoDataType*)data;
    float ffbk = int2float(pData->reverbL.delay.feedback)/32767.0f;
    int16_t t60 = (ffbk < 0.0000305) ? 0 : (int16_t)float2int(-589.03004f/fln(ffbk));
    Int16ToChar(t60,res); appendToString(res," ms");
}
static void fxProgramParamFreezeCallback(uint16_t val, void* data) { ((FxProgram13StereoDataType*)data)->freeze = val; }
static void fxProgramPresetVolumeCallback(uint16_t val,void*data) {
    ((FxProgram13StereoDataType*)data)->presetVolume.gain = val >> 2;
    fxProgramReverb3.parameters[3].rawValue=val;
}
static void fxProgramPresetVolumeDisplay(void*data,char*res) {
    decimalInt16ToChar(((FxProgram13StereoDataType*)data)->presetVolume.gain*39,res,2);
    appendToString(res,"%");
}

static void fxProgram13SetupStereo(void*data) {
    FxProgram13StereoDataType* pData = (FxProgram13StereoDataType*)data;
    // Each channel needs ~37k samples (4 diffusers × 4 delays × 2k + 4k delay)
    // L: offset 0, R: offset 40000 to avoid overlap
    initReverb3Extended(&pData->reverbL, 0);
    initReverb3Extended(&pData->reverbR, 40000);
    pData->freeze = 0;
}

FxProgram13StereoDataType fxProgram13data= {
    .mixL = 16384,
    .mixR = 16384,
    .presetVolume = { .gain =0xff, .offset = 0 },
    .freeze = 0
};

FxProgramType fxProgramReverb3 = {
    .name = "HadamardReverb",
    .parameters = {
        {
            .name = "Mix",
            .control = 0,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgramParam1Display,
            .setParameter = fxProgramParam1Callback
        },
        {
            .name = "Decay",
            .control = 1,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgramParam2Display,
            .setParameter = fxProgramParam2Callback
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
    .processSampleStereo = &fxProgram13processSampleStereo,
    .setup = &fxProgram13SetupStereo,
    .nParameters = 4,
    .isStereo = 1,
    .data = (void*)&fxProgram13data
};
} // namespace Card_Flux
