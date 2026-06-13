#include "SynthEngine.h"
#include "SynthCore.h" // for AUDIO_BASE_RATE
#include "fm_algorithms.h"
#include "fm_presets.h"
#include <math.h> // Minimal usage (control rate only if needed)

// ============================================================================
// FM Synth Engine (DX7 Compatible - Fixed Point)
// ============================================================================

// --- Constants & LUTs ---
#define FM_BLOCK_SIZE 32

// We need a sine LUT. If MathTables doesn't expose one, we use a local one.
// This is the same 1024-entry Q15 Sine LUT.
static const int16_t fmSineLUT[] = {
    0,      201,    402,    603,    804,    1005,   1206,   1406,   1607,
    1808,   2009,   2209,   2410,   2610,   2811,   3011,   3211,   3411,
    3611,   3811,   4011,   4210,   4409,   4608,   4807,   5006,   5205,
    5403,   5601,   5799,   5997,   6195,   6392,   6589,   6786,   6982,
    7179,   7375,   7571,   7766,   7961,   8156,   8351,   8545,   8739,
    8932,   9126,   9319,   9511,   9703,   9895,   10087,  10278,  10469,
    10659,  10849,  11038,  11227,  11416,  11604,  11792,  11980,  12166,
    12353,  12539,  12724,  12909,  13094,  13278,  13462,  13645,  13827,
    14009,  14191,  14372,  14552,  14732,  14911,  15090,  15268,  15446,
    15623,  15799,  15975,  16150,  16325,  16499,  16672,  16845,  17017,
    17189,  17360,  17530,  17699,  17868,  18036,  18204,  18371,  18537,
    18702,  18867,  19031,  19194,  19357,  19519,  19680,  19840,  20000,
    20159,  20317,  20474,  20631,  20787,  20942,  21096,  21249,  21402,
    21554,  21705,  21855,  22004,  22153,  22301,  22448,  22594,  22739,
    22883,  23027,  23169,  23311,  23452,  23592,  23731,  23869,  24006,
    24143,  24278,  24413,  24546,  24679,  24811,  24942,  25072,  25201,
    25329,  25456,  25582,  25707,  25831,  25954,  26077,  26198,  26318,
    26437,  26556,  26673,  26789,  26905,  27019,  27132,  27244,  27355,
    27466,  27575,  27683,  27790,  27896,  28001,  28105,  28208,  28309,
    28410,  28510,  28608,  28706,  28802,  28897,  28992,  29085,  29177,
    29268,  29358,  29446,  29534,  29621,  29706,  29790,  29873,  29955,
    30036,  30116,  30195,  30272,  30349,  30424,  30498,  30571,  30643,
    30713,  30783,  30851,  30918,  30984,  31049,  31113,  31175,  31236,
    31297,  31356,  31413,  31470,  31525,  31580,  31633,  31684,  31735,
    31785,  31833,  31880,  31926,  31970,  32014,  32056,  32097,  32137,
    32176,  32213,  32249,  32284,  32318,  32350,  32382,  32412,  32441,
    32468,  32495,  32520,  32544,  32567,  32588,  32609,  32628,  32646,
    32662,  32678,  32692,  32705,  32717,  32727,  32736,  32744,  32751,
    32757,  32761,  32764,  32766,  32767,  32766,  32764,  32761,  32757,
    32751,  32744,  32736,  32727,  32717,  32705,  32692,  32678,  32662,
    32646,  32628,  32609,  32588,  32567,  32544,  32520,  32495,  32468,
    32441,  32412,  32382,  32350,  32318,  32284,  32249,  32213,  32176,
    32137,  32097,  32056,  32014,  31970,  31926,  31880,  31833,  31785,
    31735,  31684,  31633,  31580,  31525,  31470,  31413,  31356,  31297,
    31236,  31175,  31113,  31049,  30984,  30918,  30851,  30783,  30713,
    30643,  30571,  30498,  30424,  30349,  30272,  30195,  30116,  30036,
    29955,  29873,  29790,  29706,  29621,  29534,  29446,  29358,  29268,
    29177,  29085,  28992,  28897,  28802,  28706,  28608,  28510,  28410,
    28309,  28208,  28105,  28001,  27896,  27790,  27683,  27575,  27466,
    27355,  27244,  27132,  27019,  26905,  26789,  26673,  26556,  26437,
    26318,  26198,  26077,  25954,  25831,  25707,  25582,  25456,  25329,
    25201,  25072,  24942,  24811,  24679,  24546,  24413,  24278,  24143,
    24006,  23869,  23731,  23592,  23452,  23311,  23169,  23027,  22883,
    22739,  22594,  22448,  22301,  22153,  22004,  21855,  21705,  21554,
    21402,  21249,  21096,  20942,  20787,  20631,  20474,  20317,  20159,
    20000,  19840,  19680,  19519,  19357,  19194,  19031,  18867,  18702,
    18537,  18371,  18204,  18036,  17868,  17699,  17530,  17360,  17189,
    17017,  16845,  16672,  16499,  16325,  16150,  15975,  15799,  15623,
    15446,  15268,  15090,  14911,  14732,  14552,  14372,  14191,  14009,
    13827,  13645,  13462,  13278,  13094,  12909,  12724,  12539,  12353,
    12166,  11980,  11792,  11604,  11416,  11227,  11038,  10849,  10659,
    10469,  10278,  10087,  9895,   9703,   9511,   9319,   9126,   8932,
    8739,   8545,   8351,   8156,   7961,   7766,   7571,   7375,   7179,
    6982,   6786,   6589,   6392,   6195,   5997,   5799,   5601,   5403,
    5205,   5006,   4807,   4608,   4409,   4210,   4011,   3811,   3611,
    3411,   3211,   3011,   2811,   2610,   2410,   2209,   2009,   1808,
    1607,   1406,   1206,   1005,   804,    603,    402,    201,    0,
    -201,   -402,   -603,   -804,   -1005,  -1206,  -1406,  -1607,  -1808,
    -2009,  -2209,  -2410,  -2610,  -2811,  -3011,  -3211,  -3411,  -3611,
    -3811,  -4011,  -4210,  -4409,  -4608,  -4807,  -5006,  -5205,  -5403,
    -5601,  -5799,  -5997,  -6195,  -6392,  -6589,  -6786,  -6982,  -7179,
    -7375,  -7571,  -7766,  -7961,  -8156,  -8351,  -8545,  -8739,  -8932,
    -9126,  -9319,  -9511,  -9703,  -9895,  -10087, -10278, -10469, -10659,
    -10849, -11038, -11227, -11416, -11604, -11792, -11980, -12166, -12353,
    -12539, -12724, -12909, -13094, -13278, -13462, -13645, -13827, -14009,
    -14191, -14372, -14552, -14732, -14911, -15090, -15268, -15446, -15623,
    -15799, -15975, -16150, -16325, -16499, -16672, -16845, -17017, -17189,
    -17360, -17530, -17699, -17868, -18036, -18204, -18371, -18537, -18702,
    -18867, -19031, -19194, -19357, -19519, -19680, -19840, -20000, -20159,
    -20317, -20474, -20631, -20787, -20942, -21096, -21249, -21402, -21554,
    -21705, -21855, -22004, -22153, -22301, -22448, -22594, -22739, -22883,
    -23027, -23169, -23311, -23452, -23592, -23731, -23869, -24006, -24143,
    -24278, -24413, -24546, -24679, -24811, -24942, -25072, -25201, -25329,
    -25456, -25582, -25707, -25831, -25954, -26077, -26198, -26318, -26437,
    -26556, -26673, -26789, -26905, -27019, -27132, -27244, -27355, -27466,
    -27575, -27683, -27790, -27896, -28001, -28105, -28208, -28309, -28410,
    -28510, -28608, -28706, -28802, -28897, -28992, -29085, -29177, -29268,
    -29358, -29446, -29534, -29621, -29706, -29790, -29873, -29955, -30036,
    -30116, -30195, -30272, -30349, -30424, -30498, -30571, -30643, -30713,
    -30783, -30851, -30918, -30984, -31049, -31113, -31175, -31236, -31297,
    -31356, -31413, -31470, -31525, -31580, -31633, -31684, -31735, -31785,
    -31833, -31880, -31926, -31970, -32014, -32056, -32097, -32137, -32176,
    -32213, -32249, -32284, -32318, -32350, -32382, -32412, -32441, -32468,
    -32495, -32520, -32544, -32567, -32588, -32609, -32628, -32646, -32662,
    -32678, -32692, -32705, -32717, -32727, -32736, -32744, -32751, -32757,
    -32761, -32764, -32766, -32767, -32766, -32764, -32761, -32757, -32751,
    -32744, -32736, -32727, -32717, -32705, -32692, -32678, -32662, -32646,
    -32628, -32609, -32588, -32567, -32544, -32520, -32495, -32468, -32441,
    -32412, -32382, -32350, -32318, -32284, -32249, -32213, -32176, -32137,
    -32097, -32056, -32014, -31970, -31926, -31880, -31833, -31785, -31735,
    -31684, -31633, -31580, -31525, -31470, -31413, -31356, -31297, -31236,
    -31175, -31113, -31049, -30984, -30918, -30851, -30783, -30713, -30643,
    -30571, -30498, -30424, -30349, -30272, -30195, -30116, -30036, -29955,
    -29873, -29790, -29706, -29621, -29534, -29446, -29358, -29268, -29177,
    -29085, -28992, -28897, -28802, -28706, -28608, -28510, -28410, -28309,
    -28208, -28105, -28001, -27896, -27790, -27683, -27575, -27466, -27355,
    -27244, -27132, -27019, -26905, -26789, -26673, -26556, -26437, -26318,
    -26198, -26077, -25954, -25831, -25707, -25582, -25456, -25329, -25201,
    -25072, -24942, -24811, -24679, -24546, -24413, -24278, -24143, -24006,
    -23869, -23731, -23592, -23452, -23311, -23169, -23027, -22883, -22739,
    -22594, -22448, -22301, -22153, -22004, -21855, -21705, -21554, -21402,
    -21249, -21096, -20942, -20787, -20631, -20474, -20317, -20159, -20000,
    -19840, -19680, -19519, -19357, -19194, -19031, -18867, -18702, -18537,
    -18371, -18204, -18036, -17868, -17699, -17530, -17360, -17189, -17017,
    -16845, -16672, -16499, -16325, -16150, -15975, -15799, -15623, -15446,
    -15268, -15090, -14911, -14732, -14552, -14372, -14191, -14009, -13827,
    -13645, -13462, -13278, -13094, -12909, -12724, -12539, -12353, -12166,
    -11980, -11792, -11604, -11416, -11227, -11038, -10849, -10659, -10469,
    -10278, -10087, -9895,  -9703,  -9511,  -9319,  -9126,  -8932,  -8739,
    -8545,  -8351,  -8156,  -7961,  -7766,  -7571,  -7375,  -7179,  -6982,
    -6786,  -6589,  -6392,  -6195,  -5997,  -5799,  -5601,  -5403,  -5205,
    -5006,  -4807,  -4608,  -4409,  -4210,  -4011,  -3811,  -3611,  -3411,
    -3211,  -3011,  -2811,  -2610,  -2410,  -2209,  -2009,  -1808,  -1607,
    -1406,  -1206,  -1005,  -804,   -603,   -402,   -201};

