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
#include "audio/bitcrusher.h"


void initBitcrusher(BitCrusherDataType*data)
{
    data->bitmask= (0xFFFF);
}

void setBitMask(uint8_t resolution,BitCrusherDataType*data)
{
    data->bitmask=0;
    for (uint8_t c=0;c<(16-resolution);c++)
    {
        data->bitmask <<=1;
        data->bitmask += 1;
    }
    data->bitmask = (~(data->bitmask)) | 0x8000;
}

int16_t bitCrusherProcessSample(int16_t sampleIn,BitCrusherDataType*data)
{
    return (int16_t)(((uint16_t)sampleIn) & data->bitmask);

}

} // namespace Card_Flux
