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
#include "adpcm.h"
#include "voice_data.h"
#include "pipicofx/picofxCore.h"
#include "stringFunctions.h"

// "Chatter" - Pitch shifting ADPCM player with Echo

#define DELAY_LEN 8192 // Reduced to fix RAM overflow

typedef struct {
    AdpcmState adpcm1;
    AdpcmState adpcm2;
    const uint8_t* currentSample;
    uint32_t sampleLen;
    
    // Voice 1 (Main)
    uint32_t phase1;
    uint32_t phaseInc1; // 1.0 = 65536
    uint32_t sampleIdx1;
    int16_t out1;

    // Voice 2 (Sub Octave)
    uint32_t phase2;
    uint32_t phaseInc2;
    uint32_t sampleIdx2;
    int16_t out2;

    int16_t lastTrigger;
    uint8_t currentSampleIndex; // 0-40 = Fixed, 255 = Random Mode
    
    int16_t delayBuffer[DELAY_LEN];
    uint16_t delayHead;
    uint16_t delayFeedback; 
} FxProgramSpeechDataType;

static void fxProgramSpeechSetup(void* data) {
    FxProgramSpeechDataType* pData = (FxProgramSpeechDataType*)data;
    Adpcm_Init(&pData->adpcm1);
    Adpcm_Init(&pData->adpcm2);
    pData->currentSample = 0;
    pData->lastTrigger = 0;
    
    for(int i=0;i<DELAY_LEN;i++) pData->delayBuffer[i] = 0;
    pData->delayHead = 0;
    pData->delayFeedback = 0;
}

static void fxProgramSpeechParam1Callback(uint16_t val, void* data) {
    FxProgramSpeechDataType* pData = (FxProgramSpeechDataType*)data;
    // Pitch: 0.25x to 2.0x
    pData->phaseInc1 = 16384 + (val * 28);
    pData->phaseInc2 = pData->phaseInc1 >> 1; // Octave down
}

static void fxProgramSpeechParam1Display(void* data, char* res) {
    appendToString(res, "Pitch/Speed");
}

static void fxProgramSpeechParam2Callback(uint16_t val, void* data) {
    FxProgramSpeechDataType* pData = (FxProgramSpeechDataType*)data;
    pData->delayFeedback = val; 
}

static void fxProgramSpeechParam2Display(void* data, char* res) {
    appendToString(res, "Echo Feedback");
}

static void fxProgramSpeechParam3Callback(uint16_t val, void* data) {
    FxProgramSpeechDataType* pData = (FxProgramSpeechDataType*)data;
    if (val > 3800) {
        pData->currentSampleIndex = 255; // Random Mode
    } else {
        pData->currentSampleIndex = val / 100; // 0..38 roughly
    }
}

static void fxProgramSpeechParam3Display(void* data, char* res) {
    FxProgramSpeechDataType* pData = (FxProgramSpeechDataType*)data;
    if (pData->currentSampleIndex == 255) {
        appendToString(res, "Random Texture");
    } else {
        appendToString(res, "Select Sample");
    }
}

// Simple internal random
static uint32_t speech_rand_seed = 12345;
static uint32_t speech_rand() {
    speech_rand_seed = speech_rand_seed * 1103515245 + 12345;
    return (speech_rand_seed / 65536) % 32768;
}

