#include "SynthCore.h"
#include "SynthEngine.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/watchdog.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico/util/queue.h"
#include "tusb.h"
#include <math.h>

// Forward declaration of MultiFx_Computer so SysEx preset handlers can call
// into it without creating a circular include. The static instance pointer
// is defined in main.cpp.
class MultiFx_Computer {
public:
  static MultiFx_Computer *instance;
  int32_t sampleVolumeQ16; // exposed for SaveSettingsToFlash
  void GetLiveState(uint8_t *outBlob);
};

// void Midi_Rx(); // Forward declaration

// --- Inter-core Communication ---
static SynthParams currentParams = {.pitch = 0.5f,
                                    .timbre = 0.0f,
                                    .envelope = 0.5f,
                                    .volume = 1.0f,
                                    .filterCutoff = 1.0f,
                                    .gate = false,
                                    .drone = false,
                                    .sustain = false,
                                    .mode = 0};
static SynthParams nextParams; // Buffer copy
static volatile bool paramsUpdated = false;
static volatile uint32_t paramSeqCounter =
    0; // Seqlock for torn-read protection
static volatile bool externalModeActive =
    true;                           // Match boot default: SOURCE_EXTERNAL
static bool wasExternalMode = true; // Edge detection for cleanup

// Global MIDI State
float midiMod = 0.0f;

// Audio Ring Buffer
static int16_t audioRingBufferL[SYNTH_RING_BUF_SIZE];
static int16_t audioRingBufferR[SYNTH_RING_BUF_SIZE];
static volatile uint16_t rbHead = 0;
static volatile uint16_t rbTail = 0;
static int16_t lastGoodSampleL = 0; // Underrun protection
static int16_t lastGoodSampleR = 0;
static bool sustainPedal = false;

// --- Shared State (types defined in SynthEngine.h) ---
Grain globalGrains[MAX_GRAINS];
uint32_t globalGrainSpawnTimer = 0;

Voice voices[MAX_VOICES];
EnginePool pool;
static uint32_t globalNoteOnCounter = 0;

// VABASS state (extern in SynthEngine.h)
uint32_t vabassSubPhase[MAX_VOICES] = {0};
int32_t vabassSvfBand[MAX_VOICES] = {0};
int32_t vabassFilterEnv[MAX_VOICES] = {0};
uint32_t vabassGlidePitch[MAX_VOICES] = {0};
uint32_t vabassPwmLfo = 0;

// MODAL state (extern in SynthEngine.h)
int32_t modalBandL1[MAX_VOICES] = {0};
int32_t modalBandB1[MAX_VOICES] = {0};
int32_t modalBandL2[MAX_VOICES] = {0};
int32_t modalBandB2[MAX_VOICES] = {0};
int32_t modalImpulseEnv[MAX_VOICES] = {0};


// Piano code moved to src/synths/synth_piano.cpp

// --- Voice Allocator ---
static int drumBaseOffset = 0;
static void NoteOn(uint8_t note, uint8_t velocity, bool isMidiSource) {
  // 0. Global Volume Scaling (Knob X - Shift Mode)
  // Map knob [0..1] to dynamics range [0.1..1.0]
  float scaledVol = 0.10f + (currentParams.volume * 0.90f);
  velocity = (uint8_t)(velocity * scaledVol);
  if (velocity < 1)
    velocity = 1;

  // 0b. Velocity Remapping for Piano (Avoid low-quality quiet samples)
  if (currentParams.mode == SYNTH_MODE_PIANO) {
    // Range 60..127 seems high? Let's allow slightly lower dynamic.
    // Or keep user request for loud piano?
    // Let's stick to the previous mapping but AFTER the scaling.
    // velocity = 60 + ((velocity * 67) >> 7);
    // Wait, if we scale down, we might hit the "bad sample zone".
    // Let's protect Piano slightly.
    if (velocity < 40)
      velocity = 40;
  }

  // 1. Drum Mode Special Handling (Legacy Mapping Removed)
  // We now treat Drums as Polyphonic Dynamic Voices with Map: Note ->
  // SampleIndex

  // 2. Playback Pitch Calculation (Unclamped for Karplus)
  // DSP Range: 0.5 = Unity/Middle C (60).
  // Scale: 0.25 per octave.
  // 4 Octaves Range (0..1) -> 48 Semitones.
  // Fixed point Q16: 0.5 = 32768.
  int32_t pitchQ16 = 32768 + (((int32_t)note - 60) << 16) / 48;
  // float pitch_legacy_unused = 0.5f + (float)(note - 60) / 48.0f;
  // Note: Old code clamped 0..1. We allow float overflow for extended range if
  // needed, but legacy logic clamps later.

  // 3. Check for existing voice (Retrigger)
  // Covers all modes — even Drums, OneShot, and Piano benefit from fast
  // same-note retrigger rather than allocating a second voice and dropping.
  for (int i = 0; i < MAX_VOICES; i++) {
    if (voices[i].active && voices[i].note == note) {
      voices[i].gate = true;
      voices[i].lastGate = false;
      voices[i].noteOnCounter = ++globalNoteOnCounter;
      if (currentParams.mode == SYNTH_MODE_PIANO) {
        enginePiano.initVoice(i, note, velocity);
        voices[i].envVal = 1073741824; // Snap to 1.0 for piano mechanics
      }
      if (currentParams.mode == SYNTH_MODE_MODAL) {
        voices[i].envVal = 1073741824; // Snap to 1.0 for resonator excitation
      }
      if (currentParams.mode == SYNTH_MODE_DRUMS ||
          currentParams.mode == SYNTH_MODE_SAMPLER_ONESHOT ||
          currentParams.mode == SYNTH_MODE_SAMPLER_LOOP) {
        // Restart sampler envelope from scratch for a clean retrigger
        voices[i].samplerEnvVal = 0;
        voices[i].samplerEnvStage = 0;
        // Reset playback position so the sample starts over
        voices[i].sampleIdxQ16 = 0;
        voices[i].sampleDecodeIdx = 0;
        voices[i].sampleLastVal = 0;
        voices[i].sampleCurrVal = 0;
      }
      return;
    }
  }

  // 4. Find Free Voice
  for (int i = 0; i < MAX_VOICES; i++) {
    // Monophonic modes: SAMPLER_PLAYER and VABASS always steal voice 0
    if ((currentParams.mode == SYNTH_MODE_SAMPLER_PLAYER ||
         currentParams.mode == SYNTH_MODE_VABASS) &&
        i == 0) {
      // Monophonic Stealing/Retrigger
      // Legato Logic for VABASS:
      if (currentParams.mode == SYNTH_MODE_VABASS && voices[0].active &&
          voices[0].gate) {
        // Legato: Preserve filter env, slide pitch
        voices[0].lastGate = true;
      } else {
        // Staccato / New Note
        voices[0].lastGate = false;
        voices[0].envVal = 0;
        vabassFilterEnv[0] = 0; // Reset filter envelope
      }

      voices[0].note = note;
      voices[0].velocity = velocity;
      voices[0].active = true;
      voices[0].gate = true;
      voices[0].noteOnCounter = ++globalNoteOnCounter;

      voices[0].samplerEnvVal = 0;
      voices[0].samplerEnvStage = 0;

      voices[0].currentPitch = pitchQ16;
      return;
    }

    // Accept voices that are inactive — releasing voices (envVal > 0 but gate
    // already off) are immediately reusable; requiring envVal == 0 causes drops
    // when all voices are in long release tails under fast note bursts.
    if (!voices[i].active) {
      voices[i].note = note;
      voices[i].velocity = velocity;
      voices[i].active = true;
      voices[i].gate = true;
      voices[i].lastGate = false;
      voices[i].noteOnCounter = ++globalNoteOnCounter;
      // voices[i].lpfState = 0; // Reset filter state to prevent fade-in

      // Envelope Setup
      if (currentParams.mode == SYNTH_MODE_PIANO ||
          currentParams.mode == SYNTH_MODE_MODAL ||
          currentParams.mode == SYNTH_MODE_FM) {
        voices[i].envVal = 1073741824; // Snap to 1.0
      } else {
        voices[i].envVal = 0; // Default attack from 0
      }

      // Pitch Setup
      if (currentParams.mode == SYNTH_MODE_DRUMS) {
        // Map Note to Sample using the dynamic Kit Select offset
        int sIdx = (note - 36) + drumBaseOffset;
        if (sIdx < 0)
          sIdx = 0; // Clamp
        if (sIdx > 0) {
          uint32_t sampleCount = SynthCore_GetCustomSampleCount();
          if (sampleCount > 0 && sIdx >= (int)sampleCount) {
            sIdx = (int)sampleCount - 1;
            if (sIdx < 0)
              sIdx = 0;
          }
        }
        voices[i].sampleIndex = sIdx;
        voices[i].currentPitch = 32768; // 0.5 in Q16
      } else {
        voices[i].currentPitch = pitchQ16;
      }
      voices[i].isMidi = isMidiSource; // Flag source

      // Sampler Pointer Setup
      // For Drums, or Poly Sampler, we must refresh pointer based on Index
      if (voices[i].sampleIndex >= 0) {
        voices[i].sampleLen =
            SynthCore_GetCustomSampleLen(voices[i].sampleIndex);

        voices[i].samplePtr =
            SynthCore_GetCustomSamplePtr(voices[i].sampleIndex);

        // Parse Loop Header (only for custom flash samples)
        const uint8_t *h = voices[i].samplePtr;
        if (h && (uintptr_t)h >= FLASH_SAMPLES_BASE && voices[i].sampleLen > 8) {
          voices[i].sampleLen -= 8;
          voices[i].loopStartIdx =
              h[0] | (h[1] << 8) | (h[2] << 16) | (h[3] << 24);
          voices[i].loopEndIdx =
              h[4] | (h[5] << 8) | (h[6] << 16) | (h[7] << 24);

          if (voices[i].loopEndIdx > voices[i].sampleLen)
            voices[i].loopEndIdx = voices[i].sampleLen; // Safety
          if (voices[i].loopStartIdx >= voices[i].loopEndIdx)
            voices[i].loopStartIdx = 0;

          // SKIP HEADER
          voices[i].samplePtr += 8;
        } else {
          voices[i].loopStartIdx = 0;
          voices[i].loopEndIdx = voices[i].sampleLen;
        }
      }
      if (currentParams.mode == SYNTH_MODE_PIANO)
        enginePiano.initVoice(i, note, velocity);
      if (currentParams.mode == SYNTH_MODE_FM)
        engineFM.initVoice(i, note, velocity);
      return;
    }
  }

  // 5. Steal Voice (Round Robin)
  // 5. Steal Voice (Oldest Note Priority)
  int voiceLimit = MAX_VOICES;
  if (currentParams.mode == SYNTH_MODE_STRINGS)
    voiceLimit = 4;

  int bestStealIdx = -1;
  uint32_t oldestReleasedAge = 0xFFFFFFFF;
  uint32_t oldestActiveAge = 0xFFFFFFFF;
  int oldestReleasedIdx = -1;
  int oldestActiveIdx = -1;

  for (int i = 0; i < voiceLimit; i++) {
    if (!voices[i].active) {
      bestStealIdx = i;
      break;
    }
    if (!voices[i].gate) {
      // Find oldest released note
      // A smaller noteOnCounter means the note is older!
      if (voices[i].noteOnCounter < oldestReleasedAge) {
        oldestReleasedAge = voices[i].noteOnCounter;
        oldestReleasedIdx = i;
      }
    } else {
      // Find oldest active note
      if (voices[i].noteOnCounter < oldestActiveAge) {
        oldestActiveAge = voices[i].noteOnCounter;
        oldestActiveIdx = i;
      }
    }
  }

  // Fallback: Pick oldest released, then oldest active
  if (bestStealIdx == -1) {
    if (oldestReleasedIdx != -1) {
      bestStealIdx = oldestReleasedIdx;
    } else if (oldestActiveIdx != -1) {
      bestStealIdx = oldestActiveIdx;
    } else {
      bestStealIdx = 0; // Absolute fallback, shouldn't occur
    }
  }

  Voice *v = &voices[bestStealIdx];
  v->note = note;
  v->velocity = velocity;
  v->active = true;
  v->gate = true;
  v->lastGate = false;
  v->noteOnCounter = ++globalNoteOnCounter;

  // Reset sampler envelope on voice steal to prevent clicks
  v->samplerEnvVal = 0;
  v->samplerEnvStage = 0;

  // Instant Attack for Physics Engines
  if (currentParams.mode == SYNTH_MODE_PIANO ||
      currentParams.mode == SYNTH_MODE_MODAL ||
      currentParams.mode == SYNTH_MODE_STRINGS) {
    v->envVal = 1073741824;
  } else {
    v->envVal = 0;
  }

  // Reset specific states for smooth stealing?
  // VABASS should slide? No, new note.

  if (currentParams.mode == SYNTH_MODE_DRUMS) {
    int sIdx = note - 36;
    if (sIdx < 0)
      sIdx = 0;
    v->sampleIndex = sIdx;
    v->currentPitch = 32768;

    v->sampleLen = SynthCore_GetCustomSampleLen(sIdx);
    if (v->sampleLen > 8)
      v->sampleLen -= 8;
    v->samplePtr = SynthCore_GetCustomSamplePtr(sIdx);

    const uint8_t *h = v->samplePtr;
    if (h) {
      v->loopStartIdx = h[0] | (h[1] << 8) | (h[2] << 16) | (h[3] << 24);
      v->loopEndIdx = h[4] | (h[5] << 8) | (h[6] << 16) | (h[7] << 24);
      if (v->loopEndIdx > v->sampleLen)
        v->loopEndIdx = v->sampleLen;
    } else {
      v->loopStartIdx = 0;
      v->loopEndIdx = v->sampleLen;
    }
  } else {
    // Standard Synth Mode: Update Pitch
    v->currentPitch = pitchQ16;

    if (currentParams.mode == SYNTH_MODE_PIANO) {
      enginePiano.initVoice(bestStealIdx, note, velocity);
    } else if (currentParams.mode == SYNTH_MODE_FM) {
      engineFM.initVoice(bestStealIdx, note, velocity);
    }
  }
}

