#pragma once
#include "SynthCore.h" // for SYNTH_MODE_* constants
#include <stdint.h>

// ============================================================================
// KnobLock — Large-delta unlock (NOT soft-takeover)
//
// When engaged, the knob is frozen until the user moves it significantly
// from where it was when the lock was engaged. Prevents parameter jumps
// when flipping the switch between modes.
// ============================================================================
struct KnobLock {
  bool locked = true;
  int32_t ref = 0;
  int32_t threshold = 100; // ADC counts (~2.4% of 4096 range)

  void engage(int32_t currentVal) {
    locked = true;
    ref = currentVal;
  }

  // Returns true if unlocked (parameter may be used).
  bool update(int32_t currentVal) {
    if (locked) {
      int32_t d = currentVal - ref;
      if (d < 0) d = -d;
      if (d > threshold) locked = false;
    }
    return !locked;
  }

  void unlock() { locked = false; }
};

// ============================================================================
// PageState — persistent knob values for each switch layer (0-4095 range)
// Updated only when the corresponding knob is unlocked on that page.
// ============================================================================
struct PageState {
  int32_t main = 0;
  int32_t x    = 0;
  int32_t y    = 0;
};

// ============================================================================
// MenuState — the three exclusive UI states + EXTRA page stub
// ============================================================================
enum class MenuState {
  // Switch UP:
  //   source=SYNTH: edit synth params (Main=Env/Decay, X=Pitch, Y=Timbre)
  //                 exception: Noise uses Y=Filter Cutoff
  //   source=EXT:   freeze effect (Freeze param set to 4095)
  SYNTH_EDIT,

  // Switch MIDDLE: edit effect params (Main=Blend/p0, X=Time/p1, Y=Feedback/p2)
  EFFECT_EDIT,

  // Switch DOWN: select page
  //   Main = Effect select     (Bypass/effect 0: Main = Volume instead)
  //   X    = Synth/Source select
  //   Y    = Z param (Filter Cutoff for most synths; Sample Select for samplers)
  SELECT,

  // Switch DOWN (double-tap): reserved for future performance controls
  EXTRA,
};

// ============================================================================
// SynthVoiceMap — single source of truth for synth mode → voice announcement
// ============================================================================
struct SynthVoiceEntry {
  uint8_t mode;
  uint8_t voiceIdx;
};

static const SynthVoiceEntry SYNTH_VOICE_MAP[] = {
    {SYNTH_MODE_WAVETABLE, 32},    {SYNTH_MODE_VABASS, 33},
    {SYNTH_MODE_STRINGS, 34},      {SYNTH_MODE_PIANO, 35},
    {SYNTH_MODE_MODAL, 36},        {SYNTH_MODE_FM, 43},
    {SYNTH_MODE_NOISE, 37},        {SYNTH_MODE_SAMPLER_ONESHOT, 38},
    {SYNTH_MODE_SAMPLER_LOOP, 39}, {SYNTH_MODE_SAMPLER_PLAYER, 40},
    {SYNTH_MODE_DRUMS, 41},        {SYNTH_MODE_GRANULAR, 42},
    {SYNTH_MODE_DRUM_SYNTH, 45},
};
static const int SYNTH_VOICE_MAP_SIZE =
    (int)(sizeof(SYNTH_VOICE_MAP) / sizeof(SYNTH_VOICE_MAP[0]));
static const uint8_t EXTERNAL_VOICE_IDX = 44;

static inline uint8_t SynthModeToVoice(uint8_t mode) {
  for (int i = 0; i < SYNTH_VOICE_MAP_SIZE; i++) {
    if (SYNTH_VOICE_MAP[i].mode == mode)
      return SYNTH_VOICE_MAP[i].voiceIdx;
  }
  return 32;
}

// Map knob value (0-4095) to synth mode + voice index.
// Values 0-599: external input range (caller should handle).
// Values 600-4095: split across all synth modes.
static inline uint8_t KnobToSynthMode(int32_t val, uint8_t *voiceOut) {
  const int32_t base = 700;
  const int32_t step = 250;
  uint8_t mode;
  if (val < base + step * 0)       mode = SYNTH_MODE_WAVETABLE;
  else if (val < base + step * 1)  mode = SYNTH_MODE_VABASS;
  else if (val < base + step * 2)  mode = SYNTH_MODE_STRINGS;
  else if (val < base + step * 3)  mode = SYNTH_MODE_PIANO;
  else if (val < base + step * 4)  mode = SYNTH_MODE_MODAL;
  else if (val < base + step * 5)  mode = SYNTH_MODE_FM;
  else if (val < base + step * 6)  mode = SYNTH_MODE_NOISE;
  else if (val < base + step * 7)  mode = SYNTH_MODE_SAMPLER_ONESHOT;
  else if (val < base + step * 8)  mode = SYNTH_MODE_SAMPLER_LOOP;
  else if (val < base + step * 9)  mode = SYNTH_MODE_SAMPLER_PLAYER;
  else if (val < base + step * 10) mode = SYNTH_MODE_DRUMS;
  else if (val < base + step * 11) mode = SYNTH_MODE_GRANULAR;
  else                             mode = SYNTH_MODE_DRUM_SYNTH;
  if (voiceOut)
    *voiceOut = SynthModeToVoice(mode);
  return mode;
}
