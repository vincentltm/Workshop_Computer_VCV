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
#include "audio/delay.h"
#include "audio/audiotools.h"
 
 int16_t delayMemory[DELAY_LINE_LENGTH];

 DelayDataType singletonDelay;

int16_t * getDelayMemoryPointer()
{
    return delayMemory;
}

// Added ramfunc attribute
__attribute__ ((section (".ramfunc"))) 
void clearDelayLine()
{
    uint32_t* delayMemPtr=(uint32_t*)getDelayMemoryPointer();
    for(uint32_t c=0;c<(DELAY_LINE_LENGTH>>1);c++)
    {
        *(delayMemPtr+c)=0;
    }
}

void initDelay(DelayDataType*data,int16_t *  memoryPointer,uint32_t bufferLength)
{
    data->delayLine = memoryPointer;
    data->delayBufferLength=bufferLength;
    for (uint32_t c=0;c<bufferLength;c++)
    {
        data->delayLine[c]=0;
    }
    data->delayLinePtr=0;
}
} // namespace Card_Flux
