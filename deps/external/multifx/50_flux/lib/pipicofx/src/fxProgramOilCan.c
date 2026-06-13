#include "pipicofx/fxPrograms.h"
#include "stringFunctions.h"
#include "audio/delay.h"
#include "audio/reverbUtils.h"
#include "romfunc.h"

// FxProgram29: Oil Can Echo (Tel-Ray Style)
// Physical Model of an Electrostatic Delay

#define OILCAN_BUF_SIZE 16384 // ~340ms at 48kHz

static FxProgram29DataType progData;

static void fxProgram29ProcessSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void* data, volatile uint32_t* audioState) {
    FxProgram29DataType* d = (FxProgram29DataType*)data;
    uint8_t frozen = (d->freeze > 2048);
    
    // Update Motor Phase
    int32_t rate = (500 + (d->timeVal * 2)) * AUDIO_SAMPLE_RATE_DIV;
    d->motorPhase += rate;
    
    int16_t monoInL = *inL; 
    int16_t monoInR = *inR;
    if (frozen) {
        monoInL = 0; monoInR = 0;
    }

    // --- LEFT CHANNEL ---
    // Target base delay with fractional parts for smoother modulation
    int32_t baseDelay = 15000 - ((d->timeVal * 14000) >> 12);
    if (baseDelay < 500) baseDelay = 500;

    int32_t phL = d->motorPhase; 
    int32_t triangle = ((phL >> 16) & 0x7FFF);
    if (phL & 0x80000000) triangle = 32767 - triangle;
    int32_t lfoL = triangle - 16384; 
    
    int32_t modL = (lfoL * (d->wobble >> 2)) >> 7; // Fractional delay offset (shifted 8)
    int32_t delayFracL = (baseDelay << 8) + modL;
    
    int32_t iPartL = delayFracL >> 8;
    int32_t fPartL = delayFracL & 0xFF;
    
    int32_t rIdx1L = d->writePtr - iPartL;
    while (rIdx1L < 0) rIdx1L += 16384; 
    int32_t rIdx2L = rIdx1L - 1;
    if (rIdx2L < 0) rIdx2L += 16384;
    
    // Linear interpolation
    int32_t s1L = d->buffer[rIdx1L];
    int32_t s2L = d->buffer[rIdx2L];
    int16_t wetL = (int16_t)(s1L + ((s2L - s1L) * fPartL >> 8));
    
    // Filter & Feedback
    int16_t lpAlpha = 1500 + ((4095 - d->smudge) * 5);
    int32_t diffL = (int32_t)wetL - d->lpStateL;
    d->lpStateL += (diffL * lpAlpha) >> 15;
    wetL = (int16_t)d->lpStateL;
    
    int16_t fbAmt = d->smudge << 3; 
    if (fbAmt > 31000) fbAmt = 31000;
    if (frozen) fbAmt = 32767;

    int32_t fbValL = (monoInL >> 1) + ((wetL * fbAmt) >> 15);
    // Smoother Soft Saturation
    if (fbValL > 12000) fbValL = 12000 + ((fbValL - 12000) >> 1);
    else if (fbValL < -12000) fbValL = -12000 + ((fbValL + 12000) >> 1);
    if (fbValL > 32700) fbValL = 32700; if (fbValL < -32700) fbValL = -32700;
    
    if (!frozen) d->buffer[d->writePtr] = (int16_t)fbValL;
    
    // --- RIGHT CHANNEL (Offset 16384) ---
    int32_t phR = d->motorPhase + 0x40000000; 
    int32_t triR = ((phR >> 16) & 0x7FFF);
    if (phR & 0x80000000) triR = 32767 - triR;
    int32_t lfoR = triR - 16384; 
    
    // Filtered Jitter (Mechanical instability)
    // Update RNG only every 16 samples to avoid high-frequency noise
    static uint8_t jitterDiv = 0;
    static int16_t currentJitter = 0;
    if (++jitterDiv >= 16) {
        d->rng = d->rng * 1664525 + 1013904223;
        currentJitter = (int16_t)((d->rng >> 24) & 0xFF); 
        jitterDiv = 0;
    }
    
    int32_t modR = (lfoR * (d->wobble >> 2)) >> 7; 
    int32_t delayFracR = (baseDelay << 8) + modR + (currentJitter << 4) + (480 << 8);
    
    int32_t iPartR = delayFracR >> 8;
    int32_t fPartR = delayFracR & 0xFF;
    
    int32_t rIdx1R = d->writePtr - iPartR;
    while (rIdx1R < 0) rIdx1R += 16384; 
    int32_t rIdx2R = rIdx1R - 1;
    if (rIdx2R < 0) rIdx2R += 16384;
    
    int32_t s1R = d->buffer[rIdx1R + 16384];
    int32_t s2R = d->buffer[rIdx2R + 16384];
    int16_t wetR = (int16_t)(s1R + ((s2R - s1R) * fPartR >> 8));
    
    int32_t diffR = (int32_t)wetR - d->lpStateR;
    d->lpStateR += (diffR * lpAlpha) >> 15;
    wetR = (int16_t)d->lpStateR;
    
    int32_t fbValR = (monoInR >> 1) + ((wetR * fbAmt) >> 15);
    if (fbValR > 12000) fbValR = 12000 + ((fbValR - 12000) >> 1);
    else if (fbValR < -12000) fbValR = -12000 + ((fbValR + 12000) >> 1);
    if (fbValR > 32700) fbValR = 32700; if (fbValR < -32700) fbValR = -32700;
    
    if (!frozen) d->buffer[d->writePtr + 16384] = (int16_t)fbValR;
    
    // Advance Write
    if (!frozen) {
        d->writePtr++;
        if (d->writePtr >= 16384) d->writePtr = 0;
    }
    
    // Output Mix
    int16_t mix = d->mix << 3;
    int32_t wL = (int32_t)wetL << 1; 
    int32_t wR = (int32_t)wetR << 1;
    if (wL > 32767) wL = 32767; if (wL < -32767) wL = -32767;
    if (wR > 32767) wR = 32767; if (wR < -32767) wR = -32767;

    *outL = ((*inL * (32767-mix)) >> 15) + ((wL * mix) >> 15);
    *outR = ((*inR * (32767-mix)) >> 15) + ((wR * mix) >> 15);
    
    *outL = gainStageProcessSample(*outL, &d->presetVolume);
    *outR = gainStageProcessSample(*outR, &d->presetVolume);
}