static inline int16_t FastSinQ15(uint32_t phase) {
  return fmSineLUT[(phase >> 22) & 0x3FF];
}

// ============================================================================
// Internal State Accessors
// ============================================================================

static void fm_initVoice(int voiceIdx, uint8_t note, uint8_t velocity) {
  for (int op = 0; op < MAX_ALGO_OPS; op++) {
    pool.eng.fm.opPhase[voiceIdx][op] = 0;
    pool.eng.fm.opEnvVal[voiceIdx][op] = 0; // Start at 0
    pool.eng.fm.opState[voiceIdx][op] =
        1; // Target Point 1 (Point 0 is initial)
    pool.eng.fm.feedback[voiceIdx] = 0;
  }
  pool.eng.fm.currentPreset[voiceIdx] = 255; // Force update on first render
}

// Calculate rate to reach targetLevel from currentLevel in timeMs
// All integer math.
// timeMs is uint32_t. targetLevel is int32_t (Q15). currentLevel is int32_t
// (Q15).
static int32_t calculateRate(int32_t currentLevel, int32_t targetLevel,
                             uint32_t timeMs) {
  if (timeMs == 0)
    return (targetLevel - currentLevel); // Immediate jump

  // Total samples = timeMs * (SampleRate / 1000)
  int32_t samples = (timeMs * (AUDIO_BASE_RATE / 1000));
  if (samples < 1)
    samples = 1;

  // Update envelopes PER BLOCK (32 samples)
  int32_t blocks = samples / FM_BLOCK_SIZE;
  if (blocks < 1)
    blocks = 1;

  int32_t rate = (targetLevel - currentLevel) / blocks;
  // Ensure minimal movement to prevent stalling
  if (rate == 0 && currentLevel != targetLevel) {
    rate = (targetLevel > currentLevel) ? 1 : -1;
  }
  return rate;
}

