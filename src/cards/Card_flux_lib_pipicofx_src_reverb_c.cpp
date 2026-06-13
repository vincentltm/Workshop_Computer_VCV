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
#include "audio/reverbUtils.h"
#include "audio/reverb.h"
#include "audio/delay.h"
#include "audio/audiotools.h"

/**
 * @brief feedback interpolation values for
 * delay lines of 2011, 2551, 3163, 4093 samples
 * and tau values (time when signal is at 0.001) of 0.1, 0.73333333,1.36666667,2.        
 * 
 */
typedef struct 
{
    const char name[8];
    const uint16_t delayInSamples[4];
    const int16_t feedback[4][4];
    const int16_t taus[4];
    const uint16_t allpassDelays[4];
} reverbParameterType;


static const reverbParameterType reverbParameterSet[4]= {
    {
        .name="solid",
        .delayInSamples = {487, 683, 881, 1087, },
        .feedback = {
            {
            0x3f81, 0x7454, 0x7999, 0x7b96, 
            },{
            0x2fe6, 0x6ff0, 0x771d, 0x79db, 
            },{
            0x2405, 0x6bac, 0x74a7, 0x7822, 
            },{
            0x1ac7, 0x6768, 0x7227, 0x765d, 
            },
        },
        .taus = {100, 733,1366,2000},
        .allpassDelays = {241, 331, 401, 487,}
    },
    {
        .name="saturn",
        .delayInSamples = {809, 1201, 1601, 2003,},
        .feedback = {
            {
            0x27f4, 0x6d34, 0x758b, 0x78c2, 
            },{
            0x16ba, 0x651e, 0x70ca, 0x7566, 
            },{
            0xcc7, 0x5d7c, 0x6c23, 0x7211, 
            },{
            0x72a, 0x5665, 0x67a8, 0x6ed0, 
            },},
        .taus = {100, 733,1366,2000},
        .allpassDelays = {113, 227, 337, 449,}
    },
    {
        .name="uranus",
        .delayInSamples = {1601, 2203, 2801, 3407,},
        .feedback = {
            {
            0xcc7, 0x5d7c, 0x6c23, 0x7211, 
            },{
            0x55f, 0x5311, 0x657f, 0x6d3b, 
            },{
            0x245, 0x49df, 0x5f4d, 0x68a1, 
            },{
            0xf3, 0x4196, 0x5969, 0x642a, 
            },},
        .taus = {100, 733,1366,2000},
        .allpassDelays = {113, 173, 233, 293,}
    },
    {
        .name="neptune",
        .delayInSamples = {241, 503, 761, 1021,},
        .feedback = {
            {
            0x5a7b, 0x7a15, 0x7cc9, 0x7dcb, 
            },{
            0x3e0f, 0x73f7, 0x7964, 0x7b72, 
            },{
            0x2ad0, 0x6e3d, 0x7623, 0x792c, 
            },{
            0x1d72, 0x68c1, 0x72f2, 0x76ee, 
            },},
        .taus = {100, 733,1366,2000},
        .allpassDelays = {293, 353, 419, 487,}
    }
};

const int16_t phaseshifts[4]= {22937,22937,22937,22937};

const char * getReverbParameterSetName(ReverbType*reverbData)
{
    // Safety
    if (reverbData->paramNr > 3) return "unknown";
    return reverbParameterSet[reverbData->paramNr].name;
}

int16_t getFeedback(uint8_t delayLineIndex,int16_t tau,uint8_t paramNr)
{
    int16_t feedbackVal=0;
    if (paramNr > 3) paramNr = 0; // Safety
    
    // Clamp tau to supported range
    if (tau <= reverbParameterSet[paramNr].taus[0]) return reverbParameterSet[paramNr].feedback[delayLineIndex][0];
    if (tau >= reverbParameterSet[paramNr].taus[3]) return reverbParameterSet[paramNr].feedback[delayLineIndex][3];

    for(uint8_t c=0;c<3;c++)
    {
        if (tau >= reverbParameterSet[paramNr].taus[c] && tau < reverbParameterSet[paramNr].taus[c+1])
        {
            feedbackVal = reverbParameterSet[paramNr].feedback[delayLineIndex][c] + (reverbParameterSet[paramNr].feedback[delayLineIndex][c+1]-reverbParameterSet[paramNr].feedback[delayLineIndex][c])*(tau - reverbParameterSet[paramNr].taus[c])/(reverbParameterSet[paramNr].taus[c+1]-reverbParameterSet[paramNr].taus[c]);
            break;
        } 
    }
    return feedbackVal;
}

/**
 * @brief Set the Reverb Time in milliseconds, valid values are from 100 to 2000
 * 
 * @param reverbTime 
 * @param reverbData 
 */
void setReverbTime(int16_t reverbTime,ReverbType*reverbData)
{
    for(uint8_t c=0;c<4;c++)
    {
        reverbData->feedbackValues[c]=getFeedback(c,reverbTime,reverbData->paramNr);
    }
}


void initReverb(ReverbType*reverbData,int16_t reverbTime)
{
    // Legacy support: Use stride 4096 (16k total)
    // Starts at offset 0 of delayMemory?
    // Wait, original initReverb called getDelayMemoryPointer() inside.
    int16_t * delayMemoryPointer = getDelayMemoryPointer();
    initReverbExtended(reverbData, reverbTime, delayMemoryPointer, 4096);
}