static void fxProgram29Setup(void* data) {
    FxProgram29DataType* d = (FxProgram29DataType*)data;
    d->buffer = getDelayMemoryPointer();
    for(int i=0; i<32768; i++) d->buffer[i] = 0;
    d->writePtr = 0;
    d->motorPhase = 0;
    d->lpStateL = 0;
    d->lpStateR = 0;
    d->rng = 12345;
    d->mix = 512;
    d->timeVal = 2048; 
    d->smudge = 2048;  
    d->wobble = 500;   
    d->freeze = 0;
    d->presetVolume.gain = 256;
}

// Parameters
static void fxParamTime(uint16_t val, void* data) { ((FxProgram29DataType*)data)->timeVal = val; }
static void fxDisplayTime(void* data, char* res) { 
    int32_t ms = (340 * AUDIO_SAMPLE_RATE_DIV) - ((((FxProgram29DataType*)data)->timeVal * 300 * AUDIO_SAMPLE_RATE_DIV) >> 12);
    Int16ToChar((int16_t)ms, res); appendToString(res, "ms"); 
}

static void fxParamSmudge(uint16_t val, void* data) { ((FxProgram29DataType*)data)->smudge = val; }
static void fxDisplaySmudge(void* data, char* res) { 
    Int16ToChar(((FxProgram29DataType*)data)->smudge/41, res); appendToString(res, "%"); 
}

static void fxParamWobble(uint16_t val, void* data) { ((FxProgram29DataType*)data)->wobble = val; }
static void fxDisplayWobble(void* data, char* res) { 
    Int16ToChar(((FxProgram29DataType*)data)->wobble/41, res); appendToString(res, "%"); 
}

static void fxParamMix(uint16_t val, void* data) { ((FxProgram29DataType*)data)->mix = val; }
static void fxDisplayMix(void* data, char* res) { 
    Int16ToChar(((FxProgram29DataType*)data)->mix/41, res); appendToString(res, "%"); 
}

static void fxParamFreeze(uint16_t val, void* data) { ((FxProgram29DataType*)data)->freeze = val; }

static void fxParamVol(uint16_t val, void* data) { ((FxProgram29DataType*)data)->presetVolume.gain = val >> 2; }
static void fxDisplayVol(void* data, char* res) { 
    decimalInt16ToChar(((FxProgram29DataType*)data)->presetVolume.gain*39,res,2); appendToString(res, "%"); 
}

FxProgramType fxProgramOilCan = {
    .name = "Oil Can Echo",
    .nParameters = 6,
    .parameters = {
        {.name="Mix", .control=0, .increment=1, .rawValue=0, .setParameter=fxParamMix, .getParameterDisplay=fxDisplayMix},
        {.name="Speed", .control=1, .increment=1, .rawValue=0, .setParameter=fxParamTime, .getParameterDisplay=fxDisplayTime},
        {.name="Smudge", .control=2, .increment=1, .rawValue=0, .setParameter=fxParamSmudge, .getParameterDisplay=fxDisplaySmudge},
        {.name="Freeze", .control=0xff, .increment=1, .rawValue=0, .setParameter=fxParamFreeze, .getParameterDisplay=0},
        {.name="Wobble", .control=0xff, .increment=1, .rawValue=100, .setParameter=fxParamWobble, .getParameterDisplay=fxDisplayWobble},
        {.name="Volume", .control=0xff, .increment=1, .rawValue=0x400, .setParameter=fxParamVol, .getParameterDisplay=fxDisplayVol}
    },
    .processSampleStereo = fxProgram29ProcessSampleStereo,
    .setup = fxProgram29Setup,
    .isStereo = 1,
    .data = &progData
};
