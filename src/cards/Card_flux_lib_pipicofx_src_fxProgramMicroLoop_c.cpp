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
#include "audio/audiotools.h"
#include "pipicofx/picofxCore.h"

// FxProgram25: Micro-Looper / Glitch Delay (Mood Mk1 Style)

#define MOOD_BUF_SIZE 95000 

typedef struct {
    int16_t* buffer; 
    int32_t writePtr;
    int32_t readPtrQ8; 
    
    // Parameters
    int16_t mix;
    int16_t timeVal;  
    int16_t speedVal; 
    int16_t feedback; 
    int16_t freeze; // Freeze State
    
    GainStageDataType presetVolume;
    
    // Internal State
    int32_t currentDelay; 
    int32_t targetSpeedQ8;
    int32_t smoothedSpeedQ8;
    int16_t lastReadVal; 
} FxProgram25DataType;

static FxProgram25DataType progData;

static void fxProgram25Setup(void* data) {
    FxProgram25DataType* d = (FxProgram25DataType*)data;
    d->buffer = getDelayMemoryPointer(); 
    
    for(int i=0; i<MOOD_BUF_SIZE; i++) d->buffer[i] = 0;
    
    d->writePtr = 0;
    d->readPtrQ8 = 0;
    
    d->mix = 16384;      
    d->timeVal = 32000;  
    d->speedVal = 192;   
    d->feedback = 0;
    d->freeze = 0;
    d->presetVolume.gain = 256;
    d->smoothedSpeedQ8 = 256; 
}

static void fxProgram25SetupStereo(void* data) {
    fxProgram25Setup(data);
}

static inline int16_t readInterp(int16_t* buf, int32_t ptrQ8) {
    int32_t idx = ptrQ8 >> 8;
    int32_t frac = ptrQ8 & 0xFF;
    
    // Optimization: Use if instead of while for single-step wrapping
    if(idx >= MOOD_BUF_SIZE) idx -= MOOD_BUF_SIZE;
    if(idx < 0) idx += MOOD_BUF_SIZE;
    
    int32_t idx2 = idx + 1;
    if(idx2 >= MOOD_BUF_SIZE) idx2 = 0;
    
    int16_t s1 = buf[idx];
    int16_t s2 = buf[idx2];
    
    return s1 + (((s2 - s1) * frac) >> 8);
}

static void fxProgram25ProcessSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void* data, volatile uint32_t* audioState) {
    FxProgram25DataType* d = (FxProgram25DataType*)data;
    uint8_t frozen = (d->freeze > 2048);
    
    // 1. Mono Sum Input
    int16_t mn = (*inL + *inR) >> 1;
    if (frozen) mn = 0; 
    
    // 2. Calculate Read Logic (Q8)
    int32_t spd = 0;
    int16_t v = d->speedVal;
    if (v < 128) {
        spd = -((128 - v) * 2); 
    } else {
        spd = (v - 128) * 4; 
    }
    
    d->smoothedSpeedQ8 = d->smoothedSpeedQ8 + ((spd - d->smoothedSpeedQ8) >> 8);
    
    // 3. Update Read Pointer
    d->readPtrQ8 += d->smoothedSpeedQ8;
    
    int32_t wPtr = d->writePtr;
    int32_t rPtr = d->readPtrQ8 >> 8;
    
    int32_t dist = wPtr - rPtr;
    if (dist < 0) dist += MOOD_BUF_SIZE;
    
    int32_t targetLoopLen = 1000 + (d->timeVal * 3);
    if (targetLoopLen > MOOD_BUF_SIZE - 2000) targetLoopLen = MOOD_BUF_SIZE - 2000;
    
    static int32_t smoothLen = 32000;
    smoothLen += (targetLoopLen - smoothLen) >> 8; 
    int32_t loopLen = smoothLen;
    
    int32_t minSep = 1000; 
    
    if (dist < minSep) {
        int32_t target = wPtr - loopLen;
        if (target < 0) target += MOOD_BUF_SIZE;
        d->readPtrQ8 = target << 8;
    } 
    else if (dist > loopLen) {
        int32_t target = wPtr - minSep; 
        if (target < 0) target += MOOD_BUF_SIZE;
        d->readPtrQ8 = target << 8;
    }
    
    if ((d->readPtrQ8 >> 8) >= MOOD_BUF_SIZE) d->readPtrQ8 -= (MOOD_BUF_SIZE << 8);
    if ((d->readPtrQ8 >> 8) < 0) d->readPtrQ8 += (MOOD_BUF_SIZE << 8);
    
    // 4. Read
    int16_t wetL = readInterp(d->buffer, d->readPtrQ8);
    
    int32_t fixedOffset = 2400; 
    if (fixedOffset > (loopLen >> 1)) fixedOffset = loopLen >> 1;
    
    int32_t offsetR = fixedOffset << 8; 
    int32_t rPtrR = d->readPtrQ8 + offsetR; 
    
    if ((rPtrR >> 8) >= MOOD_BUF_SIZE) rPtrR -= (MOOD_BUF_SIZE << 8);
    if ((rPtrR >> 8) < 0) rPtrR += (MOOD_BUF_SIZE << 8);

    int16_t wetR = readInterp(d->buffer, rPtrR);
    
    // Filter
    static int32_t lpL = 0, lpR = 0;
    lpL += ((wetL - lpL) * 20000) >> 15;
    lpR += ((wetR - lpR) * 20000) >> 15;
    wetL = (int16_t)lpL;
    wetR = (int16_t)lpR;
    
    // 5. Write
    if (!frozen) {
        int32_t wetSum = (wetL + wetR) >> 1;
        
        static int32_t fbLp = 0;
        fbLp += ((wetSum - fbLp) * 24000) >> 15; 
        
        int32_t fbVal = (mn >> 1) + ((fbLp * d->feedback) >> 15);
        
        // Soft saturation
        if(fbVal > 16384) fbVal = 16384 + ((fbVal - 16384) >> 2);
        else if(fbVal < -16384) fbVal = -16384 + ((fbVal + 16384) >> 2);

        if (fbVal > 32767) fbVal = 32767;
        if (fbVal < -32767) fbVal = -32767;
        
        d->buffer[wPtr] = (int16_t)fbVal;
        
        d->writePtr++;
        if (d->writePtr >= MOOD_BUF_SIZE) d->writePtr = 0;
    }
    
    // 6. Output
    int32_t mx = d->mix;
    int32_t dryL = *inL; 
    int32_t dryR = *inR;
    
    int32_t outL_val = ((dryL * (32767 - mx)) >> 15) + ((wetL * mx) >> 15); 
    int32_t outR_val = ((dryR * (32767 - mx)) >> 15) + ((wetR * mx) >> 15);
    
    *outL = (int16_t)outL_val;
    *outR = (int16_t)outR_val;
    
    *outL = gainStageProcessSample(*outL, &d->presetVolume);
    *outR = gainStageProcessSample(*outR, &d->presetVolume);
}