bool SynthCore_GetVoiceInfo(int voiceIdx, VoiceInfo *info) {
  if (voiceIdx < 0 || voiceIdx >= MAX_VOICES)
    return false;
  Voice *v = &voices[voiceIdx];
  info->pitch = v->currentPitch;
  info->gate = voices[voiceIdx].gate;
  info->active = voices[voiceIdx].active;
  info->note = voices[voiceIdx].note;
  info->velocity = voices[voiceIdx].velocity;
  info->envVal = voices[voiceIdx].envVal; // Expose Envelope
  return true;
}

volatile uint32_t midiClockTick = 0;
volatile bool midiClockRunning = false;
volatile uint8_t lastCv1CC = 0;
volatile uint8_t lastCv2CC = 0;

// Global MIDI State
volatile uint8_t globalMidiNote = 60;
volatile uint8_t globalMidiVel = 0;
volatile bool globalMidiGate = false;

// CC Input State (written by Midi_Rx on Core 0, read by HandleMidiCC on Core 0)
// No cross-core synchronization needed — both run on Core 0.
volatile uint8_t ccEffectVal = 0;
volatile uint8_t ccSynthVal = 0;
volatile uint8_t ccVolumeVal = 127;
volatile uint8_t ccSynthEnvVal = 64;
volatile uint8_t ccSynthPitchVal = 64;
volatile uint8_t ccSynthTimbreVal = 64;
volatile uint8_t ccSynthFilterVal = 127;
volatile uint8_t ccSynthSampleVal = 0;
volatile uint8_t ccFxParam0Val = 64;
volatile uint8_t ccFxParam1Val = 64;
volatile uint8_t ccFxParam2Val = 64;
volatile bool ccEffectUpdated = false;
volatile bool ccSynthUpdated = false;
volatile bool ccVolumeUpdated = false;
volatile bool ccSynthEnvUpdated = false;
volatile bool ccSynthPitchUpdated = false;
volatile bool ccSynthTimbreUpdated = false;
volatile bool ccSynthFilterUpdated = false;
volatile bool ccSynthSampleUpdated = false;
volatile bool ccFxParam0Updated = false;
volatile bool ccFxParam1Updated = false;
volatile bool ccFxParam2Updated = false;
volatile uint8_t ccPerfMainVal = 64;
volatile uint8_t ccPerfXVal = 64;
volatile uint8_t ccPerfYVal = 64;
volatile bool ccPerfMainUpdated = false;
volatile bool ccPerfXUpdated = false;
volatile bool ccPerfYUpdated = false;

// Note Stack for Last-Note Priority
#define NOTE_STACK_SIZE 16
static uint8_t noteStack[NOTE_STACK_SIZE];
static uint8_t noteStackVel[NOTE_STACK_SIZE];
static int noteStackCount = 0;

static void AddNoteToStack(uint8_t note, uint8_t vel) {
  // 1. Remove if exists (Duplicate check)
  for (int i = 0; i < noteStackCount; i++) {
    if (noteStack[i] == note) {
      // Shift down to cover local hole
      for (int j = i; j < noteStackCount - 1; j++) {
        noteStack[j] = noteStack[j + 1];
        noteStackVel[j] = noteStackVel[j + 1];
      }
      noteStackCount--;
      break;
    }
  }

  // 2. If full, remove oldest (Shift everything down)
  if (noteStackCount >= NOTE_STACK_SIZE) {
    for (int i = 0; i < noteStackCount - 1; i++) {
      noteStack[i] = noteStack[i + 1];
      noteStackVel[i] = noteStackVel[i + 1];
    }
    noteStackCount--;
  }

  // 3. Add to top
  noteStack[noteStackCount] = note;
  noteStackVel[noteStackCount] = vel;
  noteStackCount++;
}

static void RemoveNoteFromStack(uint8_t note) {
  for (int i = 0; i < noteStackCount; i++) {
    if (noteStack[i] == note) {
      // Shift down
      for (int j = i; j < noteStackCount - 1; j++) {
        noteStack[j] = noteStack[j + 1];
        noteStackVel[j] = noteStackVel[j + 1];
      }
      noteStackCount--;
      return;
    }
  }
}

static void NoteOff(uint8_t note) {
  // Search all voices for this note and release gate
  for (int i = 0; i < MAX_VOICES; i++) {
    if (voices[i].active && voices[i].note == note) {
      voices[i].gate = false;
      // For Drums, release should be fast or handled by Envelope Decay
      // For Strings/Synth, this triggers the ADSR Release phase
    }
  }
}

// SysEx Buffer
static uint8_t sysexBuf[256];
static int sysexPos = 0;

