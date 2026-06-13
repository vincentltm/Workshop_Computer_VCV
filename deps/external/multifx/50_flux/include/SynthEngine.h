#ifndef SYNTH_ENGINE_H
#define SYNTH_ENGINE_H

#include "SynthCore.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// Forward Declarations for Shared Internal Types
// Full definitions remain in SynthCore.cpp
// ============================================================================

#define MAX_VOICES 4
#define MAX_GRAINS 16

typedef struct {
  bool active;
  uint32_t phase;
  uint32_t inc;
  uint32_t length;
  uint32_t startPos;
  int32_t windowPeak;
  uint32_t windowScale;
  int32_t amp;
} Grain;

// Minimal Voice fields needed by extracted engines
// (Must match the fields in SynthCore.cpp's Voice struct exactly)
typedef struct {
  uint32_t phase;
  uint32_t phaseInc;
  int32_t envVal;
  int32_t attackRate;
  int32_t releaseRate;
  int32_t lpfState;
  uint32_t lfoPhase;
  uint32_t lfoInc;
  uint32_t noiseState;
  const uint8_t *samplePtr;
  uint32_t sampleLen;
  uint64_t sampleIdxQ16;
  int16_t sampleLastVal;
  int16_t sampleCurrVal;
  uint32_t sampleDecodeIdx;
  int32_t samplerEnvVal;
  int32_t samplerEnvStage;
  uint32_t lastLoopStart;
  bool playingReverse;
  uint32_t loopStartIdx;
  uint32_t loopEndIdx;
  bool xfActive;
  uint64_t xfIdxQ16;
  uint32_t xfDecodeIdx;
  int16_t xfCurrVal;
  int16_t xfLastVal;
  bool gate;
  bool lastGate;
  uint8_t note;
  bool active;
  float currentPitch_unused; // Deprecated
  int32_t currentPitch;      // Q16: 0.5 = 32768 (Middle C)
  int32_t smoothedSpeed;     // Q16
  int32_t sampleIndex;
  uint8_t velocity;
  bool isMidi;
  uint32_t noteOnCounter; // Tracks note age for oldest-note voice stealing
} Voice;

// Engine Pool (shared RAM — union of KS delay lines and Piano additive data)
typedef struct {
  union {
    struct {
      int16_t delayLine[4][760];
      uint16_t ptr[4];
    } ks;
    struct {
      uint8_t envCache[4][32][20];
      uint32_t phase[4][32];
      uint32_t phaseInc[4][32];
      uint32_t phaseIncScaled[4][32];
      int16_t targetMag[4][32];
      int32_t currentMag[4][32];
      uint32_t envTime[4];
      int32_t numHarmonics[4];
      int16_t a_alphas[4][4];
      int base_indices[4][4];
    } piano;
    struct {
      // 6 Operators per voice
      int32_t opPhase[4][6];
      int32_t opEnvVal[4][6];
      int32_t opTargetLevel[4][6]; // Sustained level or 0
      int32_t opRate[4][6];        // Current rate
      uint8_t opState[4][6]; // 0=Attack, 1=Decay, 2=Sustain, 3=Release, 4=Off
      int32_t feedback[4];   // Feedback memory for Op 6 (if algo uses it)
      uint8_t algorithm[4];  // 0-31
      uint8_t currentPreset[4]; // Track preset changes
    } fm;
  } eng;
} EnginePool;

// ============================================================================
// Shared Utility Functions (used by multiple engines)
// ============================================================================

// Mu-Law Decoder (G.711): 8-bit to 14-bit expansion
static inline int16_t Mulaw_Decode(uint8_t mulaw) {
  mulaw = ~mulaw;
  int sign = mulaw & 0x80;
  int exponent = (mulaw >> 4) & 0x07;
  int mantissa = mulaw & 0x0F;
  int sample = ((mantissa << 3) + 0x84) << exponent;
  sample -= 0x84;
  return (sign) ? -sample : sample;
}

// Fast integer square root (Q15 in, Q15 out) — division-free
// Input:  0..32768 representing 0.0..1.0
// Output: 0..32768 representing sqrt(input/32768)*32768
// Uses bit-stepping: shifts+adds only, ~20 cycles, minimal code size
static inline int32_t fast_isqrt_q15(int32_t x) {
  if (x <= 0)
    return 0;
  if (x >= 32768)
    return 32768;
  uint32_t val = (uint32_t)x << 15;
  uint32_t result = 0;
  uint32_t bit = 1u << 30;
  while (bit > val)
    bit >>= 2;
  while (bit != 0) {
    if (val >= result + bit) {
      val -= result + bit;
      result = (result >> 1) + bit;
    } else {
      result >>= 1;
    }
    bit >>= 2;
  }
  if (result > 32768)
    result = 32768;
  return (int32_t)result;
}

#include "MathTables.h"

// High-precision fixed-point pitch & exp functions
// (MathTables.cpp provides the LUTs)

/**
 * precise_exp2_q30
 * Calculates 2^x where x is Q16 (65536 = 1.0)
 * Returns Q30 (1073741824 = 1.0)
 */
