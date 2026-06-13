#include "audio/audiotools.h"
#include "audio/reverbUtils.h"
#include "pipicofx/fxPrograms.h"
#include "pipicofx/picofxCore.h"
#include "stringFunctions.h"

// Granular Cloud + Buffer Optimized Plate Reverb

#define N_GRAINS 8
#define BUFFER_SIZE 47000

// -- Plate Constants --
#define P_AP_IN_1 113
#define P_AP_IN_2 85
#define P_AP_IN_3 301
#define P_AP_IN_4 220

#define P_MOD_L 541
#define P_D1_L 3593
#define P_AP_T_L 1451
#define P_D2_L 3003

#define P_MOD_R 732
#define P_D1_R 3401
#define P_AP_T_R 1925
#define P_D2_R 2550

static inline int16_t plateLowpass(int16_t sample, int16_t *state,
                                   int16_t alpha) {
  int32_t diff = (int32_t)sample - *state;
  *state = *state + ((diff * alpha) >> 15);
  return *state;
}
static int16_t processPlateModAP(int16_t x, DelayDataType *d, int16_t modOffset,
                                 int16_t g) {
  int32_t nominal = d->delayInSamples;
  int32_t actual = nominal - modOffset;
  if (actual < 1)
    actual = 1;
  int32_t rdPtr = (int32_t)d->delayLinePtr - actual;
  int32_t mask = d->delayBufferLength - 1;
  int16_t w_delayed = *(d->delayLine + (rdPtr & mask));
  int32_t w_n = x + ((g * w_delayed) >> 15);
  if (w_n > 32767)
    w_n = 32767;
  if (w_n < -32767)
    w_n = -32767;
  *(d->delayLine + d->delayLinePtr) = (int16_t)w_n;
  d->delayLinePtr = (d->delayLinePtr + 1) & mask;
  int32_t y = ((-g * w_n) >> 15) + w_delayed;
  return (int16_t)y;
}

typedef struct {
  int32_t posQ8;
  int32_t speedQ8;
  int32_t winPhase;
  int32_t winLength;
  int32_t recipHalfLen; // Pre-computed: (1 << 30) / (winLength >> 1) for fast
                        // window calc
  int16_t pan;
  uint8_t active;
} Grain;

typedef struct {
  DelayDataType delayL;

  // Diffusion
  AllpassType diffL[2];
  AllpassType diffR[2];

  // -- Plate State --
  AllpassType pApIn[4];
  DelayDataType pModL, pDelay1L, pDelay2L;
  AllpassType pApTankL;
  int16_t pLpL;
  DelayDataType pModR, pDelay1R, pDelay2R;
  AllpassType pApTankR;
  int16_t pLpR;
  uint32_t pLfoPhase;

  GainStageDataType presetVolume;

  Grain grains[N_GRAINS];
  uint32_t rngSeed;
  uint8_t phase24k;

  // Parameters
  int16_t mix;
  int16_t grainSize;
  int16_t atmosphere;
  int16_t width;
  int16_t freeze; // Freeze State (0..4095)

  // Derived
  int16_t feedback;
  int16_t pitchDrift;
  int16_t densityMask;
  int16_t reverbMix;
  int16_t plateDecay;
  int32_t fbLpState; // Feedback lowpass state (moved from static local)

} FxProgram23DataType;

// Helper: Random
static inline uint32_t fast_rand(uint32_t *seed) {
  *seed = *seed * 1103515245 + 12345;
  return *seed;
}

static inline int16_t readGrainSample(DelayDataType *delay, int32_t readIdxQ8) {
  int32_t idx = readIdxQ8 >> 8;
  int32_t frac = readIdxQ8 & 0xFF;
  if (idx >= BUFFER_SIZE)
    idx -= BUFFER_SIZE;
  if (idx < 0)
    idx += BUFFER_SIZE;
  int32_t idx2 = idx + 1;
  if (idx2 >= BUFFER_SIZE)
    idx2 = 0;
  int16_t s1 = delay->delayLine[idx];
  int16_t s2 = delay->delayLine[idx2];
  return s1 + (((s2 - s1) * frac) >> 8);
}

