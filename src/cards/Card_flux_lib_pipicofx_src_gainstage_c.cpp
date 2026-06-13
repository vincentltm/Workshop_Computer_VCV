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
} // namespace Card_Flux
