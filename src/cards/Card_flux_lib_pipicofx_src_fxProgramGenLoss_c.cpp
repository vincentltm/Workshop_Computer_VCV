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
#include "audio/audiotools.h"
#include "pipicofx/picofxCore.h"

// FxProgram26: Tape Machine
// Follows the same I/O patterns as fxProgramDelay.c

#define TAPE_BUF_SIZE 8192  // Stereo buffer: L=0..4095, R=4096..8191
#define TAPE_BUF_MASK (4095)
#define SINE_TABLE_SIZE 256

static int16_t tapeSineTable[SINE_TABLE_SIZE];

// Langevin Saturation LUT (Q15 gain reduction)
static const int16_t langevinGainLUT[65] = {
    32767, 32700, 32500, 32200, 31800, 31300, 30700, 30000,
    29200, 28400, 27500, 26600, 25700, 24800, 23900, 23000,
    22200, 21400, 20600, 19900, 19200, 18600, 18000, 17500,
    17000, 16500, 16100, 15700, 15400, 15100, 14800, 14500,
    14300, 14100, 13900, 13700, 13500, 13400, 13200, 13100,
    13000, 12900, 12800, 12700, 12600, 12500, 12400, 12350,
    12300, 12250, 12200, 12150, 12100, 12050, 12000, 11950,
    11900, 11850, 11800, 11750, 11700, 11650, 11600, 11550,
    11500
};

typedef struct {
    int16_t* buffer;
    int32_t writePtr;
    uint32_t rngState;
    
    // Transport LFOs (32-bit Phase Accumulators)
    uint32_t wowPhase;
    uint32_t flutterPhase;
    
    // Filter States (Simple Tape Filter like in Delay)
    int16_t tapeL_old;
    int16_t tapeR_old;
    
    // DC Blocker state
    int32_t dcL, dcR;

    // Parameters (0..4095)
    int16_t quality;    // Main Knob
    int16_t transport;  // Knob X
    int16_t drive;      // Knob Y
    int16_t mix;        // Dry/Wet
    
    // Output Volume
    GainStageDataType presetVolume;
    
} FxProgramTapeDataType;

static FxProgramTapeDataType progData;

static inline uint32_t xorshift32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static void initSineTable() {
    if (tapeSineTable[1] != 0) return;
    for(int i=0; i<SINE_TABLE_SIZE; i++) {
        float phase = (float)i / (float)SINE_TABLE_SIZE;
        float val = 0.0f;
        // Smoother triangle-ish wave
        if (phase < 0.25f) val = phase * 4.0f;
        else if (phase < 0.75f) val = 1.0f - (phase - 0.25f) * 4.0f;
        else val = -1.0f + (phase - 0.75f) * 4.0f;
        tapeSineTable[i] = (int16_t)(val * 32767.0f);
    }
}

static inline int16_t getSineLfo(uint32_t phase) {
    uint8_t idx = phase >> 24; 
    return tapeSineTable[idx];
}

// Simple tape lowpass like in Delay effect
static inline int16_t tapeFilter(int16_t sample, int16_t* oldVal, int32_t amount) {
    // amount: 0 = no filtering (pass through), 4096 = heavy filtering
    // Default was >> 2 which is 25% blend. Let's make it variable.
    int32_t blend = amount >> 10; // 0..4
    if (blend > 4) blend = 4;
    int32_t diff = (int32_t)sample - *oldVal;
    *oldVal = *oldVal + (diff >> blend);
    return *oldVal;
}

static void fxProgram26Setup(void* data) {
    FxProgramTapeDataType* d = (FxProgramTapeDataType*)data;
    d->buffer = getDelayMemoryPointer();
    
    // Clear buffer
    for(int i=0; i < TAPE_BUF_SIZE; i++) d->buffer[i] = 0;
    
    initSineTable();
    d->writePtr = 0;
    d->rngState = 0x12345678;
    
    d->wowPhase = 0;
    d->flutterPhase = 0;
    
    d->tapeL_old = 0; 
    d->tapeR_old = 0;
    d->dcL = 0;
    d->dcR = 0;
    
    d->quality = 0;
    d->transport = 0;
    d->drive = 2048;  // 50% default
    d->mix = 32767;   // 100% wet
    
    d->presetVolume.gain = 256;
}

// Linear Interpolation Read
static inline int16_t readInterp(int16_t* buf, int32_t ptrQ8) {
    int32_t idx = (ptrQ8 >> 8) & TAPE_BUF_MASK;
    int32_t frac = ptrQ8 & 0xFF; 
    int32_t idx2 = (idx + 1) & TAPE_BUF_MASK;
    int16_t s1 = buf[idx];
    int16_t s2 = buf[idx2];
    return s1 + (((s2 - s1) * frac) >> 8);
}

