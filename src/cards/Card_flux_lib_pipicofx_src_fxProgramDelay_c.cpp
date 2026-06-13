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
#include <cstring>
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
#include "audio/audiotools.h"
#include "audio/delay.h"
#include "pipicofx/fxPrograms.h"
#include "stringFunctions.h"

/* Tape Loop (mode 3): 2 packed (µ-law) bytes per int16 word in delay line */
#define TAPE_LOOP_PACKED_SAMPLES ((uint32_t)DELAY_LINE_LENGTH * 2u)
#define TAPE_LOOP_MARGIN 2048u

// Stereo Data Type with new fields
typedef struct {
  DelayDataType delayL;
  DelayDataType delayR;
  GainStageDataType presetVolume;
  uint8_t mode; // 0: Normal, 1: PingPong, 2: Tape, 3: Lofi

  // Tape Filter State (Simple Lowpass)
  int16_t tapeL_old;
  int16_t tapeR_old;

  // Parameter Smoothing for Time
  int32_t timeSmoothed; // Q8 fixed point (value * 256)
  int32_t timeTarget;   // Target value (raw samples)

  // Lofi State
  uint64_t lofiPhase;    // Q16 phase accumulator (64-bit for large buffer span)
  int16_t inputLpf;      // Input lowpass state
  int16_t lofiOutLpf;    // Non-recursive output lowpass state
  int16_t lofiL_hold;    // Sample and hold outputs
  int16_t lofiR_hold;    // Used as WritePhase (0/1) in packed mode
  uint32_t lfoPhase;     // LFO Phase accumulator
  uint16_t lastTimeKnob; // Hysteresis state

  int16_t freeze; // Freeze Parameter (0..4095). 0=Off, >2048=On.
} FxProgram6StereoDataType;

// Helper: Simple fixed lowpass for Tape mode
static inline int16_t tapeFilter(int16_t sample, int16_t *oldVal) {
  int32_t diff = (int32_t)sample - *oldVal;
  *oldVal = *oldVal + (diff >> 2);
  return *oldVal;
}

// Helper: Mu-Law Encoding (14-bit signed -> 8-bit)
static inline int8_t linear2ulaw(int16_t sample) {
  const uint16_t BIAS = 0x84; // 132
  int16_t sign = (sample >> 8) & 0x80;
  if (sign != 0)
    sample = -sample;
  if (sample > 32767)
    sample = 32767;

  sample = (sample >> 2); // Convert to 14-bit
  sample += BIAS;
  if (sample > 0x7FFF)
    sample = 0x7FFF;

  uint16_t seg = 7;
  for (int i = 0x4000; i >= 64; i >>= 1) {
    if (sample & i)
      break;
    seg--;
  }

  uint8_t uval = (seg << 4) | ((sample >> (seg + 3)) & 0x0F);
  return (int8_t)(uval | sign);
}

// Helper: Mu-Law Decoding (8-bit -> 16-bit)
static inline int16_t ulaw2linear(int8_t u_val) {
  const uint16_t BIAS = 0x84;
  uint8_t uval = (uint8_t)u_val;
  int16_t t;

  int16_t sign = (uval & 0x80) ? -1 : 1;
  uval &= 0x7F;

  uint8_t seg = (uval >> 4) & 0x07;
  uint8_t quant = uval & 0x0F;

  t = (quant << 3) + BIAS;
  t <<= seg;

  return ((t - BIAS) << 2) * sign;
}

// Helper: Read a specific packed sample
static inline int16_t readPackedSample(FxProgram6StereoDataType *pData,
                                       uint32_t idx) {
  uint32_t len = pData->delayL.delayBufferLength;
  uint32_t maxSamples = len << 1;

  while (idx >= maxSamples)
    idx -= maxSamples;

  uint32_t wordIdx = (idx >> 1);
  int16_t memWord = pData->delayL.delayLine[wordIdx];

  int8_t bVal;
  if ((idx & 1) == 0)
    bVal = (int8_t)(memWord & 0xFF);
  else
    bVal = (int8_t)(memWord >> 8);

  return ulaw2linear(bVal);
}

// Mono processing (legacy stub)
static int16_t fxProgram6processSample(int16_t sampleIn, void *data) {
  return sampleIn;
}

