#include "audio/audiotools.h"
#include "pipicofx/fxPrograms.h"
#include "romfunc.h"
#include "stringFunctions.h"

// ShimmerVerb v3 - Lexicon-inspired plate tank with smooth octave shimmer
// Changes from v2:
//   - Larger pitch shifter buffers (8k samples = 170ms) for smoother octave
//   - Wider crossfade window (2048 samples) to reduce metallic artifacts
//   - 1-pole lowpass on shimmer feedback to tame harshness
//   - Shimmer routed only through tank (no direct-to-output)
//   - Expanded decay range up to near-infinite
//   - Reduced input diffusion buffer sizes to reclaim memory

// Tank delay times (prime-ish for richness)
#define MOD_L 541
#define D1_L 3593
#define AP_T_L 1451
#define D2_L 3003

#define MOD_R 732
#define D1_R 3401
#define AP_T_R 1925
#define D2_R 2550

// Input diffusion allpass times (smaller APs to save memory for pitch shifters)
#define AP_IN_1 97
#define AP_IN_2 73
#define AP_IN_3 241
#define AP_IN_4 179

// Helper: Plate lowpass (damping)
static inline int16_t plateLowpass(int16_t sample, int16_t *state,
                                   int16_t alpha) {
  int32_t diff = (int32_t)sample - *state;
  *state = *state + ((diff * alpha) >> 15);
  return *state;
}

// Helper: Modulated allpass for tank
static int16_t processModAP(int16_t x, DelayDataType *d, int16_t modOffset,
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
  // Pitch shifter for shimmer (larger buffers for smooth octave)
  Pitchshifter2DataType pitchShiftL;
  Pitchshifter2DataType pitchShiftR;

  // Input diffusion (4 allpasses)
  AllpassType apIn[4];

  // Tank Left
  DelayDataType modL, delay1L, delay2L;
  AllpassType apTankL;
  int16_t lpL;

  // Tank Right
  DelayDataType modR, delay1R, delay2R;
  AllpassType apTankR;
  int16_t lpR;

  // Shimmer feedback lowpass states
  int16_t shimLpL;
  int16_t shimLpR;

  // DC blocker for pitch shifter output
  int32_t dcPrevInL, dcPrevOutL;
  int32_t dcPrevInR, dcPrevOutR;

  // LFO
  uint32_t lfoPhase;

  // Parameters
  int16_t shimmerAmt; // How much shimmer feeds back
  int16_t decay;      // Tank decay
  int16_t mix;        // Dry/wet
  int16_t damping;    // HF damping
  int16_t shimDamp;   // Shimmer feedback damping (auto-derived)
  int16_t freeze;

  GainStageDataType presetVolume;
} ShimmerDataType;

static ShimmerDataType shimmerData;