static void respawnGrain(FxProgram23DataType *pData, int id) {
  uint32_t maxDelay = BUFFER_SIZE - 2000;
  uint32_t r = fast_rand(&pData->rngSeed);
  uint32_t offset = (r % maxDelay) + 500;

  int32_t ptr = pData->delayL.delayLinePtr;
  int32_t start = ptr - offset;
  if (start < 0)
    start += BUFFER_SIZE;
  pData->grains[id].posQ8 = start << 8;

  int16_t driftAmt = pData->pitchDrift;
  r = fast_rand(&pData->rngSeed);
  int32_t drift = 0;
  if (driftAmt > 0)
    drift = (r % (driftAmt * 2 + 1)) - driftAmt;
  pData->grains[id].speedQ8 = 256 + drift;

  int32_t baseSize = 500 + (pData->grainSize >> 1);
  r = fast_rand(&pData->rngSeed);
  pData->grains[id].winLength = baseSize + ((r >> 16) % 1000);

  // Pre-compute reciprocal for window calculation: (1 << 30) / (len >> 1)
  int32_t halfLen = pData->grains[id].winLength >> 1;
  if (halfLen > 0) {
    pData->grains[id].recipHalfLen = (1 << 30) / halfLen;
  } else {
    pData->grains[id].recipHalfLen = (1 << 30); // Avoid div by zero
  }

  r = fast_rand(&pData->rngSeed);
  uint8_t r1 = (r >> 16) & 0x7F;
  uint8_t r2 = (r >> 8) & 0x7F;
  pData->grains[id].pan = r1 + r2;

  pData->grains[id].winPhase = 0;
  pData->grains[id].active = 1;
}

static int16_t fxProgram23processSample(int16_t sampleIn, void *data) {
  return sampleIn;
}

