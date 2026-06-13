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
    .nParameters=2,
    .processSample = &fxProgram3processSample,
    .processSampleStereo = &fxProgram3processSampleStereo,
    .isStereo = 1,
    .setup = &fxProgram3Setup,
    .reset = 0,
    .data = (void*)&fxProgram3data,
    .parameters = {
        {
            .name="Passthrough",
            .control=0x0, // Knob 1 (Main)
            .increment=1,
            .rawValue=4095,
            .setParameter=0, // Disabled to prevent interference with mode selection
            .getParameterValue=0,
            .getParameterDisplay=fxProgramParam1Display
        },
        {
            .name="Denoise Mix",
            .control=0x1, // Knob 2 (X) - Handled globally in main.cpp
            .increment=1,
            .rawValue=4095,
            .setParameter=0, // No local callback needed, main.cpp handles it.
            .getParameterValue=0, 
            .getParameterDisplay=0 
        }
    }
};