static inline uint64_t precise_exp2_q30(int32_t x_q16) {
  int32_t intPart = x_q16 >> 16;
  int32_t fracPart = x_q16 & 0xFFFF;

  int index = fracPart >> 8;
  int ifrac = fracPart & 0xFF;

  uint32_t a = LUT_EXP2_Q30[index];
  uint32_t b = LUT_EXP2_Q30[index + 1];

  uint64_t res = (uint64_t)a + (uint32_t)(((uint64_t)(b - a) * ifrac) >> 8);

  if (intPart >= 0) {
    if (intPart > 60)
      return 0xFFFFFFFFFFFFFFFFULL;
    return res << intPart;
  } else {
    if (intPart < -60)
      return 0;
    return res >> (-intPart);
  }
}

/**
 * precise_mtof_q16
 * Calculates phaseInc for pitch in Q16 (0.5 = 32768 = Middle C)
 * Note mapping: Note = 60 + (pitch - 0.5) * 48
 */
static inline uint32_t precise_mtof_q16(int32_t pitchQ16) {
  // NoteQ16 = (60 << 16) + (pitchQ16 - 32768) * 48
  int32_t noteQ16 = (60 << 16) + (pitchQ16 - 32768) * 48;
  if (noteQ16 < 0)
    noteQ16 = 0;
  if (noteQ16 > (127 << 16))
    noteQ16 = (127 << 16);

  int32_t note = noteQ16 >> 16;
  int32_t frac = noteQ16 & 0xFFFF;

  uint32_t a = LUT_MTOF_INC[note];
  uint32_t b = LUT_MTOF_INC[note + 1];

  return a + (uint32_t)(((uint64_t)(b - a) * frac) >> 16);
}

static inline float fast_exp2(float x) {
  // Legacy support for synth params, but we should migrate to precise versions
  if (x < -10.0f)
    return 0;
  if (x > 10.0f)
    return 1024.0f;
  union {
    float f;
    int32_t i;
  } v;
  v.i = (int32_t)((x + 126.94269504f) * 8388608.0f);
  return v.f;
}

// ============================================================================
// Synth Engine Interface
// ============================================================================

typedef struct {
  // Render one sample. Returns rawOsc (before shared VCA/output stage).
  int32_t (*renderSample)(Voice *v, const SynthParams *p, int voiceIdx,
                          EnginePool *pool, bool gateRise, bool activeGate);

  // Per-block setup (e.g. Granular grain spawning, Sampler speed calc).
  void (*renderBlockSetup)(Voice *v, const SynthParams *p, int voiceIdx,
                           int count, EnginePool *pool, bool gateRise);

  // Reset engine state for a voice (AllNotesOff / mode switch)
  void (*reset)(int voiceIdx, EnginePool *pool);

  // Initialize voice data on NoteOn (e.g. Piano harmonics setup)
  void (*initVoice)(int voiceIdx, uint8_t note, uint8_t velocity);

  // Render a full block directly to L/R buffers (Piano additive).
  // Returns true if this engine handles its own output (bypasses shared VCA).
  void (*renderBlock)(Voice *v, const SynthParams *p, int voiceIdx, int count,
                      EnginePool *pool, bool gateRise, bool activeGate,
                      int32_t *bufL, int32_t *bufR);

  int16_t outputGainQ8; // Per-mode gain (256 = 0dB)
  const char *name;     // Human-readable name
} SynthEngine;

// ============================================================================
// Engine Declarations (defined in src/synths/*.cpp)
// ============================================================================

extern const SynthEngine engineNoise;
extern const SynthEngine engineWavetable;
extern const SynthEngine engineVABass;
extern const SynthEngine engineStrings;
extern const SynthEngine engineModal;
extern const SynthEngine engineGranular;
extern const SynthEngine enginePiano;
extern const SynthEngine engineSamplerOneShot;
extern const SynthEngine engineSamplerLoop;
extern const SynthEngine engineSamplerPlayer;
extern const SynthEngine engineSamplerDrums;
extern const SynthEngine engineFM;
extern const SynthEngine engineDrumSynth;

// ============================================================================
// Shared State (defined in SynthCore.cpp, accessed by engines via extern)
// ============================================================================

extern Voice voices[MAX_VOICES];
extern EnginePool pool;

// Granular Globals (Must be shared between spawn logic and render logic)
extern Grain globalGrains[MAX_GRAINS];
extern uint32_t globalGrainSpawnTimer;

// VA Bass state
extern uint32_t vabassSubPhase[MAX_VOICES];
extern int32_t vabassSvfBand[MAX_VOICES];
extern int32_t vabassFilterEnv[MAX_VOICES];
extern uint32_t vabassGlidePitch[MAX_VOICES];
extern uint32_t vabassPwmLfo;

// Modal state
extern int32_t modalBandL1[MAX_VOICES];
extern int32_t modalBandB1[MAX_VOICES];
extern int32_t modalBandL2[MAX_VOICES];
extern int32_t modalBandB2[MAX_VOICES];
extern int32_t modalImpulseEnv[MAX_VOICES];

#endif // SYNTH_ENGINE_H