// Stereo processing
static void fxProgram6processSampleStereo(int16_t *inL, int16_t *inR,
                                          int16_t *outL, int16_t *outR,
                                          void *data,
                                          volatile uint32_t *audioState) {
  FxProgram6StereoDataType *pData = (FxProgram6StereoDataType *)data;
  uint8_t frozen = (pData->freeze > 2048);

  // 0. Update Control State
  int32_t targetQ8 = pData->timeTarget << 8;
  int32_t diff = targetQ8 - pData->timeSmoothed;
  pData->timeSmoothed += (diff >> 9);

  int32_t newDelay = (int32_t)(pData->timeSmoothed >> 8);
  if (newDelay < 1)
    newDelay = 1;

  if (pData->mode != 3) {
    if (newDelay > 43000)
      newDelay = 43000;
    pData->delayL.delayInSamples = newDelay;

    if (pData->mode == 2) {
      int32_t dR = newDelay + (newDelay >> 6);
      if (dR > 44000)
        dR = 44000;
      pData->delayR.delayInSamples = dR;
    } else {
      pData->delayR.delayInSamples = newDelay;
    }
  } else {
    uint32_t bufferCapacity = pData->delayL.delayBufferLength << 1;
    pData->delayL.delayInSamples = bufferCapacity - 1024;
    pData->delayR.delayInSamples = pData->delayL.delayInSamples;
  }

  // --- MODE 3: LOFI TAPE LOOP (SMOOTH VARISPEED) ---
  if (pData->mode == 3) {
    const uint32_t bufferCapacity = TAPE_LOOP_PACKED_SAMPLES;
    const uint32_t loopDistance = (bufferCapacity > TAPE_LOOP_MARGIN)
                                      ? (bufferCapacity - TAPE_LOOP_MARGIN)
                                      : (bufferCapacity / 2u);
    uint64_t maxPhase = (uint64_t)bufferCapacity << 16;

    // 1. Calculate Tape Speed (timeSmoothed is Q8; maps to varispeed phaseInc)
    // Tuned for AUDIO_BASE_RATE (~24 kHz): ~2 s .. ~12 s loop length range
    // (at 48 kHz equivalent these would be half the wall time). Allow phaseInc
    // above 65536 so "fast" settings are actually usable at lower sample rates.
    uint64_t dividend = 174000ULL << 24;
    int32_t phaseInc = (int32_t)(dividend / (uint64_t)pData->timeSmoothed);
    if (phaseInc < 16384)
      phaseInc = 16384;
    if (phaseInc > 1048576)
      phaseInc = 1048576;

    // 2. Speed-Dependent Warble
    pData->lfoPhase += (4 * AUDIO_SAMPLE_RATE_DIV);
    int16_t lfo = (pData->lfoPhase >> 8) & 0x1FF;
    if (lfo > 255)
      lfo = 511 - lfo;
    if (pData->lfoPhase & 0x10000)
      lfo = -lfo;

    int32_t warbleIntensity = (65536 - phaseInc) >> 8;
    int32_t warbleQ8 = ((int32_t)(lfo - 128) * warbleIntensity) >> 7;

    // 3. Continuous Read
    int32_t writeIdxQ8 = (int32_t)(pData->lofiPhase >> 8);
    int32_t readIdxQ8 = writeIdxQ8 - ((int32_t)loopDistance << 8) + warbleQ8;
    int32_t bufSizeQ8 = (int32_t)bufferCapacity << 8;
    while (readIdxQ8 < 0)
      readIdxQ8 += bufSizeQ8;
    while (readIdxQ8 >= bufSizeQ8)
      readIdxQ8 -= bufSizeQ8;

    uint32_t rInt = (uint32_t)(readIdxQ8 >> 8);
    uint32_t rFrac = (uint32_t)(readIdxQ8 & 0xFF);
    uint32_t rNext = rInt + 1;
    if (rNext >= bufferCapacity)
      rNext = 0;

    uint32_t originalLen = pData->delayL.delayBufferLength;
    pData->delayL.delayBufferLength = DELAY_LINE_LENGTH;
    int16_t s1 = readPackedSample(pData, rInt);
    int16_t s2 = readPackedSample(pData, rNext);
    pData->delayL.delayBufferLength = originalLen;

    int16_t tapOut = s1 + (((s2 - s1) * (int32_t)rFrac) >> 8);

    // 4. Output Stage Filtering
    int32_t filterAlphaOut = (phaseInc >> 1) + 16384;
    if (filterAlphaOut > 65535)
      filterAlphaOut = 65535;

    int32_t diff_out = (int32_t)tapOut - pData->lofiOutLpf;
    pData->lofiOutLpf += (diff_out * filterAlphaOut) >> 16;
    int16_t finalWet = pData->lofiOutLpf;

    // 5. Selective Tape Write (Clocked by Speed)
    uint32_t oldIntPtr = (uint32_t)(pData->lofiPhase >> 16);
    pData->lofiPhase += phaseInc;
    if (pData->lofiPhase >= maxPhase)
      pData->lofiPhase -= maxPhase;
    uint32_t newIntPtr = (uint32_t)(pData->lofiPhase >> 16);

    // FREEZE LOGIC: If frozen, SKIP WRITE.
    if (newIntPtr != oldIntPtr && !frozen) {
      int16_t monoIn = (*inL + *inR) >> 1;

      int32_t filterAlphaIn = (phaseInc >> 1) + 8192;
      if (filterAlphaIn > 65535)
        filterAlphaIn = 65535;

      int32_t diff_in = (int32_t)monoIn - pData->inputLpf;
      pData->inputLpf += (diff_in * filterAlphaIn) >> 16;
      monoIn = pData->inputLpf;

      int32_t feed = (tapOut * pData->delayL.feedback) >> 15;
      feed = (feed * 13) >> 4;

      int32_t sum = (int32_t)monoIn + feed;
      if (sum > 24576)
        sum = 24576 + ((sum - 24576) >> 2);
      else if (sum < -24576)
        sum = -24576 + ((sum + 24576) >> 2);
      if (sum > 32767)
        sum = 32767;
      if (sum < -32767)
        sum = -32767;

      int8_t sample8 = linear2ulaw((int16_t)sum);

      uint32_t wIdx = newIntPtr;
      uint32_t wordIdx = (wIdx >> 1);
      if ((wIdx & 1) == 0) {
        pData->delayL.delayLine[wordIdx] =
            (pData->delayL.delayLine[wordIdx] & 0xFF00) | (uint8_t)sample8;
      } else {
        pData->delayL.delayLine[wordIdx] =
            (pData->delayL.delayLine[wordIdx] & 0x00FF) |
            ((uint16_t)(uint8_t)sample8 << 8);
      }
    }

    // 6. Loop output mixing (match dry/wet scaling to avoid +6 dB wet boost)
    int16_t mix = pData->delayL.mix;
    int32_t wet = ((int32_t)mix * (int32_t)finalWet) >> 15;
    *outL = (((32767 - mix) * (*inL)) >> 15) + wet;
    *outR = (((32767 - mix) * (*inR)) >> 15) + wet;

    *outL = gainStageProcessSample(*outL, &pData->presetVolume);
    *outR = gainStageProcessSample(*outR, &pData->presetVolume);
    return;
  }
  // --- END LOFI MODE ---

  // 1. Read
  int16_t tapL = getDelayedSample(&pData->delayL);
  int16_t tapR = getDelayedSample(&pData->delayR);

  // 2. Output
  int16_t mix = pData->delayL.mix;
  *outL = (((32767 - mix) * (*inL)) >> 15) + ((mix * tapL) >> 14);
  *outR = (((32767 - mix) * (*inR)) >> 15) + ((mix * tapR) >> 14);

  // 3. Feedback Source
  int16_t feedInL, feedInR;
  if (pData->mode == 1) {
    feedInL = tapR;
    feedInR = tapL;
  } else {
    feedInL = tapL;
    feedInR = tapR;
  }

  // 4. Feedback Gain (Freeze Logic)
  int32_t fbGainL = frozen ? 32767 : pData->delayL.feedback;
  int32_t fbGainR = frozen ? 32767 : pData->delayR.feedback;

  int32_t feedL = (feedInL * fbGainL) >> 15;
  int32_t feedR = (feedInR * fbGainR) >> 15;

  // 5. Tape Filtering (Bypass in Freeze)
  if (pData->mode == 2 && !frozen) {
    feedL = tapeFilter((int16_t)feedL, &pData->tapeL_old);
    feedR = tapeFilter((int16_t)feedR, &pData->tapeR_old);
  }

  // 6. Sum (Mute Input if Frozen)
  int32_t sumL, sumR;
  if (frozen) {
    sumL = feedL;
    sumR = feedR;
  } else {
    if (pData->mode == 1) {
      sumL = (int32_t)((*inL + *inR) >> 2) + feedL;
      sumR = feedR;
    } else {
      sumL = (int32_t)(*inL >> 1) + feedL;
      sumR = (int32_t)(*inR >> 1) + feedR;
    }
  }

  // 7. Saturation
  if (sumL > 16384)
    sumL = 16384 + ((sumL - 16384) >> 2);
  else if (sumL < -16384)
    sumL = -16384 + ((sumL + 16384) >> 2);
  if (sumR > 16384)
    sumR = 16384 + ((sumR - 16384) >> 2);
  else if (sumR < -16384)
    sumR = -16384 + ((sumR + 16384) >> 2);

  // Hard Clamp
  if (sumL > 32767)
    sumL = 32767;
  if (sumL < -32767)
    sumL = -32767;
  if (sumR > 32767)
    sumR = 32767;
  if (sumR < -32767)
    sumR = -32767;

  // 8. Write
  addSampleToDelayline((int16_t)sumL, &pData->delayL);
  addSampleToDelayline((int16_t)sumR, &pData->delayR);

  *outL = gainStageProcessSample(*outL, &pData->presetVolume);
  *outR = gainStageProcessSample(*outR, &pData->presetVolume);
}

