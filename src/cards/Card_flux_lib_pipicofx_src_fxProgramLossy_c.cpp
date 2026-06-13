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

// FxProgram27: Lossy (Digital Compression & Glitch)
// Simulates MP3/AAC compression artifacts, packet loss, digital glitching

#define LOSSY_BUF_SIZE 2048 // ~42ms at 48kHz
#define FREEZE_SIZE 256

static FxProgram27DataType progData;

// Xorshift for randomness
static uint32_t lossyRng = 987654321;
static inline uint32_t lossyRandom() {
    uint32_t x = lossyRng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return lossyRng = x;
}

static void fxProgram27Setup(void* data) {
    FxProgram27DataType* d = (FxProgram27DataType*)data;
    d->buffer = getDelayMemoryPointer();
    
    // Clear buffers
    for(int i=0; i<LOSSY_BUF_SIZE * 2; i++) d->buffer[i] = 0;
    for(int i=0; i<FREEZE_SIZE; i++) {
        d->freezeBufferL[i] = 0;
        d->freezeBufferR[i] = 0;
    }
    
    d->writePtr = 0;
    d->freezePtr = 0;
    d->lossAmount = 0;
    d->speed = 2048; // Medium speed
    d->packetMode = 0; // Clean
    d->mix = 16384; // 50%
    d->presetVolume.gain = 256;
    
    d->freezeActive = 0;
    d->speedCounter = 0;
    d->packetCounter = 0;
    d->packetLossActive = 0;
    d->lastSampleL = 0;
    d->lastSampleR = 0;
    d->compressedL = 0;
    d->compressedR = 0;
    d->sampleHoldCounter = 0;
}

static void fxProgram27SetupStereo(void* data) {
    fxProgram27Setup(data);
}

// DCT-like compression (simplified psychoacoustic-ish quantization)
static inline int16_t compressLossy(int16_t sample, int16_t lossAmt) {
    // Quantize more aggressively based on loss amount
    // This simulates MP3 quantization
    int16_t shift = lossAmt >> 9; // 0 to 8
    if (shift > 8) shift = 8;
    
    // Coarse quantization
    int16_t mask = (0xFFFF << shift);
    int16_t quant = sample & mask;
    
    // Add quantization noise for "MP3 shimmer"
    if (lossAmt > 512) {
        int16_t noise = (int16_t)(lossyRandom() & ((1 << shift) - 1));
        quant += noise;
    }
    
    return quant;
}

static void fxProgram27ProcessSampleStereo(int16_t* inL, int16_t* inR, int16_t* outL, int16_t* outR, void* data, volatile uint32_t* audioState) {
    FxProgram27DataType* d = (FxProgram27DataType*)data;
    
    int32_t dryL = *inL;
    int32_t dryR = *inR;
    
    // 1. Write to circular buffer
    d->buffer[d->writePtr] = *inL;
    d->buffer[d->writePtr + LOSSY_BUF_SIZE] = *inR;
    
    // 2. Speed Counter (controls update rate)
    d->speedCounter++;
    int32_t updateRate = (1 + (d->speed >> 7)) / AUDIO_SAMPLE_RATE_DIV; // Scale for sample rate
    
    int16_t wetL = *inL;
    int16_t wetR = *inR;
    
    // 3. Loss Algorithm (MP3-style compression)
    if (d->speedCounter >= updateRate) {
        d->speedCounter = 0;
        
        // Apply lossy compression
        int32_t tempL = compressLossy(*inL, d->lossAmount);
        int32_t tempR = compressLossy(*inR, d->lossAmount);
        
        // Safety clamp
        if (tempL > 32700) tempL = 32700; if (tempL < -32700) tempL = -32700;
        if (tempR > 32700) tempR = 32700; if (tempR < -32700) tempR = -32700;
        
        d->compressedL = (int16_t)tempL;
        d->compressedR = (int16_t)tempR;
        
        // Dynamic range compression (harsh limiting)
        if (d->lossAmount > 1024) {
            int32_t thresh = 32767 - (d->lossAmount >> 1);
            if (d->compressedL > thresh) d->compressedL = thresh;
            if (d->compressedL < -thresh) d->compressedL = -thresh;
            if (d->compressedR > thresh) d->compressedR = thresh;
            if (d->compressedR < -thresh) d->compressedR = -thresh;
        }
        
        d->lastSampleL = d->compressedL;
        d->lastSampleR = d->compressedR;
    }
    
    wetL = d->lastSampleL;
    wetR = d->lastSampleR;
    
    // 4. Packet Loss/Repeat Mode
    d->packetCounter++;
    int32_t packetRate = (200 + (d->speed >> 3)) / AUDIO_SAMPLE_RATE_DIV; // Varies with speed
    
    if (d->packetCounter >= packetRate) {
        d->packetCounter = 0;
        
        if (d->packetMode == 0) {
            // Clean - no packet effect
            d->packetLossActive = 0;
        } else if (d->packetMode == 1) {
            // Packet Loss - random dropouts
            if ((lossyRandom() & 0xFF) < 128) {
                d->packetLossActive = 1;
            } else {
                d->packetLossActive = 0;
            }
        } else {
            // Packet Repeat - freeze/stutter
            d->packetLossActive = 0;
            // Capture freeze
            int32_t freezeLen = FREEZE_SIZE;
            for(int i=0; i<freezeLen; i++) {
                int32_t idx = (d->writePtr - i) & (LOSSY_BUF_SIZE - 1);
                d->freezeBufferL[i] = d->buffer[idx];
                d->freezeBufferR[i] = d->buffer[idx + LOSSY_BUF_SIZE];
            }
            d->freezePtr = 0;
            d->freezeActive = 1;
        }
    }
    
    // Apply packet effects
    if (d->packetLossActive) {
        // Silence (packet loss)
        wetL = 0;
        wetR = 0;
    } else if (d->freezeActive && d->packetMode == 2) {
        // Packet repeat - play freeze buffer
        wetL = d->freezeBufferL[d->freezePtr];
        wetR = d->freezeBufferR[d->freezePtr];
        d->freezePtr++;
        if (d->freezePtr >= FREEZE_SIZE) {
            d->freezeActive = 0;
            d->freezePtr = 0;
        }
    }
    
    // 5. Additional "MP3 Sparkle" - high frequency quantization noise
    if (d->lossAmount > 2048) {
        int16_t sparkle = (int16_t)((lossyRandom() & 0x3FF) - 512);
        int32_t sparkleAmt = (d->lossAmount - 2048) >> 6;
        wetL += (sparkle * sparkleAmt) >> 8;
        wetR += (sparkle * sparkleAmt) >> 8;
    }
    
    // 6. Aliasing (simulate sample rate reduction artifacts)
    if (d->lossAmount > 1536) {
        // Add mirror frequencies
        static int16_t aliasL = 0, aliasR = 0;
        aliasL = (aliasL + wetL) >> 1;
        aliasR = (aliasR + wetR) >> 1;
        wetL = (wetL + aliasL) >> 1;
        wetR = (wetR + aliasR) >> 1;
    }
    
    // Advance write pointer
    d->writePtr++;
    if (d->writePtr >= LOSSY_BUF_SIZE) d->writePtr = 0;
    
    // Mix
    int32_t mx = d->mix;
    int32_t outL_val = ((dryL * (32767 - mx)) >> 15) + ((wetL * mx) >> 15);
    int32_t outR_val = ((dryR * (32767 - mx)) >> 15) + ((wetR * mx) >> 15);
    
    *outL = gainStageProcessSample((int16_t)outL_val, &d->presetVolume);
    *outR = gainStageProcessSample((int16_t)outR_val, &d->presetVolume);
}

