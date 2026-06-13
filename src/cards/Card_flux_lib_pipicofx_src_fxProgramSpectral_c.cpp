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
#include "pipicofx/fxPrograms.h"
#include "pipicofx/picofxCore.h"
#include "stringFunctions.h"
/* stripped system include */

// FxProgram28: "Spectral Freeze"
// Implemented as an Integer-Only Additive Resynthesis Filterbank (Vocoder-style)
// Analyzes audio using 16 logarithmic Biquad Bandpass Filters, tracks their envelope, 
// and drives 16 corresponding Sine Wave Oscillators to precisely resynthesize the spectrum.
// Completely bypasses the need for FFTs or noisy granular delay loops.

static FxProgram28DataType progData;

// --- DSP Primitives ---
static inline int16_t cheapSine(uint32_t phase) {
  int32_t p = (phase >> 16) & 0xFFFF;
  if (p > 32767) p = 65536 - p;
  return (int16_t)((p - 16384) * 2);
}

// --- Setup ---
static void fxProgram28Setup(void *data) {
  FxProgram28DataType *d = (FxProgram28DataType *)data;
  d->buffer = getDelayMemoryPointer(); // Kept if needed, but unused by resynthesizer
  
  float fs = AUDIO_BASE_RATE; // 24000 assumed
  float Q = 18.0f; // High Q to strictly isolate narrow spectral bands
  
  for (int i = 0; i < NUM_SPECTRAL_BANDS; i++) {
    d->bpfState1[i] = 0;
    d->bpfState2[i] = 0;
    d->envState[i] = 0;
    d->oscPhase[i] = (i * 4096) << 16; // Stagger initial sine phases
    
    // Logarithmic spread from 100 Hz to 8000 Hz
    float freq = 100.0f * powf(8000.0f / 100.0f, (float)i / (NUM_SPECTRAL_BANDS - 1));
    
    // 2-Pole IIR Resonator Mathematics
    float bw = freq / Q;
    float R = expf(-3.14159f * bw / fs);
    float r2 = R * R;
    float theta = 2.0f * 3.14159f * freq / fs;
    float coeff = 2.0f * R * cosf(theta);
    
    // Oscillator Delta
    d->oscInc[i] = (uint32_t)((freq / fs) * 4294967296.0f);
    
    // Normalization to prevent resonant explosion
    float normalized_gain = (1.0f - R) * 0.7f;
    
    // Store as Fixed Point Quantized Integers (Q14 mostly, ensures it fits without overflowing int32_t on multiply)
    d->bpfCoeff[i] = (int16_t)(coeff * 16384.0f);
    d->bpfR2[i]    = (int16_t)(r2 * 16384.0f);
    d->bpfGain[i]  = (int16_t)(normalized_gain * 32767.0f);
  }
  
  d->mix = 2048;
  d->blur = 2048;
  d->warp = 0;
  d->freeze = 0;
  d->presetVolume.gain = 256;
  d->lfoPhase = 0;
}

static void fxProgram28SetupStereo(void *data) { fxProgram28Setup(data); }