// Callbacks
static void fxProgram6Param1Callback(uint16_t val, void *data) { // Time
  FxProgram6StereoDataType *pData = (FxProgram6StereoDataType *)data;

  int16_t diff = (int16_t)val - (int16_t)pData->lastTimeKnob;
  if (diff < 0)
    diff = -diff;
  if (diff < 4)
    return;
  pData->lastTimeKnob = val;

  uint32_t minTime, maxTime;
  if (pData->mode == 3) {
    /* ~2 s min .. ~12 s max loop at 24 kHz (was ~7 s .. ~30 s at half rate) */
    minTime = 47000;
    maxTime = 283000;
  } else {
    minTime = 0;
    maxTime = 43000;
  }

  uint32_t wVal = minTime + (((uint32_t)val * (maxTime - minTime)) >> 12);
  pData->timeTarget = wVal;
  if (pData->timeSmoothed == 0)
    pData->timeSmoothed = wVal << 8;
}

static void fxProgram6Param1Display(void *data, char *res) {
  FxProgram6StereoDataType *pData = (FxProgram6StereoDataType *)data;
  int16_t dval;
  if (pData->mode == 3) {
    /* Approximate loop length in ms from smoothed time target */
    uint32_t tt = (uint32_t)(pData->timeSmoothed >> 8);
    if (tt < 1)
      tt = 1;
    uint64_t phaseInc64 = (174000ULL << 16) / (uint64_t)tt;
    if (phaseInc64 > 1048576ULL)
      phaseInc64 = 1048576ULL;
    if (phaseInc64 < 16384ULL)
      phaseInc64 = 16384ULL;
    uint32_t loopSamp = (uint32_t)((TAPE_LOOP_PACKED_SAMPLES * 65536ULL) / phaseInc64);
    dval = (int16_t)(loopSamp / (AUDIO_BASE_RATE / 1000));
    if (dval > 9999)
      dval = 9999;
  } else {
    dval = (pData->timeSmoothed >> 8) / (AUDIO_BASE_RATE / 1000);
  }
  Int16ToChar(dval, res);
  for (uint8_t c = 0; c < PARAMETER_NAME_MAXLEN - 2; c++) {
    if (*(res + c) == 0) {
      *(res + c) = 'm';
      *(res + c + 1) = 's';
      *(res + c + 2) = (char)0;
      break;
    }
  }
}