static int16_t fxProgramSpeechProcessSample(int16_t sampleIn, void* data) {
    FxProgramSpeechDataType* pData = (FxProgramSpeechDataType*)data;
    
    // Trigger on input
    int16_t absIn = (sampleIn > 0) ? sampleIn : -sampleIn;
    if (absIn > 2000 && pData->lastTrigger < 2000) { // Lowered threshold from 12000
        // Retrigger
        Adpcm_Init(&pData->adpcm1);
        Adpcm_Init(&pData->adpcm2);
        
        int idx = 0;
        uint32_t startOffset = 0;

        if (pData->currentSampleIndex == 255) {
            // Random Mode
            idx = speech_rand() % 40; 
            uint32_t len = getVoiceLen(idx);
            if (len > 2000) {
                startOffset = speech_rand() % (len - 1000);
                startOffset &= 0xFFFFFFFE; // Align to even for nibble logic
            }
        } else {
            idx = pData->currentSampleIndex;
            if (idx > 40) idx = 40;
        }

        pData->currentSample = getVoiceData(idx);
        pData->sampleLen = getVoiceLen(idx);
        
        // Reset Voices with Offset
        pData->sampleIdx1 = startOffset;
        pData->phase1 = 0;
        pData->out1 = 0;
        
        pData->sampleIdx2 = startOffset; 
        pData->phase2 = 0;
        pData->out2 = 0;
    }
    pData->lastTrigger = absIn;

    // Process Voice 1
    if (pData->currentSample && pData->sampleIdx1 < pData->sampleLen) {
        pData->phase1 += pData->phaseInc1;
        while (pData->phase1 >= 65536) {
            pData->phase1 -= 65536;
            // ADPCM Decode
            uint8_t byte = pData->currentSample[pData->sampleIdx1 >> 1];
            uint8_t nibble = (pData->sampleIdx1 & 1) ? (byte >> 4) : (byte & 0x0F);
            pData->out1 = Adpcm_Decode(&pData->adpcm1, nibble);
            pData->sampleIdx1++;
            if (pData->sampleIdx1 >= pData->sampleLen) break;
        }
    } else {
        pData->out1 = 0;
    }

    // Process Voice 2 (Sub Octave)
    if (pData->currentSample && pData->sampleIdx2 < pData->sampleLen) {
        pData->phase2 += pData->phaseInc2;
        while (pData->phase2 >= 65536) {
            pData->phase2 -= 65536;
             // ADPCM Decode
            uint8_t byte = pData->currentSample[pData->sampleIdx2 >> 1];
            uint8_t nibble = (pData->sampleIdx2 & 1) ? (byte >> 4) : (byte & 0x0F);
            pData->out2 = Adpcm_Decode(&pData->adpcm2, nibble);
            pData->sampleIdx2++;
            if (pData->sampleIdx2 >= pData->sampleLen) break;
        }
    } else {
        pData->out2 = 0;
    }

    // Mix (50/50)
    int32_t voiceMix = (pData->out1 + pData->out2) >> 1;
    
    // Delay Logic
    int32_t delayOut = pData->delayBuffer[pData->delayHead];
    int32_t feedback = (delayOut * pData->delayFeedback) >> 12; 
    
    // Write new value to delay buffer
    int32_t newVal = voiceMix + feedback;
    if (newVal > 32767) newVal = 32767; 
    if (newVal < -32768) newVal = -32768;
    
    pData->delayBuffer[pData->delayHead] = (int16_t)newVal;
    pData->delayHead++;
    if (pData->delayHead >= DELAY_LEN) pData->delayHead = 0;
    
    return (int16_t)newVal;
}

FxProgramSpeechDataType fxProgramSpeechData;

FxProgramType fxProgramSpeech = {
    .name = "Chatter",
    .parameters = {
        {
            .name = "Pitch",
            .control = 0,
            .rawValue = 2048,
            .increment = 64,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgramSpeechParam1Display,
            .setParameter = fxProgramSpeechParam1Callback
        },
        {
            .name = "Echo",
            .control = 1,
            .rawValue = 0,
            .increment = 64,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgramSpeechParam2Display,
            .setParameter = fxProgramSpeechParam2Callback
        },
        {
            .name = "Sample",
            .control = 2,
            .rawValue = 0,
            .increment = 100,
            .getParameterValue = 0,
            .getParameterDisplay = fxProgramSpeechParam3Display,
            .setParameter = fxProgramSpeechParam3Callback
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
    .processSample = fxProgramSpeechProcessSample,
    .processSampleStereo = 0,
    .setup = fxProgramSpeechSetup,
    .reset = 0,
    .nParameters = 3,
    .isStereo = 0,
    .data = (void*)&fxProgramSpeechData
};

} // namespace Card_Flux