void ProcessSysEx(uint8_t *data, int len) {
  if (len < 3 || data[0] != 0xF0 || data[1] != 0x7D)
    return;

  uint8_t cmd = data[2];

  // CMD 1: Set Settings
  if (cmd == 0x01 && len >= 17) {
    settings.voicePreviewEnabled = data[3];

    settings.pulse1Mode = data[4];
    settings.pulse1Ch = data[5];
    settings.pulse1Arg = data[6];
    settings.pulse2Mode = data[7];
    settings.pulse2Ch = data[8];
    settings.pulse2Arg = data[9];

    settings.cv1Mode = data[10];
    settings.cv1Ch = data[11];
    settings.cv1Arg = data[12];
    settings.cv2Mode = data[13];
    settings.cv2Ch = data[14];
    settings.cv2Arg = data[15];

    if (len >= 23) {
      settings.knobMainCC = data[16];
      settings.knobXCC = data[17];
      settings.knobYCC = data[18];
      settings.cv1Range = data[19];
      settings.cv2Range = data[20];
      settings.audioInScale = data[21];
      settings.audioInHold = data[22];
    }
    if (len >= 26) {
      settings.pitchRangeSemis = data[23];
      settings.rootNote = data[24];
    }
    if (len >= 42) {
      for (int i = 0; i < 16; i++) {
        settings.sequencerSteps[i] = data[25 + i];
      }
      settings.sequencerLength = data[41];
    }
    if (len >= 44) {
      settings.selectedSynthMode = data[42];
      // Magic check moved to 43-46
      uint32_t receivedMagic =
          data[43] | (data[44] << 8) | (data[45] << 16) | (data[46] << 24);
      (void)receivedMagic;
    }
    if (len >= 48) {
      settings.globalVolume = data[47];
    }
    if (len >= 60) {
      settings.ccEffectSelect = data[48] & 0x7F;
      settings.ccSynthSelect = data[49] & 0x7F;
      settings.ccVolume = data[50] & 0x7F;
      settings.ccChannel = data[51] & 0x7F;
      settings.ccSynthEnv = data[52] & 0x7F;
      settings.ccSynthPitch = data[53] & 0x7F;
      settings.ccSynthTimbre = data[54] & 0x7F;
      settings.ccSynthFilter = data[55] & 0x7F;
      settings.ccSynthSample = data[56] & 0x7F;
      settings.ccFxParam0 = data[57] & 0x7F;
      settings.ccFxParam1 = data[58] & 0x7F;
      settings.ccFxParam2 = data[59] & 0x7F;
    }
    // New Phase-2 fields (optional, backward-compatible)
    if (len >= 62) {
      settings.bpmEnabled = data[60] & 0x7F;
      settings.bpm = data[61] & 0x7F; // 20..127 stored directly (clamp on read)
    }
    if (len >= 64) {
      settings.pulse1ClockDiv = data[62] & 0x7F;
      settings.pulse2ClockDiv = data[63] & 0x7F;
    }
    if (len >= 81) { // 16 steps for Seq B (start at 64)
      for (int i = 0; i < 16; i++)
        settings.sequencerSteps2[i] = data[64 + i] & 0x7F;
      settings.sequencerLength2 = data[80] & 0x7F;
    }
    if (len >= 82) {
      // 14-bit MIDI BPM (must match CC perf path + main.cpp internal clock:
      // total = bpm | (bpmHi << 7))
      uint16_t totalBpm =
          (data[61] & 0x7F) | ((uint16_t)(data[81] & 0x7F) << 7);
      if (totalBpm < 20)
        totalBpm = 20;
      if (totalBpm > 256)
        totalBpm = 256;
      settings.bpm = (uint8_t)(totalBpm & 0x7F);
      settings.bpmHi = (uint8_t)((totalBpm >> 7) & 0x7F);
    }
    if (len >= 87) { // EG: eg1Env, eg2Env, reserved (start at 82)
      settings.eg1Env = data[82] & 0x7F;
      settings.eg2Env = data[83] & 0x7F;
      settings.egReserved1 = data[84] & 0x7F;
      settings.egReserved2 = data[85] & 0x7F;
      settings.egReserved3 = data[86] & 0x01;
    }
    if (len >= 92) { // Unified clock sources (start at 87)
      settings.pulse1ClockSrc = data[87] & 0x03;
      settings.pulse2ClockSrc = data[88] & 0x03;
      settings.cv1ClockSrc = data[89] & 0x03;
      settings.cv2ClockSrc = data[90] & 0x03;
      settings.egTrigSrc = data[91] & 0x03;
    }
    if (len >= 97) { // gate bitmasks (start at 92)
      settings.seqAGateMask = data[92] & 0x7F;
      settings.seqAGateMaskHi = data[93] & 0x7F;
      settings.seqBGateMask = data[94] & 0x7F;
      settings.seqBGateMaskHi = data[95] & 0x7F;
      settings.cv1ClockDiv = data[96] & 0x0F;
    }
    if (len >= 98) {
      settings.cv2ClockDiv = data[97] & 0x0F;
    }
    if (len >= 103) { // Pulse PPQN & Pulse Width (start at 98)
      settings.pulse1PPQN = data[98] & 0x7F;
      settings.pulse2PPQN = data[99] & 0x7F;
      settings.pulse1Width = data[100] & 0x7F;
      settings.pulse2Width = data[101] & 0x7F;
      settings.audio2Mode = data[102] & 0x7F;
    }
    if (len >= 106) {
      settings.seqAGateMaskExtra = data[103] & 0x03;
      settings.seqBGateMaskExtra = data[104] & 0x03;
      settings.isCardLocked = data[105] & 0x01;
    }
    if (len >= 122) {
      settings.currentEffectIndex = data[106];
      settings.currentSource = data[107];

      // FX Params (14-bit)
      settings.fxParam0 = data[108] | (data[109] << 7);
      settings.fxParam1 = data[110] | (data[111] << 7);
      settings.fxParam2 = data[112] | (data[113] << 7);

      // Synth Params (14-bit)
      settings.synthParamPitch = data[114] | (data[115] << 7);
      settings.synthParamTimbre = data[116] | (data[117] << 7);
      settings.synthParamEnv = data[118] | (data[119] << 7);
      settings.synthParamFilter = data[120] | (data[121] << 7);
    }
    // Performance CC numbers [123-125]
    if (len >= 126) {
      settings.ccPerfMain = data[123] & 0x7F;
      settings.ccPerfX = data[124] & 0x7F;
      settings.ccPerfY = data[125] & 0x7F;
    }
    // Sequencer scale settings [126-133]
    if (len >= 134) {
      settings.seqAScale = data[126] & 0x07;
      settings.seqARoot = data[127] & 0x0F;
      settings.seqAOctave = data[128] & 0x0F;
      settings.seqAOctRange = data[129] & 0x0F;
      settings.seqBScale = data[130] & 0x07;
      settings.seqBRoot = data[131] & 0x0F;
      settings.seqBOctave = data[132] & 0x0F;
      settings.seqBOctRange = data[133] & 0x0F;
    }
  }
  // CMD 2: Save
  else if (cmd == 0x02) {
    SaveSettingsToFlash();
  }
  // CMD 3: Read
  else if (cmd == 0x03) {
    // Extended response (grows from 60 to 105 bytes).
    // Byte map:
    //  [0..2]   = F0 7D 03 header
    //  [3]      = voicePreviewEnabled
    //  [4..15]  = pulse/cv mode/ch/arg (x4 x3)
    //  [16..18] = knobCC x3
    //  [19..20] = cv unipolar
    //  [21..24] = audioIn settings + pitchRange + rootNote
    //  [25..40] = sequencerSteps[16]
    //  [41..44] = FLASH_MAGIC (7-bit)
    //  [45]     = sequencerLength
    //  [46]     = selectedSynthMode
    //  [47..58] = CC routing (12 bytes)
    //  [59..60] = bpmEnabled, bpm
    //  [61..62] = pulse1ClockDiv, pulse2ClockDiv
    //  [63..78] = sequencerSteps2[16]
    //  [79]     = sequencerLength2
    //  [80]     = (padding/reserved)
    //  [81..85] = eg1Attack/Decay/Sustain/Release/Target
    //  [86]     = (padding/reserved)
    //  [87]     = pulse1Prob (stored in pulse1Arg), pulse2Prob in pulse2Arg
    //  [104]    = F7
    uint8_t response[140];
    memset(response, 0, sizeof(response));
    response[0] = 0xF0;
    response[1] = 0x7D;
    response[2] = 0x03;
    response[3] = settings.voicePreviewEnabled;

    response[4] = settings.pulse1Mode;
    response[5] = settings.pulse1Ch;
    response[6] = settings.pulse1Arg;
    response[7] = settings.pulse2Mode;
    response[8] = settings.pulse2Ch;
    response[9] = settings.pulse2Arg;

    response[10] = settings.cv1Mode;
    response[11] = settings.cv1Ch;
    response[12] = settings.cv1Arg;
    response[13] = settings.cv2Mode;
    response[14] = settings.cv2Ch;
    response[15] = settings.cv2Arg;

    response[16] = settings.knobMainCC;
    response[17] = settings.knobXCC;
    response[18] = settings.knobYCC;

    response[19] = settings.cv1Range;
    response[20] = settings.cv2Range;

    response[21] = settings.audioInScale;
    response[22] = settings.audioInHold;
    response[23] = settings.pitchRangeSemis;
    response[24] = settings.rootNote;

    for (int i = 0; i < 16; i++)
      response[25 + i] = settings.sequencerSteps[i] & 0x7F;

    response[41] = settings.sequencerLength & 0x7F;
    response[42] = settings.selectedSynthMode & 0x7F;

    // Version Magic (7-bit encoded) moved to 43-46
    response[43] = (FLASH_MAGIC) & 0x7F;
    response[44] = (FLASH_MAGIC >> 8) & 0x7F;
    response[45] = (FLASH_MAGIC >> 16) & 0x7F;
    response[46] = (FLASH_MAGIC >> 24) & 0x7F;

    response[47] = settings.globalVolume & 0x7F;

    // CC Input Routing (indices 48-59 = 12 bytes)
    response[48] = settings.ccEffectSelect & 0x7F;
    response[49] = settings.ccSynthSelect & 0x7F;
    response[50] = settings.ccVolume & 0x7F;
    response[51] = settings.ccChannel & 0x7F;
    response[52] = settings.ccSynthEnv & 0x7F;
    response[53] = settings.ccSynthPitch & 0x7F;
    response[54] = settings.ccSynthTimbre & 0x7F;
    response[55] = settings.ccSynthFilter & 0x7F;
    response[56] = settings.ccSynthSample & 0x7F;
    response[57] = settings.ccFxParam0 & 0x7F;
    response[58] = settings.ccFxParam1 & 0x7F;
    response[59] = settings.ccFxParam2 & 0x7F;

    // Phase-2 extensions
    response[60] = settings.bpmEnabled & 0x7F;
    // 14-bit MIDI BPM (same encoding as SysEx set + GetLiveState + CC perf)
    uint16_t totalBpm =
        (settings.bpm & 0x7F) | ((uint16_t)(settings.bpmHi & 0x7F) << 7);
    if (settings.bpmHi == 0 && settings.bpm > 127)
      totalBpm = settings.bpm; // legacy flash: full 8-bit in bpm field
    if (totalBpm < 20)
      totalBpm = 20;
    if (totalBpm > 256)
      totalBpm = 256;
    response[61] = totalBpm & 0x7F;
    response[62] = settings.pulse1ClockDiv & 0x7F;
    response[63] = settings.pulse2ClockDiv & 0x7F;
    for (int i = 0; i < 16; i++)
      response[64 + i] = settings.sequencerSteps2[i] & 0x7F;
    response[80] = settings.sequencerLength2 & 0x7F;
    response[81] = (totalBpm >> 7) & 0x7F; // BPM MSB (7 bits)
    response[82] = settings.eg1Env & 0x7F;
    response[83] = settings.eg2Env & 0x7F;
    response[84] = settings.egReserved1 & 0x7F;
    response[85] = settings.egReserved2 & 0x7F;
    response[86] = settings.egReserved3 & 0x01;
    // [87..90] Unified clock sources
    response[87] = settings.pulse1ClockSrc & 0x03;
    response[88] = settings.pulse2ClockSrc & 0x03;
    response[89] = settings.cv1ClockSrc & 0x03;
    response[90] = settings.cv2ClockSrc & 0x03;
    // [91..95] EG trigger src + sequencer gate bitmasks
    response[91] = settings.egTrigSrc & 0x03;
    response[92] = settings.seqAGateMask & 0x7F;
    response[93] = settings.seqAGateMaskHi & 0x7F;
    response[94] = settings.seqBGateMask & 0x7F;
    response[95] = settings.seqBGateMaskHi & 0x7F;
    // [96..97] CV column clock dividers
    response[96] = settings.cv1ClockDiv & 0x7F;
    response[97] = settings.cv2ClockDiv & 0x7F;
    // [98..101] Pulse PPQN and Pulse Width
    response[98] = settings.pulse1PPQN & 0x7F;
    response[99] = settings.pulse2PPQN & 0x7F;
    response[100] = settings.pulse1Width & 0x7F;
    response[101] = settings.pulse2Width & 0x7F;
    response[102] = settings.audio2Mode & 0x7F;
    response[103] = settings.seqAGateMaskExtra & 0x03;
    response[104] = settings.seqBGateMaskExtra & 0x03;
    response[105] = settings.isCardLocked & 0x01;
    // [106..121] Runtime parameters
    response[106] = settings.currentEffectIndex & 0x7F;
    response[107] = settings.currentSource & 0x7F;
    response[108] = settings.fxParam0 & 0x7F;
    response[109] = (settings.fxParam0 >> 7) & 0x7F;
    response[110] = settings.fxParam1 & 0x7F;
    response[111] = (settings.fxParam1 >> 7) & 0x7F;
    response[112] = settings.fxParam2 & 0x7F;
    response[113] = (settings.fxParam2 >> 7) & 0x7F;
    response[114] = settings.synthParamPitch & 0x7F;
    response[115] = (settings.synthParamPitch >> 7) & 0x7F;
    response[116] = settings.synthParamTimbre & 0x7F;
    response[117] = (settings.synthParamTimbre >> 7) & 0x7F;
    response[118] = settings.synthParamEnv & 0x7F;
    response[119] = (settings.synthParamEnv >> 7) & 0x7F;
    response[120] = settings.synthParamFilter & 0x7F;
    response[121] = (settings.synthParamFilter >> 7) & 0x7F;

    // Performance CC numbers [123-125]
    response[123] = settings.ccPerfMain & 0x7F;
    response[124] = settings.ccPerfX & 0x7F;
    response[125] = settings.ccPerfY & 0x7F;
    // Sequencer scale settings [126-133]
    response[126] = settings.seqAScale & 0x07;
    response[127] = settings.seqARoot & 0x0F;
    response[128] = settings.seqAOctave & 0x0F;
    response[129] = settings.seqAOctRange & 0x0F;
    response[130] = settings.seqBScale & 0x07;
    response[131] = settings.seqBRoot & 0x0F;
    response[132] = settings.seqBOctave & 0x0F;
    response[133] = settings.seqBOctRange & 0x0F;
    response[134] = 0xF7;
    tud_midi_stream_write(0, response, 135);
  }
  // CMD 4: Reboot
  else if (cmd == 0x04) {
    watchdog_reboot(0, 0, 0);
  }

  // CMD 0x16: Get Live State  [F0 7D 16 F7]  (len=4)
  // Returns: [F0 7D 16 blob(30 bytes) F7] = 34 bytes total (was 29)
  // Extended bytes [20-24] carry perf-controllable values for UI sync.
  else if (cmd == 0x16 && len >= 4) {
    if (MultiFx_Computer::instance) {
      uint8_t blob[30];
      MultiFx_Computer::instance->GetLiveState(blob);
      uint8_t pkt[34];
      pkt[0] = 0xF0;
      pkt[1] = 0x7D;
      pkt[2] = 0x16;
      memcpy(&pkt[3], blob, 30);
      pkt[33] = 0xF7;
      tud_midi_stream_write(0, pkt, 34);
    }
  }
}

