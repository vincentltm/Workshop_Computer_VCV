#ifndef ADPCM_H
#define ADPCM_H

#include <stdint.h>

// IMA ADPCM State
typedef struct {
    int16_t valprev;
    int8_t index;
} AdpcmState;

// Simple Stateless Helpers if needed, but State struct is better for streams
static const int8_t adpcmIndexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

static const int16_t adpcmStepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

static inline void Adpcm_Init(AdpcmState* s) {
    s->valprev = 0;
    s->index = 0;
}

static inline int16_t Adpcm_Decode(AdpcmState* s, uint8_t nibble) {
    int step = adpcmStepTable[s->index];
    int diffq = step >> 3;
    if (nibble & 4) diffq += step;
    if (nibble & 2) diffq += (step >> 1);
    if (nibble & 1) diffq += (step >> 2);
    
    if (nibble & 8) s->valprev -= diffq;
    else s->valprev += diffq;
    
    if (s->valprev > 32767) s->valprev = 32767;
    else if (s->valprev < -32768) s->valprev = -32768;
    
    s->index += adpcmIndexTable[nibble];
    if (s->index < 0) s->index = 0;
    if (s->index > 88) s->index = 88;
    
    return s->valprev;
}

#endif
