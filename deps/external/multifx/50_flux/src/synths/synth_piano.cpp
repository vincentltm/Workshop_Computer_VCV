// ============================================================================
// Piano Additive Synth Engine — Extracted from SynthCore.cpp
// ============================================================================
#include "SynthEngine.h"
#include <math.h>

// --- PianoData.h AMY legacy data ---
#ifndef PROGMEM
#define PROGMEM
#endif
typedef struct {
  uint32_t num_sample_times;
  const uint16_t *sample_times;
  uint32_t num_velocities;
  const uint8_t *velocities;
  uint32_t num_notes;
  const uint8_t *notes;
  const uint8_t *num_harmonics;
  const uint16_t *harmonics_freq;
  const uint8_t *harmonics_mags;
} interp_partials_voice_t;

#include "PianoData.h"

// ============================================================================
// Lookup Tables
// ============================================================================

// 1024-entry Sine LUT (Q15 signed)
static const int16_t pianoSineLUT[1024] = {
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

// dB-to-linear LUT for harmonic magnitudes
static const int16_t pianoDbToLinLUT[256] = {
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     3,     8,     13,
    19,    25,    32,    40,    49,    59,    70,    83,    97,    113,   131,
    151,   173,   199,   227,   259,   294,   334,   379,   430,   486,   549,
    621,   700,   790,   890,   1003,  1129,  1271,  1430,  1609,  1809,  2034,
    2286,  2570,  2887,  3243,  3643,  4092,  4595,  5160,  5794,  6505,  7302,
    8197,  9202,  10329, 11593, 13012, 14603, 16389, 18393, 20641, 23164, 25994,
    29170, 32734, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767,
};

#define PIANO_MAX_PARTIALS 16
#define SYNTH_BLOCK_SIZE 32

// ============================================================================
// Voice Initialization (called from NoteOn)
// ============================================================================
static void piano_initVoice(int voiceIdx, uint8_t note, uint8_t velocity) {
  Voice *v = &voices[voiceIdx];

  // 1. Find lower bound pitch/velocity indices
  uint8_t pitch_index = 0, vel_index = 0;
  while (pitch_index < NUM_PIANO_NOTES - 1 &&
         piano_notes[pitch_index + 1] < note)
    pitch_index++;
  while (vel_index < NUM_PIANO_VELOCITIES - 1 &&
         piano_velocities[vel_index + 1] < velocity)
    vel_index++;

  // 2. Interp weights (Q14)
  int32_t pAlphaQ14 = 0;
  if (pitch_index < NUM_PIANO_NOTES - 1) {
    pAlphaQ14 = ((int32_t)(note - piano_notes[pitch_index]) << 14) /
                (piano_notes[pitch_index + 1] - piano_notes[pitch_index]);
  }
  int32_t vAlphaQ14 = 0;
  if (vel_index < NUM_PIANO_VELOCITIES - 1) {
    vAlphaQ14 = ((int32_t)(velocity - piano_velocities[vel_index]) << 14) /
                (piano_velocities[vel_index + 1] - piano_velocities[vel_index]);
  }

  // 3. Bilinear Alphas (Q14)
  pool.eng.piano.a_alphas[voiceIdx][0] =
      (int16_t)(((16384 - pAlphaQ14) * (16384 - vAlphaQ14)) >> 14);
  pool.eng.piano.a_alphas[voiceIdx][1] =
      (int16_t)(((16384 - pAlphaQ14) * vAlphaQ14) >> 14);
  pool.eng.piano.a_alphas[voiceIdx][2] =
      (int16_t)((pAlphaQ14 * (16384 - vAlphaQ14)) >> 14);
  pool.eng.piano.a_alphas[voiceIdx][3] =
      (int16_t)((pAlphaQ14 * vAlphaQ14) >> 14);

  // 4. Harmonic Base Indices
  for (int i = 0; i < 4; i++) {
    int nIdx = (i < 2) ? pitch_index
                       : (pitch_index < NUM_PIANO_NOTES - 1 ? pitch_index + 1
                                                            : pitch_index);
    int vIdx = (i % 2 == 0)
                   ? vel_index
                   : (vel_index < NUM_PIANO_VELOCITIES - 1 ? vel_index + 1
                                                           : vel_index);
    int note_num = NUM_PIANO_VELOCITIES * nIdx + vIdx;
    int baseIdx = 0;
    for (int k = 0; k < note_num; ++k)
      baseIdx += piano_num_harmonics[k];
    pool.eng.piano.base_indices[voiceIdx][i] = baseIdx;
  }

  // 5. Harmonic Count
  int note_num_base = NUM_PIANO_VELOCITIES * pitch_index + vel_index;
  int numH = piano_num_harmonics[note_num_base];
  if (numH > PIANO_MAX_PARTIALS)
    numH = PIANO_MAX_PARTIALS;
  pool.eng.piano.numHarmonics[voiceIdx] = numH;

  // 6. Cache Interpolated Mag Envelopes & Freqs
  for (int h = 0; h < numH; h++) {
    float fL = (float)
        piano_harmonics_freq[pool.eng.piano.base_indices[voiceIdx][0] + h];
    float fH = (float)
        piano_harmonics_freq[pool.eng.piano.base_indices[voiceIdx][2] + h];

    // Precise frequency calculation (Fixed point replaces powf)
    int32_t mQ16 =
        (int32_t)(((fL + (fH - fL) * ((float)pAlphaQ14 / 16384.0f))) *
                  655.36f); // Note units to Q16
    // pitchQ16 where Middle C (60) is 0.5 (32768). Mapping is: pitch = 0.5 +
    // (note - 60) / 48
    int32_t pQ16 = 32768 + (mQ16 - (60 << 16)) / 48;
    pool.eng.piano.phaseInc[voiceIdx][h] = precise_mtof_q16(pQ16)
                                           * AUDIO_SAMPLE_RATE_DIV; // Scale for sample rate
    // Randomize phase to smear strike impulse
    v->noiseState = v->noiseState * 1664525 + 1013904223;
    pool.eng.piano.phase[voiceIdx][h] = v->noiseState;
    pool.eng.piano.currentMag[voiceIdx][h] = 0;

    for (int t = 0; t < 20; t++) {
      int32_t m_pl_vl =
          (int32_t)piano_harmonics_mags
              [(pool.eng.piano.base_indices[voiceIdx][0] + h) * 20 + t]
          << 14;
      int32_t m_pl_vh =
          (int32_t)piano_harmonics_mags
              [(pool.eng.piano.base_indices[voiceIdx][1] + h) * 20 + t]
          << 14;
      int32_t m_ph_vl =
          (int32_t)piano_harmonics_mags
              [(pool.eng.piano.base_indices[voiceIdx][2] + h) * 20 + t]
          << 14;
      int32_t m_ph_vh =
          (int32_t)piano_harmonics_mags
              [(pool.eng.piano.base_indices[voiceIdx][3] + h) * 20 + t]
          << 14;

      int64_t mag = (int64_t)m_pl_vl * pool.eng.piano.a_alphas[voiceIdx][0] +
                    (int64_t)m_pl_vh * pool.eng.piano.a_alphas[voiceIdx][1] +
                    (int64_t)m_ph_vl * pool.eng.piano.a_alphas[voiceIdx][2] +
                    (int64_t)m_ph_vh * pool.eng.piano.a_alphas[voiceIdx][3];

      if (mQ16 > (105 << 16)) { // Approx 20kHz
        pool.eng.piano.envCache[voiceIdx][h][t] = 0;
      } else {
        pool.eng.piano.envCache[voiceIdx][h][t] = (uint8_t)(mag >> 28);
      }
    }
  }
  pool.eng.piano.envTime[voiceIdx] = 0;
}

// ============================================================================
// Full Block Render (handles envelope, additive synthesis, and stereo output)
// ============================================================================
static void piano_renderBlock(Voice *v, const SynthParams *p, int voiceIdx,
                              int count, EnginePool *pool, bool gateRise,
                              bool activeGate, int32_t *bufL, int32_t *bufR) {
  // --- ENVELOPE INTERPOLATION ---
  // ENVELOPE INTERPOLATION (Fixed point)
  int32_t envShiftQ16 = (int32_t)((p->envelope - 0.3f) * 8.0f * 65536.0f);
  uint32_t pTimeScaleQ30 = precise_exp2_q30(envShiftQ16);
  float pTimeScale = (float)pTimeScaleQ30 / 1073741824.0f;

  // float pPitchRatio_unused = fast_exp2((p->pitch - 0.5f) * 2.0f);

  float tMsReal = (float)pool->eng.piano.envTime[voiceIdx] * 1000.0f /
                  (float)AUDIO_BASE_RATE; // Dynamic base rate

  uint32_t tMs;
  if (tMsReal < 128.0f) {
    tMs = (uint32_t)tMsReal;
  } else {
    tMs = 128 + (uint32_t)((tMsReal - 128.0f) / pTimeScale);
  }

  if (tMs >= 4096)
    tMs = 4095;

  {
    int tL = 0;
    for (int t = 0; t < 19; t++) {
      if (piano_sample_times_ms[t] <= tMs)
        tL = t;
      else
        break;
    }

    int32_t tAlphaQ8 = 0;

    if (tMs < piano_sample_times_ms[0]) {
      tAlphaQ8 = (tMs * 256) / piano_sample_times_ms[0];
    } else {
      int tH = (tL < 19) ? tL + 1 : tL;
      if (tH > tL) {
        int32_t den = piano_sample_times_ms[tH] - piano_sample_times_ms[tL];
        tAlphaQ8 = ((int32_t)(tMs - piano_sample_times_ms[tL]) << 8) / den;
        if (tAlphaQ8 > 256)
          tAlphaQ8 = 256;
      }
    }

    float pPitchRatio = 1.0f;
    if (!v->isMidi) {
      // Ratio = 2^((Pitch - 0.5) * 4.0).
      int32_t pitchShiftQ16 = (int32_t)((p->pitch - 0.5f) * 4.0f * 65536.0f);
      uint32_t ratioQ30 = precise_exp2_q30(pitchShiftQ16);
      pPitchRatio = (float)ratioQ30 / 1073741824.0f;
    }
    int32_t bQ15 = (int32_t)(p->timbre * 32768.0f);
    int nH = pool->eng.piano.numHarmonics[voiceIdx];

    for (int h = 0; h < nH; h++) {
      pool->eng.piano.phaseIncScaled[voiceIdx][h] =
          (uint32_t)(pool->eng.piano.phaseInc[voiceIdx][h] * pPitchRatio);

      int32_t db;
      if (tMs < piano_sample_times_ms[0]) {
        int32_t targetDb = pool->eng.piano.envCache[voiceIdx][h][0];
        db = (tAlphaQ8 * targetDb) >> 8;
      } else {
        int32_t dbL_val = pool->eng.piano.envCache[voiceIdx][h][tL];
        int tH = (tL < 19) ? tL + 1 : tL;
        int32_t dbH_val = pool->eng.piano.envCache[voiceIdx][h][tH];
        db = ((256 - tAlphaQ8) * dbL_val + tAlphaQ8 * dbH_val) >> 8;
      }

      if (db > 255)
        db = 255;
      if (db < 0)
        db = 0;

      int32_t linQ15 = pianoDbToLinLUT[db];
      if (h > 0) {
        int32_t atten = 32768 - (((32768 - bQ15) * h) >> 2);
        if (atten < 0)
          atten = 0;
        linQ15 = (linQ15 * atten) >> 15;
      }
      pool->eng.piano.targetMag[voiceIdx][h] =
          (int16_t)linQ15; // If this is a new strike (Gate Rise), reset energy
      if (gateRise) {
        pool->eng.piano.currentMag[voiceIdx][h] = 0;
        pool->eng.piano.phase[voiceIdx][h] =
            0; // Reset Phase to prevent smearing
      }
    }
  }
  pool->eng.piano.envTime[voiceIdx] += count;

  // --- ADDITIVE RENDERING ---
  int32_t pianoAcc[SYNTH_BLOCK_SIZE] = {0};
  int nH = pool->eng.piano.numHarmonics[voiceIdx];
  for (int h = 0; h < nH; h++) {
    int32_t targetVal = (int32_t)pool->eng.piano.targetMag[voiceIdx][h] << 8;
    int32_t curMag = pool->eng.piano.currentMag[voiceIdx][h];
    if (curMag == 0 && targetVal == 0)
      continue;

    int32_t delta = (targetVal - curMag) >> 4;
    curMag += delta;
    pool->eng.piano.currentMag[voiceIdx][h] = curMag;

    int32_t hMag = curMag >> 8;
    uint32_t ph = pool->eng.piano.phase[voiceIdx][h];
    uint32_t phInc = pool->eng.piano.phaseIncScaled[voiceIdx][h];

    // Anti-aliasing: skip harmonics above 16kHz
    if (phInc > 1431655765)
      continue;

    for (int k = 0; k < count; k++) {
      int idx = (ph >> 22) & 0x3FF;
      int frac = (ph >> 7) & 0x7FFF;
      int16_t s1 = pianoSineLUT[idx];
      int16_t s2 = pianoSineLUT[(idx + 1) & 0x3FF];
      int32_t sine = s1 + (((s2 - s1) * frac) >> 15);
      pianoAcc[k] += (sine * hMag) >> 15;
      ph += phInc;
    }
    pool->eng.piano.phase[voiceIdx][h] = ph;
  }

  // Safety saturation
  for (int k = 0; k < count; k++) {
    if (pianoAcc[k] > 90000)
      pianoAcc[k] = 90000 + (pianoAcc[k] - 90000) / 4;
    else if (pianoAcc[k] < -90000)
      pianoAcc[k] = -90000 + (pianoAcc[k] + 90000) / 4;
  }

  // --- ENVELOPE + STEREO OUTPUT ---
  for (int k = 0; k < count; k++) {
    // VCA envelope
    if (!activeGate) {
      if (v->envVal > 0) {
        v->envVal -= v->releaseRate;
        if (v->envVal < 0) {
          v->envVal = 0;
          v->active = false;
        }
      }
    } else {
      if (v->envVal < 1073741824) {
        v->envVal += v->attackRate;
        if (v->envVal > 1073741824)
          v->envVal = 1073741824;
      }
    }

    int32_t sum = pianoAcc[k];

    // Note-based stereo pan (low notes left, high notes right)
    int32_t pan = (v->note - 24) * 32768 / (104 - 24);
    if (pan < 0)
      pan = 0;
    if (pan > 32767)
      pan = 32767;
    int32_t lPan = 32767 - pan;
    int32_t rPan = pan;

    int32_t monoOut = (sum * 3) >> 4;
    int32_t v_gain = v->envVal >> 15;
    int32_t out = (monoOut * v_gain) >> 15;

    if (out > 32767)
      out = 32767;
    else if (out < -32768)
      out = -32768;

    // DC-blocking LPF
    v->lpfState += ((out - v->lpfState) * AUDIO_BASE_RATE) >> 16;
    int32_t filteredOut = v->lpfState;

    int32_t v_vel = v->velocity;
    int32_t sL = (filteredOut * lPan) >> 15;
    int32_t sR = (filteredOut * rPan) >> 15;

    bufL[k] += (sL * v_vel) >> 7;
    bufR[k] += (sR * v_vel) >> 7;
  }

  // Completion check
  if (!activeGate && v->envVal == 0) {
    v->active = false;
  }
}

// ============================================================================
// Engine Definition
// ============================================================================
const SynthEngine enginePiano = {.renderSample =
                                     nullptr, // Piano uses renderBlock instead
                                 .renderBlockSetup = nullptr,
                                 .reset = nullptr,
                                 .initVoice = piano_initVoice,
                                 .renderBlock = piano_renderBlock,
                                 .outputGainQ8 = 640,
                                 .name = "Piano"};