static void fxProgram6Param2Callback(uint16_t val, void *data) { // Feedback
  FxProgram6StereoDataType *pData = (FxProgram6StereoDataType *)data;
  uint32_t wVal = val << 3;
  pData->delayL.feedback = (int16_t)wVal;
  pData->delayR.feedback = (int16_t)wVal;
}

static void fxProgram6Param2Display(void *data, char *res) {
  FxProgram6StereoDataType *pData = (FxProgram6StereoDataType *)data;
  Int16ToChar(pData->delayL.feedback / 328, res);
  for (uint8_t c = 0; c < PARAMETER_NAME_MAXLEN - 1; c++) {
    if (*(res + c) == 0) {
      *(res + c) = '%';
      *(res + c + 1) = (char)0;
      break;
    }
  }
}

static void fxProgram6Param3Callback(uint16_t val, void *data) { // Mix
  FxProgram6StereoDataType *pData = (FxProgram6StereoDataType *)data;
  int16_t wVal = val << 3;
  pData->delayL.mix = wVal;
  pData->delayR.mix = wVal;
}

static void fxProgram6Param3Display(void *data, char *res) {
  FxProgram6StereoDataType *pData = (FxProgram6StereoDataType *)data;
  Int16ToChar(pData->delayL.mix / 328, res);
  for (uint8_t c = 0; c < PARAMETER_NAME_MAXLEN - 1; c++) {
    if (*(res + c) == 0) {
      *(res + c) = '%';
      *(res + c + 1) = (char)0;
      break;
    }
  }
}

