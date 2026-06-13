#ifndef SAM_H
#define SAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void SAM_Init(void);
void SAM_SetInput(const char* input);
void SAM_SetSpeed(uint8_t speed);
void SAM_SetPitch(uint8_t pitch);
void SAM_SetThroat(uint8_t throat);
void SAM_SetMouth(uint8_t mouth);

// Prepare the buffer for processing
int SAM_Prepare(void);

// Render a single sample (replacement for buffer rendering)
// Returns 8-bit audio sample scaled to int16
int16_t SAM_ProcessSample(void);

// Check if SAM is currently speaking
uint8_t SAM_IsTalking(void);

#ifdef __cplusplus
}
#endif

#endif
