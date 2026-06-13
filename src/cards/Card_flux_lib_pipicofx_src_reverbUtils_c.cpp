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
#include "stdint.h"
#include "audio/reverbUtils.h"
#include "audio/audiotools.h"

// Added ramfunc attribute to definition
__attribute__ ((section (".ramfunc"))) 
int16_t morphingAllpassProcessSample(int16_t sampleIn,AllpassType*allpass,AudioProcessor processor,void * processorData,volatile uint32_t * audioStatePtr)
{
    int16_t sampleOut;
    int32_t sampleInterm;
    
    uint16_t idx = (allpass->delayPtr - allpass->delayInSamples) & allpass->bufferSize;
    int16_t delayedIn = *(allpass->delayLineIn + idx);
    int16_t delayedOut = *(allpass->delayLineOut + idx);

    sampleInterm = ((allpass->coefficient * sampleIn) >> 15) + delayedIn - ((delayedOut * allpass->coefficient) >> 15);

    sampleInterm=clip(sampleInterm,audioStatePtr);
    sampleOut = (int16_t)sampleInterm;
    *(allpass->delayLineIn + allpass->delayPtr) = sampleIn;
    *(allpass->delayLineOut + allpass->delayPtr) = processor(sampleOut,processorData,audioStatePtr);
    allpass->delayPtr++;
    allpass->delayPtr &= allpass->bufferSize;
    return sampleOut;
}

// Added ramfunc attribute to definition
__attribute__ ((section (".ramfunc"))) 
void hadamardDiffuserProcessArray(int32_t * channels,HadamardDiffuserType*data,volatile uint32_t * audioStatePtr)
{
    int32_t sum_first, sum_second;
    int32_t diff_first, diff_second;
    for(uint8_t c=0;c<4;c++)
    {
        data->delayPointers[c][data->delayPointer] = clip(channels[c],audioStatePtr);
    }

    // Use & (size-1) assuming power of 2 size
    uint16_t ptr = data->delayPointer;
    uint16_t mask = data->diffusorSize - 1;

    sum_first = (data->delayPointers[0][(ptr - data->delayTimes[0]) & mask] ) +
                (data->delayPointers[1][(ptr - data->delayTimes[1]) & mask] ); 
    sum_second = (data->delayPointers[2][(ptr - data->delayTimes[2]) & mask] ) +
                (data->delayPointers[3][(ptr - data->delayTimes[3]) & mask] );   
    diff_first = (data->delayPointers[0][(ptr - data->delayTimes[0]) & mask] ) -
                (data->delayPointers[1][(ptr - data->delayTimes[1]) & mask] );  
    diff_second = (data->delayPointers[2][(ptr - data->delayTimes[2]) & mask] ) -
                (data->delayPointers[3][(ptr - data->delayTimes[3]) & mask] );                       
    channels[0] = (sum_first + sum_second) >> 1;
    channels[1] = (diff_first + diff_second) >> 1;
    channels[2] = (sum_first - sum_second) >> 1;
    channels[3] = (diff_first + diff_second) >> 1;

    data->delayPointer++;
    data->delayPointer &= mask;
}

} // namespace Card_Flux