void Midi_Rx() {
  uint8_t packet[4];
  while (tud_midi_available()) {
    if (tud_midi_packet_read(packet)) {
      uint8_t code = packet[0] & 0x0F;
      uint8_t status = packet[1];
      uint8_t data1 = packet[2];
      uint8_t data2 = packet[3];

      // Note On/Off/CC (Code 0x9, 0x8, 0xB)
      // Note On/Off/CC (Code 0x9, 0x8, 0xB)
      if (code == 0x9 && data2 > 0) {
        // Channel filter: honour settings.ccChannel (16 = omni)
        uint8_t noteChannel = packet[1] & 0x0F;
        bool noteChMatch = (settings.ccChannel == 16 || settings.ccChannel == noteChannel);
        if (!noteChMatch)
          continue;

        // Global Tracking (Last Note Priority)
        AddNoteToStack(data1, data2);
        if (noteStackCount > 0) {
          globalMidiNote = noteStack[noteStackCount - 1];
          globalMidiVel = noteStackVel[noteStackCount - 1];
          globalMidiGate = true;
        } else {
          globalMidiVel = data2;
        }

        if (!externalModeActive)
          NoteOn(data1, data2, true);
      } else if (code == 0x8 || (code == 0x9 && data2 == 0)) {
        // Channel filter
        uint8_t noteChannel = packet[1] & 0x0F;
        bool noteChMatch = (settings.ccChannel == 16 || settings.ccChannel == noteChannel);
        if (!noteChMatch)
          continue;

        // Global Tracking
        RemoveNoteFromStack(data1);
        if (noteStackCount > 0) {
          globalMidiNote =
              noteStack[noteStackCount - 1]; // Return to previous note
          globalMidiVel =
              noteStackVel[noteStackCount - 1]; // Return to previous velocity
          globalMidiGate = true;
        } else {
          globalMidiGate = false; // All keys released
                                  // maintain last globalMidiNote/Vel
        }

        NoteOff(data1);
      } else if (code == 0xB) {
        uint8_t channel = packet[1] & 0x0F;

        // Mod Wheel (Internal)
        if (data1 == 1)
          midiMod = (float)data2 / 127.0f;
        // Sustain (Internal)
        else if (data1 == 64)
          sustainPedal = (data2 >= 64);

        // CV Output Tracking
        // Check CV1
        if (settings.cv1Mode == 2 && data1 == settings.cv1Arg) {
          // Check Channel (0-15) or Omni (16)
          if (settings.cv1Ch == 16 || settings.cv1Ch == channel) {
            lastCv1CC = data2;
          }
        }
        // Check CV2
        if (settings.cv2Mode == 2 && data1 == settings.cv2Arg) {
          if (settings.cv2Ch == 16 || settings.cv2Ch == channel) {
            lastCv2CC = data2;
          }
        }

        // CC Input Routing (parameter control)
        bool chMatch =
            (settings.ccChannel == 16 || settings.ccChannel == channel);
        if (chMatch) {
          if (data1 == settings.ccEffectSelect) {
            ccEffectVal = data2;
            ccEffectUpdated = true;
          }
          if (data1 == settings.ccSynthSelect) {
            ccSynthVal = data2;
            ccSynthUpdated = true;
          }
          if (data1 == settings.ccVolume) {
            ccVolumeVal = data2;
            ccVolumeUpdated = true;
          }
          if (data1 == settings.ccSynthEnv) {
            ccSynthEnvVal = data2;
            ccSynthEnvUpdated = true;
          }
          if (data1 == settings.ccSynthPitch) {
            ccSynthPitchVal = data2;
            ccSynthPitchUpdated = true;
          }
          if (data1 == settings.ccSynthTimbre) {
            ccSynthTimbreVal = data2;
            ccSynthTimbreUpdated = true;
          }
          if (data1 == settings.ccSynthFilter) {
            ccSynthFilterVal = data2;
            ccSynthFilterUpdated = true;
          }
          if (data1 == settings.ccSynthSample) {
            ccSynthSampleVal = data2;
            ccSynthSampleUpdated = true;
          }
          if (data1 == settings.ccFxParam0) {
            ccFxParam0Val = data2;
            ccFxParam0Updated = true;
          }
          if (data1 == settings.ccFxParam1) {
            ccFxParam1Val = data2;
            ccFxParam1Updated = true;
          }
          if (data1 == settings.ccFxParam2) {
            ccFxParam2Val = data2;
            ccFxParam2Updated = true;
          }
          // Performance knob CCs (EXTRA page)
          if (data1 == settings.ccPerfMain) {
            ccPerfMainVal = data2;
            ccPerfMainUpdated = true;
          }
          if (data1 == settings.ccPerfX) {
            ccPerfXVal = data2;
            ccPerfXUpdated = true;
          }
          if (data1 == settings.ccPerfY) {
            ccPerfYVal = data2;
            ccPerfYUpdated = true;
          }
          // CC 7 always controls volume (standard MIDI)
          if (data1 == 7 && settings.ccVolume != 7) {
            ccVolumeVal = data2;
            ccVolumeUpdated = true;
          }
        }
      }

      // RealTime Messages (Single Byte Packet, Code 0xF or 0x5)
      // Clock (F8), Start (FA), Cont (FB), Stop (FC)
      if (code == 0xF || code == 0x5) {
        if (packet[1] == 0xF8)
          midiClockTick++;
        else if (packet[1] == 0xFA) {
          midiClockRunning = true;
          midiClockTick = 0;
        } else if (packet[1] == 0xFB)
          midiClockRunning = true;
        else if (packet[1] == 0xFC)
          midiClockRunning = false;
      }

      // SysEx Handling
      // 0x4: Sysex Start/Cont (3 bytes)
      if (code == 0x4) {
        if (status == 0xF0)
          sysexPos = 0; // Start
        if (sysexPos < 250) {
          sysexBuf[sysexPos++] = status;
          sysexBuf[sysexPos++] = data1;
          sysexBuf[sysexPos++] = data2;
        }
      }
      // 0x5: Sysex End (1 byte)
      else if (code == 0x5) {
        if (status == 0xF0)
          sysexPos = 0;
        if (sysexPos < 250)
          sysexBuf[sysexPos++] = status;
        ProcessSysEx(sysexBuf, sysexPos);
        sysexPos = 0;
      }
      // 0x6: Sysex End (2 bytes)
      else if (code == 0x6) {
        if (status == 0xF0)
          sysexPos = 0;
        if (sysexPos < 250) {
          sysexBuf[sysexPos++] = status;
          sysexBuf[sysexPos++] = data1;
        }
        ProcessSysEx(sysexBuf, sysexPos);
        sysexPos = 0;
      }
      // 0x7: Sysex End (3 bytes)
      else if (code == 0x7) {
        if (status == 0xF0)
          sysexPos = 0;
        if (sysexPos < 250) {
          sysexBuf[sysexPos++] = status;
          sysexBuf[sysexPos++] = data1;
          sysexBuf[sysexPos++] = data2;
        }
        ProcessSysEx(sysexBuf, sysexPos);
        sysexPos = 0;
      }
    }
  }
}

