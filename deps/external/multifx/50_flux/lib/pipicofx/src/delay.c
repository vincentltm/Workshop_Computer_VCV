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