static void fxProgram23processSampleStereo(int16_t *inL, int16_t *inR,
                                           int16_t *outL, int16_t *outR,
                                           void *data,
                                           volatile uint32_t *audioState) {
  FxProgram23DataType *pData = (FxProgram23DataType *)data;
  static int16_t wetL_hold = 0;
  static int16_t wetR_hold = 0;
  int16_t mn = (*inL + *inR) >> 1;
  uint8_t frozen = (pData->freeze > 2048);

  // At 24kHz effective rate, process every sample (no decimation needed)
  {

    // 1. Grains
    int32_t accumL = 0;
    int32_t accumR = 0;
    int activeCount = pData->densityMask;

    for (int i = 0; i < N_GRAINS; i++) {
      if (i >= activeCount) {
        pData->grains[i].winPhase++;
        if (pData->grains[i].winPhase >= pData->grains[i].winLength)
          respawnGrain(pData, i);
        continue;
      }
      int32_t ph = pData->grains[i].winPhase;
      int32_t len = pData->grains[i].winLength;
      int32_t win = 0;
      // Hann-like parabolic window: smoother than triangle, avoids clicks
      // win = 4 * ph * (len - ph) / (len * len), scaled to Q15
      // Using pre-computed reciprocal: recip = (1 << 30) / (len >> 1)
      int32_t recip = pData->grains[i].recipHalfLen;
      if (ph < (len >> 1)) {
        // Rising half: parabolic ramp = ph * (len - ph) / (len/2)^2 * 32767
        int32_t t = (int32_t)(((uint32_t)ph * (uint32_t)recip) >> 15);
        // t is 0..32767 linear ramp. Square-root-ish parabola:
        // win = t * (2*32767 - t) >> 15  (peaks at 32767 when t=32767)
        win = (int32_t)(((int64_t)t * (65534 - t)) >> 15);
      } else {
        int32_t t = (int32_t)(((uint32_t)(len - ph) * (uint32_t)recip) >> 15);
        win = (int32_t)(((int64_t)t * (65534 - t)) >> 15);
      }
      if (win > 32767)
        win = 32767;
      if (win < 0)
        win = 0;

      int16_t samp = readGrainSample(&pData->delayL, pData->grains[i].posQ8);
      int32_t gOut = (samp * win) >> 15;

      int32_t effPan;
      int32_t w = pData->width;
      if (w == 255)
        effPan = pData->grains[i].pan;
      else
        effPan = 128 + (((pData->grains[i].pan - 128) * w) >> 8);
      if (effPan < 0)
        effPan = 0;
      if (effPan > 255)
        effPan = 255;

      accumL += (gOut * (255 - effPan)) >> 8;
      accumR += (gOut * effPan) >> 8;

      pData->grains[i].posQ8 += pData->grains[i].speedQ8;
      pData->grains[i].winPhase++;
      if (pData->grains[i].winPhase >= len)
        respawnGrain(pData, i);
    }

    // Soft limit grain accumulator before scaling to prevent harsh peaks
    if (accumL > 200000)
      accumL = 200000 + ((accumL - 200000) >> 2);
    else if (accumL < -200000)
      accumL = -200000 + ((accumL + 200000) >> 2);
    if (accumR > 200000)
      accumR = 200000 + ((accumR - 200000) >> 2);
    else if (accumR < -200000)
      accumR = -200000 + ((accumR + 200000) >> 2);

    int32_t rawL = accumL >> 3;
    int32_t rawR = accumR >> 3;

    // 2. Grain Diffusion (Short)
    int16_t tempL = (int16_t)clip(rawL, audioState);
    int16_t tempR = (int16_t)clip(rawR, audioState);
    for (int k = 0; k < 2; k++) {
      tempL = allpassProcessSample(tempL, &pData->diffL[k], audioState);
      tempR = allpassProcessSample(tempR, &pData->diffR[k], audioState);
    }

    // 3. Plate Reverb Engine
    // Input: Mono Sum of Diffused Grains
    int16_t pIn = (tempL + tempR) >> 1;

    // Input Diffusion
    for (int k = 0; k < 4; k++)
      pIn = allpassProcessSample(pIn, &pData->pApIn[k], audioState);

    // Tank Loop
    int16_t tankOutL = getDelayedSample(&pData->pDelay2L);
    int16_t tankOutR = getDelayedSample(&pData->pDelay2R);

    pData->pLfoPhase += 4096;
    int16_t modBase = (pData->pLfoPhase >> 16) & 0x7FFF;
    if (pData->pLfoPhase & 0x80000000)
      modBase = 32767 - modBase;
    int16_t modL = (modBase * 100) >> 15; // Small fixed depth
    int16_t modR = ((32767 - modBase) * 100) >> 15;

    // Left Side
    int16_t tInL = pIn + ((pData->plateDecay * tankOutR) >> 15);
    int16_t sL = processPlateModAP(tInL, &pData->pModL, modL, 16384);
    addSampleToDelayline(sL, &pData->pDelay1L);
    sL = getDelayedSample(&pData->pDelay1L);
    sL = plateLowpass(sL, &pData->pLpL, 16000); // Fixed Damping
    sL = allpassProcessSample(sL, &pData->pApTankL, audioState);
    addSampleToDelayline(sL, &pData->pDelay2L);

    // Right Side
    int16_t tInR = pIn + ((pData->plateDecay * tankOutL) >> 15);
    int16_t sR = processPlateModAP(tInR, &pData->pModR, modR, 16384);
    addSampleToDelayline(sR, &pData->pDelay1R);
    sR = getDelayedSample(&pData->pDelay1R);
    sR = plateLowpass(sR, &pData->pLpR, 16000);
    sR = allpassProcessSample(sR, &pData->pApTankR, audioState);
    addSampleToDelayline(sR, &pData->pDelay2R);

    // Plate Output (Tap end of chain)
    int16_t pOutL = sL;
    int16_t pOutR = sR;

    // 4. Output Mixing
    // Grains (tempL/R) + Reverb (pOutL/R * Mix)
    int32_t rMix = pData->reverbMix;
    int32_t finL = tempL + ((pOutL * rMix) >> 15);
    int32_t finR = tempR + ((pOutR * rMix) >> 15);

    // 5. Feedback (Into Grain Buffer) - FREEZE LOGIC
    if (!frozen) {
      int32_t fbMono = (tempL + tempR) >> 1;
      pData->fbLpState += ((fbMono - pData->fbLpState) * 8000) >> 15;

      int32_t feed = (pData->fbLpState * pData->feedback) >> 15;
      int32_t recVal = mn + feed;

      if (recVal > 32000)
        recVal = 32000 + (recVal - 32000) / 4;
      else if (recVal < -32000)
        recVal = -32000 + (recVal + 32000) / 4;
      if (recVal > 32767)
        recVal = 32767;
      if (recVal < -32767)
        recVal = -32767;

      uint16_t wPos = pData->delayL.delayLinePtr;
      pData->delayL.delayLine[wPos] = (int16_t)recVal;
      wPos++;
      if (wPos >= BUFFER_SIZE)
        wPos = 0;
      pData->delayL.delayLinePtr = wPos;
    }
    // If Frozen: Do not update delayLinePtr, effectively freezing buffer
    // content. Grains read from static buffer.

    wetL_hold = (int16_t)clip(finL, audioState);
    wetR_hold = (int16_t)clip(finR, audioState);
  }

  // Balanced dry/wet mix: both use >> 15 scaling for unity gain
  int16_t mx = pData->mix;
  *outL = clip((((32767 - mx) * (*inL)) >> 15) + ((mx * wetL_hold) >> 14),
               audioState);
  *outR = clip((((32767 - mx) * (*inR)) >> 15) + ((mx * wetR_hold) >> 14),
               audioState);

  *outL = gainStageProcessSample(*outL, &pData->presetVolume);
  *outR = gainStageProcessSample(*outR, &pData->presetVolume);
}