static void fxProgram6ParamFreezeCallback(uint16_t val, void *data) {
  ((FxProgram6StereoDataType *)data)->freeze = val;
}

static void fxProgramPresetVolumeCallback(uint16_t val, void *data) {
  ((FxProgram6StereoDataType *)data)->presetVolume.gain = val >> 2;
}

static void fxProgramPresetVolumeDisplay(void *data, char *res) {
  FxProgram6StereoDataType *pData = (FxProgram6StereoDataType *)data;
  decimalInt16ToChar(pData->presetVolume.gain * 39, res, 2);
  for (uint8_t c = 0; c < PARAMETER_NAME_MAXLEN - 1; c++) {
    if (*(res + c) == 0) {
      *(res + c) = '%';
      *(res + c + 1) = (char)0;
      break;
    }
  }
}

// Setup Functions
static void fxProgram6SetupStereo(void *data) {
  FxProgram6StereoDataType *pData = (FxProgram6StereoDataType *)data;
  int16_t *delayMem = getDelayMemoryPointer();
  initDelay(&pData->delayL, delayMem, DELAY_LINE_LENGTH / 2);
  initDelay(&pData->delayR, delayMem + (DELAY_LINE_LENGTH / 2),
            DELAY_LINE_LENGTH / 2);
  pData->mode = 0;
  pData->tapeL_old = 0;
  pData->tapeR_old = 0;
  pData->timeTarget = 480 << 3;
  pData->timeSmoothed = pData->timeTarget << 8;
  pData->lofiPhase = 0;
  pData->inputLpf = 0;
  pData->lofiL_hold = 0;
  pData->lofiR_hold = 0;
  pData->lfoPhase = 0;
  pData->lastTimeKnob = 0;
  pData->freeze = 0;
}

static void fxProgram6SetupStereoNormal(void *data) {
  fxProgram6SetupStereo(data);
  ((FxProgram6StereoDataType *)data)->mode = 0;
}
static void fxProgram6SetupStereoPingPong(void *data) {
  fxProgram6SetupStereo(data);
  ((FxProgram6StereoDataType *)data)->mode = 1;
}
static void fxProgram6SetupStereoTape(void *data) {
  fxProgram6SetupStereo(data);
  ((FxProgram6StereoDataType *)data)->mode = 2;
}
static void fxProgram6SetupStereoLofi(void *data) {
  FxProgram6StereoDataType *pData = (FxProgram6StereoDataType *)data;
  int16_t *delayMem = getDelayMemoryPointer();
  initDelay(&pData->delayL, delayMem, DELAY_LINE_LENGTH);
  initDelay(&pData->delayR, delayMem, 100);
  pData->mode = 3;
  pData->lofiPhase = 0;
  pData->lofiL_hold = 0;
  pData->lofiR_hold = 0;
  pData->lfoPhase = 0;
  pData->lastTimeKnob = 0;
  pData->timeTarget = 165000;
  pData->timeSmoothed = pData->timeTarget << 8;
  pData->freeze = 0;
}

// Shared Data Initialization
FxProgram6StereoDataType fxProgram6data = {
    .delayL.mix = 16384,
    .delayR.mix = 16384,
    .presetVolume = {.gain = 0xff, .offset = 0},
    .mode = 0,
    .timeSmoothed = 0,
    .timeTarget = 0,
    .tapeL_old = 0,
    .tapeR_old = 0,
    .lofiPhase = 0,
    .inputLpf = 0,
    .lofiOutLpf = 0,
    .lofiL_hold = 0,
    .lofiR_hold = 0,
    .lfoPhase = 0,
    .lastTimeKnob = 0,
    .freeze = 0};

