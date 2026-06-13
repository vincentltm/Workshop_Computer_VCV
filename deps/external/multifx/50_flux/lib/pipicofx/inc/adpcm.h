#ifndef ADPCM_H
#define ADPCM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// IMA ADPCM State
typedef struct {
    int16_t prevSample;
    int8_t  prevIndex;
} AdpcmState;

void Adpcm_Init(AdpcmState* state);

// Decode one nibble (4-bits) into one 16-bit sample
int16_t Adpcm_Decode(AdpcmState* state, uint8_t nibble);

// Decode sample block
// Returns number of samples decoded
void Adpcm_DecodeBlock(AdpcmState* state, const uint8_t* inData, int16_t* outData, uint32_t nSamples);

#ifdef __cplusplus
}
#endif

#endif