static void shimmerProcessStereo(int16_t *inL, int16_t *inR, int16_t *outL,
                                 int16_t *outR, void *data,
                                 volatile uint32_t *audioState) {
  ShimmerDataType *d = (ShimmerDataType *)data;
  uint8_t frozen = (d->freeze > 2048);

  // === 1. Read tank outputs first (before writing new samples) ===
  int16_t tankOutL = getDelayedSample(&d->delay2L);
  int16_t tankOutR = getDelayedSample(&d->delay2R);

  // === 2. Generate shimmer (pitch shift the tank outputs) ===
  int16_t shimRawL =
      pitchShifter2ProcessSample(tankOutL, &d->pitchShiftL, audioState);
  int16_t shimRawR =
      pitchShifter2ProcessSample(tankOutR, &d->pitchShiftR, audioState);

  // === 2b. DC blocker on pitch shifter output ===
  // y[n] = x[n] - x[n-1] + 0.995 * y[n-1]  (32604/32768 in Q15)
  int32_t dcL = shimRawL - d->dcPrevInL + ((d->dcPrevOutL * 32604) >> 15);
  d->dcPrevInL = shimRawL;
  d->dcPrevOutL = dcL;
  int16_t shimL =
      (int16_t)(dcL > 32767 ? 32767 : (dcL < -32767 ? -32767 : dcL));

  int32_t dcR = shimRawR - d->dcPrevInR + ((d->dcPrevOutR * 32604) >> 15);
  d->dcPrevInR = shimRawR;
  d->dcPrevOutR = dcR;
  int16_t shimR =
      (int16_t)(dcR > 32767 ? 32767 : (dcR < -32767 ? -32767 : dcR));

  // === 3. Lowpass filter shimmer feedback to reduce metallic artifacts ===
  d->shimLpL = d->shimLpL + (((shimL - d->shimLpL) * d->shimDamp) >> 15);
  d->shimLpR = d->shimLpR + (((shimR - d->shimLpR) * d->shimDamp) >> 15);

  // === 4. Mix input with filtered shimmer feedback ===
  int32_t shimFeedL = (d->shimLpL * d->shimmerAmt) >> 15;
  int32_t shimFeedR = (d->shimLpR * d->shimmerAmt) >> 15;

  int32_t reverbInL, reverbInR;
  if (frozen) {
    // Freeze: no new input, shimmer continues
    reverbInL = shimFeedL;
    reverbInR = shimFeedR;
  } else {
    reverbInL = (*inL >> 1) + shimFeedL;
    reverbInR = (*inR >> 1) + shimFeedR;
  }

  // Soft clip input
  if (reverbInL > 28000)
    reverbInL = 28000 + ((reverbInL - 28000) >> 2);
  else if (reverbInL < -28000)
    reverbInL = -28000 + ((reverbInL + 28000) >> 2);
  if (reverbInR > 28000)
    reverbInR = 28000 + ((reverbInR - 28000) >> 2);
  else if (reverbInR < -28000)
    reverbInR = -28000 + ((reverbInR + 28000) >> 2);

  // === 5. Input diffusion (mono sum through 4 allpasses) ===
  int16_t pIn = ((int16_t)reverbInL + (int16_t)reverbInR) >> 1;
  for (int k = 0; k < 4; k++) {
    pIn = allpassProcessSample(pIn, &d->apIn[k], audioState);
  }

  // === 6. Tank LFO for modulation ===
  d->lfoPhase += 2400; // Slower LFO ~0.56Hz for smoother modulation
  int16_t modBase = (d->lfoPhase >> 16) & 0x7FFF;
  if (d->lfoPhase & 0x80000000)
    modBase = 32767 - modBase;
  int16_t modL = (modBase * 60) >> 15; // ±30 samples modulation (gentler)
  int16_t modR = ((32767 - modBase) * 60) >> 15;

  // === 7. Tank decay (adjust for freeze) ===
  int16_t tankDecay = frozen ? 32700 : d->decay;

  // === 8. Left Tank ===
  // Input = diffused mono + cross-feed from right tank
  int16_t tInL = pIn + ((tankDecay * tankOutR) >> 15);

  // Modulated allpass
  int16_t sL = processModAP(tInL, &d->modL, modL, 16384);

  // First delay
  addSampleToDelayline(sL, &d->delay1L);
  sL = getDelayedSample(&d->delay1L);

  // Damping lowpass
  sL = plateLowpass(sL, &d->lpL, d->damping);

  // Tank allpass
  sL = allpassProcessSample(sL, &d->apTankL, audioState);

  // Second delay
  addSampleToDelayline(sL, &d->delay2L);

  // === 9. Right Tank ===
  int16_t tInR = pIn + ((tankDecay * tankOutL) >> 15);
  int16_t sR = processModAP(tInR, &d->modR, modR, 16384);
  addSampleToDelayline(sR, &d->delay1R);
  sR = getDelayedSample(&d->delay1R);
  sR = plateLowpass(sR, &d->lpR, d->damping);
  sR = allpassProcessSample(sR, &d->apTankR, audioState);
  addSampleToDelayline(sR, &d->delay2R);

  // === 10. Output mixing ===
  // Tank output only — no direct shimmer to output for cleaner sound
  int16_t wetL = sL;
  int16_t wetR = sR;

  int16_t mix = d->mix;
  *outL =
      clip((((32767 - mix) * (*inL)) >> 15) + ((mix * wetL) >> 14), audioState);
  *outR =
      clip((((32767 - mix) * (*inR)) >> 15) + ((mix * wetR) >> 14), audioState);

  *outL = gainStageProcessSample(*outL, &d->presetVolume);
  *outR = gainStageProcessSample(*outR, &d->presetVolume);
}