// Params
static void fxProgramParam1Callback(uint16_t val, void *data) { // Mix
  ((FxProgram23DataType *)data)->mix = val << 3;
}
static void fxProgramParam1Display(void *data, char *res) {
  Int16ToChar(((FxProgram23DataType *)data)->mix / 328, res);
  appendToString(res, "%");
}

static void fxProgramParam2Callback(uint16_t val, void *data) { // Size
  ((FxProgram23DataType *)data)->grainSize = val << 3;
}
static void fxProgramParam2Display(void *data, char *res) {
  Int16ToChar(20 + ((((FxProgram23DataType *)data)->grainSize >> 3) / 9), res);
  appendToString(res, "ms");
}

static void fxProgramParam3Callback(uint16_t val, void *data) { // Atmosphere
  FxProgram23DataType *pData = (FxProgram23DataType *)data;
  pData->atmosphere = val << 3;

  pData->feedback = (val * 14000) >> 12; // Max ~0.45
  pData->pitchDrift = val >> 10;
  pData->densityMask = 4 + ((val * 4) >> 12);
  pData->reverbMix = (val * 24000) >> 12;
  pData->plateDecay = 10000 + ((val * 19000) >> 12); // Max 29000 (~0.88)
}
static void fxProgramParam3Display(void *data, char *res) {
  Int16ToChar(((FxProgram23DataType *)data)->atmosphere / 328, res);
  appendToString(res, "%");
}

static void fxProgramParam4Callback(uint16_t val, void *data) { // Width
  ((FxProgram23DataType *)data)->width = val;
}
static void fxProgramParam4Display(void *data, char *res) {
  int16_t w = ((FxProgram23DataType *)data)->width;
  if (w > 180)
    appendToString(res, "Wide");
  else if (w > 50)
    appendToString(res, "Mid");
  else
    appendToString(res, "Mono");
}

static void fxProgramParamFreezeCallback(uint16_t val, void *data) { // Freeze
  ((FxProgram23DataType *)data)->freeze = val;
}

static void fxProgramPresetVolumeCallback(uint16_t val, void *data) {
  ((FxProgram23DataType *)data)->presetVolume.gain = val >> 2;
}
static void fxProgramPresetVolumeDisplay(void *data, char *res) {
  decimalInt16ToChar(((FxProgram23DataType *)data)->presetVolume.gain * 39, res,
                     2);
  appendToString(res, "%");
}

