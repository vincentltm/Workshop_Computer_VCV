#include "pipicofx/fxPrograms.h"
#include "stringFunctions.h"
#include "audio/delay.h"
#include "audio/reverbUtils.h"
#include "romfunc.h"

// Dattorro Plate Reverb (48kHz)

// Delay Lengths (Samples)
#define AP_IN_1 229
#define AP_IN_2 172
#define AP_IN_3 611
#define AP_IN_4 447

// Left Tank
#define AP_MOD_L 1083
#define DELAY_1_L 7187 
#define AP_TANK_L 2903
#define DELAY_2_L 6007

// Right Tank
#define AP_MOD_R 1464
#define DELAY_1_R 6803
#define AP_TANK_R 3850 
#define DELAY_2_R 5101

// Coefficients
#define COEFF_IN_1 24576 
#define COEFF_IN_2 24576 
#define COEFF_IN_3 20480 
#define COEFF_IN_4 20480 
#define COEFF_TANK_AP 16384 

typedef struct {
    // Input Diffusion
    AllpassType apIn[4];

    // Tank Left
    DelayDataType modApL; 
    DelayDataType delay1L;
    AllpassType apTankL;
    DelayDataType delay2L;
    int16_t lpStateL;

    // Tank Right
    DelayDataType modApR;
    DelayDataType delay1R;
    AllpassType apTankR;
    DelayDataType delay2R;
    int16_t lpStateR;

    // Modulation
    uint32_t lfoPhase;
    int16_t lfoDepth;

    // Control State
    int16_t decay;  // Feedback gain
    int16_t bandwidth; // Input Lowpass
    int16_t damping; // Tank Lowpass
    int16_t mix;
    
    int16_t freeze; // Freeze State (0..4095)

    int16_t inputLpState;

    GainStageDataType presetVolume;
} FxProgram21DataType;


// Helper: Filter
static inline int16_t plateLowpass(int16_t sample, int16_t* state, int16_t alpha) {
    int32_t diff = (int32_t)sample - *state;
    *state = *state + ((diff * alpha) >> 15);
    return *state;
}

// Helper: Modulated Allpass Process
static int16_t processModulatedAP(int16_t x, DelayDataType* d, int16_t modOffset, int16_t g) {
    int32_t nominal = d->delayInSamples;
    int32_t actual = nominal - modOffset; 
    if (actual < 1) actual = 1;

    int32_t rdPtr = (int32_t)d->delayLinePtr - actual;
    uint32_t mask = d->delayBufferLength - 1;
    
    int16_t w_delayed = *(d->delayLine + (rdPtr & mask));
    
    int32_t w_n = x + ((g * w_delayed) >> 15);
    
    if (w_n > 32767) w_n = 32767;
    if (w_n < -32767) w_n = -32767;
    
    *(d->delayLine + d->delayLinePtr) = (int16_t)w_n;
    d->delayLinePtr = (d->delayLinePtr + 1) & mask;
    
    int32_t y = ((-g * w_n) >> 15) + w_delayed;
    return (int16_t)y;
}