// [Definitions Moved to Top]

// Mulaw_Decode and fast_exp2 now provided by SynthEngine.h

// Control Rate MTOF
#include "MathTables.h"

// ...

// --- Global Grain Rendering ---
static void RenderGlobalGrains(int32_t *mixBuf, int count) {
  // We use voices[0] as the sample source for the cloud.
  // Assuming all granular voices share the same sample buffer.
  const uint8_t *sPtr = voices[0].samplePtr;
  uint32_t sLen = voices[0].sampleLen;

  if (!sPtr || sLen == 0)
    return;

  for (int k = 0; k < count; k++) {
    int32_t sum = 0;
    int activeCount = 0;

    for (int g = 0; g < MAX_GRAINS; g++) {
      if (globalGrains[g].active) {
        Grain *gr = &globalGrains[g];
        activeCount++;

        uint32_t ph = gr->phase >> 16;
        uint32_t ln = gr->length >> 16;

        // Bounds Check
        if (ph >= ln) {
          gr->active = false;
          continue;
        }

        // Sample
        uint32_t cur = gr->startPos + ph;
        if (cur >= sLen)
          cur %= sLen;

        int16_t s = Mulaw_Decode(sPtr[cur]);

        // Window (Triangle)
        int32_t w = 0;

        // Optimized: Use Pre-calculated Scale
        if (ln > 0) {
          uint32_t half = ln >> 1;
          if (ph < half)
            w = ph * gr->windowScale;
          else
            w = (ln - ph) * gr->windowScale;
        }

        // Apply Window and Per-Grain Amplitude (Volume Stamp)
        int32_t val = (s * w) >> 16;
        sum += (val * gr->amp) >> 16;

        gr->phase += gr->inc;
      }
    }

    // Soft Clip / Accumulate
    // Normalize:
    // >> 4 (Div 16) was too quiet.
    // >> 2 (Div 4) was still a bit quiet (-12dB).
    // >> 1 (Div 2) gives +6dB more (-6dB total).
    // With soft limiter, this should match sampler levels better.
    mixBuf[k] += (sum >> 1);
  }
}

