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
#include "audio/compressor.h"
#include "romfunc.h"
#include "fastExpLog.h"
#include "audio/firstOrderIirFilter.h"



int16_t compressor3ProcessSample(int16_t sampleIn,CompressorDataType*data)
{
    int16_t absSample;
    int16_t sampleOut;
    int32_t intermAvg;

    sampleOut = applyGain(sampleIn,data->currentAvg,data);
    /*
    if(sampleOut < 0)
    {
        absSample = -sampleOut;
    }
    else
    {
        absSample = sampleOut;
    }*/
    if(sampleIn < 0)
    {
        absSample = -sampleIn;
    }
    else
    {
        absSample = sampleIn;
    }
    intermAvg = firstOrderIirDualCoeffLPProcessSample(absSample,&data->avgLowpass);
    data->currentAvg = (int16_t)intermAvg; 
    return sampleOut;
}


} // namespace Card_Flux