static void fxProgram26ProcessSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void* data, volatile uint32_t* audioState) {
    FxProgramTapeDataType* d = (FxProgramTapeDataType*)data;
    
    int32_t q = d->quality;     // 0..4095
    int32_t t = d->transport;   // 0..4095
    int32_t drv = d->drive;     // 0..4095
    
    // === 1. INPUT GAIN ===
    int32_t inGain = 256 + (drv >> 4);  // 256..512 (1x..2x in Q8)
    
    int32_t dryL = (*inL * inGain) >> 9;
    int32_t dryR = (*inR * inGain) >> 9;
    
    // === 2. SOFT SATURATION ===
    int32_t absL = (dryL > 0) ? dryL : -dryL;
    int32_t absR = (dryR > 0) ? dryR : -dryR;
    
    int32_t idxL = absL >> 9; if (idxL > 64) idxL = 64;
    int32_t idxR = absR >> 9; if (idxR > 64) idxR = 64;
    
    int32_t satL = (dryL * langevinGainLUT[idxL]) >> 15;
    int32_t satR = (dryR * langevinGainLUT[idxR]) >> 15;
    
    // Soft clip
    if (satL > 16384) satL = 16384 + ((satL - 16384) >> 2);
    else if (satL < -16384) satL = -16384 + ((satL + 16384) >> 2);
    if (satR > 16384) satR = 16384 + ((satR - 16384) >> 2);
    else if (satR < -16384) satR = -16384 + ((satR + 16384) >> 2);
    
    // At high Quality degradation, add extra "copy" saturation passes
    if (q > 2500) {
        // Second saturation pass ("copy of a copy")
        absL = (satL > 0) ? satL : -satL;
        absR = (satR > 0) ? satR : -satR;
        idxL = absL >> 9; if (idxL > 64) idxL = 64;
        idxR = absR >> 9; if (idxR > 64) idxR = 64;
        satL = (satL * langevinGainLUT[idxL]) >> 15;
        satR = (satR * langevinGainLUT[idxR]) >> 15;
    }
    
    // Hard clamp
    if (satL > 32767) satL = 32767; if (satL < -32767) satL = -32767;
    if (satR > 32767) satR = 32767; if (satR < -32767) satR = -32767;
    
    // === 3. BIT CRUSHING at high Quality (copy degradation) ===
    // q > 3000: start reducing bit depth
    if (q > 3000) {
        int32_t crush = (q - 3000) >> 8;  // 0..4
        if (crush > 4) crush = 4;
        // Reduce resolution
        int32_t mask = ~((1 << crush) - 1);
        satL = satL & mask;
        satR = satR & mask;
    }
    
    // === 4. WRITE TO TAPE BUFFER ===
    d->buffer[d->writePtr] = (int16_t)satL;
    d->buffer[d->writePtr + 4096] = (int16_t)satR;
    
    // === 5. TRANSPORT (Wow & Flutter) ===
    uint32_t wowInc = (44739 + ((t * 134000) >> 12)) * AUDIO_SAMPLE_RATE_DIV;
    d->wowPhase += wowInc;
    
    uint32_t flutInc = (447392 + ((t * 447000) >> 12)) * AUDIO_SAMPLE_RATE_DIV;
    d->flutterPhase += flutInc;
    
    int32_t wowDepth = (t * 50) >> 12;      // 0..50 samples (was 30)
    int32_t flutterDepth = (t * 6) >> 12;   // 0..6 samples (was 3)
    
    int32_t wowVal = (getSineLfo(d->wowPhase) * wowDepth) >> 15;
    int32_t flutVal = (getSineLfo(d->flutterPhase) * flutterDepth) >> 15;
    int32_t totalMod = wowVal + flutVal;
    
    // === 6. READ FROM TAPE ===
    int32_t headGap = 500;
    int32_t readIdx = d->writePtr - headGap - totalMod;
    while (readIdx < 0) readIdx += 4096;
    
    int32_t readPosQ8 = readIdx << 8;
    int32_t tapL = readInterp(d->buffer, readPosQ8);
    int32_t tapR = readInterp(d->buffer + 4096, readPosQ8);
    
    // === 7. POST FILTERING (Quality = bandwidth degradation) ===
    // At q=0: minimal filtering. At q=4095: VERY heavy filtering (telephone)
    // Run filter multiple times at high Q for steeper rolloff
    tapL = tapeFilter(tapL, &d->tapeL_old, q);
    tapR = tapeFilter(tapR, &d->tapeR_old, q);
    
    // Second filter pass at q > 2000 (steeper rolloff)
    if (q > 2000) {
        // Use dcL/dcR as second filter state
        int16_t tmpL = (int16_t)tapL;
        int16_t tmpR = (int16_t)tapR;
        tapL = tapeFilter(tmpL, (int16_t*)&d->dcL, q);
        tapR = tapeFilter(tmpR, (int16_t*)&d->dcR, q);
    }
    
    // === 8. NOISE (subtle tape hiss) ===
    // Start later (q > 1000) and keep it warm
    if (q > 1000) {
        int32_t noiseLevel = (q - 1000) >> 5;  // 0..96 (was 0..40)
        uint32_t rnd = xorshift32(&d->rngState);
        int16_t noise = (int16_t)(rnd & 0xFFFF);
        
        // Heavily filtered pink noise (warm hiss, not bright hiss)
        // Multiple averaging passes to remove high frequencies
        int32_t warmNoise = (noise + (int16_t)(rnd >> 16)) >> 1;
        warmNoise = (warmNoise + (int16_t)((rnd >> 8) & 0xFFFF)) >> 1;
        warmNoise = warmNoise >> 1;  // Extra reduction
        
        // Subtle modulation: slightly louder with signal
        int32_t sigLevel = (tapL > 0 ? tapL : -tapL) >> 12;  // 0..8
        int32_t modNoise = (warmNoise * noiseLevel) >> 15;
        modNoise = (modNoise * (8 + sigLevel)) >> 4;
        
        tapL += modNoise;
        tapR += modNoise;
    }
    
    // === 9. RANDOM DROPOUTS at extreme Quality ===
    if (q > 3500) {
        uint32_t rnd = xorshift32(&d->rngState);
        if ((rnd & 0xFFFF) < ((uint32_t)(q - 3500) >> 2)) {
            // Brief dropout
            tapL = tapL >> 2;
            tapR = tapR >> 2;
        }
    }
    
    // === 10. OUTPUT ===
    int32_t outValL = tapL << 1;
    int32_t outValR = tapR << 1;
    
    // Clamp
    if (outValL > 32767) outValL = 32767; if (outValL < -32767) outValL = -32767;
    if (outValR > 32767) outValR = 32767; if (outValR < -32767) outValR = -32767;
    
    d->writePtr = (d->writePtr + 1) & TAPE_BUF_MASK;
    
    *outL = gainStageProcessSample((int16_t)outValL, &d->presetVolume);
    *outR = gainStageProcessSample((int16_t)outValR, &d->presetVolume);
}