void initReverbExtended(ReverbType*reverbData, int16_t reverbTime, int16_t* startAddr, int32_t stride)
{
    // Manually map pointers based on startAddr + stride
    for(uint8_t c=0;c<4;c++)
    {
        reverbData->delayPointers[c] = startAddr + c * stride;
    }
    
    // Set up Allpasses at end of each comb delay?
    // Original: 4096 comb + 1024 allpass?
    // Original logic:
    // delayPointers[c] = base + c*4096.
    // allpasses[c].in = base + 4*4096 + c*1024.
    // So allpasses lived in a specific block 16k-20k (Total 4k for APs).
    
    // In Extended Mode, we assume 'stride' covers Comb + AP?
    // Or we place APs after all combs.
    // Let's place APs *inside* the stride if stride is large enough?
    // Or place them at end of total block?
    
    // Let's define: Stride is for COMB.
    // APs need allocation too.
    // To keep it simple, we'll place APs at end of the 4th stride?
    // Or allocate APs relative to startAddr.
    
    // Let's use:
    // Combs: start + 0, start + stride, start + 2*stride, start + 3*stride.
    // APs: start + 4*stride. (Assuming we have space).
    // APs need 4x 512 + 4x 1024? No.
    // Original: delayLineIn (1024), delayLineOut (512). Total 6k bytes (1536 ints) per AP? 
    // Wait. delayLineOut - delayLineIn = 512.
    // bufferSize = 0x1FF (512).
    // So allpass[c] needs 512 samples.
    // Total 4 Allpasses = 2048 samples.
    
    // Let's place APs at: startAddr + 4 * stride.
    // Ensure caller provides (4*stride + 2048) space!
    
    int16_t* apBase = startAddr + 4 * stride;
    
    for(uint8_t c=0;c<4;c++)
    {
        reverbData->allpasses[c].delayLineIn = apBase + c * 512; 
        reverbData->allpasses[c].delayLineOut = apBase + c * 512; // In-place? or dual buffer?
        // Original: delayLineOut = delayLineIn + 512.
        // Wait, original:
        // delayLineIn = base + 16k + c*1024
        // delayLineOut = base + 16k + c*1024 + 512.
        // So they used 1024 space for each AP, but bufferSize was 512.
        // Probably safe.
        
        reverbData->allpasses[c].delayLineIn = apBase + c * 1024;
        reverbData->allpasses[c].delayLineOut = apBase + c * 1024 + 512;

        reverbData->allpasses[c].oldValues=0;
        reverbData->allpasses[c].coefficient=phaseshifts[c];
        reverbData->allpasses[c].delayPtr=0;
        reverbData->allpasses[c].bufferSize=0x1FF;
        
        // Safety check for paramNr
        uint8_t p = reverbData->paramNr;
        if (p > 3) p=0;
        reverbData->allpasses[c].delayInSamples = reverbParameterSet[p].allpassDelays[c];
    }
    
    setReverbTime(reverbTime,reverbData);
    reverbData->delayPointer=0;    
}

int16_t reverbProcessSample(int16_t sampleIn,ReverbType*reverbData)
{
    int16_t sampleOut;
    int16_t reverbSignal;
    int32_t sampleInterm;
    volatile uint32_t *  audioStatePtr = getAudioStatePtr();

    reverbSignal = 0;

    for (uint8_t rc=0;rc < 4;rc++)
    {
        sampleInterm = sampleIn + 
        ((((reverbData->delayPointers[rc][(reverbData->delayPointer-reverbParameterSet[reverbData->paramNr].delayInSamples[rc]) & 0xFFF] >> 1) + 
           (reverbData->delayPointers[rc][(reverbData->delayPointer-reverbParameterSet[reverbData->paramNr].delayInSamples[rc]-1) & 0xFFF] >> 1))
        *(reverbData->feedbackValues[rc])) >> 15);
        reverbData->delayPointers[rc][reverbData->delayPointer & 0xFFF] = (int16_t)clip(sampleInterm,audioStatePtr);
    }
    reverbData->delayPointer++;
    
    for (uint8_t c=0;c<4;c++)
    {
        reverbSignal += reverbData->delayPointers[c][(reverbData->delayPointer-reverbParameterSet[reverbData->paramNr].delayInSamples[c]) & 0xFFF] >> 2;
    }

    for (uint8_t c=0;c<4;c++)
    {
        reverbSignal = allpassProcessSample(reverbSignal,reverbData->allpasses+c,audioStatePtr);
    }

    sampleOut = ((((1 << 15) - reverbData->mix)*sampleIn) >> 15) + ((reverbData->mix*reverbSignal) >> 15);
    return sampleOut;
}

int16_t reverbProcessSampleWet(int16_t sampleIn,ReverbType*reverbData)
{
    int16_t reverbSignal;
    int32_t sampleInterm;
    volatile uint32_t *  audioStatePtr = getAudioStatePtr();

    reverbSignal = 0;

    for (uint8_t rc=0;rc < 4;rc++)
    {
        sampleInterm = sampleIn + 
        ((((reverbData->delayPointers[rc][(reverbData->delayPointer-reverbParameterSet[reverbData->paramNr].delayInSamples[rc]) & 0xFFF] >> 1) + 
           (reverbData->delayPointers[rc][(reverbData->delayPointer-reverbParameterSet[reverbData->paramNr].delayInSamples[rc]-1) & 0xFFF] >> 1))
        *(reverbData->feedbackValues[rc])) >> 15);
        reverbData->delayPointers[rc][reverbData->delayPointer & 0xFFF] = (int16_t)clip(sampleInterm,audioStatePtr);
    }
    reverbData->delayPointer++;
    
    for (uint8_t c=0;c<4;c++)
    {
        reverbSignal += reverbData->delayPointers[c][(reverbData->delayPointer-reverbParameterSet[reverbData->paramNr].delayInSamples[c]) & 0xFFF] >> 2;
    }

    for (uint8_t c=0;c<4;c++)
    {
        reverbSignal = allpassProcessSample(reverbSignal,reverbData->allpasses+c,audioStatePtr);
    }

    return reverbSignal;
}

} // namespace Card_Flux
