#include "pipicofx/fxPrograms.h"
#include "stringFunctions.h"

// Stereo data type
typedef struct {
    Reverb2Type reverbL;
    Reverb2Type reverbR;
    GainStageDataType presetVolume;
    int16_t freeze;
} FxProgram12StereoDataType;

// Stereo processing
static void fxProgram12processSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void*data, volatile uint32_t*audioState)
{
    FxProgram12StereoDataType* pData = (FxProgram12StereoDataType*)data;
    uint8_t frozen = (pData->freeze > 2048);
    int16_t monoInput = frozen ? 0 : ((*inL + *inR) >> 1);
    int16_t savedDecayL = pData->reverbL.decay;
    int16_t savedDecayR = pData->reverbR.decay;
    if (frozen) { pData->reverbL.decay = 32767; pData->reverbR.decay = 32767; }
    int16_t wetL = reverb2ProcessSampleWet(monoInput, &pData->reverbL);
    int16_t wetR = reverb2ProcessSampleWet(monoInput, &pData->reverbR);
    if (frozen) { pData->reverbL.decay = savedDecayL; pData->reverbR.decay = savedDecayR; }
    int16_t mix = pData->reverbL.mix;
    *outL = (((32767 - mix) * (*inL)) >> 15) + ((mix * wetL) >> 16);
    *outR = (((32767 - mix) * (*inR)) >> 15) + ((mix * wetR) >> 16);
    *outL = gainStageProcessSample(*outL, &pData->presetVolume);
    *outR = gainStageProcessSample(*outR, &pData->presetVolume);
}

static void fxProgramParam1Callback(uint16_t val,void*data) {
    FxProgram12StereoDataType* pData= (FxProgram12StereoDataType*)data;
    pData->reverbL.mix = (val << 3); pData->reverbR.mix = (val << 3);
    fxProgramReverb2.parameters[0].rawValue = val;
}
static void fxProgramParam1Display(void*data,char*res) {
    Int16ToChar(((FxProgram12StereoDataType*)data)->reverbL.mix/328,res);
    appendToString(res,"%");
}
static void fxProgramParam2Callback(uint16_t val,void*data) {
    FxProgram12StereoDataType* pData= (FxProgram12StereoDataType*)data;
    pData->reverbL.decay = (val << 3); pData->reverbR.decay = (val << 3);
    fxProgramReverb2.parameters[1].rawValue = val;
}
static void fxProgramParam2Display(void*data,char*res) { Int16ToChar(((FxProgram12StereoDataType*)data)->reverbL.decay,res); }
static void fxProgramParamFreezeCallback(uint16_t val, void* data) { ((FxProgram12StereoDataType*)data)->freeze = val; }
static void fxProgramPresetVolumeCallback(uint16_t val,void*data) {
    ((FxProgram12StereoDataType*)data)->presetVolume.gain = val >> 2;
    fxProgramReverb2.parameters[3].rawValue=val;
}
static void fxProgramPresetVolumeDisplay(void*data,char*res) {
    decimalInt16ToChar(((FxProgram12StereoDataType*)data)->presetVolume.gain*39,res,2);
    appendToString(res,"%");
}

static void fxProgram12SetupStereo(void*data) {
    FxProgram12StereoDataType* pData = (FxProgram12StereoDataType*)data;
    // Each channel needs ~24k samples (8k allpass + 16k delays)
    // L: offset 0, R: offset 25000 to avoid overlap
    initReverb2Extended(&pData->reverbL, 0);
    initReverb2Extended(&pData->reverbR, 25000);
    pData->freeze = 0;
}

FxProgram12StereoDataType fxProgram12data= { .reverbL.mix=16384, .reverbR.mix=16384, .presetVolume = { .gain =0xff, .offset=0 }, .freeze = 0 };

FxProgramType fxProgramReverb2 = {
    .name = "AllpassReverb", .nParameters=4,
    .parameters = {
        { .name="Mix", .control=0, .increment=1, .rawValue=0, .setParameter=fxProgramParam1Callback, .getParameterDisplay=fxProgramParam1Display },
        { .name="Decay", .control=1, .increment=1, .rawValue=0, .setParameter=fxProgramParam2Callback, .getParameterDisplay=fxProgramParam2Display },
        { .name="Freeze", .control=0xff, .increment=1, .rawValue=0, .setParameter=fxProgramParamFreezeCallback, .getParameterDisplay=0 },
        { .name="Volume", .control=0xff, .increment=1, .rawValue=0x3ff, .setParameter=fxProgramPresetVolumeCallback, .getParameterDisplay=fxProgramPresetVolumeDisplay }
    },
    .processSampleStereo = &fxProgram12processSampleStereo, .setup = &fxProgram12SetupStereo, .isStereo = 1, .data = (void*)&fxProgram12data
};