// Program Definitions
FxProgramType fxProgramDelay = {
    .name = "Delay",
    .nParameters = 5,
    .parameters = {{.name = "Mix            ",
                    .control = 0,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param3Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param3Callback},
                   {.name = "Time           ",
                    .control = 1,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param1Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param1Callback},
                   {.name = "Feedback       ",
                    .control = 2,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param2Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param2Callback},
                   {.name = "Freeze",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0,
                    .setParameter = fxProgram6ParamFreezeCallback,
                    .getParameterValue = 0,
                    .getParameterDisplay = 0},
                   {.name = "Volume",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0x3ff,
                    .setParameter = fxProgramPresetVolumeCallback,
                    .getParameterValue = 0,
                    .getParameterDisplay = fxProgramPresetVolumeDisplay}},
    .processSample = &fxProgram6processSample,
    .processSampleStereo = &fxProgram6processSampleStereo,
    .setup = &fxProgram6SetupStereoNormal,
    .reset = 0,
    .isStereo = 1,
    .data = (void *)&fxProgram6data};

FxProgramType fxProgramPingPong = {
    .name = "Ping Pong Delay",
    .nParameters = 5,
    .parameters = {{.name = "Mix            ",
                    .control = 0,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param3Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param3Callback},
                   {.name = "Time           ",
                    .control = 1,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param1Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param1Callback},
                   {.name = "Feedback       ",
                    .control = 2,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param2Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param2Callback},
                   {.name = "Freeze",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0,
                    .setParameter = fxProgram6ParamFreezeCallback,
                    .getParameterValue = 0,
                    .getParameterDisplay = 0},
                   {.name = "Volume",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0x3ff,
                    .setParameter = fxProgramPresetVolumeCallback,
                    .getParameterValue = 0,
                    .getParameterDisplay = fxProgramPresetVolumeDisplay}},
    .processSample = &fxProgram6processSample,
    .processSampleStereo = &fxProgram6processSampleStereo,
    .setup = &fxProgram6SetupStereoPingPong,
    .reset = 0,
    .isStereo = 1,
    .data = (void *)&fxProgram6data};

FxProgramType fxProgramTape = {
    .name = "Tape Delay",
    .nParameters = 5,
    .parameters = {{.name = "Mix            ",
                    .control = 0,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param3Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param3Callback},
                   {.name = "Time           ",
                    .control = 1,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param1Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param1Callback},
                   {.name = "Feedback       ",
                    .control = 2,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param2Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param2Callback},
                   {.name = "Freeze",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0,
                    .setParameter = fxProgram6ParamFreezeCallback,
                    .getParameterValue = 0,
                    .getParameterDisplay = 0},
                   {.name = "Volume",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0x3ff,
                    .setParameter = fxProgramPresetVolumeCallback,
                    .getParameterValue = 0,
                    .getParameterDisplay = fxProgramPresetVolumeDisplay}},
    .processSample = &fxProgram6processSample,
    .processSampleStereo = &fxProgram6processSampleStereo,
    .setup = &fxProgram6SetupStereoTape,
    .reset = 0,
    .isStereo = 1,
    .data = (void *)&fxProgram6data};

FxProgramType fxProgramLofiDelay = {
    .name = "Tape Loop",
    .nParameters = 5,
    .parameters = {{.name = "Mix            ",
                    .control = 0,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param3Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param3Callback},
                   {.name = "Time           ",
                    .control = 1,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param1Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param1Callback},
                   {.name = "Feedback       ",
                    .control = 2,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgram6Param2Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgram6Param2Callback},
                   {.name = "Freeze",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0,
                    .setParameter = fxProgram6ParamFreezeCallback,
                    .getParameterValue = 0,
                    .getParameterDisplay = 0},
                   {.name = "Volume",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0x3ff,
                    .setParameter = fxProgramPresetVolumeCallback,
                    .getParameterValue = 0,
                    .getParameterDisplay = fxProgramPresetVolumeDisplay}},
    .processSample = &fxProgram6processSample,
    .processSampleStereo = &fxProgram6processSampleStereo,
    .setup = &fxProgram6SetupStereoLofi,
    .reset = 0,
    .isStereo = 1,
    .data = (void *)&fxProgram6data};
} // namespace Card_Flux
