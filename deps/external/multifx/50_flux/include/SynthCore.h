#ifndef SYNTH_CORE_H
#define SYNTH_CORE_H

#include <stdbool.h>
#include <stdint.h>

#include "pipicofx/fxPrograms.h" // Shared sample rate defines

// Target rate for samples stored in flash (previously 22050)
#define SAMPLE_FILE_RATE 24000

// Unity playback speed for samples at the current sample rate
// = (SAMPLE_FILE_RATE / AUDIO_BASE_RATE) * 65536
#define SAMPLER_SPEED_Q16 ((SAMPLE_FILE_RATE * 65536) / AUDIO_BASE_RATE)

// Synth Modes
typedef enum {
  SYNTH_MODE_NOISE = 0,
  SYNTH_MODE_SAMPLER_ONESHOT, // Pure trigger, play to end
  SYNTH_MODE_SAMPLER_LOOP,    // Sustain loop while gated
  SYNTH_MODE_SAMPLER_PLAYER,  // Continuous drone loop
  SYNTH_MODE_DRUMS,           // Polyphonic Drum Kit (samples)
  SYNTH_MODE_WAVETABLE,       // Wavetable Morph
  SYNTH_MODE_VABASS,   // VA Lead/Bass (dual SAW + sub, 4-pole ladder filter)
  SYNTH_MODE_STRINGS,  // Karplus-Strong
  SYNTH_MODE_PIANO,    // Additive Piano
  SYNTH_MODE_MODAL,    // Modal Resonator (Bells/Percussion)
  SYNTH_MODE_GRANULAR, // Granular Cloud
  SYNTH_MODE_FM,       // DX7-style FM Synth
  SYNTH_MODE_DRUM_SYNTH // Synthesized Drum Machine (BD/SD/OH/CH, no samples)
} SynthMode;


// Shared Parameter Structure
typedef struct {
  float pitch;    // 0.0 to 1.0 (Pitch / Playback Speed)
  float timbre;   // 0.0 to 1.0 (Filter / Color / Sample Select)
  float envelope; // 0.0 to 1.0 (Env / Grain Size?)
  float volume;   // 0.0 to 1.0 (Output Volume / Grain Amp)
  float
      pitchBendSemis; // Calculated Pitch Bend in Semitones (from Knob/Settings)
  float filterCutoff; // Added for separate Filter Control (Switch Down)
  bool gate;          // Gate Status
  bool drone;         // Drone Mode Active
  bool sustain;       // Sustain Pedal Active (CC 64)
  uint8_t mode;       // SynthMode
} SynthParams;

// Voice Info for UI/CV
typedef struct {
  float pitch; // 0.0-1.0
  bool gate;
  bool active;
  uint8_t note;
  uint8_t velocity;
  uint32_t envVal; // Q30 Envelope Value
} VoiceInfo;

// Audio Block Size
#define SYNTH_BLOCK_SIZE 32
#define SYNTH_RING_BUF_SIZE                                                    \
  64 // Reduced further to fit clock logic (still 2× block size)

// Public Interface
void SynthCore_Init();
void SynthCore_Start();
bool SynthCore_GetSample(int16_t *outL,
                         int16_t *outR); // Returns true if sample available
void SynthCore_UpdateParams(SynthParams *params);
void SynthCore_SetSamplePtr(const uint8_t *ptr, uint32_t len,
                            int index); // Set sample for Sampler Mode
void SynthCore_SetDrumBase(
    int baseIndex);           // Set base sample index for Drum Mode
void SynthCore_AllNotesOff(); // Hard reset all voices and drones
bool SynthCore_GetVoiceInfo(int voiceIdx, VoiceInfo *info); // Get voice status
void SynthCore_TriggerNote(
    uint8_t note, uint8_t velocity);      // Manually trigger note from Core 0
void SynthCore_ReleaseNote(uint8_t note); // Release hardware-triggered note

// Custom Sample Support (Flash @ 0x10180000)
void SynthCore_ScanFlashSamples();
const uint8_t *SynthCore_GetCustomSamplePtr(int index);
uint32_t SynthCore_GetCustomSampleLen(int index);
uint32_t SynthCore_GetCustomSampleCount();

// Flash Storage
#define FLASH_SAMPLES_BASE 0x10180000
#define FLASH_MAGIC 0x53584660 // "MFXS" (SynthMode Persist)


// Settings are persisted in a dedicated flash sector immediately before the
// sampler region. This prevents SaveSettingsToFlash() from erasing the
// beginning of sample PCM data when persisting settings.
#define FLASH_SETTINGS_BASE (FLASH_SAMPLES_BASE - 0x1000) // one 4KB flash sector

