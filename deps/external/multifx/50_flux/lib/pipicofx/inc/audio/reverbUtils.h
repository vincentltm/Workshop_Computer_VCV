#ifndef _REVERB_UTILS_C_
#define _REVERB_UTILS_C_
#include "stdint.h"
#include "audiotools.h"

typedef struct 
{
    int16_t coefficient;
    uint16_t delayPtr;
    uint16_t delayInSamples;
    int16_t oldValues;
    uint16_t bufferSize; 
    int16_t * delayLineIn;
    int16_t * delayLineOut;
} AllpassType;

typedef struct 
{
    int16_t * delayPointers[4];
    int16_t delayTimes[4];
    int16_t delayPointer;
    uint16_t diffusorSize;
} HadamardDiffuserType;


// Removed ramfunc attribute
static inline int16_t allpassProcessSample(int16_t sampleIn, AllpassType* allpass, volatile uint32_t* audioStatePtr)
{
    int16_t sampleOut;
    int32_t sampleInterm;
    
    uint16_t idx = (allpass->delayPtr - allpass->delayInSamples) & allpass->bufferSize;
    int16_t delayedIn = *(allpass->delayLineIn + idx);
    int16_t delayedOut = *(allpass->delayLineOut + idx);
    
    sampleInterm = ((allpass->coefficient * sampleIn) >> 15) + delayedIn - ((delayedOut * allpass->coefficient) >> 15);
    
    sampleInterm = clip(sampleInterm, audioStatePtr);
    sampleOut = (int16_t)sampleInterm;
    
    *(allpass->delayLineIn + allpass->delayPtr) = sampleIn;
    *(allpass->delayLineOut + allpass->delayPtr) = sampleOut;
    
    allpass->delayPtr++;
    allpass->delayPtr &= allpass->bufferSize;
    
    return sampleOut;
}

// Declarations without attributes (attributes moved to definitions)
int16_t morphingAllpassProcessSample(int16_t sampleIn,AllpassType*allpass,AudioProcessor processor,void * processorData,volatile uint32_t * audioStatePtr);
void hadamardDiffuserProcessArray(int32_t * channels,HadamardDiffuserType*data,volatile uint32_t * audioStatePtr);

#endif