// --- DSP HELPER: Render 1 Block (32 samples) for 1 Voice ---
static void RenderVoiceBlock(Voice *v, const SynthParams *p, int count,
                             int32_t *bufL, int32_t *bufR) {
  if (!v->active && v->envVal == 0 && v->samplerEnvVal == 0 && !v->gate) {
    return;
  }

  // Per-Block Gate Rise Detection
  // bool gateRise = v->gate && !v->lastGate;

  // 1. Calculate Envelope Rates (all integer — no float)
  int32_t attackRate, releaseRate;
  // Convert envelope to Q15 once for all calculations below
  int32_t envQ15 = (int32_t)(p->envelope * 32768.0f);
  if (envQ15 < 0)
    envQ15 = 0;
  if (envQ15 > 32768)
    envQ15 = 32768;

  // Envelope rates are in units-per-sample. Divide by AUDIO_SAMPLE_RATE_DIV
  // so that all modes have the same real-time duration regardless of whether
  // the system runs at 48kHz (div=1) or 24kHz (div=2).
  // All base constants below are calibrated for 48kHz.

  // Envelope Logic (Simplified from original)
  if (p->mode == SYNTH_MODE_SAMPLER_ONESHOT ||
      p->mode == SYNTH_MODE_SAMPLER_LOOP || p->mode == SYNTH_MODE_DRUMS) {
    attackRate = 200000000 / AUDIO_SAMPLE_RATE_DIV;

    // Enabled Envelope Control for One Shot too
    if (p->mode == SYNTH_MODE_DRUMS || p->mode == SYNTH_MODE_SAMPLER_ONESHOT) {
      // Main Knob (Envelope) controls Decay/Release
      // t_sq = t * t * 100000  (integer: (envQ15 * envQ15 >> 15) * 100000 >>
      // 15)
      int32_t t_sq = (int32_t)(((int64_t)envQ15 * envQ15 * 100000) >> 30);
      releaseRate = (100000 - t_sq) / AUDIO_SAMPLE_RATE_DIV;
      if (releaseRate < 50)
        releaseRate = 50; // Never fully 0 to avoid stuck
      if (envQ15 > 32112) // 0.98 * 32768
        releaseRate = 0;  // Full Sustain at max
    } else {
      // Loop Mode -> envelope controls speed (handled elsewhere), fixed release
      releaseRate = 100000 / AUDIO_SAMPLE_RATE_DIV;
    }
  } else {
    // Thresholds: 0.3 = 9830, 0.7 = 22938 in Q15
    if (envQ15 < 9830) { // PLUCK
      attackRate = 200000000 / AUDIO_SAMPLE_RATE_DIV;
      // t = envParam / 0.3  →  tQ15 = envQ15 * 32768 / 9830
      int32_t tQ15 = (int32_t)(((int64_t)envQ15 << 15) / 9830);
      if (tQ15 > 32768)
        tQ15 = 32768;
      // releaseRate = 223700 - t * (223700 - 22370) = 223700 - t * 201330
      releaseRate = (223700 - (int32_t)(((int64_t)tQ15 * 201330) >> 15)) /
                    AUDIO_SAMPLE_RATE_DIV;
    } else if (envQ15 < 22938) { // SUSTAIN
      // t = (envParam - 0.3) / 0.4  →  tQ15 = (envQ15 - 9830) * 32768 / 13107
      int32_t tQ15 = (int32_t)(((int64_t)(envQ15 - 9830) << 15) / 13107);
      if (tQ15 > 32768)
        tQ15 = 32768;
      if (tQ15 < 0)
        tQ15 = 0;
      attackRate = (2200000 - (int32_t)(((int64_t)tQ15 * 1750000) >> 15)) /
                   AUDIO_SAMPLE_RATE_DIV;
      releaseRate = (80000 - (int32_t)(((int64_t)tQ15 * 40000) >> 15)) /
                    AUDIO_SAMPLE_RATE_DIV;
    } else { // SWELL
      // t = (envParam - 0.7) / 0.3  →  tQ15 = (envQ15 - 22938) * 32768 / 9830
      int32_t tQ15 = (int32_t)(((int64_t)(envQ15 - 22938) << 15) / 9830);
      if (tQ15 > 32768)
        tQ15 = 32768;
      if (tQ15 < 0)
        tQ15 = 0;
      attackRate = (44700 - (int32_t)(((int64_t)tQ15 * 37000) >> 15)) /
                   AUDIO_SAMPLE_RATE_DIV;
      releaseRate = (40000 - (int32_t)(((int64_t)tQ15 * 22000) >> 15)) /
                    AUDIO_SAMPLE_RATE_DIV;
    }
  }
  // Fix: Granular & Sampler Player must sustain regardless of p->envelope
  if (p->mode == SYNTH_MODE_GRANULAR || p->mode == SYNTH_MODE_SAMPLER_PLAYER) {
    // Fast Attack, Moderate Release to keep VCA open
    attackRate = 200000000 / AUDIO_SAMPLE_RATE_DIV;
    releaseRate = 50000 / AUDIO_SAMPLE_RATE_DIV;
  }
  // Fix: VABASS uses Standard Envelope but needs Moderate Attack to prevent
  // clicks 5ms Attack (5000000) vs Instant (200000000)
  if (p->mode == SYNTH_MODE_VABASS) {
    attackRate = 5000000 / AUDIO_SAMPLE_RATE_DIV;
  }

  // Fix: Modal needs instant attack for physics exciter to work properly
  // Physics-based engines need instant attack for exciters (strike/pluck).
  if (p->mode == SYNTH_MODE_MODAL || p->mode == SYNTH_MODE_PIANO ||
      p->mode == SYNTH_MODE_STRINGS) {
    attackRate = 200000000 / AUDIO_SAMPLE_RATE_DIV;
    // For Modal, ensure a robust release even if knob is at extremes
    if (p->mode == SYNTH_MODE_MODAL && releaseRate < 500)
      releaseRate = 500;

    // Slower release for Piano Mode (x4 Longer tail)
    if (p->mode == SYNTH_MODE_PIANO)
      releaseRate = releaseRate >> 2;
  }
  v->attackRate = attackRate;
  v->releaseRate = releaseRate;

  // Gate
  bool activeGate = v->gate;
  if (p->drone && p->mode != SYNTH_MODE_DRUMS)
    activeGate = true;
  // activeGate determined by v->gate (Pulse/MIDI).
  // Removed forced activeGate for OneShot to allow re-triggering via
  // gateRise.

  // Sampler Speed & Offset config moved to individual engine renderBlock calls

  const bool isSampler =
      (p->mode == SYNTH_MODE_SAMPLER_ONESHOT ||
       p->mode == SYNTH_MODE_SAMPLER_LOOP ||
       p->mode == SYNTH_MODE_SAMPLER_PLAYER || p->mode == SYNTH_MODE_DRUMS ||
       p->mode == SYNTH_MODE_DRUM_SYNTH);

  // --- Sampler Block Setup (Integrated) ---
  if (isSampler) {
    int voiceIdx = v - voices;
    bool gateRise = activeGate && !v->lastGate;

    switch (p->mode) {
    case SYNTH_MODE_SAMPLER_ONESHOT:
      engineSamplerOneShot.renderBlockSetup(v, p, voiceIdx, count, &pool,
                                            gateRise);
      break;
    case SYNTH_MODE_SAMPLER_LOOP:
      engineSamplerLoop.renderBlockSetup(v, p, voiceIdx, count, &pool,
                                         gateRise);
      break;
    case SYNTH_MODE_SAMPLER_PLAYER:
      engineSamplerPlayer.renderBlockSetup(v, p, voiceIdx, count, &pool,
                                           gateRise);
      break;
    case SYNTH_MODE_DRUMS:
      engineSamplerDrums.renderBlockSetup(v, p, voiceIdx, count, &pool,
                                          gateRise);
      break;
    case SYNTH_MODE_DRUM_SYNTH:
      engineDrumSynth.renderBlockSetup(v, p, voiceIdx, count, &pool,
                                          gateRise);
      break;
    }
  }

  // --- Piano Block Render (envelope + additive + stereo output) ---
  if (p->mode == SYNTH_MODE_PIANO) {
    int voiceIdx = v - voices;
    bool gateRise = activeGate && !v->lastGate;
    enginePiano.renderBlock(v, p, voiceIdx, count, &pool, gateRise, activeGate,
                            bufL, bufR);
    // Piano handles everything: envelope, rendering, stereo output, velocity,
    // completion. Update lastGate and return — skip the per-sample render
    // loop entirely.
    v->lastGate = v->gate;
    return;
  }

  // --- FM Block Render ---
  if (p->mode == SYNTH_MODE_FM) {
    int voiceIdx = v - voices;
    bool gateRise = activeGate && !v->lastGate;
    engineFM.renderBlock(v, p, voiceIdx, count, &pool, gateRise, activeGate,
                         bufL, bufR);
    v->lastGate = v->gate;
    return;
  }

  // --- Engine Block Setup (Granular spawning) ---
  if (!isSampler && p->mode == SYNTH_MODE_GRANULAR) {
    int voiceIdx = v - voices;
    bool gateRise = activeGate && !v->lastGate;
    engineGranular.renderBlockSetup(v, p, voiceIdx, count, &pool, gateRise);
  }

  // --- RENDER LOOP ---
  for (int k = 0; k < count; k++) {
    int32_t val = 0;

    if (isSampler) {
      // Dispatch to sampler engine (handles gate, decode, envelope,
      // completion)
      bool gateRise = activeGate && !v->lastGate;
      int voiceIdx = v - voices;
      switch (p->mode) {
      case SYNTH_MODE_SAMPLER_ONESHOT:
        val = engineSamplerOneShot.renderSample(v, p, voiceIdx, &pool, gateRise,
                                                activeGate);
        break;
      case SYNTH_MODE_SAMPLER_LOOP:
        val = engineSamplerLoop.renderSample(v, p, voiceIdx, &pool, gateRise,
                                             activeGate);
        break;
      case SYNTH_MODE_SAMPLER_PLAYER:
        val = engineSamplerPlayer.renderSample(v, p, voiceIdx, &pool, gateRise,
                                               activeGate);
        break;
      case SYNTH_MODE_DRUMS:
        val = engineSamplerDrums.renderSample(v, p, voiceIdx, &pool, gateRise,
                                              activeGate);
        break;
      case SYNTH_MODE_DRUM_SYNTH:
        val = engineDrumSynth.renderSample(v, p, voiceIdx, &pool, gateRise,
                                              activeGate);
        break;
      }
      v->lastGate = activeGate;

    } else {
      // Synth Engines
      int32_t rawOsc = 0;
      bool gateRise = activeGate && !v->lastGate;

      // Handle Gate / Sustain Logic
      if (!activeGate && !sustainPedal) {
        // Only decay if pedal is also up
        if (v->envVal > 0) {
          v->envVal -= v->releaseRate;
          if (v->envVal < 0) {
            v->envVal = 0;
            v->active = false;
          }
        }
      } else {
        // Attack or Sustain
        if (v->envVal < 1073741824) { // Q30 1.0 is 2^30
          v->envVal += v->attackRate;
          if (v->envVal > 1073741824)
            v->envVal = 1073741824;
        }
      }

      // --- LFO & Pitch Calculation (Integrated) ---

      {
        // Dispatch to extracted engine
        const SynthEngine *eng = nullptr;
        switch (p->mode) {
        case SYNTH_MODE_NOISE:
          eng = &engineNoise;
          break;
        case SYNTH_MODE_WAVETABLE:
          eng = &engineWavetable;
          break;
        case SYNTH_MODE_VABASS:
          eng = &engineVABass;
          break;
        case SYNTH_MODE_STRINGS:
          eng = &engineStrings;
          break;
        case SYNTH_MODE_MODAL:
          eng = &engineModal;
          break;
        case SYNTH_MODE_GRANULAR:
          eng = &engineGranular;
          break;
        default:
          break;
        }
        if (eng && eng->renderSample) {
          int voiceIdx = v - voices;
          rawOsc =
              eng->renderSample(v, p, voiceIdx, &pool, gateRise, activeGate);
        }
      }

      // Output Stage (Mono mixing fallback)
      if (rawOsc != 0) {
        if (p->mode == SYNTH_MODE_STRINGS || p->mode == SYNTH_MODE_MODAL) {
          // PURE PHYSICS / ADDITIVE MODE:
          // Ignore VCA Envelope for audio volume.
          // Let the String Physics (Feedback) dictate the decay.
          // 'envVal' is just a timer to eventually free the voice.

          // Apply a fast fade-in (Attack) to prevent clicks?
          // envVal grows activeGate... but we just want full dynamics.
          // Let's just output rawOsc. The Pluck/Bow logic handles energy.

          // Apply Envelope as "Safety Gating" and Volume Control
          // Matches physics but ensures silence at end
          int32_t gain = v->envVal >> 15;
          // User requested to half the gain for Modal (>>16 instead of >>15)
          val = (rawOsc * gain) >> 16;

          // Simple Limiter
          if (val > 32000)
            val = 32000;
          if (val < -32000)
            val = -32000;

          // OPTIONAL: Soft mute only at the VERY end of the timeout to avoid
          // click on voice kill
          if (v->envVal < 1000000) {
            // Last ~0.5s of the 10s timeout
            val = (val * (v->envVal >> 4)) >> 16;
          }
        } else {
          // LPG Filter (One Pole Lowpass) for WAVETABLE / NOISE / SAW /
          // GRANULAR
          int32_t baseCutoff = 100;

          if (p->mode == SYNTH_MODE_WAVETABLE || p->mode == SYNTH_MODE_NOISE) {
            // Use Explicit Filter Cutoff (Switch Down Control)
            // Map 0.0..1.0 to 100..32000 (integer: cutoffQ15 * 30000 >> 15 +
            // 100)
            int32_t cutoffQ15 = (int32_t)(p->filterCutoff * 32768.0f);
            if (cutoffQ15 < 0)
              cutoffQ15 = 0;
            if (cutoffQ15 > 32768)
              cutoffQ15 = 32768;
            baseCutoff = 100 + ((cutoffQ15 * 30000) >> 15);
            if (baseCutoff > 32000)
              baseCutoff = 32000;

            // Filter Coefficient Calculation
            // t = cutoff / SampleRate * 2^16 (AUDIO_BASE_RATE = 48000 or 24000)
            int32_t t = (baseCutoff * 65536) / AUDIO_BASE_RATE;
            if (t > 65535)
              t = 65535;
            if (t < 100)
              t = 100;

            // Apply LPF
            v->lpfState += ((rawOsc - v->lpfState) * t) >> 16;
            int32_t filtered = v->lpfState;

            // Apply VCA Gain (envVal is Q30)
            int32_t gain = v->envVal >> 15; // 0..32768
            val = (filtered * gain) >> 15;
          } else if (p->mode == SYNTH_MODE_VABASS) {
            // VA Bass has its own built-in resonant filter
            // Just apply VCA envelope
            int32_t gain = v->envVal >> 15;
            // User requested to half the gain for VA Bass (>>16 instead of
            // >>15)
            val = (rawOsc * gain) >> 16;
          } else if (p->mode == SYNTH_MODE_GRANULAR) {
            // Granular uses its own internal VCA (windowing), so we just
            // apply master env
            int32_t gain = v->envVal >> 15;
            val = (rawOsc * gain) >> 15;
            val = val << 3; // Increased to 8x for presence
          }
        }
      }

      // Check Synth Completion
      // Allow Strings to die when envelope hits 0 (Silence).
      if (!activeGate && v->envVal == 0 && v->samplerEnvVal == 0) {
        v->active = false;
      }
    }
    // Apply MIDI Velocity (0..127)
    val = (val * v->velocity) >> 7;

    // Stereo Panning (Strings, Modal, DrumSynth)
    int voiceIdx = v - voices;
    if (p->mode == SYNTH_MODE_STRINGS || p->mode == SYNTH_MODE_MODAL) {
      // Spread voices: 0=20%L, 1=40%L, 2=40%R, 3=20%R
      static const int8_t panTable[4] = {-40, -20, 20, 40}; // -64 to +64 scale
      int32_t pan = panTable[voiceIdx & 3];                 // -40 to +40
      int32_t valL = (val * (64 - pan)) >> 6;
      int32_t valR = (val * (64 + pan)) >> 6;
      bufL[k] += valL;
      bufR[k] += valR;
    } else if (p->mode == SYNTH_MODE_DRUM_SYNTH) {
      // Drum synth: all voices center (no panning)
      bufL[k] += val;
      bufR[k] += val;
    } else {
      // Mono (center) for other synths
      bufL[k] += val;
      bufR[k] += val;
    }
  }

  // Update State (skip for sampler modes — they update lastGate per-sample)
  if (p->mode != SYNTH_MODE_SAMPLER_ONESHOT &&
      p->mode != SYNTH_MODE_SAMPLER_LOOP &&
      p->mode != SYNTH_MODE_SAMPLER_PLAYER && p->mode != SYNTH_MODE_DRUMS) {
    v->lastGate = v->gate;
  }
}