static void fm_updateEnvelopes(int voiceIdx, const FmPreset *preset,
                               bool activeGate, int32_t envSpeedModQ8) {
  for (int op = 0; op < MAX_ALGO_OPS; op++) {
    const FmOpParams *params = &preset->ops[op]; // 0..5

    int currentLevel = pool.eng.fm.opEnvVal[voiceIdx][op];
    uint8_t targetIdx = pool.eng.fm.opState[voiceIdx][op]; // Target Point Index

    // --- State Machine ---
    // 1. Check if we reached target
    bool reached = false;
    // envLevels is int16_t Q15. targetLevel needs to be int32_t Q15.
    // No float scaling needed!
    int32_t targetLevelVal = params->envLevels[targetIdx];

    // Optimize: If time is 0, we jump immediately.
    if (params->envTimes[targetIdx] == 0) {
      currentLevel = targetLevelVal;
      reached = true;
    } else {
      // Move towards target
      int32_t rate = pool.eng.fm.opRate[voiceIdx][op];
      currentLevel += rate;

      if ((rate > 0 && currentLevel >= targetLevelVal) ||
          (rate < 0 && currentLevel <= targetLevelVal)) {
        currentLevel = targetLevelVal;
        reached = true;
      }
    }

    // 2. Handle State Transitions
    if (reached) {
      int sustainIdx = params->envCount - 2;
      if (sustainIdx < 0)
        sustainIdx = 0;

      if (activeGate) {
        if (targetIdx < sustainIdx) {
          targetIdx++; // Advance to next segment
        }
      } else {
        if (targetIdx < params->envCount - 1) {
          targetIdx = params->envCount - 1;
        }
        if (targetIdx == params->envCount - 1 &&
            currentLevel == targetLevelVal) {
          // Envelope Finished
          // Force silence if level is low (fix for non-zero release levels like
          // 6)
          if (currentLevel < 40) {
            currentLevel = 0;
            targetLevelVal = 0;
          }
        }
      }

      // Recalculate Rate for new target
      int32_t newTargetLevel = params->envLevels[targetIdx];
      uint32_t time = params->envTimes[targetIdx];

      // User Envelope Knob Mod (envSpeedModQ8: 256 = 1.0x)
      uint64_t timeScaled = ((uint64_t)time * envSpeedModQ8) >> 8;

      pool.eng.fm.opRate[voiceIdx][op] =
          calculateRate(currentLevel, newTargetLevel, (uint32_t)timeScaled);
    } else {
      // Check for Gate Off interrupt
      if (!activeGate && targetIdx < params->envCount - 1) {
        targetIdx = params->envCount - 1;
        targetLevelVal = params->envLevels[targetIdx];
        uint32_t time = params->envTimes[targetIdx];

        uint64_t timeScaled = ((uint64_t)time * envSpeedModQ8) >> 8;
        pool.eng.fm.opRate[voiceIdx][op] =
            calculateRate(currentLevel, targetLevelVal, (uint32_t)timeScaled);
      }
    }

    pool.eng.fm.opState[voiceIdx][op] = targetIdx;
    pool.eng.fm.opEnvVal[voiceIdx][op] = currentLevel;
  }
}