static void fxParamQuality(uint16_t val, void* data) { ((FxProgramTapeDataType*)data)->quality = val; }
static void fxDisplayQuality(void* data, char* res) { 
    int val = ((FxProgramTapeDataType*)data)->quality;
    if (val < 1000) appendToString(res, "Studio");
    else if (val < 3000) appendToString(res, "Used");
    else appendToString(res, "Destroyed");
}

static void fxParamTransport(uint16_t val, void* data) { ((FxProgramTapeDataType*)data)->transport = val; }
static void fxDisplayTransport(void* data, char* res) { 
    decimalInt16ToChar(((FxProgramTapeDataType*)data)->transport / 41, res, 2); 
    appendToString(res, "%"); 
}

static void fxParamDrive(uint16_t val, void* data) { ((FxProgramTapeDataType*)data)->drive = val; }
static void fxDisplayDrive(void* data, char* res) { 
    decimalInt16ToChar(((FxProgramTapeDataType*)data)->drive / 41, res, 2); 
    appendToString(res, "% Drive"); 
}



static void fxParamVol(uint16_t val, void* data) { ((FxProgramTapeDataType*)data)->presetVolume.gain = val >> 2; }
static void fxDisplayVol(void* data, char* res) { decimalInt16ToChar(((FxProgramTapeDataType*)data)->presetVolume.gain*39,res,2); appendToString(res, "%"); }

FxProgramType fxProgramGenLoss = {
    .name = "Tape Machine",
    .nParameters = 4,
    .parameters = {
        {.name="Quality", .control=0, .increment=1, .rawValue=0, .setParameter=fxParamQuality, .getParameterDisplay=fxDisplayQuality},
        {.name="Mechanics", .control=1, .increment=1, .rawValue=0, .setParameter=fxParamTransport, .getParameterDisplay=fxDisplayTransport},
        {.name="Drive", .control=2, .increment=1, .rawValue=2048, .setParameter=fxParamDrive, .getParameterDisplay=fxDisplayDrive},
        {.name="Volume", .control=0xff, .increment=1, .rawValue=0x400, .setParameter=fxParamVol, .getParameterDisplay=fxDisplayVol}
    },
    .processSampleStereo = fxProgram26ProcessSampleStereo,
    .setup = fxProgram26Setup,
    .isStereo = 1,
    .data = &progData
};

} // namespace Card_Flux
