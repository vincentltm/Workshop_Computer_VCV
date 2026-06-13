#ifndef _DELAY_H_
#define _DELAY_H_
#include <stdint.h>
#define DELAY_LINE_LENGTH 88500
#include "audiotools.h"

typedef struct {
  int16_t *delayLine;
  uint32_t delayLinePtr;
  int32_t delayInSamples;
  int16_t feedback;
  uint32_t delayBufferLength;
  int16_t mix;
  AudioProcessor feedbackFunction;
  void *feebackData;
} DelayDataType;

void initDelay(DelayDataType *data, int16_t *memoryPointer,
               uint32_t bufferLength);
int16_t *getDelayMemoryPointer();
void clearDelayLine(); // Removed attribute

// Inline Implementations - Removed ramfunc attributes

static inline int16_t delayLineProcessSample(int16_t sampleIn,
                                             DelayDataType *data) {
  if (!data || !data->delayLine || data->delayBufferLength == 0) {
    return sampleIn;
  }

  int32_t delayInSamples = data->delayInSamples;
  if (delayInSamples < 1) {
    delayInSamples = 1;
  } else if ((uint32_t)delayInSamples >= data->delayBufferLength) {
    delayInSamples = (int32_t)data->delayBufferLength - 1;
  }

  int32_t delayIdx;
  int16_t sampleOut;
  int32_t sampleFedBack;
  volatile uint32_t *audioStatePtr = getAudioStatePtr();

  // Non-POT Safe Wrapping
  int32_t readPtr = (int32_t)data->delayLinePtr - delayInSamples;
  if (readPtr < 0)
    readPtr += data->delayBufferLength;
  while (readPtr < 0)
    readPtr += data->delayBufferLength;

  delayIdx = readPtr;

  sampleOut = ((*(data->delayLine + delayIdx) * data->mix) >> 15) +
              ((sampleIn * (32767 - data->mix)) >> 15);
  sampleFedBack = *(data->delayLine + delayIdx);

  if (data->feedbackFunction != 0) {
    sampleFedBack = (int32_t)data->feedbackFunction(
        (int16_t)sampleFedBack, data->feebackData, audioStatePtr);
  }
  sampleFedBack = ((data->feedback * sampleFedBack) >> 15);

  sampleFedBack = clip(sampleIn + sampleFedBack, audioStatePtr);
  *(data->delayLine + data->delayLinePtr) = (int16_t)sampleFedBack;

  data->delayLinePtr++;
  if (data->delayLinePtr >= data->delayBufferLength)
    data->delayLinePtr = 0;

  return sampleOut;
}

static inline int16_t delayLineWetProcessSample(int16_t sampleIn,
                                                DelayDataType *data) {
  if (!data || !data->delayLine || data->delayBufferLength == 0) {
    return sampleIn;
  }

  int32_t delayInSamples = data->delayInSamples;
  if (delayInSamples < 1) {
    delayInSamples = 1;
  } else if ((uint32_t)delayInSamples >= data->delayBufferLength) {
    delayInSamples = (int32_t)data->delayBufferLength - 1;
  }

  int32_t delayIdx;
  int16_t sampleOut;
  int32_t sampleFedBack;
  volatile uint32_t *audioStatePtr = getAudioStatePtr();

  int32_t readPtr = (int32_t)data->delayLinePtr - delayInSamples;
  if (readPtr < 0)
    readPtr += data->delayBufferLength;
  while (readPtr < 0)
    readPtr += data->delayBufferLength;

  delayIdx = readPtr;

  sampleOut = *(data->delayLine + delayIdx);
  sampleFedBack = *(data->delayLine + delayIdx);

  if (data->feedbackFunction != 0) {
    sampleFedBack = (int32_t)data->feedbackFunction(
        (int16_t)sampleFedBack, data->feebackData, audioStatePtr);
  }
  sampleFedBack = ((data->feedback * sampleFedBack) >> 15);

  sampleFedBack = clip(sampleIn + sampleFedBack, audioStatePtr);
  *(data->delayLine + data->delayLinePtr) = (int16_t)sampleFedBack;

  data->delayLinePtr++;
  if (data->delayLinePtr >= data->delayBufferLength)
    data->delayLinePtr = 0;

  return sampleOut;
}

static inline int16_t getDelayedSample(DelayDataType *data) {
  if (!data || !data->delayLine || data->delayBufferLength == 0) {
    return 0;
  }

  int32_t delayInSamples = data->delayInSamples;
  if (delayInSamples < 1) {
    delayInSamples = 1;
  } else if ((uint32_t)delayInSamples >= data->delayBufferLength) {
    delayInSamples = (int32_t)data->delayBufferLength - 1;
  }

  int32_t delayIdx;
  int16_t sampleOut;

  int32_t readPtr = (int32_t)data->delayLinePtr - delayInSamples;
  if (readPtr < 0)
    readPtr += data->delayBufferLength;
  while (readPtr < 0)
    readPtr += data->delayBufferLength;

  delayIdx = readPtr;
  sampleOut = *(data->delayLine + delayIdx);

  return sampleOut;
}

static inline void addSampleToDelayline(int16_t sampleIn, DelayDataType *data) {
  if (!data || !data->delayLine || data->delayBufferLength == 0) {
    return;
  }

  *(data->delayLine + data->delayLinePtr) = sampleIn;
  data->delayLinePtr++;
  if (data->delayLinePtr >= data->delayBufferLength)
    data->delayLinePtr = 0;
}

#endif