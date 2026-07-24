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

#include "audio/gainstage.h"
#include "audio/audiotools.h"

void initGainstage(GainStageDataType*data)
{
    data->gain=256;
    data->offset=0;
}

int16_t gainStageProcessSample(int16_t sampleIn,GainStageDataType*data)
{
    int16_t sampleOut;
    int32_t sampleWord = (int32_t)sampleIn;
    volatile uint32_t * audioStatePtr = getAudioStatePtr();
    sampleWord = sampleWord* data->gain;
    sampleWord >>= 8;
    sampleWord = sampleWord + data->offset;

    sampleOut = (int16_t)clip(sampleWord,audioStatePtr);
    return sampleOut;
}