// Parameter callbacks
static void paramMixCallback(uint16_t val, void *data) {
  ((ShimmerDataType *)data)->mix = val << 3;
}
static void paramMixDisplay(void *data, char *res) {
  Int16ToChar(((ShimmerDataType *)data)->mix / 328, res);
  appendToString(res, "%");
}

static void paramShimmerCallback(uint16_t val, void *data) {
  ShimmerDataType *d = (ShimmerDataType *)data;
  // Shimmer amount 0..22000 (slightly lower ceiling for cleaner feedback)
  d->shimmerAmt = (val * 22) >> 4;

  // Shimmer feedback damping: more shimmer = more filtering to control
  // harshness Range: 24000 (bright, low shimmer) down to 12000 (dark, high
  // shimmer)
  d->shimDamp = 24000 - ((val * 12) >> 4);
  if (d->shimDamp < 10000)
    d->shimDamp = 10000;

  // Left: always octave up. Right: varies from fifth to octave
  d->pitchShiftL.delayIncrement = 8; // Octave up

  // Right channel: more shimmer = higher pitch (fifth -> octave)
  if (val < 1500)
    d->pitchShiftR.delayIncrement = 6; // Fifth
  else if (val < 3000)
    d->pitchShiftR.delayIncrement = 7; // Maj7
  else
    d->pitchShiftR.delayIncrement = 8; // Octave
}
static void paramShimmerDisplay(void *data, char *res) {
  int16_t amt = ((ShimmerDataType *)data)->shimmerAmt;
  if (amt < 5000)
    appendToString(res, "Subtle");
  else if (amt < 15000)
    appendToString(res, "Shimmer");
  else
    appendToString(res, "Bright");
}

static void paramDecayCallback(uint16_t val, void *data) {
  ShimmerDataType *d = (ShimmerDataType *)data;
  // Decay: 18000 (short) to 32500 (near-infinite) — wider range than v2
  d->decay = 18000 + ((val * 14500) >> 12);
  if (d->decay > 32500)
    d->decay = 32500;

  // Damping: more decay = less damping for brighter tail
  // Range: 22000 (heavy damping) down to 6000 (very bright)
  d->damping = 22000 - ((val * 16000) >> 12);
  if (d->damping < 6000)
    d->damping = 6000;
}
static void paramDecayDisplay(void *data, char *res) {
  int16_t dec = ((ShimmerDataType *)data)->decay;
  if (dec < 22000)
    appendToString(res, "Short");
  else if (dec < 28000)
    appendToString(res, "Medium");
  else if (dec < 31000)
    appendToString(res, "Long");
  else
    appendToString(res, "Infinite");
}

static void paramFreezeCallback(uint16_t val, void *data) {
  ((ShimmerDataType *)data)->freeze = val;
}

static void paramVolumeCallback(uint16_t val, void *data) {
  ((ShimmerDataType *)data)->presetVolume.gain = val >> 2;
}
static void paramVolumeDisplay(void *data, char *res) {
  decimalInt16ToChar(((ShimmerDataType *)data)->presetVolume.gain * 39, res, 2);
  appendToString(res, "%");
}

