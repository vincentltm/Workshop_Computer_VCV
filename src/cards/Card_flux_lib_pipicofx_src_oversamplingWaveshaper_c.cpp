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
#include "audio/oversamplingWaveshaper.h"
#include "audio/secondOrderIirFilter.h"
/* stripped system include */
// #include "drivers/i2s.h"

//v oversampling factor as power of two
#define OVERSAMPLING_FACTOR 2 



void initOversamplingWaveshaper(OversamplingWaveshaperDataType*data)
{
    data->oldValue = 0;
    initWaveShaper(&data->waveshaper,&waveShaperDefaultOverdrive);
}

//uint16_t oversampledBuffer[AUDIO_BUFFER_SIZE*2*(1 << OVERSAMPLING_FACTOR)];

/*
void  applyOversamplingDistortion(uint16_t*data,OversamplingWaveshaperDataType* waveshaper)
{
    int16_t oversample;
    for (uint16_t c=0;c<AUDIO_BUFFER_SIZE*2*(1 << OVERSAMPLING_FACTOR);c++)
    {
        if ((c&OVERSAMPLING_FACTOR)!=0)
        {
            oversample=data[c >> OVERSAMPLING_FACTOR];
        }
        else
        {
            oversample=0;
        }

        if ((c&OVERSAMPLING_FACTOR)!=0)
        {
            data[c >> OVERSAMPLING_FACTOR] = oversample;
        }
    }
}
*/
int16_t  OversamplingDistortionProcessSample(int16_t sample,OversamplingWaveshaperDataType* waveshaper)
{
    int32_t osVal1, osVal2, osVal3, osVal4;
    int16_t outVal;
    int16_t diff = sample - waveshaper->oldValue;
    osVal1 = waveshaper->oldValue + (diff >> 2); // old + 0.25*(new-old)
    osVal2 = waveshaper->oldValue + (diff >> 1); // old +0.5*(new-old)
    osVal3 = sample - (diff >> 2); // old + 0.75*(new-old)
    osVal4 = sample; // new
    waveshaper->oldValue=sample;
    osVal1 = waveShaperProcessSample(osVal1,&waveshaper->waveshaper);
    osVal2 = waveShaperProcessSample(osVal2,&waveshaper->waveshaper);
    osVal3 = waveShaperProcessSample(osVal3,&waveshaper->waveshaper);
    osVal4 = waveShaperProcessSample(osVal4,&waveshaper->waveshaper);

    outVal = (int16_t)((osVal1+osVal2+osVal3+osVal4) >> 2);
    //osVal4 = secondOrderIirFilterProcessSample(osVal4,&waveshaper->oversamplingFilter);
    return outVal;
}

void oversamplingWaveshaperReset(OversamplingWaveshaperDataType*data)
{
    data->oldValue=0;
}


} // namespace Card_Flux