static void fxProgram23SetupStereo(void *data) {
  FxProgram23DataType *pData = (FxProgram23DataType *)data;
  int16_t *ptr = getDelayMemoryPointer();

  // 1. Granular Buffer (50k)
  initDelay(&pData->delayL, ptr, BUFFER_SIZE);
  for (int i = 0; i < BUFFER_SIZE; i++)
    ptr[i] = 0;
  ptr += BUFFER_SIZE;

  // 2. Grain Diffusion (4k)
  int16_t diffSizes[2] = {499, 977};
  for (int c = 0; c < 2; c++) {
    for (int s = 0; s < 2; s++) {
      AllpassType *ap = (c == 0) ? &pData->diffL[s] : &pData->diffR[s];
      ap->delayLineIn = ptr;
      ap->delayLineOut = ptr + 1024;
      ap->bufferSize = 1023;
      ap->delayInSamples = diffSizes[s] + (c * 23);
      ap->coefficient = 22000;
      ap->delayPtr = 0;
      ap->oldValues = 0;
      for (int k = 0; k < 2048; k++)
        *(ptr + k) = 0;
      ptr += 2048;
    }
  }

  // 3. Plate Init (~36k remaining available, using ~32k)
  // Input APs (2k total)
  int16_t pApLens[4] = {P_AP_IN_1, P_AP_IN_2, P_AP_IN_3, P_AP_IN_4};
  for (int i = 0; i < 4; i++) {
    pData->pApIn[i].delayLineIn = ptr;
    pData->pApIn[i].delayLineOut = ptr + 512;
    pData->pApIn[i].bufferSize = 511; // Small APs
    pData->pApIn[i].delayInSamples = pApLens[i];
    pData->pApIn[i].coefficient = 23000;
    pData->pApIn[i].delayPtr = 0;
    pData->pApIn[i].oldValues = 0;
    for (int k = 0; k < 1024; k++)
      *(ptr + k) = 0;
    ptr += 1024;
  }

  // Tank L
  initDelay(&pData->pModL, ptr, 2048);
  pData->pModL.delayInSamples = P_MOD_L;
  ptr += 2048;
  initDelay(&pData->pDelay1L, ptr, 4096);
  pData->pDelay1L.delayInSamples = P_D1_L;
  ptr += 4096;

  pData->pApTankL.delayLineIn = ptr;
  pData->pApTankL.delayLineOut = ptr + 2048;
  pData->pApTankL.bufferSize = 2047;
  pData->pApTankL.delayInSamples = P_AP_T_L;
  pData->pApTankL.coefficient = 16384;
  pData->pApTankL.delayPtr = 0;
  pData->pApTankL.oldValues = 0;
  for (int k = 0; k < 4096; k++)
    *(ptr + k) = 0;
  ptr += 4096;

  initDelay(&pData->pDelay2L, ptr, 4096);
  pData->pDelay2L.delayInSamples = P_D2_L;
  ptr += 4096;

  // Tank R
  initDelay(&pData->pModR, ptr, 2048);
  pData->pModR.delayInSamples = P_MOD_R;
  ptr += 2048;
  initDelay(&pData->pDelay1R, ptr, 4096);
  pData->pDelay1R.delayInSamples = P_D1_R;
  ptr += 4096;

  pData->pApTankR.delayLineIn = ptr;
  pData->pApTankR.delayLineOut = ptr + 2048;
  pData->pApTankR.bufferSize = 2047;
  pData->pApTankR.delayInSamples = P_AP_T_R;
  pData->pApTankR.coefficient = 16384;
  pData->pApTankR.delayPtr = 0;
  pData->pApTankR.oldValues = 0;
  for (int k = 0; k < 4096; k++)
    *(ptr + k) = 0;
  ptr += 4096;

  initDelay(&pData->pDelay2R, ptr, 4096);
  pData->pDelay2R.delayInSamples = P_D2_R;
  ptr += 4096;

  pData->mix = 16384;
  pData->grainSize = 16000;
  pData->atmosphere = 10000;
  pData->width = 255;
  pData->feedback = 0;
  pData->pitchDrift = 0;
  pData->densityMask = 10;
  pData->rngSeed = 9932;
  pData->presetVolume.gain = 0x100;
  pData->reverbMix = 0;
  pData->plateDecay = 16000;
  pData->freeze = 0;
  pData->fbLpState = 0;

  for (int i = 0; i < N_GRAINS; i++) {
    pData->grains[i].active = 0;
    respawnGrain(pData, i);
    pData->grains[i].winPhase = i * (pData->grains[i].winLength / N_GRAINS);
  }
}
static void fxProgram23Setup(void *data) {}

FxProgram23DataType fxProgram23data;

FxProgramType fxProgramGranular = {
    .name = "Clouds",
    .nParameters = 6,
    .parameters = {{.name = "Mix            ",
                    .control = 0,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgramParam1Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgramParam1Callback},
                   {.name = "Size           ",
                    .control = 1,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgramParam2Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgramParam2Callback},
                   {.name = "Atmo           ",
                    .control = 2,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = &fxProgramParam3Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgramParam3Callback},
                   {.name = "Freeze         ",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0,
                    .getParameterDisplay = 0,
                    .getParameterValue = 0,
                    .setParameter = &fxProgramParamFreezeCallback},
                   {.name = "Width          ",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 255,
                    .getParameterDisplay = &fxProgramParam4Display,
                    .getParameterValue = 0,
                    .setParameter = &fxProgramParam4Callback},
                   {.name = "Volume",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0x3ff,
                    .setParameter = fxProgramPresetVolumeCallback,
                    .getParameterValue = 0,
                    .getParameterDisplay = fxProgramPresetVolumeDisplay}},
    .processSample = &fxProgram23processSample,
    .processSampleStereo = &fxProgram23processSampleStereo,
    .setup = &fxProgram23SetupStereo,
    .reset = 0,
    .isStereo = 1,
    .data = (void *)&fxProgram23data};
