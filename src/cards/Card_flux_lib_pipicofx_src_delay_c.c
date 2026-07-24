/* Automatically generated C wrapper (compiled as C99, not C++) */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <math.h>
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
#include "pico_mocks_c.h"

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