static void fxProgram21processSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void*data, volatile uint32_t* audioStatePtr)
{
    FxProgram21DataType* pData = (FxProgram21DataType*)data;
    uint8_t frozen = (pData->freeze > 2048);

    // 1. Mono Sum + Input Tone
    int16_t input = (*inL + *inR) >> 1;
    input = plateLowpass(input, &pData->inputLpState, pData->bandwidth);
    input = input >> 1; // 6dB headroom
    
    // FREEZE: Mute Input
    if (frozen) input = 0;

    // 2. Input Diffusion (4 Series APs)
    for(int i=0; i<4; i++) {
        input = allpassProcessSample(input, &pData->apIn[i], audioStatePtr);
    }
    
    // 3. Tank Figure-8 Loop
    int16_t tankOutL = getDelayedSample(&pData->delay2L);
    int16_t tankOutR = getDelayedSample(&pData->delay2R);
    
    // Modulation
    pData->lfoPhase += 4096; 
    int16_t modBase = (pData->lfoPhase >> 16) & 0x7FFF;
    if (pData->lfoPhase & 0x80000000) modBase = 32767 - modBase;
    int16_t modL = (modBase * pData->lfoDepth) >> 15;
    int16_t modR = ((32767 - modBase) * pData->lfoDepth) >> 15; 

    // FREEZE: Force Decay to Max (Unit Gain)
    int16_t decayVal = frozen ? 32767 : pData->decay;

    // --- Left Tank ---
    int16_t inLeft = input + ((decayVal * tankOutR) >> 15);
    
    int16_t sL = processModulatedAP(inLeft, &pData->modApL, modL, 16384); 
    
    addSampleToDelayline(sL, &pData->delay1L);
    sL = getDelayedSample(&pData->delay1L);
    
    // Damping (Bypass in Freeze?)
    if (!frozen) sL = plateLowpass(sL, &pData->lpStateL, pData->damping);
    
    sL = allpassProcessSample(sL, &pData->apTankL, audioStatePtr);
    addSampleToDelayline(sL, &pData->delay2L);

    // --- Right Tank ---
    int16_t inRight = input + ((decayVal * tankOutL) >> 15);
    
    int16_t sR = processModulatedAP(inRight, &pData->modApR, modR, 16384);
    
    addSampleToDelayline(sR, &pData->delay1R);
    sR = getDelayedSample(&pData->delay1R);
    
    if (!frozen) sR = plateLowpass(sR, &pData->lpStateR, pData->damping);
    
    sR = allpassProcessSample(sR, &pData->apTankR, audioStatePtr);
    addSampleToDelayline(sR, &pData->delay2R);

    // 4. Output Taps
    int16_t wetL = sL; 
    int16_t wetR = sR;

    int16_t mix = pData->mix;
    *outL = clip((((32767 - mix) * (*inL)) >> 15) + ((mix * wetL) >> 14), audioStatePtr);
    *outR = clip((((32767 - mix) * (*inR)) >> 15) + ((mix * wetR) >> 14), audioStatePtr);
    
    *outL = gainStageProcessSample(*outL, &pData->presetVolume);
    *outR = gainStageProcessSample(*outR, &pData->presetVolume);
}


// Initialization
static void fxProgram21Setup(void* data) {
    FxProgram21DataType* pData = (FxProgram21DataType*)data;
    int16_t* ptr = getDelayMemoryPointer();
    
    // Input APs
    int16_t apLens[4] = {AP_IN_1, AP_IN_2, AP_IN_3, AP_IN_4};
    int16_t apCoeffs[4] = {COEFF_IN_1, COEFF_IN_2, COEFF_IN_3, COEFF_IN_4};
    
    for(int i=0; i<4; i++) {
        pData->apIn[i].delayLineIn = ptr;
        pData->apIn[i].delayLineOut = ptr + 1024;
        pData->apIn[i].bufferSize = 1023; 
        pData->apIn[i].delayInSamples = apLens[i];
        pData->apIn[i].coefficient = apCoeffs[i];
        pData->apIn[i].delayPtr = 0;
        pData->apIn[i].oldValues = 0;
        for(int k=0; k<2048; k++) *(ptr+k)=0;
        ptr += 2048; 
    }

    // -- Left Tank --
    initDelay(&pData->modApL, ptr, 2048);
    pData->modApL.delayInSamples = AP_MOD_L;
    ptr += 2048;
    
    initDelay(&pData->delay1L, ptr, 8192); 
    pData->delay1L.delayInSamples = DELAY_1_L;
    ptr += 8192;
    
    pData->apTankL.delayLineIn = ptr;
    pData->apTankL.delayLineOut = ptr + 4096; 
    pData->apTankL.bufferSize = 4095;
    pData->apTankL.delayInSamples = AP_TANK_L;
    pData->apTankL.coefficient = COEFF_TANK_AP;
    pData->apTankL.delayPtr = 0;
    pData->apTankL.oldValues = 0;
    for(int k=0;k<8192;k++) *(ptr+k)=0;
    ptr += 8192;
    
    initDelay(&pData->delay2L, ptr, 8192); 
    pData->delay2L.delayInSamples = DELAY_2_L;
    ptr += 8192;

    // -- Right Tank --
    initDelay(&pData->modApR, ptr, 2048);
    pData->modApR.delayInSamples = AP_MOD_R;
    ptr += 2048;

    initDelay(&pData->delay1R, ptr, 8192); 
    pData->delay1R.delayInSamples = DELAY_1_R;
    ptr += 8192;
    
    pData->apTankR.delayLineIn = ptr;
    pData->apTankR.delayLineOut = ptr + 4096; 
    pData->apTankR.bufferSize = 4095;
    pData->apTankR.delayInSamples = AP_TANK_R;
    pData->apTankR.coefficient = COEFF_TANK_AP;
    pData->apTankR.delayPtr = 0;
    pData->apTankR.oldValues = 0;
    for(int k=0;k<8192;k++) *(ptr+k)=0;
    ptr += 8192;

    initDelay(&pData->delay2R, ptr, 8192); 
    pData->delay2R.delayInSamples = DELAY_2_R;
    ptr += 8192;
    
    pData->freeze = 0;
}