// Settings Struct (Stored at FLASH_SETTINGS_BASE + 8)
// Pulse Modes:
//   0 = MIDI Gate (level), 1 = MIDI Trigger (short pulse)
//   2 = Clock Out (square wave, source = pulse1/2ClockSrc)
//   3 = Probabilistic Trigger, 4 = Pass-through (Pulse In → Out)
//   5 = Voice Audio (PWM), 6 = Seq A Gate, 7 = Seq B Gate
// EG Trigger Sources (egTrigSrc):
//   0 = MIDI Gate, 1 = Pulse 1 In, 2 = Pulse 2 In
// CV Modes:
//   0 = MIDI Pitch (1V/Oct), 1 = MIDI Velocity, 2 = MIDI CC, 3 = Synth Envelope
//   4 = Random S&H, 5 = Step Sequencer, 6 = Voice Audio (PWM), 7 = Internal EG
//   8 = LFO Utility (shape set via cv1/2Ch: 0=Sine,1=Tri,2=SawUp,3=SawDn,4=Sqr,5=SmRnd)
//   9 = Generative Sequencer (Turing Machine; spice set via cv1/2Arg 0-127)
// Clock Sources (for clockSrc fields):
//   0 = MIDI Clock, 1 = Internal BPM, 2 = Pulse 1 In, 3 = Pulse 2 In
typedef struct {
  uint8_t voicePreviewEnabled; // 0=Off, 1=On

  uint8_t pulse1Mode;
  uint8_t pulse1Ch;
  uint8_t pulse1Arg; // Arg: note filter / PPQN div / prob 0-127
  uint8_t pulse2Mode;
  uint8_t pulse2Ch;
  uint8_t pulse2Arg;

  uint8_t cv1Mode;
  uint8_t cv1Ch;
  uint8_t cv1Arg; // Arg: CC#, S&H range, etc.
  uint8_t cv2Mode;
  uint8_t cv2Ch;
  uint8_t cv2Arg;

  // Global Knobs (MIDI CC Output)
  uint8_t knobMainCC;
  uint8_t knobXCC;
  uint8_t knobYCC;

  uint8_t cv1Range; // 0=±6V, 1=±3V, 2=±2V, 3=±1V, 4=0-6V, 5=0-3V, 6=0-1V
  uint8_t cv2Range;

  // Audio In Processing
  uint8_t audioInScale;    // 0=Off, 1=Chromatic, 2=Major, 3=Minor, ...
  uint8_t audioInHold;     // 0=Off, 1=S&H Pitch on Gate Rise
  uint8_t pitchRangeSemis; // ±semis range (default 36)
  uint8_t rootNote;        // 0..127 (default 60 = C4)

  // Sequencer A (left column — CV1 / Pulse 1)
  uint8_t sequencerSteps[16]; // MIDI note value 0..127
  uint8_t sequencerLength;    // 1..16

  uint8_t selectedSynthMode; // Persist Synth Mode

  uint32_t magic;       // FLASH_MAGIC to verify valid settings
  uint8_t globalVolume; // 0..127

  // CC Input Routing
  uint8_t ccEffectSelect;
  uint8_t ccSynthSelect;
  uint8_t ccVolume;
  uint8_t ccChannel; // 0-15=channel, 16=omni
  uint8_t ccSynthEnv;
  uint8_t ccSynthPitch;
  uint8_t ccSynthTimbre;
  uint8_t ccSynthFilter;
  uint8_t ccSynthSample;
  uint8_t ccFxParam0;
  uint8_t ccFxParam1;
  uint8_t ccFxParam2;

  // --- New fields (Phase 2) ---
  // Internal BPM clock
  uint8_t bpmEnabled; // kept for compat; now use pulse1/2ClockSrc == 1
  uint8_t bpm;        // 20..255 BPM (stored directly)

  // Clock dividers per pulse/CV output
  // 0=/1 (pass), 1=/2, 2=/4, 3=/8, 4=x2, 5=x4
  uint8_t pulse1ClockDiv;
  uint8_t pulse2ClockDiv;

  // Sequencer B (right column — CV2 / Pulse 2)
  uint8_t sequencerSteps2[16]; // MIDI note value 0..127
  uint8_t sequencerLength2;    // 1..16

  // Independent Envelope Generator (for CV out mode 7)
  uint8_t eg1Env;     // CV1 envelope control 0..127 (mapped to ADSR)
  uint8_t eg2Env;     // CV2 envelope control 0..127 (mapped to ADSR)
  uint8_t egReserved1;
  uint8_t egReserved2;
  uint8_t egReserved3;

  // Unified clock sources (0=MIDI, 1=InternalBPM, 2=Pulse1In, 3=Pulse2In)
  uint8_t pulse1ClockSrc; // clock source for Pulse1 Clock Out + Probabilistic
  uint8_t pulse2ClockSrc; // clock source for Pulse2 Clock Out + Probabilistic
  uint8_t cv1ClockSrc;    // clock source for CV1 S&H and Sequencer A
  uint8_t cv2ClockSrc;    // clock source for CV2 S&H and Sequencer B

  // Clock dividers for CV/Seq columns (0=/1, 1=/2, 2=/4, 3=/8, 4=x2, 5=x4)
  uint8_t cv1ClockDiv; // divider for Seq A / CV1 S&H
  uint8_t cv2ClockDiv; // divider for Seq B / CV2 S&H

  // Internal EG trigger source (0=MIDI gate, 1=Pulse1In, 2=Pulse2In)
  uint8_t egTrigSrc;

  // Sequencer step gate bitmasks (bit N = step N active)
  uint8_t seqAGateMask;   // bits 0-6
  uint8_t seqAGateMaskHi; // bits 7-13
  uint8_t seqAGateMaskExtra; // bits 14-15
  uint8_t seqBGateMask;
  uint8_t seqBGateMaskHi;
  uint8_t seqBGateMaskExtra;

  // High bits for >255 BPM (appended to maintain flash memory layout)
  uint8_t bpmHi;

  // Pulse input clock resolution (1=Quarter, 2=8th, 4=16th, 24=Sync24)
  uint8_t pulse1PPQN;
  uint8_t pulse2PPQN;

  // Pulse output gate width (0=5ms trigger, 1..100 = 1% to 100% duty cycle)
  uint8_t pulse1Width;
  uint8_t pulse2Width;

  // Audio 2 In configurable routing mode (0=SynthY, 1=AudioMix, 2=Pitch2)
  uint8_t audio2Mode;

  // Selected Effect Index and Routing Source
  uint8_t currentEffectIndex;
  uint8_t currentSource;

  // Saved effect parameters
  uint16_t fxParam0;
  uint16_t fxParam1;
  uint16_t fxParam2;

  // Saved synth parameters (scaled 0-16383 to fit 14-bit resolution if needed, or 16-bit)
  uint16_t synthParamPitch;
  uint16_t synthParamTimbre;
  uint16_t synthParamEnv;
  uint16_t synthParamFilter;

  // Dedicated Card Builder (locks device to a single effect and synth)
  uint8_t isCardLocked;

  // CC numbers for the 3 EXTRA-page (double-tap) performance knobs
  uint8_t ccPerfMain;  // default 88 — BPM or Pulse 1 probability
  uint8_t ccPerfX;     // default 89 — CV1 perf param (spice / seq length / LFO speed)
  uint8_t ccPerfY;     // default 90 — CV2 perf param

  // Sequencer scale/quantization settings (UI display only, no audio effect)
  // Scale: 0=chromatic, 1=major, 2=minor, 3=pentamaj, 4=pentamin, 5=voltage
  uint8_t seqAScale;    // 0-5
  uint8_t seqARoot;     // 0-11 (chromatic note)
  uint8_t seqAOctave;   // 0-8  (MIDI octave)
  uint8_t seqAOctRange; // 1-8  (octave span)
  uint8_t seqBScale;
  uint8_t seqBRoot;
  uint8_t seqBOctave;
  uint8_t seqBOctRange;

} DeviceSettings;