// MACRO PARAMETERS
// Main Knob: Mix (direct control)
static void fxParamMix(uint16_t val, void* data) {
    ((FxProgram27DataType*)data)->mix = val << 3;
}
static void fxDisplayMix(void* data, char* res) {
    Int16ToChar(((FxProgram27DataType*)data)->mix/328, res);
    appendToString(res, "%");
}

// X Knob: Compression (Loss Amount)
static void fxParamCompression(uint16_t val, void* data) {
    FxProgram27DataType* d = (FxProgram27DataType*)data;
    d->lossAmount = val;
}
static void fxDisplayCompression(void* data, char* res) {
    Int16ToChar(((FxProgram27DataType*)data)->lossAmount/41, res);
    appendToString(res, "%");
}

// Y Knob: Glitch (Speed + Packet Mode macro)
// 0-33%: Fast clean
// 33-66%: Medium with packet loss
// 66-100%: Slow with packet repeat (freeze)
static void fxParamGlitch(uint16_t val, void* data) {
    FxProgram27DataType* d = (FxProgram27DataType*)data;
    
    if (val < 1365) {
        // Fast & Clean
        d->speed = 4095 - (val * 2); // Fast (high value)
        d->packetMode = 0; // Clean
    } else if (val < 2730) {
        // Medium & Packet Loss
        d->speed = 2048; // Medium
        d->packetMode = 1; // Packet Loss
    } else {
        // Slow & Freeze/Stutter
        d->speed = (val - 2730) >> 1; // Slow (low value)
        d->packetMode = 2; // Packet Repeat
    }
}
static void fxDisplayGlitch(void* data, char* res) {
    FxProgram27DataType* d = (FxProgram27DataType*)data;
    if (d->packetMode == 0) appendToString(res, "Clean");
    else if (d->packetMode == 1) appendToString(res, "Loss");
    else appendToString(res, "Freeze");
}

static void fxParamVol(uint16_t val, void* data) {
    ((FxProgram27DataType*)data)->presetVolume.gain = val >> 2;
}
static void fxDisplayVol(void* data, char* res) {
    decimalInt16ToChar(((FxProgram27DataType*)data)->presetVolume.gain*39, res, 2);
    appendToString(res, "%");
}

FxProgramType fxProgramLossy = {
    .name = "Lossy",
    .nParameters = 4,
    .parameters = {
        {.name="Mix", .control=0, .increment=1, .rawValue=2048, .setParameter=fxParamMix, .getParameterDisplay=fxDisplayMix},
        {.name="Compress", .control=1, .increment=1, .rawValue=0, .setParameter=fxParamCompression, .getParameterDisplay=fxDisplayCompression},
        {.name="Glitch", .control=2, .increment=1, .rawValue=0, .setParameter=fxParamGlitch, .getParameterDisplay=fxDisplayGlitch},
        {.name="Volume", .control=0xff, .increment=1, .rawValue=0x400, .setParameter=fxParamVol, .getParameterDisplay=fxDisplayVol}
    },
    .processSampleStereo = fxProgram27ProcessSampleStereo,
    .setup = fxProgram27SetupStereo,
    .isStereo = 1,
    .data = &progData
};

} // namespace Card_Flux
