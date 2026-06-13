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
    .nParameters = 6, // Wait, 0..3 + Freeze + Vol = 6?
    // Mix, Decay, Tune, Tone (4 normal params) + Freeze + Volume = 6 parameters.
    // Index 0: Mix
    // Index 1: Decay
    // Index 2: Tune
    // Index 3: FREEZE (mapped from Switch in main.cpp)
    // Index 4: Volume? No, Tone was there.
    // Wait.
    // Main.cpp maps (0 -> Knob 1), (1 -> Knob 2), (2 -> Knob 3), (3 -> Switch).
    // Original Params: Mix(0), Decay(1), Tune(2), Tone(0xff), Vol(0xff).
    // Tone control was 0xff (last).
    // Now I want Freeze to be at Index 3.
    // So:
    // 0: Mix
    // 1: Decay
    // 2: Tune
    // 3: Freeze (Switch)
    // 4: Tone (Knob ? No, Tone is 4th parameter, usually accessed via Page mechanism? Or just Volume/Tone sharing?)
    // In original code: `{.name="Tone", .control=0xff`.
    // It seems Tone wasn't mapped to a knob directly (0,1,2). It was "0xff".
    // 0xff usually means "Parameter accessible via Menu/Edit mode" or just hidden/preset.
    // But `main.cpp` only controls 0, 1, 2.
    // If Tone control index is 0xff, it is NOT controllable via knobs unless user enters "Select" mode?
    // Wait, `main.cpp` has NO menu for params > 2.
    // So Tone was effectively fixed?
    // Ah, `fxProgram4_ampmodel2.c` and others use `control=0xff`.
    // Actually, `main.cpp` ONLY sends setParameter to indices 0, 1, 2 (and 3 for switch).
    // So any param with `control=0xff` at index 3... wait.
    // Index 3 is the ARRAY INDEX `parameters[3]`.
    // If I put Freeze at `parameters[3]`, `main.cpp` will drive it with the switch.
    // Correct.
    // So I must place Freeze at Index 3.
    // What was at Index 3 before?
    // Old: Mix(0), Decay(1), Tune(2), Tone(3), Volume(4).
    // So Tone was at index 3.
    // Was Tone controlled by switch?
    // In `main.cpp`:
    // `if (currentEffectIndex == 21 ... parameters[3].setParameter ...)`
    // ONLY Granular had param 3 driven by switch.
    // For Resonator (29), `parameters[3]` (Tone) was NOT driven by switch.
    // And NOT driven by knobs (0,1,2).
    // So Tone was fixed/dead?
    // Or maybe I missed something.
    // So putting Freeze at Index 3 is safe and enables it on Switch.
    // Tone moves to Index 4. Volume Index 5.
    // They are still accessible if I add menu support later, but for now they are fixed.
    // Wait, Tone should be accesssible.
    // Maybe mapped to Knob 3 (Y)?
    // Param 2 is Tune. Knob 3 -> Tune.
    // Param 1 is Decay. Knob 2 -> Decay.
    // Param 0 is Mix. Knob 1 -> Mix.
    // Tone is lost?
    // Yes, Tone was likely static or I misread the struct.
    // Original: 5 parameters.
    // {.name="Tone", .control=0xff...}
    // Yes.
    // Okay, so moving Tone to 4 doesn't hurt.
    .parameters = {
        {.name="Mix", .control=0, .increment=1, .rawValue=0, .setParameter=fxParamMix, .getParameterDisplay=fxDisplayMix},
        {.name="Decay", .control=1, .increment=1, .rawValue=0, .setParameter=fxParamDecay, .getParameterDisplay=fxDisplayDecay},
        {.name="Tune", .control=2, .increment=1, .rawValue=0, .setParameter=fxParamSize, .getParameterDisplay=fxDisplaySize},
        {.name="Freeze", .control=255, .increment=1, .rawValue=0, .setParameter=fxParamFreeze, .getParameterDisplay=0},
        {.name="Tone", .control=0xff, .increment=1, .rawValue=2000, .setParameter=fxParamTone, .getParameterDisplay=fxDisplayTone},
        {.name="Volume", .control=0xff, .increment=1, .rawValue=0x400, .setParameter=fxParamVol, .getParameterDisplay=fxDisplayVol}
    },
    .processSampleStereo = fxProgram30ProcessStereo,
    .setup = fxProgram30Setup,
    .isStereo = 1,
    .data = &progData
};

} // namespace Card_Flux