static void fxParam1(uint16_t val, void* data) { ((FxProgram25DataType*)data)->mix = val << 3; }
static void fxDisplay1(void* data, char* res) { Int16ToChar(((FxProgram25DataType*)data)->mix/328, res); appendToString(res, "%"); }

static void fxParam2(uint16_t val, void* data) { ((FxProgram25DataType*)data)->timeVal = val << 3; }
static void fxDisplay2(void* data, char* res) { 
    int32_t ms = (((FxProgram25DataType*)data)->timeVal * 1333) >> 15;
    Int16ToChar((int16_t)ms, res); appendToString(res, "ms"); 
}

static void fxParam3(uint16_t val, void* data) { ((FxProgram25DataType*)data)->speedVal = val >> 4; } 
static void fxDisplay3(void* data, char* res) {
    int16_t v = ((FxProgram25DataType*)data)->speedVal;
    if (v < 110) appendToString(res, "Rev");
    else if (v < 146) appendToString(res, "Stop");
    else appendToString(res, "Fwd");
}

static void fxParam4(uint16_t val, void* data) { ((FxProgram25DataType*)data)->feedback = (val * 30000) >> 12; } 
static void fxDisplay4(void* data, char* res) { Int16ToChar(((FxProgram25DataType*)data)->feedback/328, res); appendToString(res, "%"); }

static void fxParamFreeze(uint16_t val, void* data) { ((FxProgram25DataType*)data)->freeze = val; }

static void fxParamVol(uint16_t val, void* data) { ((FxProgram25DataType*)data)->presetVolume.gain = val >> 2; }
static void fxDisplayVol(void* data, char* res) { decimalInt16ToChar(((FxProgram25DataType*)data)->presetVolume.gain*39,res,2); appendToString(res, "%"); }

FxProgramType fxProgramMicroLoop = {
    .name = "Micro Looper",
    .parameters = {
        {
            .name = "Mix",
            .control = 0,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplay1,
            .setParameter = fxParam1
        },
        {
            .name = "Length",
            .control = 1,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplay2,
            .setParameter = fxParam2
        },
        {
            .name = "Speed",
            .control = 2,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplay3,
            .setParameter = fxParam3
        },
        {
            .name = "Freeze",
            .control = 0xff,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = 0,
            .setParameter = fxParamFreeze
        },
        {
            .name = "Repeats",
            .control = 0xff,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = fxDisplay4,
            .setParameter = fxParam4
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
    .processSampleStereo = fxProgram25ProcessSampleStereo,
    .setup = fxProgram25SetupStereo,
    .nParameters = 6,
    .isStereo = 1,
    .data = &progData
};

} // namespace Card_Flux