// Global Settings Instance (Defined in main.cpp)
extern DeviceSettings settings;
void SaveSettingsToFlash();


void Midi_Rx();
extern volatile uint32_t midiClockTick;
extern volatile bool midiClockRunning;
extern volatile uint8_t lastCv1CC;
extern volatile uint8_t lastCv2CC;

// Global MIDI State
extern volatile uint8_t globalMidiNote;
extern volatile uint8_t globalMidiVel;
extern volatile bool globalMidiGate;

// CC Input State (written by Midi_Rx on Core 0, read by HandleMidiCC on Core 0)
extern volatile uint8_t ccEffectVal;      // effect select
extern volatile uint8_t ccSynthVal;       // synth/source select
extern volatile uint8_t ccVolumeVal;      // volume
extern volatile uint8_t ccSynthEnvVal;    // synth envelope
extern volatile uint8_t ccSynthPitchVal;  // synth pitch
extern volatile uint8_t ccSynthTimbreVal; // synth timbre
extern volatile uint8_t ccSynthFilterVal; // synth filter cutoff
extern volatile uint8_t ccSynthSampleVal; // synth sample select
extern volatile uint8_t ccFxParam0Val;    // effect param 0
extern volatile uint8_t ccFxParam1Val;    // effect param 1
extern volatile uint8_t ccFxParam2Val;    // effect param 2
extern volatile bool ccEffectUpdated;
extern volatile bool ccSynthUpdated;
extern volatile bool ccVolumeUpdated;
extern volatile bool ccSynthEnvUpdated;
extern volatile bool ccSynthPitchUpdated;
extern volatile bool ccSynthTimbreUpdated;
extern volatile bool ccSynthFilterUpdated;
extern volatile bool ccSynthSampleUpdated;
extern volatile bool ccFxParam0Updated;
extern volatile bool ccFxParam1Updated;
extern volatile bool ccFxParam2Updated;
extern volatile uint8_t ccPerfMainVal;  // EXTRA perf: BPM or P1 probability
extern volatile uint8_t ccPerfXVal;     // EXTRA perf: CV1
extern volatile uint8_t ccPerfYVal;     // EXTRA perf: CV2
extern volatile bool ccPerfMainUpdated;
extern volatile bool ccPerfXUpdated;
extern volatile bool ccPerfYUpdated;

// External Mode: Tell Core 1 to skip synth rendering (saves CPU for effects)
void SynthCore_SetExternalMode(bool enabled);

#endif
