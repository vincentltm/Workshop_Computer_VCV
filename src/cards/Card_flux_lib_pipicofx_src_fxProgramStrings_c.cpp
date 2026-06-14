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
#include "audio/delay.h"
#include "audio/reverbUtils.h"
#include "romfunc.h"

// FxProgram30: "Resonator Strings"
// Physical Model of 4 Sympathetic Strings (Karplus-Strong network).

typedef struct {
    int16_t* delayLine;
    int32_t length;
    int32_t writePtr;
    int32_t lpState; 
} StringType;

typedef struct {
    int16_t* buffer;
    
    // 4 Strings
    StringType str[4];
    
    // Parameters
    int16_t mix;
    int16_t decay; 
    int16_t tone;  
    int16_t size; 
    int16_t freeze; // Freeze State (0..4095)
    
    // State
    int16_t lastSizeParam;
    
    GainStageDataType presetVolume;
} FxProgram30DataType;

static FxProgram30DataType progData;

static void tuneStrings(FxProgram30DataType* d) {
    int32_t baseLen = 50 + ((d->size * 1950) >> 12);
    
    d->str[0].length = baseLen;
    d->str[1].length = (baseLen * 2) / 3; 
    d->str[2].length = (baseLen * 4) / 5; 
    d->str[3].length = baseLen / 2;       
    
    for(int i=0; i<4; i++) if (d->str[i].length < 2) d->str[i].length = 2;
}

static inline int16_t processString(StringType* s, int16_t in, int16_t fbAmt, int16_t dampAmt) {
    int32_t rIdx = s->writePtr - s->length;
    while(rIdx < 0) rIdx += 8192; 
    
    int16_t out = s->delayLine[rIdx];
    
    int16_t alpha = 2000 + ((dampAmt * 30000) >> 12);
    s->lpState += ((out - s->lpState) * alpha) >> 15;
    int16_t filtered = (int16_t)s->lpState;
    
    int32_t fb = (filtered * fbAmt) >> 15;
    
    int32_t node = in + fb;
    
    // Soft saturation for string energy
    if(node > 16384) node = 16384 + ((node - 16384) >> 2);
    else if(node < -16384) node = -16384 + ((node + 16384) >> 2);

    if(node > 32700) node = 32700;
    if(node < -32700) node = -32700;
    
    s->delayLine[s->writePtr] = (int16_t)node;
    s->writePtr++;
    if(s->writePtr >= 8192) s->writePtr = 0;
    
    return out;
}

static void fxProgram30ProcessStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void* data, volatile uint32_t* audioState) {
    FxProgram30DataType* d = (FxProgram30DataType*)data;
    uint8_t frozen = (d->freeze > 2048);
    
    if (d->size != d->lastSizeParam) {
        tuneStrings(d);
        d->lastSizeParam = d->size;
    }
    
    // Params
    int16_t feedback = d->decay << 3; 
    if (feedback > 32250) feedback = 32250; 
    
    // FREEZE overrides Feedback
    if (frozen) feedback = 32767; // Max sustain
    
    int16_t tone = d->tone;
    
    // Excitation (Muted if Frozen)
    int16_t exciteL = frozen ? 0 : (*inL >> 3);
    int16_t exciteR = frozen ? 0 : (*inR >> 3);
    
    int16_t s0 = processString(&d->str[0], exciteL, feedback, tone);
    int16_t s1 = processString(&d->str[1], exciteL, feedback, tone);
    
    int16_t s2 = processString(&d->str[2], exciteR, feedback, tone);
    int16_t s3 = processString(&d->str[3], exciteR, feedback, tone);
    
    // Output Mix: Boost wet (2x) to compensate for excitation reduction
    int32_t wetL = (int32_t)(s0 + s1);
    int32_t wetR = (int32_t)(s2 + s3);
    if (wetL > 32767) wetL = 32767; if (wetL < -32767) wetL = -32767;
    if (wetR > 32767) wetR = 32767; if (wetR < -32767) wetR = -32767;
    
    int16_t mix = d->mix << 3;
    *outL = ((*inL * (32767-mix)) >> 15) + ((wetL * mix) >> 15);
    *outR = ((*inR * (32767-mix)) >> 15) + ((wetR * mix) >> 15);
    
    *outL = gainStageProcessSample(*outL, &d->presetVolume);
    *outR = gainStageProcessSample(*outR, &d->presetVolume);
}