// --- Core 1 Main Loop ---
void __not_in_flash_func(core1_entry)() {
  // tusb_init() moved to main() on Core 0.
  // Core 1 handles audio synthesis only.

  while (1) {
    // Seqlock read: ensure we get a consistent snapshot of params
    if (paramsUpdated) {
      uint32_t seq1, seq2;
      SynthParams tmp;
      do {
        seq1 = paramSeqCounter;
        __dmb(); // data memory barrier
        tmp = nextParams;
        __dmb();
        seq2 = paramSeqCounter;
      } while (seq1 != seq2 || (seq1 & 1));
      currentParams = tmp;
      paramsUpdated = false;
    }

    // --- EXTERNAL MODE: Skip all synth rendering to free CPU for Core 0 ---
    if (externalModeActive) {
      // On transition INTO external mode: silence all voices once
      if (!wasExternalMode) {
        for (int i = 0; i < MAX_VOICES; i++) {
          voices[i].gate = false;
          voices[i].active = false;
          voices[i].envVal = 0;
          voices[i].samplerEnvVal = 0;
        }
        // Drain the ring buffer so Core 0 doesn't read stale synth audio
        rbTail = rbHead;
        wasExternalMode = true;
      }
      // Sleep to avoid busy-looping (1ms = 48 samples worth)
      busy_wait_us_32(1000);
      continue;
    }
    wasExternalMode = false;

    SynthParams p = currentParams;

    // --- Pulse/CV Gate Logic (Legacy Restoration) ---
    static bool lastPulseGate = false;
    bool pulseGate = p.gate;

    // Skip global gate logic for modes that handle their own triggers
    bool skipLegacyPulse = (currentParams.mode == SYNTH_MODE_DRUMS ||
                            currentParams.mode == SYNTH_MODE_SAMPLER_ONESHOT ||
                            currentParams.mode == SYNTH_MODE_SAMPLER_LOOP ||
                            currentParams.mode == SYNTH_MODE_GRANULAR);

    if (pulseGate && !lastPulseGate && !skipLegacyPulse) {
      // Pulse Rising Edge -> Trigger Voice 0
      if (currentParams.mode == SYNTH_MODE_SAMPLER_LOOP) {
        voices[0].active = true;
        voices[0].sampleIdxQ16 = (uint64_t)voices[0].lastLoopStart << 16;
        voices[0].sampleDecodeIdx = voices[0].lastLoopStart;
        voices[0].sampleCurrVal = 0;
        voices[0].sampleLastVal = 0;
      } else {
        // Standard Synth / OneShot Trigger
        // FORCE Reset of Pitch/Role to Manual (Pulse)
        voices[0].isMidi = false;
        voices[0].note = 60;
        voices[0].currentPitch = 32768; // 0.5 in Q16

        if (!voices[0].active) {
          voices[0].active = true;
          voices[0].phase = 0;
        }
        voices[0].gate = true;
        voices[0].lastGate = false; // Force re-trigger in Render
        voices[0].envVal = 0;

        // Apply Volume Knob dynamics to Pulse Trigger too (10% floor)
        float scaledVol = 0.10f + (currentParams.volume * 0.90f);
        voices[0].velocity = (uint8_t)(127 * scaledVol);

        if (currentParams.mode == SYNTH_MODE_PIANO) {
          enginePiano.initVoice(0, 60, 127);
        }
      }
    }

    // Ensure Voice 0 is active if Gate is HIGH (Drone/Sustain)
    bool isOneShot = (currentParams.mode == SYNTH_MODE_SAMPLER_ONESHOT ||
                      currentParams.mode == SYNTH_MODE_DRUMS);
    if (pulseGate && !voices[0].active && !isOneShot) {
      voices[0].active = true;
      voices[0].note = 60;
      voices[0].currentPitch = 32768; // 0.5 in Q16
      voices[0].phase = 0;
      voices[0].gate = true;
      voices[0].lastGate = false;
      voices[0].envVal = 0;
      voices[0].samplerEnvVal = 0;
      voices[0].samplerEnvStage = 0;

      float scaledVol = 0.10f + (currentParams.volume * 0.90f);
      voices[0].velocity = (uint8_t)(127 * scaledVol);
    }

    if (currentParams.mode == SYNTH_MODE_SAMPLER_PLAYER) {
      if (!voices[0].active) {
        voices[0].active = true;
        voices[0].gate = true;
        voices[0].note = 60;
        voices[0].currentPitch = 32768;
      }
      voices[0].isMidi = false; // Force manual control for player
      voices[0].gate = true;
    }
    if (!pulseGate && lastPulseGate) {
      voices[0].gate = false;
    }
    lastPulseGate = pulseGate;

    // --- Fixed-Point Pitch Update Loop ---
    int32_t knobX_q16 = (int32_t)(p.pitch * 65536.0f);
    if (currentParams.mode == SYNTH_MODE_GRANULAR)
      knobX_q16 = 32768; // 0.5

    // Deadzone (0.45..0.55 -> 0.5)
    if (knobX_q16 > 29491 && knobX_q16 < 36044)
      knobX_q16 = 32768;

    int32_t pitchOffsetQ16 = (knobX_q16 - 32768) >> 1; // (knobX - 0.5) * 0.5

    for (int i = 0; i < MAX_VOICES; i++) {
      if (voices[i].active) {
        int32_t currentOffset = pitchOffsetQ16;
        // MIDI separation
        if (voices[i].isMidi && currentParams.mode != SYNTH_MODE_DRUMS)
          currentOffset = 0;

        int32_t finalPitchQ16 = voices[i].currentPitch + currentOffset;

        // Precise Frequency Calculation (Fixed Point)
        uint32_t targetPhaseInc;
        if (currentParams.mode == SYNTH_MODE_SAMPLER_ONESHOT ||
            currentParams.mode == SYNTH_MODE_SAMPLER_LOOP ||
            currentParams.mode == SYNTH_MODE_SAMPLER_PLAYER ||
            currentParams.mode == SYNTH_MODE_DRUMS) {
          // Sampler Speed: Unity at 0.5, +/- 2 Octave Range
          int32_t pitchShiftQ16 = (finalPitchQ16 - 32768)
                                  << 2; // (finalPitch - 0.5) * 4
          targetPhaseInc =
              (uint32_t)((SAMPLER_SPEED_Q16 *
                          (uint64_t)precise_exp2_q30(pitchShiftQ16)) >>
                         30);
        } else {
          targetPhaseInc = precise_mtof_q16(finalPitchQ16);
        }

        // Scale phaseInc for sample rate: at 24kHz (div=2) we need 2× the
        // phase increment for oscillators to produce the same pitch as at
        // 48kHz. Samplers already include the rate correction in
        // SAMPLER_SPEED_Q16.
        if (currentParams.mode != SYNTH_MODE_SAMPLER_ONESHOT &&
            currentParams.mode != SYNTH_MODE_SAMPLER_LOOP &&
            currentParams.mode != SYNTH_MODE_SAMPLER_PLAYER &&
            currentParams.mode != SYNTH_MODE_DRUMS) {
          targetPhaseInc *= AUDIO_SAMPLE_RATE_DIV;
        }

        // Slew Logic (VA Bass only)
        int32_t diff = (int32_t)targetPhaseInc - (int32_t)voices[i].phaseInc;
        bool useSlew = (currentParams.mode == SYNTH_MODE_VABASS);

        if (!useSlew || abs(diff) < 100) {
          voices[i].phaseInc = targetPhaseInc;
        } else {
          voices[i].phaseInc += (diff >> 6);
        }
      }
    }
    // --- Audio Rendering Loop (Legacy Restoration) ---
    int count = (rbHead - rbTail + SYNTH_RING_BUF_SIZE) % SYNTH_RING_BUF_SIZE;
    if (count < 32) {
      int32_t mixBufL[32] = {0};
      int32_t mixBufR[32] = {0};
      for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].active || voices[i].envVal > 0 ||
            voices[i].samplerEnvVal > 0) {
          RenderVoiceBlock(&voices[i], &p, 32, mixBufL, mixBufR);
        }
      }

      // Global Granular (Mono -> Stereo for now)
      if (p.mode == SYNTH_MODE_GRANULAR) {
        RenderGlobalGrains(mixBufL, 32);
        for (int k = 0; k < 32; k++)
          mixBufR[k] += mixBufL[k];
      }

      // Output
      int nextHead = (rbHead + 1) % SYNTH_RING_BUF_SIZE;
      for (int k = 0; k < 32; k++) {
        int32_t outL = mixBufL[k];
        int32_t outR = mixBufR[k];

        // Limiter logic (keep new one)
        if (outL > 24000) {
          outL = 24000 + (outL - 24000) / 2;
          if (outL > 30000)
            outL = 30000 + (outL - 30000) / 4;
        } else if (outL < -24000) {
          outL = -24000 + (outL + 24000) / 2;
          if (outL < -30000)
            outL = -30000 + (outL + 30000) / 4;
        }

        if (outR > 24000) {
          outR = 24000 + (outR - 24000) / 2;
          if (outR > 30000)
            outR = 30000 + (outR - 30000) / 4;
        } else if (outR < -24000) {
          outR = -24000 + (outR + 24000) / 2;
          if (outR < -30000)
            outR = -30000 + (outR + 30000) / 4;
        }

        if (outL > 32000)
          outL = 32000;
        if (outL < -32000)
          outL = -32000;
        if (outR > 32000)
          outR = 32000;
        if (outR < -32000)
          outR = -32000;

        audioRingBufferL[nextHead] = (int16_t)outL;
        audioRingBufferR[nextHead] = (int16_t)outR;
        rbHead = nextHead;
        nextHead = (nextHead + 1) % SYNTH_RING_BUF_SIZE;
      }
    } else {
      // USB Task handled on Core 0
      busy_wait_us_32(10);
    }
  }
}

#include "tusb.h"

// --- Public API ---