// --- Process ---
static void fxProgram28ProcessSampleStereo(int16_t *inL, int16_t *inR,
                                           int16_t *outL, int16_t *outR,
                                           void *data,
                                           volatile uint32_t *audioState) {
  FxProgram28DataType *d = (FxProgram28DataType *)data;
  
  int32_t monoIn = (*inL + *inR) >> 1;
  uint8_t isFrozen = (d->freeze > 2048);
  int32_t blurVal = d->blur;
  int32_t warpAmt = d->warp;

  // Global LFO for Wow & Flutter
  d->lfoPhase += (10 + (warpAmt >> 4)) * AUDIO_SAMPLE_RATE_DIV;
  int16_t lfoSine = cheapSine(d->lfoPhase); // Q15 range
  
  // Envelope Release math mapping Blur curve (Q15 32500 to 32766 indicates slow exponential drop)
  int32_t decayCoeff = 32500 + ((blurVal * 266) >> 12); 
  if (isFrozen) decayCoeff = 32767; // Infinite sustained hold

  int32_t outSumL = 0;
  int32_t outSumR = 0;

  for (int i = 0; i < NUM_SPECTRAL_BANDS; i++) {
    // 1. ANALYSIS: Pass audio through Resonator BPF
    int32_t inScaled = (monoIn * d->bpfGain[i]) >> 15;
    
    // IIR Difference Equation (Integer Q14)
    int32_t y = inScaled + ((d->bpfCoeff[i] * d->bpfState1[i]) >> 14) - ((d->bpfR2[i] * d->bpfState2[i]) >> 14);
    d->bpfState2[i] = d->bpfState1[i];
    d->bpfState1[i] = y;
    
    // Bounds wrap prevention
    if (d->bpfState1[i] > 1000000) d->bpfState1[i] = 1000000;
    if (d->bpfState1[i] < -1000000) d->bpfState1[i] = -1000000;

    // 2. ENVELOPE: Absolute follower
    int32_t absY = y;
    if (absY < 0) absY = -absY;

    if (!isFrozen) {
      if (absY > d->envState[i]) {
        d->envState[i] = d->envState[i] + ((absY - d->envState[i]) >> 5); // Smooth attack
      } else {
        d->envState[i] = (d->envState[i] * decayCoeff) >> 15; // Slow decay (blur)
      }
    } 
    // If Frozen, the current `envState` holds its exact value, causing the oscillators below to sustain infinitely.

    // 3. SYNTHESIS: Additive Sine Resynthesis
    uint32_t detune = 0;
    if (warpAmt > 0) {
      // De-tune the oscillators based on LFO. Higher bands detune slightly more to create choral shimmer.
      int32_t sweep = (warpAmt * lfoSine) >> 15; 
      detune = (d->oscInc[i] * sweep * (i+1)) >> 19;
    }
    
    d->oscPhase[i] += d->oscInc[i] + detune;
    int16_t currentSine = cheapSine(d->oscPhase[i]);
    
    // Amplitude modulation by Envelope follower
    int32_t bandOutput = (currentSine * d->envState[i]) >> 14; 
    
    // 4. MIXING: Spatialize alternating odd/even bands left and right to create a massive wide stereo pad
    if (i % 2 == 0) {
      outSumL += bandOutput;
      outSumR += (bandOutput >> 1);
    } else {
      outSumR += bandOutput;
      outSumL += (bandOutput >> 1);
    }
  }

  // Final gain staging and Soft Clipper
  int32_t mixWet = d->mix;
  int32_t mixDry = 4095 - d->mix;
  
  outSumL = outSumL >> 2; 
  outSumR = outSumR >> 2;

  if(outSumL > 25000) outSumL = 25000 + ((outSumL - 25000)>>1);
  else if(outSumL < -25000) outSumL = -25000 + ((outSumL + 25000)>>1);
  if(outSumR > 25000) outSumR = 25000 + ((outSumR - 25000)>>1);
  else if(outSumR < -25000) outSumR = -25000 + ((outSumR + 25000)>>1);
  
  int32_t finalL = (((int32_t)*inL * mixDry) >> 12) + ((outSumL * mixWet) >> 12);
  int32_t finalR = (((int32_t)*inR * mixDry) >> 12) + ((outSumR * mixWet) >> 12);
  
  if (finalL > 32767) finalL = 32767; else if (finalL < -32767) finalL = -32767;
  if (finalR > 32767) finalR = 32767; else if (finalR < -32767) finalR = -32767;

  *outL = gainStageProcessSample((int16_t)finalL, &d->presetVolume);
  *outR = gainStageProcessSample((int16_t)finalR, &d->presetVolume);
}

// --- Callbacks ---
static void fxParamMix(uint16_t val, void *data) { ((FxProgram28DataType *)data)->mix = val; }
static void fxDisplayMix(void *data, char *res) { Int16ToChar(((FxProgram28DataType *)data)->mix / 41, res); appendToString(res, "%"); }
static void fxParamBlur(uint16_t val, void *data) { ((FxProgram28DataType *)data)->blur = val; }
static void fxDisplayBlur(void *data, char *res) { Int16ToChar(((FxProgram28DataType *)data)->blur / 41, res); appendToString(res, "%"); }
static void fxParamWarp(uint16_t val, void *data) { ((FxProgram28DataType *)data)->warp = val; }
static void fxDisplayWarp(void *data, char *res) { Int16ToChar(((FxProgram28DataType *)data)->warp / 41, res); appendToString(res, "%"); }
static void fxParamFreeze(uint16_t val, void *data) { ((FxProgram28DataType *)data)->freeze = val; }
static void fxParamVol(uint16_t val, void *data) { ((FxProgram28DataType *)data)->presetVolume.gain = val >> 2; }
static void fxDisplayVol(void *data, char *res) { decimalInt16ToChar(((FxProgram28DataType *)data)->presetVolume.gain * 39, res, 2); appendToString(res, "%"); }

FxProgramType fxProgramSpectral = {
    .name = "Spectral Freeze",
    .nParameters = 5,
    .parameters = {
        {.name = "Mix", .control = 2, .increment = 1, .rawValue = 2048, .setParameter = fxParamMix, .getParameterDisplay = fxDisplayMix},
        {.name = "Blur", .control = 0, .increment = 1, .rawValue = 2048, .setParameter = fxParamBlur, .getParameterDisplay = fxDisplayBlur},
        {.name = "Warp", .control = 1, .increment = 1, .rawValue = 0, .setParameter = fxParamWarp, .getParameterDisplay = fxDisplayWarp},
        {.name = "Freeze", .control = 0xff, .increment = 1, .rawValue = 0, .setParameter = fxParamFreeze, .getParameterDisplay = 0},
        {.name = "Volume", .control = 0xff, .increment = 1, .rawValue = 0x400, .setParameter = fxParamVol, .getParameterDisplay = fxDisplayVol}
    },
    .processSampleStereo = fxProgram28ProcessSampleStereo,
    .setup = fxProgram28SetupStereo,
    .isStereo = 1,
    .data = &progData
};

} // namespace Card_Flux