static void fxProgram30Setup(void* data) {
    FxProgram30DataType* d = (FxProgram30DataType*)data;
    d->buffer = getDelayMemoryPointer(); 
    
    for(int i=0; i<32768; i++) d->buffer[i] = 0;
    
    d->str[0].delayLine = d->buffer;
    d->str[1].delayLine = d->buffer + 8192;
    d->str[2].delayLine = d->buffer + 16384;
    d->str[3].delayLine = d->buffer + 24576;
    
    for(int i=0; i<4; i++) {
        d->str[i].writePtr = 0;
        d->str[i].lpState = 0;
        d->str[i].length = 100;
    }
    
    d->mix = 16384;
    d->decay = 2000;
    d->tone = 3000;
    d->size = 2000;
    d->lastSizeParam = -1;
    d->presetVolume.gain = 256;
    d->freeze = 0;
    
    tuneStrings(d);
}

// Params
static void fxParamMix(uint16_t val, void* data) { ((FxProgram30DataType*)data)->mix = val; }
static void fxDisplayMix(void* data, char* res) { Int16ToChar(((FxProgram30DataType*)data)->mix/41, res); appendToString(res, "%"); }

static void fxParamDecay(uint16_t val, void* data) { ((FxProgram30DataType*)data)->decay = val; }
static void fxDisplayDecay(void* data, char* res) { Int16ToChar(((FxProgram30DataType*)data)->decay/41, res); appendToString(res, "%"); }

static void fxParamTone(uint16_t val, void* data) { ((FxProgram30DataType*)data)->tone = val; }
static void fxDisplayTone(void* data, char* res) { 
    int16_t v = ((FxProgram30DataType*)data)->tone;
    if (v > 3000) appendToString(res, "Bright"); 
    else if (v > 1500) appendToString(res, "Warm"); 
    else appendToString(res, "Dark"); 
}

static void fxParamSize(uint16_t val, void* data) { ((FxProgram30DataType*)data)->size = val; }
static void fxDisplaySize(void* data, char* res) { Int16ToChar(((FxProgram30DataType*)data)->size/41, res); appendToString(res, "%"); }

static void fxParamFreeze(uint16_t val, void* data) { ((FxProgram30DataType*)data)->freeze = val; }

static void fxParamVol(uint16_t val, void* data) { ((FxProgram30DataType*)data)->presetVolume.gain = val >> 2; }
static void fxDisplayVol(void* data, char* res) { decimalInt16ToChar(((FxProgram30DataType*)data)->presetVolume.gain*39,res,2); appendToString(res, "%"); }

FxProgramType fxProgramStrings = {
    .name = "Resonator",
    .parameters = {
        {
            .name = "Mix",
            .control = 0,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplayMix,
            .setParameter = fxParamMix
        },
        {
            .name = "Decay",
            .control = 1,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplayDecay,
            .setParameter = fxParamDecay
        },
        {
            .name = "Tune",
            .control = 2,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplaySize,
            .setParameter = fxParamSize
        },
        {
            .name = "Freeze",
            .control = 255,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = 0,
            .setParameter = fxParamFreeze
        },
        {
            .name = "Tone",
            .control = 0xff,
            .rawValue = 2000,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplayTone,
            .setParameter = fxParamTone
        },
        {
            .name = "Volume",
            .control = 0xff,
            .rawValue = 0x400,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplayVol,
            .setParameter = fxParamVol
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
    .processSampleStereo = fxProgram30ProcessStereo,
    .setup = fxProgram30Setup,
    .nParameters = 6,
    .isStereo = 1,
    .data = &progData
};

} // namespace Card_Flux