static void shimmerSetup(void *data) {
  ShimmerDataType *d = (ShimmerDataType *)data;
  int16_t *ptr = getDelayMemoryPointer();

  // Clear memory (total usage: ~48k int16_t)
  for (int i = 0; i < 50000; i++)
    ptr[i] = 0;

  // === Pitch shifters (8k each = 16k total) ===
  // Larger buffers (170ms window) for much smoother octave shifting
  initPitchshifter2(&d->pitchShiftL, ptr);
  d->pitchShiftL.delayIncrement = 8;      // Octave up
  d->pitchShiftL.buffersizePowerTwo = 13; // 8192 samples
  d->pitchShiftL.crossFadeWidthPwr2 = 11; // 2048-sample crossfade
  ptr += 8192;

  initPitchshifter2(&d->pitchShiftR, ptr);
  d->pitchShiftR.delayIncrement = 8; // Octave up
  d->pitchShiftR.buffersizePowerTwo = 13;
  d->pitchShiftR.crossFadeWidthPwr2 = 11;
  ptr += 8192;

  // === Input diffusion allpasses (4 x 512 = 2k total) ===
  // Reduced from 1024-per-AP to save memory for pitch shifters
  int16_t apLens[4] = {AP_IN_1, AP_IN_2, AP_IN_3, AP_IN_4};
  for (int i = 0; i < 4; i++) {
    d->apIn[i].delayLineIn = ptr;
    d->apIn[i].delayLineOut = ptr + 256;
    d->apIn[i].bufferSize = 255;
    d->apIn[i].delayInSamples = apLens[i];
    d->apIn[i].coefficient = 22000;
    d->apIn[i].delayPtr = 0;
    d->apIn[i].oldValues = 0;
    ptr += 512;
  }

  // === Tank Left (~14k) ===
  initDelay(&d->modL, ptr, 2048);
  d->modL.delayInSamples = MOD_L;
  ptr += 2048;

  initDelay(&d->delay1L, ptr, 4096);
  d->delay1L.delayInSamples = D1_L;
  ptr += 4096;

  d->apTankL.delayLineIn = ptr;
  d->apTankL.delayLineOut = ptr + 2048;
  d->apTankL.bufferSize = 2047;
  d->apTankL.delayInSamples = AP_T_L;
  d->apTankL.coefficient = 16384;
  d->apTankL.delayPtr = 0;
  d->apTankL.oldValues = 0;
  ptr += 4096;

  initDelay(&d->delay2L, ptr, 4096);
  d->delay2L.delayInSamples = D2_L;
  ptr += 4096;

  // === Tank Right (~14k) ===
  initDelay(&d->modR, ptr, 2048);
  d->modR.delayInSamples = MOD_R;
  ptr += 2048;

  initDelay(&d->delay1R, ptr, 4096);
  d->delay1R.delayInSamples = D1_R;
  ptr += 4096;

  d->apTankR.delayLineIn = ptr;
  d->apTankR.delayLineOut = ptr + 2048;
  d->apTankR.bufferSize = 2047;
  d->apTankR.delayInSamples = AP_T_R;
  d->apTankR.coefficient = 16384;
  d->apTankR.delayPtr = 0;
  d->apTankR.oldValues = 0;
  ptr += 4096;

  initDelay(&d->delay2R, ptr, 4096);
  d->delay2R.delayInSamples = D2_R;
  ptr += 4096;

  // Total: 16384 (pitch) + 2048 (diffusion) + 28672 (tank) = 47104 int16_t
  // Well within 89500 budget

  // Initialize state
  d->lpL = 0;
  d->lpR = 0;
  d->shimLpL = 0;
  d->shimLpR = 0;
  d->dcPrevInL = 0;
  d->dcPrevOutL = 0;
  d->dcPrevInR = 0;
  d->dcPrevOutR = 0;
  d->lfoPhase = 0;
  d->shimmerAmt = 12000;
  d->decay = 26000;
  d->damping = 14000;
  d->shimDamp = 18000;
  d->mix = 16384;
  d->freeze = 0;
  d->presetVolume.gain = 0xff;
}

static int16_t shimmerProcessMono(int16_t sampleIn, void *data) {
  return sampleIn;
}

FxProgramType fxProgramShimmerVerb = {
    .name = "ShimmerVerb",
    .nParameters = 5,
    .parameters = {{.name = "Mix",
                    .control = 0,
                    .increment = 1,
                    .rawValue = 2048,
                    .setParameter = paramMixCallback,
                    .getParameterDisplay = paramMixDisplay},
                   {.name = "Shimmer",
                    .control = 1,
                    .increment = 1,
                    .rawValue = 2048,
                    .setParameter = paramShimmerCallback,
                    .getParameterDisplay = paramShimmerDisplay},
                   {.name = "Decay",
                    .control = 2,
                    .increment = 1,
                    .rawValue = 2048,
                    .setParameter = paramDecayCallback,
                    .getParameterDisplay = paramDecayDisplay},
                   {.name = "Freeze",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0,
                    .setParameter = paramFreezeCallback,
                    .getParameterDisplay = 0},
                   {.name = "Volume",
                    .control = 0xff,
                    .increment = 1,
                    .rawValue = 0x3ff,
                    .setParameter = paramVolumeCallback,
                    .getParameterDisplay = paramVolumeDisplay}},
    .processSample = shimmerProcessMono,
    .processSampleStereo = shimmerProcessStereo,
    .setup = shimmerSetup,
    .reset = 0,
    .isStereo = 1,
    .data = (void *)&shimmerData};