FxProgram21DataType fxProgram21data = {
    .presetVolume = { .gain = 0xFF },
    .decay = 16000,
    .bandwidth = 32000,
    .damping = 10000,
    .mix = 16384,
    .lfoDepth = 200,
    .freeze = 0
};

// Callbacks
static void fxProgramParam1Callback(uint16_t val,void*data) { // Mix
    ((FxProgram21DataType*)data)->mix = val << 3;
}
static void fxProgramParam1Display(void*data,char*res) {
    Int16ToChar(((FxProgram21DataType*)data)->mix/328,res);
    appendToString(res,"%");
}

static void fxProgramParam2Callback(uint16_t val,void*data) { // Decay
    int16_t d = val << 3;
    if (d > 32000) d = 32000;
    ((FxProgram21DataType*)data)->decay = d;
}
static void fxProgramParam2Display(void*data,char*res) {
    Int16ToChar(((FxProgram21DataType*)data)->decay/328,res);
}

static void fxProgramParam3Callback(uint16_t val,void*data) { // Tone
    int16_t bw = 1000 + ((val * 31767) >> 12);
    ((FxProgram21DataType*)data)->bandwidth = bw;
}
static void fxProgramParam3Display(void*data,char*res) {
    Int16ToChar(((FxProgram21DataType*)data)->bandwidth/328,res);
}

static void fxProgramParamFreezeCallback(uint16_t val,void*data) {
    ((FxProgram21DataType*)data)->freeze = val;
}

static void fxProgramPresetVolumeCallback(uint16_t val,void*data) {
    ((FxProgram21DataType*)data)->presetVolume.gain = val >> 2;
}
static void fxProgramPresetVolumeDisplay(void*data,char*res) {
    decimalInt16ToChar(((FxProgram21DataType*)data)->presetVolume.gain*39,res,2);
    for (uint8_t c=0;c<PARAMETER_NAME_MAXLEN-1;c++) {
        if(*(res+c)==0) { *(res+c)='%'; *(res+c+1)=0; break; }
    }
}


FxProgramType fxProgramPlateReverb = {
    .name = "Plate Reverb",
    .nParameters = 5,
    .parameters = {
        { .name="Mix", .control=0, .increment=1, .rawValue=0, .setParameter=fxProgramParam1Callback, .getParameterDisplay=fxProgramParam1Display },
        { .name="Decay", .control=1, .increment=1, .rawValue=0, .setParameter=fxProgramParam2Callback, .getParameterDisplay=fxProgramParam2Display },
        { .name="Tone", .control=2, .increment=1, .rawValue=0, .setParameter=fxProgramParam3Callback, .getParameterDisplay=fxProgramParam3Display },
        { .name="Freeze", .control=0xff, .increment=1, .rawValue=0, .setParameter=fxProgramParamFreezeCallback, .getParameterValue=0, .getParameterDisplay=0 },
        { .name="Volume", .control=0xff, .increment=1, .rawValue=0x3ff, .setParameter=fxProgramPresetVolumeCallback, .getParameterDisplay=fxProgramPresetVolumeDisplay }
    },
    .processSampleStereo = &fxProgram21processSampleStereo,
    .setup = &fxProgram21Setup,
    .isStereo = 1,
    .data = (void*)&fxProgram21data
};