void SynthCore_Init() {
  // tusb_init() moved to core1_entry
  // Zero out all voices
  for (int i = 0; i < MAX_VOICES; i++) {
    voices[i].phase = 0;
    voices[i].phaseInc = 0;
    voices[i].envVal = 0;
    voices[i].lpfState = 0;
    voices[i].lfoPhase = 0;
    voices[i].noiseState = 0x12345678 + i; // Seeding
    voices[i].samplePtr = 0;
    voices[i].sampleLen = 0;
    voices[i].sampleIdxQ16 = 0;
    voices[i].active = false;
    voices[i].gate = false;
    voices[i].lastGate = false;
    voices[i].note = 0;
    voices[i].smoothedSpeed = 32768;
    // Global Grains Init
    if (i == 0) {
      for (int g = 0; g < MAX_GRAINS; g++)
        globalGrains[g].active = false;
      globalGrainSpawnTimer = 0;
    }

    // Map samples for Drum Mode
    if (i < (int)SynthCore_GetCustomSampleCount()) {
      voices[i].sampleIndex = i;
    } else {
      voices[i].sampleIndex = 0;
    }
  }

  currentParams.pitch = 0.5f;
  currentParams.timbre = 1.0f;
  currentParams.envelope = 0.1f;
  currentParams.pitchBendSemis = 0.0f;
  currentParams.gate = false;
  currentParams.drone = false;
  currentParams.mode = SYNTH_MODE_WAVETABLE;

  // Initialize Settings Defaults (in case EEPROM invalid)
  settings.pitchRangeSemis = 12; // Default +/- 1 Octave (Matches Piano)
                                 // settings.voicePreviewEnabled = 0;
                                 // ... other defaults if needed ...

  // These were likely for a single global 'synth' struct, now handled
  // per-voice or by currentParams synth.noiseState = 12345; // Handled per
  // voice synth.sampleIdxQ16 = 0;   // Handled per voice synth.samplePtr = 0;
  // // Handled per voice synth.sampleLen = 0;      // Handled per voice
}

void SynthCore_SetSamplePtr(const uint8_t *ptr, uint32_t len, int index) {
  if (!ptr)
    return; // Safety Check

  // Update all voices (Polyphonic Sampler behavior)
  for (int i = 0; i < MAX_VOICES; i++) {
    voices[i].sampleIndex = index;
    voices[i].samplePtr = ptr;
    voices[i].sampleLen = len;

    // Only custom flash samples have the 8-byte loop header
    if (ptr && (uintptr_t)ptr >= FLASH_SAMPLES_BASE && len > 8) {
      voices[i].loopStartIdx =
          ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
      voices[i].loopEndIdx =
          ptr[4] | (ptr[5] << 8) | (ptr[6] << 16) | (ptr[7] << 24);

      voices[i].samplePtr += 8;
      voices[i].sampleLen -= 8;
    } else {
      voices[i].loopStartIdx = 0;
      voices[i].loopEndIdx = len;
    }

    // Reset Granular (Global)
    if (i == 0) {
      for (int g = 0; g < MAX_GRAINS; g++)
        globalGrains[g].active = false;
      globalGrainSpawnTimer = 0;
    }

    voices[i].sampleCurrVal = 0;
    voices[i].sampleLastVal = 0;
    voices[i].sampleIdxQ16 = 0;
    voices[i].sampleDecodeIdx = 0;
    // grains array replaces grainPhase
  }
}

void SynthCore_AllNotesOff() {
  for (int i = 0; i < MAX_VOICES; i++) {
    voices[i].gate = false;
    voices[i].active = false;
    voices[i].envVal = 0;
    voices[i].samplerEnvVal = 0;
    voices[i].sampleIdxQ16 = 0;
    voices[i].lpfState = 0;
    voices[i].phase = 0;

    // Reset VABASS state
    vabassSubPhase[i] = 0;
    vabassSvfBand[i] = 0;
    vabassFilterEnv[i] = 0;
    vabassGlidePitch[i] = 0;
  }
  vabassPwmLfo = 0; // Reset PWM LFO (shared across voices)
  for (int i = 0; i < MAX_VOICES; i++) {

    // Reset MODAL state
    modalBandB1[i] = 0;
    modalBandL2[i] = 0;
    modalBandB2[i] = 0;
    modalImpulseEnv[i] = 0;
  }
}

void SynthCore_SetDrumBase(int baseIndex) {
  drumBaseOffset = baseIndex;
  for (int i = 0; i < MAX_VOICES; i++) {
    int targetSample = baseIndex + i; // Map key to sample

    // Safety check handled by GetCustomSamplePtr returning NULL if out of
    // bounds
    voices[i].samplePtr = SynthCore_GetCustomSamplePtr(targetSample);
    voices[i].sampleLen = SynthCore_GetCustomSampleLen(targetSample);

    // Extract Loop Points for Drums
    const uint8_t *ptr = voices[i].samplePtr;
    if (ptr && voices[i].sampleLen > 8) {
      voices[i].loopStartIdx =
          ptr[0] | (ptr[1] << 8) | (ptr[2] << 16) | (ptr[3] << 24);
      voices[i].loopEndIdx =
          ptr[4] | (ptr[5] << 8) | (ptr[6] << 16) | (ptr[7] << 24);
      voices[i].sampleLen -= 8;
      voices[i].samplePtr += 8;
    } else {
      voices[i].loopStartIdx = 0;
      voices[i].loopEndIdx = voices[i].sampleLen;
    }
    // Header already subtracted above (line 2275) — no double subtraction
    voices[i].sampleIndex = targetSample;

    voices[i].sampleIdxQ16 = 0;
    voices[i].sampleDecodeIdx = 0;
    // ADPCM Reset Removed
  }
}

void SynthCore_Start() { multicore_launch_core1(core1_entry); }

void SynthCore_UpdateParams(SynthParams *params) {
  // Seqlock write: odd counter = write in progress
  paramSeqCounter++;
  __dmb();
  nextParams = *params;
  __dmb();
  paramSeqCounter++;
  paramsUpdated = true;
}

bool SynthCore_GetSample(int16_t *outL, int16_t *outR) {
  if (rbTail == rbHead) {
    // Underrun: repeat last good sample instead of silence
    // This masks timing jitter from the sampleReady change
    *outL = lastGoodSampleL;
    *outR = lastGoodSampleR;
    return true;
  }

  *outL = audioRingBufferL[rbTail];
  *outR = audioRingBufferR[rbTail];
  lastGoodSampleL = *outL;
  lastGoodSampleR = *outR;

  rbTail = (rbTail + 1) % SYNTH_RING_BUF_SIZE;
  return true;
}

// ---// Custom Sample// Flash Storage for Custom Samples
// Moved to 1.5MB offset to avoid code overlap (Code is ~966KB)
// Definitions moved to header
#define MAX_CUSTOM_SAMPLES 64

static const uint8_t *customSamplePtrs[MAX_CUSTOM_SAMPLES];
static uint32_t customSampleLens[MAX_CUSTOM_SAMPLES];
static uint32_t customSampleCount = 0;

// Helper for debug blink (Direct GPIO on ComputerCard LEDs)
// Helper for debug blink (Direct GPIO on ComputerCard LEDs)
static void blink_debug(int code) {
  // LED 3 = GPIO 13 (Bottom Right)
  const int LED_PIN = 13;

  gpio_set_function(LED_PIN, GPIO_FUNC_SIO);
  gpio_set_dir(LED_PIN, GPIO_OUT);

  // FAST Blinks for speed (50ms ON, 50ms OFF)
  for (int k = 0; k < code; k++) {
    gpio_put(LED_PIN, 1);
    busy_wait_ms(50);
    gpio_put(LED_PIN, 0);
    busy_wait_ms(50);
  }
  // No long wait at end, just return to logic

  gpio_set_function(LED_PIN, GPIO_FUNC_PWM);
}

void SynthCore_ScanFlashSamples() {
  // Silent Start (Faster Boot)

  const uint32_t *flash_header = (const uint32_t *)FLASH_SAMPLES_BASE;

  // Check Magic
  if (flash_header[0] != FLASH_MAGIC) {
    customSampleCount = 0;
    blink_debug(5); // Debug 5: ERROR (Magic Mismatch)
    return;
  }

  // Magic Found - Proceed silently

  uint32_t count = flash_header[1];
  if (count > MAX_CUSTOM_SAMPLES)
    count = MAX_CUSTOM_SAMPLES;

  // Start scanning after the 256-byte metadata block
  const uint8_t *ptr = (const uint8_t *)(FLASH_SAMPLES_BASE + 256);

  for (uint32_t i = 0; i < count; i++) {
    // Safety: Check if we are still within a reasonable flash range
    // (Increased to 16MB to support high-capacity boards)
    if ((uintptr_t)ptr > (FLASH_SAMPLES_BASE + 16 * 1024 * 1024))
      break;

    // Alignment: Ensure ptr is 4-byte aligned
    ptr = (const uint8_t *)(((uintptr_t)ptr + 3) & ~3);

    volatile uint32_t numSamples =
        *((const uint32_t *)ptr); // volatile to force read

    // Safety check: if length is invalid (erased flash, 0, or absurdly large),
    // abort
    if (numSamples == 0 || numSamples == 0xFFFFFFFF ||
        numSamples > (16 * 1024 * 1024)) {
      break;
    }

    ptr += 4;

    customSamplePtrs[i] = ptr;
    customSampleLens[i] = numSamples;

    // Skip over the PCM-8 data (1 byte per sample)
    ptr += numSamples;
  }

  customSampleCount = count;

  // Debug 3: Success
  blink_debug(3);
}

uint32_t SynthCore_GetCustomSampleCount() { return customSampleCount; }

const uint8_t *SynthCore_GetCustomSamplePtr(int index) {
  if (index < 0 || index >= (int)customSampleCount)
    return 0;
  return customSamplePtrs[index];
}

uint32_t SynthCore_GetCustomSampleLen(int index) {
  if (index < 0 || index >= (int)customSampleCount)
    return 0;
  return customSampleLens[index];
}

void SynthCore_TriggerNote(uint8_t note, uint8_t velocity) {
  // Wrapper to allow Core 0 to trigger notes
  // Pulse/Manual Trigger -> isMidiSource = false
  if (externalModeActive)
    return; // Don't trigger synth voices in external mode
  NoteOn(note, velocity, false);
}

void SynthCore_ReleaseNote(uint8_t note) {
  if (externalModeActive)
    return;
  NoteOff(note);
}

void SynthCore_SetExternalMode(bool enabled) { externalModeActive = enabled; }