static void fm_renderBlock(Voice *v, const SynthParams *p, int voiceIdx,
                           int count, EnginePool *pool, bool gateRise,
                           bool activeGate, int32_t *bufL, int32_t *bufR) {

  // 1. Select Preset
  // p->filterCutoff is 0.0-1.0. Map to 0-31.
  // INVERTED: 1.0 (Default) -> Preset 0 (Brass 1). 0.0 -> Preset 31 (SFX).
  int presetIdx = 31 - (int)(p->filterCutoff * 31.99f);
  if (presetIdx < 0)
    presetIdx = 0;
  if (presetIdx > 31)
    presetIdx = 31;

  const FmPreset *preset = &fm_presets[presetIdx];
  pool->eng.fm.algorithm[voiceIdx] = preset->algorithm;

  // Check for Preset Change
  if (pool->eng.fm.currentPreset[voiceIdx] != presetIdx) {
    pool->eng.fm.currentPreset[voiceIdx] = presetIdx;

    // If voice is active/gated, force a re-trigger to smooth transition
    // If not gated, we should probably kill it to prevent 'ghost' tails from
    // wrong algo
    if (activeGate) {
      gateRise = true;
    } else {
      // Kill voice immediately if not held, to prevent weird tails
      v->active = false;
      v->envVal = 0;
      return; // Stop processing this block
    }
  }

  // 2. Gate Rise Reset
  if (gateRise) {
    pool->eng.fm.feedback[voiceIdx] = 0;

    for (int op = 0; op < MAX_ALGO_OPS; op++) {
      // Force Start Level (Q15 directly)
      pool->eng.fm.opEnvVal[voiceIdx][op] = preset->ops[op].envLevels[0];

      pool->eng.fm.opState[voiceIdx][op] = 1;

      // Calc initial rates
      int32_t start = pool->eng.fm.opEnvVal[voiceIdx][op];
      int32_t end = preset->ops[op].envLevels[1];
      pool->eng.fm.opRate[voiceIdx][op] =
          calculateRate(start, end, preset->ops[op].envTimes[1]);

      // Reset Phase for consistent attack
      pool->eng.fm.opPhase[voiceIdx][op] = 0;
    }
  }

  // 3. User Controls (Float -> Fixed mappings)

  // Envelope Speed:
  // 0.0 -> 0.2x (51 in Q8)
  // 0.5 -> 1.0x (256 in Q8)
  // 1.0 -> 5.0x (1280 in Q8)
  int32_t envSpeedModQ8;
  if (p->envelope < 0.5f) {
    // Range 0.2 to 1.0 (51 to 256). Span 205.
    // p->envelope * 2 * 205 + 51?
    // Let's keep it simple: envelope * 410 + 51
    envSpeedModQ8 = (int32_t)(p->envelope * 410.0f) + 51;
  } else {
    // Range 1.0 to 5.0 (256 to 1280). Span 1024.
    // (p->envelope - 0.5) * 2 * 1024 + 256
    envSpeedModQ8 = (int32_t)((p->envelope - 0.5f) * 2048.0f) + 256;
  }

  // Timbre / Mod Depth:
  // Range 0..2.5x
  // Q12: 2.5 * 4096 = 10240.
  int32_t modDepthQ12 = (int32_t)(p->timbre * 10240.0f);

  // Velocity Scaling (0-127 → Q8, capped at ~1.5x to prevent clipping)
  int32_t velocityQ8 = v->velocity + (v->velocity >> 1); // max 190 vs old 254
  if (velocityQ8 < 10)
    velocityQ8 = 10; // Floor to avoid silence

  // 4. Update Envelopes
  fm_updateEnvelopes(voiceIdx, preset, activeGate, envSpeedModQ8);

  // 5. Setup for Audio Render
  const struct FmAlgorithm *algo = &algorithms[preset->algorithm];

  uint32_t baseInc = v->phaseInc; // Q16
  uint32_t opIncs[6];
  int32_t opAmps[6];

  for (int i = 0; i < 6; i++) {
    // Frequency: BaseInc * Ratio(Q8)
    // Result needs to be Q16.
    // baseInc(Q16) * ratio(Q8) >> 8 = Q16.
    // Careful with overflow if baseInc is high.
    // baseInc is up to 10kHz~. 48kHz = full.
    // Max Ratio 255.
    // 64-bit mutiply needed safety.
    opIncs[i] = (uint32_t)(((uint64_t)baseInc * preset->ops[i].ratio) >> 8);

    // Amplitude:
    // Level (Q15) * PresetAmp (Q12).
    // Modulators scaled by ModDepth (Q12).
    // Global Scaling 0.25 (>> 2).

    int32_t level = pool->eng.fm.opEnvVal[voiceIdx][i]; // Q15
    int32_t opGain = preset->ops[i].amp;                // Q12

    bool isCarrier = (algo->ops[i] & OUT_BUS_ADD);

    if (isCarrier) {
      // Apply Velocity Scaling to Carriers (Volume Dynamics)
      opGain = (opGain * velocityQ8) >> 8;
    } else {
      // Modulators: Apply Timbre Knob and preserve depth for richness
      opGain = (opGain * modDepthQ12) >> 12;
      opGain >>= 1;
    }

    // Final Amp: Level(Q15) * Gain(Q12) >> 12 -> Q15
    opAmps[i] = (level * opGain) >> 12;
  }

  // 6. Audio Loop (DSP)
  // Static to avoid ~512 bytes of stack allocation inside the ISR which
  // overflows the RP2040 IRQ stack and causes a crash after brief FM play.
  // FM is only ever called from one context (Core 1 block renderer) so
  // static is safe here.
  static int32_t bus[3][FM_BLOCK_SIZE];
  for (int b = 0; b < 3; b++)
    for (int k = 0; k < count; k++)
      bus[b][k] = 0;

  static int32_t outputL[FM_BLOCK_SIZE];
  for (int k = 0; k < count; k++) {
    outputL[k] = 0;
  }

  // Count carriers for output normalization
  int carrierCount = 0;
  for (int op = 0; op < MAX_ALGO_OPS; op++) {
    if (algo->ops[op] & OUT_BUS_ADD)
      carrierCount++;
  }
  if (carrierCount < 1)
    carrierCount = 1;

  for (int op = 0; op < MAX_ALGO_OPS; op++) {
    uint8_t flags = algo->ops[op];

    int32_t *inBuf = nullptr;
    if (flags & IN_BUS_ONE)
      inBuf = bus[0];
    else if (flags & IN_BUS_TWO)
      inBuf = bus[1];

    int32_t fbVal = 0;
    if (flags & FB_IN)
      fbVal = pool->eng.fm.feedback[voiceIdx];

    int32_t *outBuf = nullptr;

    if (flags & OUT_BUS_ONE)
      outBuf = bus[0];
    else if (flags & OUT_BUS_TWO)
      outBuf = bus[1];

    // Loop unroll or optimize? 32 samples.
    for (int k = 0; k < count; k++) {
      pool->eng.fm.opPhase[voiceIdx][op] += opIncs[op];
      uint32_t p = pool->eng.fm.opPhase[voiceIdx][op];

      int32_t mod = 0;
      if (inBuf)
        mod = inBuf[k] << 14;
      if (flags & FB_IN) {
        // feedback is Q15. preset->feedback is Q15.
        // Product is Q30, which matches phase mod scale (Q29~Q32).
        // Previous float equivalent: fb * C * 32768.
        // Here: fb * (C * 32768).
        mod += (fbVal * preset->feedback);
      }

      // Output Calculation
      // Sin(Q15) * Amp(Q15?) >> 15 -> Q15.
      int32_t sample =
          (int32_t)(((int64_t)FastSinQ15(p + mod) * opAmps[op]) >> 15);

      if (flags & FB_OUT) {
        fbVal = sample;
        pool->eng.fm.feedback[voiceIdx] = sample;
      }

      if (outBuf) {
        if (!(flags & OUT_BUS_ADD))
          outBuf[k] = sample;
        else
          outBuf[k] += sample;
      }

      if (flags & OUT_BUS_ADD) {
        outputL[k] += sample;
      }
    }
  }

  // 7. Mix to Final Output (normalize by carrier count to prevent clipping)
  for (int k = 0; k < count; k++) {
    int32_t normalized = outputL[k] / carrierCount;
    bufL[k] += normalized;
    bufR[k] += normalized;
  }

  if (!activeGate) {
    bool allDone = true;
    for (int i = 0; i < 6; i++) {
      // Threshold of 40 (approx -60dB) to handle non-zero release tails
      if (pool->eng.fm.opEnvVal[voiceIdx][i] > 40) {
        allDone = false;
        break;
      }
    }
    if (allDone) {
      v->active = false;
      v->envVal = 0; // Required to signal "Free Voice" to NoteOn allocator
    }
  }
}

extern const SynthEngine engineFM = {
    .renderSample = nullptr,
    .renderBlockSetup = nullptr,
    .reset = nullptr,
    .initVoice = fm_initVoice,
    .renderBlock = fm_renderBlock,
    .outputGainQ8 = 16, // Reduced from 24 for headroom with multi-carrier algos
    .name = "FM Synth"};
