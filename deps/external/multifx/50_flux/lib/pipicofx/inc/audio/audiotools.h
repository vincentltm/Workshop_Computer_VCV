#ifndef _AUDIOTOOLS_H_
#define _AUDIOTOOLS_H_


#define AUDIO_STATE_ON 0
#define AUDIO_STATE_BUFFER_UNDERRUN 1
#define AUDIO_STATE_INPUT_ON 2
#define AUDIO_STATE_INPUT_BUFFER_OVERRUN 3
#define AUDIO_STATE_DMA_FAILURE 4
#define AUDIO_STATE_INPUT_CLIPPED 5
#define AUDIO_STATE_OUTPUT_CLIPPED 6

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

volatile uint32_t * getAudioStatePtr(); 

// Removed ramfunc attribute for static inline functions
static inline int32_t clip(int32_t sample, volatile uint32_t* audioStatePtr)
{
    if (sample > 32767)
    {
        *audioStatePtr |= (1 << AUDIO_STATE_OUTPUT_CLIPPED);
        return 32767;
    }
    else if (sample < -32768)
    {
        *audioStatePtr |= (1 << AUDIO_STATE_OUTPUT_CLIPPED);
        return -32768;
    }
    else
    {
        return sample;
    }
}

static inline int32_t clip_input(int32_t sample, volatile uint32_t* audioStatePtr)
{
    if (sample > 32767)
    {
        *audioStatePtr |= (1 << AUDIO_STATE_INPUT_CLIPPED);
        return 32767;
    }
    else if (sample < -32768)
    {
        *audioStatePtr |= (1 << AUDIO_STATE_INPUT_CLIPPED);
        return -32768;
    }
    else
    {
        return sample;
    }
}

typedef int16_t (*AudioProcessor)(int16_t sampleIn,void*data,volatile uint32_t*audioState);
typedef void (*AudioProcessorStereo)(int16_t* sampleInL, int16_t* sampleInR, int16_t* sampleOutL, int16_t* sampleOutR, void*data, volatile uint32_t*audioState); 

#ifdef __cplusplus
}
#endif

#endif