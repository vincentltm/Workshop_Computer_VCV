#include "adpcm.h"

// IMA ADPCM Index Table
static const int8_t indexTable[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8
};

// IMA ADPCM Step Table
static const uint16_t stepTable[89] = {
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

void Adpcm_Init(AdpcmState* state) {
    state->prevSample = 0;
    state->prevIndex = 0;
}

int16_t Adpcm_Decode(AdpcmState* state, uint8_t nibble) {
    int32_t step = stepTable[state->prevIndex];
    int32_t diff = step >> 3;

    if (nibble & 4) diff += step;
    if (nibble & 2) diff += (step >> 1);
    if (nibble & 1) diff += (step >> 2);

    if (nibble & 8) state->prevSample -= diff;
    else state->prevSample += diff;

    // Clamp Sample
    if (state->prevSample > 32767) state->prevSample = 32767;
    else if (state->prevSample < -32768) state->prevSample = -32768;

    // Update Index
    state->prevIndex += indexTable[nibble & 7];
    if (state->prevIndex < 0) state->prevIndex = 0;
    else if (state->prevIndex > 88) state->prevIndex = 88;

    return state->prevSample;
}

void Adpcm_DecodeBlock(AdpcmState* state, const uint8_t* inData, int16_t* outData, uint32_t nSamples) {
    // nSamples is number of OUTPUT samples.
    // Each input byte is 2 samples.
    for (uint32_t i = 0; i < nSamples; i++) {
        uint8_t byte = inData[i >> 1];
        uint8_t nibble = (i & 1) ? (byte >> 4) : (byte & 0x0F); // Low nibble first? 
        // Standard IMA usually sends Low Nibble first.
        outData[i] = Adpcm_Decode(state, nibble);
    }
}
