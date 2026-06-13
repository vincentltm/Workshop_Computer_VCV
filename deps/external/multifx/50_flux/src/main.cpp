#include "ComputerCard.h"
#include "SynthCore.h"
#include "adpcm.h"
#include "grids_patterns.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "menu.h"
#include "pico/multicore.h"
#include "pipicofx/fxPrograms.h"
#include "tusb.h"
#include "voice_data.h"
#include <math.h>

// Grids State
static uint8_t gridsStep = 0;

static int16_t gridsP1Trig = 0;
static int16_t gridsP2Trig = 0;
static int16_t gridsCv1Trig = 0;
static int16_t gridsCv2Trig = 0;
static uint8_t gridsPerturbation[3] = {0, 0, 0};

/*
   MultiFX Firmware v3.1 (Refactored Menu System)
   - Core 0: Effects, UI, Audio Routing
   - Core 1: Synth Engine (Drone/Gate, Pitch, Timbre, Envelope)

   Menu State Machine:
     SYNTH_EDIT    — Switch UP + source=SYNTH  → edit synth params
                     Switch UP + source=EXT    → freeze effect
     EFFECT_EDIT   — Switch MIDDLE (normal)    → edit effect params
     EFFECT_SELECT — Switch DOWN (single)      → select effect + volume + extra
     SYNTH_SELECT  — Switch DOWN (double-tap)  → select synth/source + volume +
   extra OR effect-0 in Middle
*/

FxProgramType *currentEffect = nullptr;
int currentEffectIndex = -1;

enum InputSource { SOURCE_EXTERNAL, SOURCE_SYNTH };
InputSource currentSource = SOURCE_EXTERNAL;
uint8_t selectedSynthMode = 0;

DeviceSettings settings;

void SaveSettingsToFlash();

class MultiFx_Computer : public ComputerCard {
public:
  MultiFx_Computer() {
    instance = this;

    // CRITICAL: Effects MUST be initialized at startup!
    // Many effects have pointer fields in their data structs (buffer,
    // delayLine, etc.) that are ONLY set by setup(). Without this,
    // processSample crashes on NULL access.
    for (int i = 0; i < N_FX_PROGRAMS; i++) {
      if (fxPrograms[i] && fxPrograms[i]->setup) {
        fxPrograms[i]->setup(fxPrograms[i]->data);
      }
    }

    SynthCore_ScanFlashSamples();
    SynthCore_Init();

    uint32_t activeCount = GetActiveSampleCount();
    if (selectedSampleIdx >= (int)activeCount)
      selectedSampleIdx = activeCount - 1;
    if (selectedSampleIdx < 0)
      selectedSampleIdx = 0;

    SynthCore_Start();
    LoadSettings();
  }

  static MultiFx_Computer *instance;

  void BackgroundLoop() override {
    tud_task();
    Midi_Rx();

    static uint32_t lastLiveCheck = 0;
    static uint8_t lastBlob[32] = {0};
    uint32_t now = to_ms_since_boot(get_absolute_time());

    // Poll hardware state every 50ms
    if (now - lastLiveCheck > 50) {
      lastLiveCheck = now;
      if (tud_midi_mounted()) {
        uint8_t blob[32];
        GetLiveState(blob);
        // Only broadcast if the state actually changed
        if (initialized && memcmp(blob, lastBlob, 32) != 0) {
          memcpy(lastBlob, blob, 32);
          uint8_t pkt[36];
          pkt[0] = 0xF0;
          pkt[1] = 0x7D;
          pkt[2] = 0x16; // CMD 0x16: Live State Broadcast
          memcpy(&pkt[3], blob, 32);
          pkt[35] = 0xF7;
          tud_midi_stream_write(0, pkt, 36);
        } else if (!initialized) {
          // Keep base line matched while starting up
          memcpy(lastBlob, blob, 32);
        }
      }
    }
    if (pendingSave) {
      pendingSave = false;
      SaveSettingsToFlash();
    }
  }

  // =========================================================================
  // State
  // =========================================================================
  bool initialized = false;
  bool pendingSave = false;
  int startupFrames = AUDIO_BASE_RATE; // ~1 second of samples

  // --- Menu State Machine ---
  MenuState menuState = MenuState::EFFECT_EDIT;
  MenuState lastMenuState = MenuState::EFFECT_EDIT;

  // Knob locks (large-delta unlock, not soft-takeover)
  KnobLock lockMain; // Shared across all pages (re-engaged on page switch)
  KnobLock lockX;
  KnobLock lockY;

  // Internal page state — persistent knob values per switch layer (0-4095
  // range) Updated only when the knob is unlocked on that page.
  PageState pageSynthEdit = {2047, 2047, 0};     // UP:  env, pitch, timbre
  PageState pageEffectEdit = {2800, 2048, 2048}; // MID: fx p0, p1, p2
  PageState pageSelect = {0, 0, 4095}; // DOWN: effectSel, synthSel, zParam

  // Double-tap detection (activates EXTRA page — reserved for future use)
  int32_t timeSinceDownRelease = 1000;
  bool extraPageActive = false;

  // Synth binary LED flash timer (control ticks at ~750 Hz)
  // When > 0: show synth/source as binary on LEDs instead of effect binary
  int32_t synthBinaryFlashTimer = 0;

  // Synth parameters (persistent across mode switches)
  float synthParameterPitch = 0.5f;
  float synthParameterTimbre = 0.0f;
  float synthParameterEnv = 0.1f;
  float synthParameterFilterCutoff = 1.0f;

  // Sample / Volume
  int selectedSampleIdx = 0;
  int32_t sampleVolumeQ16 = 65536;

  // Voice announcement (ADPCM)
  int voiceTriggerTimer = 0;
  int nextVoiceToPlay = 0;
  AdpcmState adpcm;
  const uint8_t *voicePtr = 0;
  uint32_t voiceLen = 0;
  uint64_t voiceIdxQ16 = 0;
  const uint32_t voiceSpeedQ16 =
      SAMPLER_SPEED_Q16; // 22050/AUDIO_BASE_RATE playback speed
  uint32_t voiceDecodeIdx = 0;
  int32_t voiceCurrVal = 0;
  int32_t voiceLastVal = 0;

  // VU meter
  int32_t envIn = 0;
  int32_t envOut = 0;

  // Fast trigger detection for drums (audio-rate)
  bool fastTrigA1 = false;
  bool fastTrigA2 = false;
  int32_t peakTrigA1 = 0;
  int32_t peakTrigA2 = 0;

  // =========================================================================
  // Helpers
  // =========================================================================
  const uint8_t *GetUnifiedVoiceData(int index) {
    uint32_t customCount = SynthCore_GetCustomSampleCount();
    if (customCount > 0)
      return SynthCore_GetCustomSamplePtr(index);
    return 0; // No fallback! Voice announcements are not sampler data.
  }
  uint32_t GetUnifiedVoiceLen(int index) {
    uint32_t customCount = SynthCore_GetCustomSampleCount();
    if (customCount > 0)
      return SynthCore_GetCustomSampleLen(index);
    return 0; // No fallback!
  }
  uint32_t GetActiveSampleCount() {
    uint32_t customCount = SynthCore_GetCustomSampleCount();
    return (customCount > 0) ? customCount : 0; 
  }

  void TriggerVoice(int voiceIdx, int delayFrames = 4800) {
    bool wantsVoice = settings.voicePreviewEnabled || (settings.cv1Mode == 6) ||
                      (settings.cv2Mode == 6) || (settings.pulse1Mode == 5) ||
                      (settings.pulse2Mode == 5);
    if (!wantsVoice)
      return;
    nextVoiceToPlay = voiceIdx;
    voiceTriggerTimer = delayFrames;
    voiceIdxQ16 = 0;
    voicePtr = 0;
  }

  void __attribute__((noinline)) SetSampleForCurrentMode(int sampleIdx) {
    selectedSampleIdx = sampleIdx;
    if (selectedSynthMode == SYNTH_MODE_DRUMS) {
      SynthCore_SetDrumBase(sampleIdx);
    } else {
      const uint8_t *ptr = GetUnifiedVoiceData(sampleIdx);
      uint32_t len = GetUnifiedVoiceLen(sampleIdx);
      SynthCore_SetSamplePtr(ptr, len, sampleIdx);
    }
  }

  // Engage all locks simultaneously (call on any state transition)
  void EngageAllLocks(int32_t rawMain, int32_t unifiedX, int32_t unifiedY) {
    lockMain.engage(rawMain);
    lockX.engage(unifiedX);
    lockY.engage(unifiedY);
  }

  // Apply sane default synth parameters when switching to a new synth mode
  void ApplySynthModeDefaults(uint8_t newMode) {
    synthParameterPitch = 0.5f;

    if (newMode == SYNTH_MODE_SAMPLER_PLAYER) {
      synthParameterEnv = 1.0f;
    } else if (newMode == SYNTH_MODE_PIANO || newMode == SYNTH_MODE_STRINGS) {
      synthParameterEnv = 0.6f;
    } else {
      synthParameterEnv = 0.4f;
    }

    if (newMode == SYNTH_MODE_SAMPLER_PLAYER) {
      synthParameterTimbre = 0.0f;
    } else if (newMode == SYNTH_MODE_GRANULAR) {
      synthParameterTimbre = 0.5f;
    } else if (newMode == SYNTH_MODE_SAMPLER_ONESHOT ||
               newMode == SYNTH_MODE_SAMPLER_LOOP ||
               newMode == SYNTH_MODE_DRUMS) {
      synthParameterTimbre = 0.0f;
    } else if (newMode == SYNTH_MODE_VABASS) {
      synthParameterTimbre = 0.5f;
    } else {
      synthParameterTimbre = 0.2f;
    }

    synthParameterFilterCutoff = 1.0f;
  }

  // =========================================================================
  // Settings
  // =========================================================================
  void LoadSettings() {
    const uint32_t *header = (const uint32_t *)FLASH_SETTINGS_BASE;
    if (header[0] == FLASH_MAGIC) {
      const DeviceSettings *flashSettings =
          (const DeviceSettings *)(FLASH_SETTINGS_BASE + 8);
      if (flashSettings->magic == FLASH_MAGIC) {
        settings = *flashSettings;
        // Sanitize new fields that may be zero from older firmware saves
        if (settings.ccPerfMain == 0)
          settings.ccPerfMain = 88;
        if (settings.ccPerfX == 0)
          settings.ccPerfX = 89;
        if (settings.ccPerfY == 0)
          settings.ccPerfY = 90;
        if (settings.seqAOctRange == 0)
          settings.seqAOctRange = 2;
        if (settings.seqBOctRange == 0)
          settings.seqBOctRange = 2;
      } else {
        SetDefaultSettings();
      }
    } else {
      SetDefaultSettings();
    }
    ApplySettings();
  }

  void SetDefaultSettings() {
    settings.voicePreviewEnabled = 1;
    settings.pulse1Mode = 0;
    settings.pulse1Ch = 0;
    settings.pulse1Arg = 60;
    settings.pulse2Mode = 1;
    settings.pulse2Ch = 0;
    settings.pulse2Arg = 60;
    settings.cv1Mode = 0;
    settings.cv1Ch = 0;
    settings.cv1Arg = 0;
    settings.cv2Mode = 2;
    settings.cv2Ch = 0;
    settings.cv2Arg = 1;
    settings.knobMainCC = 0;
    settings.knobXCC = 0;
    settings.knobYCC = 0;
    settings.pitchRangeSemis = 36;
    settings.cv1Range = 0;
    settings.cv2Range = 0;
    settings.audioInScale = 0;
    settings.audioInHold = 0;
    settings.rootNote = 60;
    for (int i = 0; i < 16; i++)
      settings.sequencerSteps[i] = 0;
    settings.sequencerLength = 16;
    settings.selectedSynthMode = 0;
    settings.globalVolume = 127;
    // CC Input Routing defaults
    settings.ccEffectSelect = 20;
    settings.ccSynthSelect = 21;
    settings.ccVolume = 7;
    settings.ccChannel = 16; // Omni
    settings.ccSynthEnv = 73;
    settings.ccSynthPitch = 74;
    settings.ccSynthTimbre = 71;
    settings.ccSynthFilter = 22;
    settings.ccSynthSample = 23;
    settings.ccFxParam0 = 85;
    settings.ccFxParam1 = 86;
    settings.ccFxParam2 = 87;
    // New Phase-2 fields
    settings.bpmEnabled = 0; // default: use MIDI clock
    settings.bpm = 120;
    settings.pulse1ClockDiv = 0; // /1 (pass-through)
    settings.pulse2ClockDiv = 0;
    settings.pulse1PPQN = 1; // 1 = Quarter Note (default)
    settings.pulse2PPQN = 1;
    settings.pulse1Width = 50; // 50% gate (default)
    settings.pulse2Width = 50;
    for (int i = 0; i < 16; i++)
      settings.sequencerSteps2[i] = 0;
    settings.sequencerLength2 = 16;
    settings.eg1Env = 64;
    settings.eg2Env = 64;
    settings.egReserved1 = 0;
    settings.egReserved2 = 0;
    settings.egReserved3 = 0;
    // Unified clock sources: default MIDI clock
    settings.pulse1ClockSrc = 0;
    settings.pulse2ClockSrc = 0;
    settings.cv1ClockSrc = 0;
    settings.cv2ClockSrc = 0;
    // EG trigger source: default MIDI gate
    settings.egTrigSrc = 0;
    // Sequencer gate bitmasks: all steps active by default (14-bit hi/lo +
    // 2-bit extra = 16 steps)
    settings.seqAGateMask = 0x7F;      // steps 0-6
    settings.seqAGateMaskHi = 0x7F;    // steps 7-13
    settings.seqAGateMaskExtra = 0x03; // steps 14-15
    settings.seqBGateMask = 0x7F;
    settings.seqBGateMaskHi = 0x7F;
    settings.seqBGateMaskExtra = 0x03;
    // CV column clock dividers: default /1 (pass-through)
    settings.cv1ClockDiv = 0;
    settings.cv2ClockDiv = 0;

    // Initialization of saved state
    settings.currentEffectIndex = 0;
    settings.currentSource = SOURCE_EXTERNAL;
    settings.fxParam0 = 1000;
    settings.fxParam1 = 1000;
    settings.fxParam2 = 1000;
    settings.synthParamPitch = 8192;   // 0.5f normalized
    settings.synthParamTimbre = 3276;  // 0.2f normalized
    settings.synthParamEnv = 6553;     // 0.4f normalized
    settings.synthParamFilter = 16383; // 1.0f normalized

    settings.isCardLocked = 0; // Unlocked card by default

    // Performance knob CC defaults (EXTRA page: double-tap + hold switch down)
    settings.ccPerfMain = 88; // BPM or Pulse 1 probability
    settings.ccPerfX = 89;    // CV1 perf param
    settings.ccPerfY = 90;    // CV2 perf param

    // Sequencer scale defaults
    settings.seqAScale = 1;    // major
    settings.seqARoot = 0;     // C
    settings.seqAOctave = 3;   // octave 3
    settings.seqAOctRange = 2; // 2 octave span
    settings.seqBScale = 1;
    settings.seqBRoot = 0;
    settings.seqBOctave = 3;
    settings.seqBOctRange = 2;

    settings.magic = FLASH_MAGIC;
  }

  void ApplySettings() {
    selectedSynthMode = settings.selectedSynthMode;
    // Backwards compatibility for new Pulse fields (0xFF from old flash
    // reading)
    if (settings.pulse1Width > 100)
      settings.pulse1Width = 50;
    if (settings.pulse2Width > 100)
      settings.pulse2Width = 50;
    if (settings.pulse1PPQN == 0 || settings.pulse1PPQN > 24)
      settings.pulse1PPQN = 1;
    if (settings.pulse2PPQN == 0 || settings.pulse2PPQN > 24)
      settings.pulse2PPQN = 1;

    // Backwards compatibility: if globalVolume is 0 (old firmware save),
    // default to max
    if (settings.globalVolume == 0) {
      settings.globalVolume = 127;
    }
    sampleVolumeQ16 = ((uint32_t)settings.globalVolume << 9);
    if (settings.globalVolume == 127)
      sampleVolumeQ16 = 65536;

    // Sanitize extra gate mask bits (ensure only 2 bits)
    settings.seqAGateMaskExtra &= 0x03;
    settings.seqBGateMaskExtra &= 0x03;

    // Normalize BPM storage to 14-bit MIDI encoding (bpm | (bpmHi << 7)).
    // Older SysEx briefly stored an 8-bit low byte + (total>>8) in bpmHi,
    // which broke internal clock and live-state sync for 128–255 BPM.
    {
      uint16_t tb = (settings.bpm & 0x7F) |
                    ((uint16_t)(settings.bpmHi & 0x7F) << 7);
      if (settings.bpmHi == 0 && settings.bpm > 127)
        tb = settings.bpm;
      if (tb < 20)
        tb = 20;
      if (tb > 256)
        tb = 256;
      settings.bpm = (uint8_t)(tb & 0x7F);
      settings.bpmHi = (uint8_t)((tb >> 7) & 0x7F);
    }

    // Apply Live Mode State
    currentEffectIndex = settings.currentEffectIndex;
    if (currentEffectIndex >= N_FX_PROGRAMS)
      currentEffectIndex = 0;
    currentSource = (InputSource)settings.currentSource;

    // Apply Synth Parameters to globals
    synthParameterPitch = (float)settings.synthParamPitch / 16383.0f;
    synthParameterTimbre = (float)settings.synthParamTimbre / 16383.0f;
    synthParameterEnv = (float)settings.synthParamEnv / 16383.0f;
    synthParameterFilterCutoff = (float)settings.synthParamFilter / 16383.0f;

    // Trigger Core 1 to initialize engine state (External vs Synth)
    SynthCore_SetExternalMode(currentSource == SOURCE_EXTERNAL);

    if (selectedSynthMode == SYNTH_MODE_DRUMS) {
      SynthCore_SetDrumBase(selectedSampleIdx);
    } else if (selectedSynthMode == SYNTH_MODE_SAMPLER_ONESHOT ||
               selectedSynthMode == SYNTH_MODE_SAMPLER_LOOP ||
               selectedSynthMode == SYNTH_MODE_SAMPLER_PLAYER ||
               selectedSynthMode == SYNTH_MODE_GRANULAR) {
      SetSampleForCurrentMode(selectedSampleIdx);
    }

    // Note: Parameter application is handled inside ProcessSample's initialized
    // block
  }

  // =========================================================================
  // ProcessSample
  // =========================================================================
  void __not_in_flash_func(ProcessSample)() override {
    auto softLimit = [](int32_t x) -> int32_t {
      const int32_t knee = 24000;
      if (x > knee)
        return knee + ((x - knee) >> 1);
      else if (x < -knee)
        return -(knee + ((-x - knee) >> 1));
      return x;
    };

    // ---- Startup / Initialization ----
    if (!initialized) {
      if (startupFrames > 0) {
        int16_t inL_raw = AudioIn1() << 4;
        int16_t inR_raw = AudioIn2() << 4;
        int32_t absL = (inL_raw > 0 ? inL_raw : -inL_raw);
        int32_t absR = (inR_raw > 0 ? inR_raw : -inR_raw);
        if (absL > noiseFloorL)
          noiseFloorL = absL;
        if (absR > noiseFloorR)
          noiseFloorR = absR;
        startupFrames--;
        return;
      }
      gateThreshL = noiseFloorL + 300;
      if (gateThreshL < 300)
        gateThreshL = 300;
      gateThreshR = noiseFloorR + 300;
      if (gateThreshR < 300)
        gateThreshR = 300;

      // Restore Effect
      currentEffect = fxPrograms[currentEffectIndex];
      if (currentEffect && currentEffect->setup) {
        currentEffect->setup(currentEffect->data);
      }

      // Sync effect params to current knob positions (or baked values if
      // locked)
      if (currentEffect) {
        int32_t km = KnobVal(Main), kx = KnobVal(X), ky = KnobVal(Y);
        if (settings.isCardLocked) {
          km = settings.fxParam0;
          kx = settings.fxParam1; // Note: using kx/ky as raw placeholders
          ky = settings.fxParam2;
        }
        int32_t cv1 = Connected(CV1) ? CVIn(0) : 0;
        int32_t cv2 = Connected(CV2) ? CVIn(1) : 0;
        int32_t ux = kx + cv1;
        if (ux < 0)
          ux = 0;
        if (ux > 4095)
          ux = 4095;
        int32_t uy = ky + cv2;
        if (uy < 0)
          uy = 0;
        if (uy > 4095)
          uy = 4095;

        // Apply to effect
        if (currentEffect->nParameters > 0 &&
            currentEffect->parameters[0].setParameter) {
          currentEffect->parameters[0].setParameter((uint16_t)km,
                                                    currentEffect->data);
          currentEffect->parameters[0].rawValue = (int16_t)km;
        }
        if (currentEffect->nParameters > 1 &&
            currentEffect->parameters[1].setParameter) {
          currentEffect->parameters[1].setParameter((uint16_t)ux,
                                                    currentEffect->data);
          currentEffect->parameters[1].rawValue = (int16_t)ux;
        }
        if (currentEffect->nParameters > 2 &&
            currentEffect->parameters[2].setParameter) {
          currentEffect->parameters[2].setParameter((uint16_t)uy,
                                                    currentEffect->data);
          currentEffect->parameters[2].rawValue = (int16_t)uy;
        }
      }

      // Sync synth params to current knob positions (or baked values if locked)
      {
        int32_t km = KnobVal(Main), kx = KnobVal(X), ky = KnobVal(Y);
        if (settings.isCardLocked) {
          km = (settings.synthParamEnv * 4095) / 16383;
          kx = (settings.synthParamPitch * 4095) / 16383;
          ky = (settings.synthParamTimbre * 4095) / 16383;
        }
        int32_t cv1 = Connected(CV1) ? CVIn(0) : 0;
        int32_t cv2 = Connected(CV2) ? CVIn(1) : 0;
        int32_t ux = kx + cv1;
        if (ux < 0)
          ux = 0;
        if (ux > 4095)
          ux = 4095;
        int32_t uy = ky + cv2;
        if (uy < 0)
          uy = 0;
        if (uy > 4095)
          uy = 4095;

        synthParameterEnv = (float)km / 4095.0f;
        synthParameterPitch = (float)ux / 4095.0f;
        synthParameterTimbre = (float)uy / 4095.0f;

        SynthParams p = {0};
        p.pitch = synthParameterPitch;
        p.timbre = synthParameterTimbre;
        p.envelope = synthParameterEnv;
        p.filterCutoff = synthParameterFilterCutoff;
        p.mode = selectedSynthMode;
        p.volume = 1.0f;
        SynthCore_UpdateParams(&p);
      }

      initialized = true;
    }

    // ---- Control Decimator (every 64 samples ≈ 1.3ms) ----
    static uint8_t controlDecimator = 0;
    controlDecimator++;

    static int32_t rawX = 0;
    static int32_t unifiedX = 0;
    static int32_t rawY = 0;
    static int32_t unifiedY = 0;
    static int32_t rawMain = 0;
    static Switch sw = ComputerCard::Switch::Up;

    // Switch debounce
    static Switch lastRawSw = ComputerCard::Switch::Up;
    static uint8_t swDebounceCount = 0;

    // Voice timer (decimated)
    if (voiceTriggerTimer > 0) {
      if (voiceTriggerTimer < 64)
        voiceTriggerTimer = 0;
      else
        voiceTriggerTimer -= 64;
      if (voiceTriggerTimer == 0) {
        Adpcm_Init(&adpcm);
        voicePtr = getVoiceData(nextVoiceToPlay);
        voiceLen = getVoiceLen(nextVoiceToPlay);
        voiceIdxQ16 = 0;
        voiceDecodeIdx = 0;
        voiceCurrVal = 0;
        voiceLastVal = 0;
      }
    }

    if (controlDecimator < 64)
      goto audio_path;
    controlDecimator = 0;

    // ---- Read controls ----
    {
      if (timeSinceDownRelease < 1000)
        timeSinceDownRelease++;

      rawX = KnobVal(X);
      this->rawX = rawX; // sync to member for handler methods
      {
        int32_t cv1 = Connected(CV1) ? CVIn(0) : 0;
        unifiedX = rawX + cv1;
        if (unifiedX < 0)
          unifiedX = 0;
        if (unifiedX > 4095)
          unifiedX = 4095;
      }
      rawY = KnobVal(Y);
      this->rawY = rawY; // sync to member for handler methods
      {
        int32_t cv2 = Connected(CV2) ? CVIn(1) : 0;
        unifiedY = rawY + cv2;
        if (unifiedY < 0)
          unifiedY = 0;
        if (unifiedY > 4095)
          unifiedY = 4095;
      }
      rawMain = KnobVal(Main);

      // Debounce switch
      Switch rawSw = SwitchVal();
      if (rawSw == lastRawSw) {
        if (swDebounceCount < 10)
          swDebounceCount++;
        else
          sw = rawSw;
      } else {
        swDebounceCount = 0;
        lastRawSw = rawSw;
      }
    }

    // ---- Compute new MenuState ----
    {
      static bool wasDown = false;
      MenuState newState = menuState;

      if (sw == ComputerCard::Switch::Down) {
        if (!wasDown) {
          wasDown = true;
          // Double-tap detection: activate EXTRA page (reserved for future use)
          if (timeSinceDownRelease < 200) {
            extraPageActive = true;
          } else {
            extraPageActive = false;
          }
          // (Removed voice announcement here per user request)
        }
        newState = extraPageActive ? MenuState::EXTRA : MenuState::SELECT;
      } else {
        if (wasDown) {
          wasDown = false;
          timeSinceDownRelease = 0;
          // Apply selected effect on switch release only when coming from
          // SELECT page,
          // NOT when releasing from EXTRA page (that would reset effect params
          // mid-performance).
          if (menuState == MenuState::SELECT) {
            if (currentEffect != fxPrograms[currentEffectIndex]) {
              currentEffect = fxPrograms[currentEffectIndex];
              if (currentEffect && currentEffect->setup)
                currentEffect->setup(currentEffect->data);
              if (currentEffect) {
                if (currentEffect->nParameters > 0 &&
                    currentEffect->parameters[0].setParameter) {
                  currentEffect->parameters[0].setParameter(
                      (uint16_t)pageEffectEdit.main, currentEffect->data);
                  currentEffect->parameters[0].rawValue =
                      (int16_t)pageEffectEdit.main;
                }
                if (currentEffect->nParameters > 1 &&
                    currentEffect->parameters[1].setParameter) {
                  currentEffect->parameters[1].setParameter(
                      (uint16_t)pageEffectEdit.x, currentEffect->data);
                  currentEffect->parameters[1].rawValue =
                      (int16_t)pageEffectEdit.x;
                }
                if (currentEffect->nParameters > 2 &&
                    currentEffect->parameters[2].setParameter) {
                  currentEffect->parameters[2].setParameter(
                      (uint16_t)pageEffectEdit.y, currentEffect->data);
                  currentEffect->parameters[2].rawValue =
                      (int16_t)pageEffectEdit.y;
                }
              }
              if (currentEffect && currentEffect->reset)
                currentEffect->reset(currentEffect->data);
            }
            pendingSave = true; // Auto-save when leaving SELECT menu
          }
        }

        if (sw == ComputerCard::Switch::Up)
          newState = MenuState::SYNTH_EDIT;
        else if (sw == ComputerCard::Switch::Middle)
          newState = MenuState::EFFECT_EDIT;
      }

      // Detect state transition → engage all locks
      if (newState != menuState) {
        if (menuState == MenuState::SYNTH_EDIT && currentSource == SOURCE_EXTERNAL) {
          if (currentEffect) {
            for (int i = 0; i < currentEffect->nParameters; i++) {
              const char *name = currentEffect->parameters[i].name;
              if (name && name[0] == 'F' && name[1] == 'r' && name[2] == 'e' &&
                  name[3] == 'e' && name[4] == 'z' && name[5] == 'e') {
                if (currentEffect->parameters[i].setParameter)
                  currentEffect->parameters[i].setParameter(0, currentEffect->data);
              }
            }
          }
        }
        EngageAllLocks(rawMain, unifiedX, unifiedY);
        menuState = newState;
      }
    }

    // ---- Handle incoming MIDI CC (before state dispatch) ----
    HandleMidiCC(rawMain, unifiedX, unifiedY);

    // ---- Decrement synth binary LED flash timer ----
    if (synthBinaryFlashTimer > 0)
      synthBinaryFlashTimer--;

    // ---- Dispatch to state handler ----
    switch (menuState) {
    case MenuState::SYNTH_EDIT:
      HandleSynthEdit(rawMain, unifiedX, unifiedY);
      break;
    case MenuState::EFFECT_EDIT:
      HandleEffectEdit(rawMain, unifiedX, unifiedY);
      break;
    case MenuState::SELECT:
      HandleSelect(rawMain, unifiedX, unifiedY);
      break;
    case MenuState::EXTRA:
      HandleExtra(rawMain, unifiedX, unifiedY);
      break;
    }

    // ---- Output Logic (Pulse & CV) ----
    // Run BEFORE ApplySynthParams so sequencer advances and CV output
    // updates before the synth reads gate and pitch — prevents 1-tick
    // pitch mismatch that causes audible glide on loopback patches.
    HandleOutputs();

    // ---- Assemble and send SynthParams ----
    ApplySynthParams(unifiedX, unifiedY, rawMain);

    // End of control decimator block
    goto end_control;

  audio_path:
  end_control:;

    // =========================================================================
    // AUDIO PATH (per sample)
    // =========================================================================
    int16_t inL = 0, inR = 0;

    if (currentSource == SOURCE_SYNTH) {
      int16_t synthL, synthR;
      if (SynthCore_GetSample(&synthL, &synthR)) {
        inL = synthL;
        inR = synthR;
      }
    } else {
      // External input with noise gate + denoise
      static uint32_t ditherSeed = 1234567;
      auto fast_rand = [&]() {
        ditherSeed = ditherSeed * 1103515245 + 12345;
        return ditherSeed;
      };
      auto get_dither = [&]() {
        int32_t r1 = (fast_rand() >> 16) & 0xF;
        int32_t r2 = (fast_rand() >> 16) & 0xF;
        return r1 - r2;
      };

      int32_t inL_raw = AudioIn(0) << 4;
      int32_t inR_raw = AudioIn(1) << 4;

      // Real-time DC blocking (High Pass) for audio feedback loops
      // R=0.99 at 48kHz → R=0.98 at 24kHz for same ~80Hz corner freq
      // (R = 1 - 2*pi*fc/fs, so fc/fs doubles at 24kHz, so reduce R)
      static int32_t dcL_x = 0, dcL_y = 0;
      int32_t dcL = inL_raw - dcL_x + ((dcL_y * 32113) >> 15); // R≈0.980
      dcL_x = inL_raw;
      dcL_y = dcL;

      static int32_t dcR_x = 0, dcR_y = 0;
      int32_t dcR = inR_raw - dcR_x + ((dcR_y * 32113) >> 15);
      dcR_x = inR_raw;
      dcR_y = dcR;

      int32_t sL = dcL + get_dither();
      int32_t sR = dcR + get_dither();
      // ADC artifact notch: at 48kHz this suppressed a 12kHz switching
      // artifact. At 24kHz that artifact is at Nyquist and filtered by the
      // ComputerCard decimator, so the notch is not needed — use signal as-is.
      inL = (int16_t)(sL > 32767 ? 32767 : (sL < -32768 ? -32768 : sL));
      inR = (int16_t)(sR > 32767 ? 32767 : (sR < -32768 ? -32768 : sR));

      // Denoise mix: fixed at full filtering (realtime control removed)
      static const int32_t globalDenoiseMix = 4096;

      int32_t rawL = inL, rawR = inR;

      // Soft noise gate — envelope follower.
      // >>11 at 48kHz → >>10 at 24kHz to maintain same release time.
      gateEnvL -= (gateEnvL >> 10);
      int32_t absL = (inL > 0 ? inL : -inL);
      if (absL > gateEnvL)
        gateEnvL = absL;
      gateEnvR -= (gateEnvR >> 10);
      int32_t absR = (inR > 0 ? inR : -inR);
      if (absR > gateEnvR)
        gateEnvR = absR;

      if (gateEnvL < gateThreshL) {
        int32_t ratio = (gateEnvL << 15) / gateThreshL;
        inL = (inL * ratio) >> 15;
      }
      if (Disconnected(Audio2)) {
        inR = inL;
        gateEnvR = gateEnvL;
      }
      if (gateEnvR < gateThreshR) {
        int32_t ratio = (gateEnvR << 15) / gateThreshR;
        inR = (inR * ratio) >> 15;
      }

      // Gentle lowpass anti-alias filter.
      // coeff 18000 at 48kHz ≈ fc~8kHz → keep fc~8kHz at 24kHz: use ~26000.
      lpL += ((inL - lpL) * 26000) >> 15;
      inL = (int16_t)lpL;
      lpR += ((inR - lpR) * 26000) >> 15;
      inR = (int16_t)lpR;

      // Crossfade raw vs denoised
      inL = (rawL * (4096 - globalDenoiseMix) + inL * globalDenoiseMix) >> 12;
      inR = (rawR * (4096 - globalDenoiseMix) + inR * globalDenoiseMix) >> 12;

      // External volume
      if (sampleVolumeQ16 < 65536) {
        inL = (int16_t)(((int32_t)inL * sampleVolumeQ16) >> 16);
        inR = (int16_t)(((int32_t)inR * sampleVolumeQ16) >> 16);
      }
    }

    // Mono normalization
    if (Disconnected(Audio2))
      inR = inL;

    // Effect processing
    int16_t outL = 0, outR = 0;
    if (currentEffect &&
        (currentEffect->processSample || currentEffect->processSampleStereo)) {
      if (currentEffect->isStereo && currentEffect->processSampleStereo) {
        volatile uint32_t *audioState = getAudioStatePtr();
        currentEffect->processSampleStereo(&inL, &inR, &outL, &outR,
                                           currentEffect->data, audioState);
      } else {
        int32_t inMono = ((int32_t)inL + (int32_t)inR) >> 1;
        int16_t out =
            currentEffect->processSample((int16_t)inMono, currentEffect->data);
        outL = out;
        outR = out;
      }
    } else {
      outL = inL;
      outR = inR;
    }

    // Output dithering + soft limit
    {
      static uint32_t outDitherSeed = 9876543;
      auto fast_rand2 = [&]() {
        outDitherSeed = outDitherSeed * 1103515245 + 12345;
        return outDitherSeed;
      };
      auto get_dither2 = [&]() {
        int32_t r1 = (fast_rand2() >> 16) & 0xF;
        int32_t r2 = (fast_rand2() >> 16) & 0xF;
        return r1 - r2;
      };
      int32_t dL = softLimit(outL + get_dither2());
      int32_t dR = softLimit(outR + get_dither2());
      int16_t finalL = (int16_t)(dL >> 4);
      int16_t finalR = (int16_t)(dR >> 4);
      if (finalL > 2047)
        finalL = 2047;
      if (finalL < -2048)
        finalL = -2048;
      if (finalR > 2047)
        finalR = 2047;
      if (finalR < -2048)
        finalR = -2048;

      // Drum trigger detection (audio rate)
      int32_t rawIn1 = AudioIn(0);
      int32_t rawIn2 = AudioIn(1);
      if (rawIn1 > 400) {
        fastTrigA1 = true;
        if (rawIn1 > peakTrigA1) peakTrigA1 = rawIn1;
      }
      if (rawIn2 > 400) {
        fastTrigA2 = true;
        if (rawIn2 > peakTrigA2) peakTrigA2 = rawIn2;
      }

      // Voice announcement playback
      int32_t voiceVal = 0;
      if (voicePtr && (voiceIdxQ16 >> 16) < voiceLen) {
        uint32_t targetIdx = voiceIdxQ16 >> 16;
        int timeout = 64;
        while (voiceDecodeIdx <= targetIdx && voiceDecodeIdx < voiceLen &&
               timeout > 0) {
          voiceLastVal = voiceCurrVal;
          uint8_t byte = voicePtr[voiceDecodeIdx >> 1];
          uint8_t nibble = (voiceDecodeIdx & 1) ? (byte >> 4) : (byte & 0x0F);
          voiceCurrVal = Adpcm_Decode(&adpcm, nibble);
          voiceDecodeIdx++;
          timeout--;
        }
        int32_t frac = voiceIdxQ16 & 0xFFFF;
        voiceVal =
            voiceLastVal + (((voiceCurrVal - voiceLastVal) * frac) >> 16);

        // Route voice to CV/Pulse outputs if configured (mode 6 = Voice Audio)
        if (settings.cv1Mode == 6)
          CVOut(0, voiceVal >> 4);
        if (settings.cv2Mode == 6)
          CVOut(1, voiceVal >> 4);
        // Pulse voice audio: 1-bit output based on sign of audio (AM-style PWM)
        // gpio_put(pin, !state) because hardware is active-low
        // Pulse Voice Audio is mode 5 (matches UI option value)
        if (settings.pulse1Mode == 5)
          gpio_put(PULSE_1_RAW_OUT, voiceVal <= 0);
        if (settings.pulse2Mode == 5)
          gpio_put(PULSE_2_RAW_OUT, voiceVal <= 0);

        if (settings.voicePreviewEnabled) {
          // Voice preview cuts through audio. Divide by 16 (>> 4) to map 16-bit
          // ADPCM down to 12-bit DAC levels.
          int32_t vv = voiceVal >> 4;
          finalL = (int16_t)(softLimit(vv) >> 4);
          finalR = (int16_t)(softLimit(vv) >> 4);
        }

        voiceIdxQ16 += voiceSpeedQ16;
      }
      // Audio 2 External Mixing (Synth Mode only)
      if (currentSource == SOURCE_SYNTH && settings.audio2Mode == 1 &&
          Connected(Audio2)) {
        // Raw AudioIn(1) is already centered around 0 (adcInR)
        int32_t scaled = rawIn2 << 4;

        // High-pass DC Blocker (R≈0.98) for 24kHz to prevent feedback loop rail
        // offsets
        static int32_t dcIn2_x = 0;
        static int32_t dcIn2_y = 0;
        int32_t filtered = scaled - dcIn2_x + ((dcIn2_y * 32113) >> 15);
        dcIn2_x = scaled;
        dcIn2_y = filtered;

        int32_t extIn = filtered;

        // Lowpass at ~8kHz to avoid "horrible noise" on external synth mix
        // (26000 for 24kHz)
        static int32_t lpAuxR = 0;
        lpAuxR += ((extIn - lpAuxR) * 26000) >> 15;
        extIn = lpAuxR >> 4;

        int32_t mixedL = finalL + extIn;
        int32_t mixedR = finalR + extIn;
        finalL =
            (int16_t)(mixedL > 32767 ? 32767
                                     : (mixedL < -32768 ? -32768 : mixedL));
        finalR =
            (int16_t)(mixedR > 32767 ? 32767
                                     : (mixedR < -32768 ? -32768 : mixedR));
      }

      AudioOut(0, finalL);
      AudioOut(1, finalR);

      // ---- LED Logic ----
      if (sw == ComputerCard::Switch::Down) {
        // SELECT page: show synth binary briefly after selection, then effect
        // binary
        if (synthBinaryFlashTimer > 0) {
          // Flash: show current synth/source as binary index
          uint8_t visualId = 0;
          if (currentSource == SOURCE_SYNTH) {
            for (int i = 0; i < SYNTH_VOICE_MAP_SIZE; i++) {
              if (SYNTH_VOICE_MAP[i].mode == selectedSynthMode) {
                visualId = (uint8_t)(i + 1);
                break;
              }
            }
          }
          for (int i = 0; i < 6; i++) {
            if ((visualId >> i) & 1)
              LedOn(i);
            else
              LedOff(i);
          }
        } else {
          // Normal: show current effect index in binary
          for (int i = 0; i < 6; i++) {
            if ((currentEffectIndex >> i) & 1)
              LedOn(i);
            else
              LedOff(i);
          }
        }
      } else {
        // VU meter
        int32_t absOut = (finalL > 0 ? finalL : -finalL);
        int32_t absOutR = (finalR > 0 ? finalR : -finalR);
        if (absOutR > absOut)
          absOut = absOutR;
        envOut -= (envOut >> 12);
        if (absOut > envOut)
          envOut = absOut;
        envIn -= (envIn >> 12);
        int32_t absIn = (inL > 0 ? inL : -inL);
        if (absIn > envIn)
          envIn = absIn;

        LedOff(0);
        LedOff(2);
        LedOff(4);
        if (envIn > 400)
          LedOn(4);
        if (envIn > 6000)
          LedOn(2);
        if (envIn > 31000)
          LedOn(0);
        LedOff(1);
        LedOff(3);
        LedOff(5);
        if (envOut > 400)
          LedOn(5);
        if (envOut > 6000)
          LedOn(3);
        if (envOut > 31000)
          LedOn(1);
      }
    }
  }

  // =========================================================================
  // HandleMidiCC — Apply incoming CC values, bypass knob locks, re-engage
  // them Synth CCs and Effect CCs are fully independent and always active.
  // =========================================================================
  void __not_in_flash_func(HandleMidiCC)(int32_t rawMain, int32_t unifiedX,
                                         int32_t unifiedY) {

    // --- Volume CC (always active) ---
    if (ccVolumeUpdated) {
      ccVolumeUpdated = false;
      sampleVolumeQ16 = ((int32_t)ccVolumeVal * 516);
      if (ccVolumeVal == 127)
        sampleVolumeQ16 = 65536;
      lockMain.engage(
          rawMain); // re-lock Main (volume is on Main in SELECT/Bypass)
    }

    // --- Effect select CC (always active) ---
    if (ccEffectUpdated) {
      ccEffectUpdated = false;
      if (settings.isCardLocked)
        return; // Block changes on locked cards
      int newIdx = ((int)ccEffectVal * N_FX_PROGRAMS) / 128;
      if (newIdx < 0)
        newIdx = 0;
      if (newIdx >= N_FX_PROGRAMS)
        newIdx = N_FX_PROGRAMS - 1;
      if (newIdx != currentEffectIndex) {
        currentEffectIndex = newIdx;
        currentEffect = fxPrograms[currentEffectIndex];
        if (currentEffect && currentEffect->setup)
          currentEffect->setup(currentEffect->data);
        if (currentEffect) {
          if (currentEffect->nParameters > 0 &&
              currentEffect->parameters[0].setParameter) {
            currentEffect->parameters[0].setParameter(2800,
                                                      currentEffect->data);
            currentEffect->parameters[0].rawValue = 2800;
          }
          if (currentEffect->nParameters > 1 &&
              currentEffect->parameters[1].setParameter) {
            currentEffect->parameters[1].setParameter(2048,
                                                      currentEffect->data);
            currentEffect->parameters[1].rawValue = 2048;
          }
          if (currentEffect->nParameters > 2 &&
              currentEffect->parameters[2].setParameter) {
            currentEffect->parameters[2].setParameter(2048,
                                                      currentEffect->data);
            currentEffect->parameters[2].rawValue = 2048;
          }
        }
        if (currentEffect && currentEffect->reset)
          currentEffect->reset(currentEffect->data);
        TriggerVoice(currentEffectIndex, 4800);
        synthBinaryFlashTimer = 0; // Effect changed: clear synth flash
        EngageAllLocks(rawMain, unifiedX, unifiedY);
      }
    }

    // --- Synth/source select CC (always active) ---
    if (ccSynthUpdated) {
      ccSynthUpdated = false;
      if (settings.isCardLocked)
        return;
      if (ccSynthVal == 0) {
        if (currentSource != SOURCE_EXTERNAL) {
          currentSource = SOURCE_EXTERNAL;
          SynthCore_SetExternalMode(true);
          TriggerVoice(EXTERNAL_VOICE_IDX, 4800);
          synthBinaryFlashTimer = 750;
          EngageAllLocks(rawMain, unifiedX, unifiedY);
        }
      } else {
        int modeIdx = ((int)(ccSynthVal - 1) * SYNTH_VOICE_MAP_SIZE) / 127;
        if (modeIdx < 0)
          modeIdx = 0;
        if (modeIdx >= SYNTH_VOICE_MAP_SIZE)
          modeIdx = SYNTH_VOICE_MAP_SIZE - 1;
        uint8_t newMode = SYNTH_VOICE_MAP[modeIdx].mode;
        uint8_t voiceIdx = SYNTH_VOICE_MAP[modeIdx].voiceIdx;
        if (currentSource != SOURCE_SYNTH || newMode != selectedSynthMode) {
          currentSource = SOURCE_SYNTH;
          SynthCore_SetExternalMode(false);
          selectedSynthMode = newMode;
          TriggerVoice(voiceIdx, 4800);
          SynthCore_AllNotesOff();
          ApplySynthModeDefaults(newMode);
          if (newMode == SYNTH_MODE_DRUMS) {
            SynthCore_SetDrumBase(selectedSampleIdx);
          } else if (newMode == SYNTH_MODE_SAMPLER_ONESHOT ||
                     newMode == SYNTH_MODE_SAMPLER_LOOP ||
                     newMode == SYNTH_MODE_SAMPLER_PLAYER ||
                     newMode == SYNTH_MODE_GRANULAR) {
            SetSampleForCurrentMode(selectedSampleIdx);
          }
          synthBinaryFlashTimer = 750;
          EngageAllLocks(rawMain, unifiedX, unifiedY);
        }
      }
    }

    // --- Synth parameter CCs (always active) ---
    if (ccSynthEnvUpdated) {
      ccSynthEnvUpdated = false;
      synthParameterEnv = (float)ccSynthEnvVal / 127.0f;
      lockMain.engage(rawMain);
    }
    if (ccSynthPitchUpdated) {
      ccSynthPitchUpdated = false;
      synthParameterPitch = (float)ccSynthPitchVal / 127.0f;
      lockX.engage(unifiedX);
    }
    if (ccSynthTimbreUpdated) {
      ccSynthTimbreUpdated = false;
      synthParameterTimbre = (float)ccSynthTimbreVal / 127.0f;
      lockY.engage(unifiedY);
    }
    if (ccSynthFilterUpdated) {
      ccSynthFilterUpdated = false;
      synthParameterFilterCutoff = (float)ccSynthFilterVal / 127.0f;
      lockY.engage(unifiedY); // filter cutoff shares Y lock
    }
    if (ccSynthSampleUpdated) {
      ccSynthSampleUpdated = false;
      uint32_t totalSamples = GetActiveSampleCount();
      if (totalSamples > 0) {
        int newSample = ((int)ccSynthSampleVal * (int)totalSamples) / 128;
        if (newSample >= (int)totalSamples)
          newSample = (int)totalSamples - 1;
        if (newSample != selectedSampleIdx) {
          SetSampleForCurrentMode(newSample);
          
          // Only overwrite Timbre (Y knob) and engage lock if the synth mode 
          // uses the Y knob for sample selection. Granular and Sampler Player 
          // use the Y knob for start/scan position instead!
          if (selectedSynthMode == SYNTH_MODE_SAMPLER_ONESHOT ||
              selectedSynthMode == SYNTH_MODE_SAMPLER_LOOP ||
              selectedSynthMode == SYNTH_MODE_DRUMS) {
            synthParameterTimbre = ((float)newSample + 0.5f) / (float)totalSamples;
            lockY.engage(unifiedY); // sample select shares Y lock
          }
        }
      }
    }

    // --- Effect parameter CCs (always active, independent of switch
    // position)
    // ---
    if (ccFxParam0Updated) {
      ccFxParam0Updated = false;
      int32_t ival = ((int32_t)ccFxParam0Val * 4095) / 127;
      if (currentEffect && currentEffect->nParameters > 0 &&
          currentEffect->parameters[0].setParameter) {
        currentEffect->parameters[0].setParameter((uint16_t)ival,
                                                  currentEffect->data);
        currentEffect->parameters[0].rawValue = (int16_t)ival;
      }
      lockMain.engage(rawMain);
    }
    if (ccFxParam1Updated) {
      ccFxParam1Updated = false;
      int32_t ival = ((int32_t)ccFxParam1Val * 4095) / 127;
      if (currentEffect && currentEffect->nParameters > 1 &&
          currentEffect->parameters[1].setParameter) {
        currentEffect->parameters[1].setParameter((uint16_t)ival,
                                                  currentEffect->data);
        currentEffect->parameters[1].rawValue = (int16_t)ival;
      }
      lockX.engage(unifiedX);
    }
    if (ccFxParam2Updated) {
      ccFxParam2Updated = false;
      int32_t ival = ((int32_t)ccFxParam2Val * 4095) / 127;
      if (currentEffect && currentEffect->nParameters > 2 &&
          currentEffect->parameters[2].setParameter) {
        currentEffect->parameters[2].setParameter((uint16_t)ival,
                                                  currentEffect->data);
        currentEffect->parameters[2].rawValue = (int16_t)ival;
      }
      lockY.engage(unifiedY);
    }
    // Performance CC handling — directly updates the same fields used by
    // HandleExtra
    if (ccPerfMainUpdated) {
      ccPerfMainUpdated = false;
      // Main CC always sets BPM regardless of bpmEnabled
      uint16_t bpm = 20 + ((uint32_t)ccPerfMainVal * 236) / 127;
      settings.bpm = bpm & 0x7F;
      settings.bpmHi = (bpm >> 7) & 0x7F;
    }
    if (ccPerfXUpdated) {
      ccPerfXUpdated = false;
      uint8_t val = ccPerfXVal;
      if (settings.pulse1Mode == 10) {
        settings.cv1Arg = val; // Grids X position
      } else if (settings.cv1Mode == 9 || settings.cv1Mode == 8 ||
                 settings.cv1Mode == 10) {
        settings.cv1Arg = val; // Gen Seq spice / LFO speed / CV Trigger length
      } else if (settings.cv1Mode == 5) {
        settings.sequencerLength = 1 + (((uint32_t)val * 15) + 63) / 127; // Step seq length
      } else {
        settings.cv1Arg =
            val; // All other CV1 modes: arg is the general parameter
      }
      // Also update Pulse 1 Bernoulli/probabilistic probability
      if (settings.pulse1Mode == 8 || settings.pulse1Mode == 3) {
        settings.pulse1Arg = val;
      }
    }
    if (ccPerfYUpdated) {
      ccPerfYUpdated = false;
      uint8_t val = ccPerfYVal;
      if (settings.pulse1Mode == 10) {
        settings.cv2Arg = val; // Grids Y
      } else if (settings.cv2Mode == 9 || settings.cv2Mode == 8 ||
                 settings.cv2Mode == 10) {
        settings.cv2Arg = val;
      } else if (settings.cv2Mode == 5) {
        settings.sequencerLength2 = 1 + (((uint32_t)val * 15) + 63) / 127;
      }
      // Also update Pulse 2 Bernoulli/probabilistic probability
      if (settings.pulse2Mode == 9 || settings.pulse2Mode == 3) {
        settings.pulse2Arg = val;
      }
    }
  }

  // Menu State Handlers
  // =========================================================================

  // Switch UP + source=SYNTH → edit synth params
  // Switch UP + source=EXT  → freeze effect
  void __not_in_flash_func(HandleSynthEdit)(int32_t rawMain, int32_t unifiedX,
                                            int32_t unifiedY) {
    if (currentSource == SOURCE_EXTERNAL) {
      // Freeze: set any "Freeze" parameter to 4095
      if (currentEffect) {
        for (int i = 0; i < currentEffect->nParameters; i++) {
          const char *name = currentEffect->parameters[i].name;
          if (name && name[0] == 'F' && name[1] == 'r' && name[2] == 'e' &&
              name[3] == 'e' && name[4] == 'z' && name[5] == 'e') {
            if (currentEffect->parameters[i].setParameter)
              currentEffect->parameters[i].setParameter(4095,
                                                        currentEffect->data);
          }
        }
      }
      HandleEffectEdit(rawMain, unifiedX, unifiedY);
      return;
    }

    // Synth source: edit synth parameters
    if (selectedSynthMode == SYNTH_MODE_SAMPLER_PLAYER) {
      // Main → Length/Direction (bipolar), X → Pitch, Y → Start Point
      if (lockMain.update(rawMain)) {
        pageSynthEdit.main = rawMain;
        synthParameterEnv = (rawMain / 2048.0f) - 1.0f;
      }
      if (lockX.update(unifiedX)) {
        pageSynthEdit.x = unifiedX;
        static float smoothedPitch = 0.5f;
        static int32_t lastStableX = 0;
        if (abs(unifiedX - lastStableX) > 4)
          lastStableX = unifiedX;
        float target = lastStableX / 4095.0f;
        smoothedPitch = smoothedPitch * 0.9f + target * 0.1f;
        synthParameterPitch = smoothedPitch;
      }
      if (lockY.update(unifiedY)) {
        pageSynthEdit.y = unifiedY;
        synthParameterTimbre = unifiedY / 4095.0f;
      }
    } else {
      // Standard: Main → Env/Decay, X → Pitch, Y → Timbre
      // Exception: Noise uses Y → Filter Cutoff instead of Timbre
      if (lockMain.update(rawMain)) {
        pageSynthEdit.main = rawMain;
        synthParameterEnv = rawMain / 4095.0f;
      }
      if (lockX.update(unifiedX)) {
        pageSynthEdit.x = unifiedX;
        static float smoothedPitch = 0.5f;
        static int32_t lastStableX = 0;
        if (abs(unifiedX - lastStableX) > 4)
          lastStableX = unifiedX;
        float target = lastStableX / 4095.0f;
        smoothedPitch = smoothedPitch * 0.9f + target * 0.1f;
        synthParameterPitch = smoothedPitch;
      }
      if (lockY.update(unifiedY)) {
        pageSynthEdit.y = unifiedY;
        if (selectedSynthMode == SYNTH_MODE_NOISE) {
          // Noise: Y controls filter cutoff (more musical than timbre)
          synthParameterFilterCutoff = unifiedY / 4095.0f;
        } else {
          synthParameterTimbre = unifiedY / 4095.0f;
        }
      }
    }
  }

  // Switch MIDDLE (normal effects) → edit effect params
  void __not_in_flash_func(HandleEffectEdit)(int32_t rawMain, int32_t unifiedX,
                                             int32_t unifiedY) {
    if (!currentEffect)
      return;

    if (lockMain.update(rawMain)) {
      if (currentEffectIndex == 0) {
        // Bypass: Main knob = output volume
        sampleVolumeQ16 = 6554 + ((rawMain * 14) + (rawMain >> 1));
        if (sampleVolumeQ16 > 65536)
          sampleVolumeQ16 = 65536;
      } else if (currentEffect && currentEffect->nParameters > 0 &&
                 currentEffect->parameters[0].setParameter) {
        currentEffect->parameters[0].setParameter((uint16_t)rawMain,
                                                  currentEffect->data);
        currentEffect->parameters[0].rawValue = (int16_t)rawMain;
        pageEffectEdit.main = rawMain;
      }
    }
    if (lockX.update(unifiedX) && currentEffect->nParameters > 1 &&
        currentEffect->parameters[1].setParameter) {
      currentEffect->parameters[1].setParameter((uint16_t)unifiedX,
                                                currentEffect->data);
      // Store raw knob (not unified) so the Web UI slider reflects physical knob
      // position; the CV offset is broadcast separately as a modulation hint.
      currentEffect->parameters[1].rawValue = (int16_t)rawX;
      pageEffectEdit.x = unifiedX;
    }
    if (lockY.update(unifiedY) && currentEffect->nParameters > 2 &&
        currentEffect->parameters[2].setParameter) {
      currentEffect->parameters[2].setParameter((uint16_t)unifiedY,
                                                currentEffect->data);
      currentEffect->parameters[2].rawValue = (int16_t)rawY;
      pageEffectEdit.y = unifiedY;
    }
  }

  // =========================================================================
  // HandleSelect — Switch DOWN: effect select (Main), synth select (X), Z param
  // (Y) Special case: Bypass (effect 0) uses Main for Volume instead of effect
  // select.
  // =========================================================================
  void __not_in_flash_func(HandleSelect)(int32_t rawMain, int32_t unifiedX,
                                         int32_t unifiedY) {
    // Main: Effect selection with hysteresis
    if (lockMain.update(rawMain)) {
      pageSelect.main = rawMain;
      if (!settings.isCardLocked) {
        int32_t step = 4096 / N_FX_PROGRAMS;
        int32_t hyst = 25;
        int rawIndex = (rawMain * N_FX_PROGRAMS) / 4096;
        if (rawIndex < 0)
          rawIndex = 0;
        if (rawIndex >= N_FX_PROGRAMS)
          rawIndex = N_FX_PROGRAMS - 1;
        if (rawIndex != currentEffectIndex) {
          bool transition = false;
          if (rawIndex > currentEffectIndex) {
            if (rawMain > (rawIndex * step) + hyst)
              transition = true;
          } else {
            if (rawMain < (currentEffectIndex * step) - hyst)
              transition = true;
          }
          if (rawMain == 0 || rawMain > 4090)
            transition = true;
          if (transition) {
            currentEffectIndex = rawIndex;
            TriggerVoice(currentEffectIndex, 12000);
            synthBinaryFlashTimer = 0; // effect changed: clear synth flash
            EngageAllLocks(rawMain, unifiedX, unifiedY);
          }
        }
      }
    }

    // X knob → synth/source select
    if (lockX.update(unifiedX) && !settings.isCardLocked) {
      pageSelect.x = unifiedX;
      if (unifiedX < 600) {
        // External input
        if (currentSource != SOURCE_EXTERNAL) {
          currentSource = SOURCE_EXTERNAL;
          SynthCore_SetExternalMode(true);
          TriggerVoice(EXTERNAL_VOICE_IDX, 4800);
          synthBinaryFlashTimer = 750; // flash synth binary for ~1 second
          EngageAllLocks(rawMain, unifiedX, unifiedY);
        }
      } else {
        // Synth mode select
        uint8_t voiceIdx = 0;
        uint8_t newMode = KnobToSynthMode(unifiedX, &voiceIdx);
        bool changed =
            (currentSource != SOURCE_SYNTH) || (newMode != selectedSynthMode);
        if (changed) {
          currentSource = SOURCE_SYNTH;
          SynthCore_SetExternalMode(false);
          selectedSynthMode = newMode;
          TriggerVoice(voiceIdx, 4800);
          SynthCore_AllNotesOff();
          ApplySynthModeDefaults(newMode);
          if (newMode == SYNTH_MODE_DRUMS) {
            SynthCore_SetDrumBase(selectedSampleIdx);
          } else if (newMode == SYNTH_MODE_SAMPLER_ONESHOT ||
                     newMode == SYNTH_MODE_SAMPLER_LOOP ||
                     newMode == SYNTH_MODE_SAMPLER_PLAYER ||
                     newMode == SYNTH_MODE_GRANULAR) {
            SetSampleForCurrentMode(selectedSampleIdx);
          }
          synthBinaryFlashTimer = 750;

          if (newMode == SYNTH_MODE_SAMPLER_PLAYER) {
            synthParameterEnv = (pageSynthEdit.main / 2048.0f) - 1.0f;
          } else {
            synthParameterEnv = pageSynthEdit.main / 4095.0f;
          }
          synthParameterPitch = pageSynthEdit.x / 4095.0f;
          if (newMode == SYNTH_MODE_NOISE) {
            synthParameterFilterCutoff = pageSynthEdit.y / 4095.0f;
          } else {
            synthParameterTimbre = pageSynthEdit.y / 4095.0f;
          }

          EngageAllLocks(rawMain, unifiedX, unifiedY);
        }
      }
    }

    // Y → Z param (filter cutoff for most synths; sample select for samplers)
    HandleZParam(unifiedY);
  }

  // HandleZParam — Y knob on SELECT page: filter cutoff or sample select
  void __not_in_flash_func(HandleZParam)(int32_t unifiedY) {
    if (currentSource != SOURCE_SYNTH)
      return;

    bool isSamplerMode = (selectedSynthMode == SYNTH_MODE_SAMPLER_PLAYER ||
                          selectedSynthMode == SYNTH_MODE_SAMPLER_ONESHOT ||
                          selectedSynthMode == SYNTH_MODE_SAMPLER_LOOP ||
                          selectedSynthMode == SYNTH_MODE_GRANULAR);

    if (isSamplerMode) {
      // Y → sample select (no preview: AllNotesOff prevents stale playback)
      if (lockY.update(unifiedY)) {
        pageSelect.y = unifiedY;
        uint32_t totalSamples = GetActiveSampleCount();
        int newSample = (unifiedY * (int)totalSamples) / 4096;
        if (newSample >= (int)totalSamples)
          newSample = (int)totalSamples - 1;
        if (newSample != selectedSampleIdx) {
          SynthCore_AllNotesOff(); // Stop current playback before swapping
                                   // sample
          SetSampleForCurrentMode(newSample);
        }
      }
    } else {
      // Y → filter cutoff (Z param)
      if (lockY.update(unifiedY)) {
        pageSelect.y = unifiedY;
        synthParameterFilterCutoff = (float)unifiedY / 4095.0f;
      }
    }
  }

  // HandleExtra — Switch DOWN (double-tap + hold): EXTRA performance page.
  // Main knob → BPM (if internal BPM active).
  // X knob    → CV1 perf param + Pulse 1 Bernoulli probability (shared
  // control). Y knob    → CV2 perf param + Pulse 2 probability. Writes directly
  // to the relevant settings fields so save/load and web UI sync work.
  void __not_in_flash_func(HandleExtra)(int32_t rawMain, int32_t unifiedX,
                                        int32_t unifiedY) {
    // ── Main knob: BPM only ──
    if (lockMain.update(rawMain)) {
      if (settings.bpmEnabled) {
        uint16_t bpm = 20 + ((uint32_t)rawMain * 236) / 4095;
        settings.bpm = bpm & 0x7F;
        settings.bpmHi = bpm >> 7;
      }
    }

    // ── X knob: CV1 perf param ──
    if (lockX.update(unifiedX)) {
      uint8_t val = (uint8_t)(((uint32_t)unifiedX * 127) / 4095);
      if (settings.pulse1Mode == 10) {
        settings.cv1Arg = val; // Grids X
      } else if (settings.cv1Mode == 9 || settings.cv1Mode == 8) {
        settings.cv1Arg = val; // Gen Seq spice / LFO speed
      } else if (settings.cv1Mode == 5) {
        settings.sequencerLength = 1 + (((uint32_t)val * 15) + 63) / 127;
      } else if (settings.cv1Mode == 7) {
        settings.eg1Env = val; // Envelope control
      }
    }

    // ── Y knob: CV2 perf param ──
    if (lockY.update(unifiedY)) {
      uint8_t val = (uint8_t)(((uint32_t)unifiedY * 127) / 4095);
      if (settings.pulse1Mode == 10) {
        settings.cv2Arg = val; // Grids Y
      } else if (settings.cv2Mode == 9 || settings.cv2Mode == 8) {
        settings.cv2Arg = val;
      } else if (settings.cv2Mode == 5) {
        settings.sequencerLength2 = 1 + (((uint32_t)val * 15) + 63) / 127;
      } else if (settings.cv2Mode == 7) {
        settings.eg2Env = val; // Envelope control
      }
    }
  }

  // =========================================================================
  // QuantizePitchToNote helper
  // =========================================================================
  static inline uint8_t QuantizePitchToNote(float pitch, uint8_t scaleId,
                                            uint8_t rootId) {
    if (scaleId > 0 && scaleId < 8) {
      uint16_t mask = 0xFFF;
      switch (scaleId) {
      case 2:
        mask = 0xAB5;
        break;
      case 3:
        mask = 0x5AD;
        break;
      case 4:
        mask = 0x295;
        break;
      case 5:
        mask = 0x4A9;
        break;
      case 6:
        mask = 0x555;
        break;
      case 7:
        mask = 0x492;
        break;
      }
      int32_t rawSemi = (int32_t)(pitch * 24.0f + (pitch >= 0.0f ? 0.5f : -0.5f));
      int octave = rawSemi / 12;
      int noteNum = rawSemi % 12;
      if (noteNum < 0) {
        noteNum += 12;
        octave--;
      }
      int rootChroma = rootId % 12;
      for (int k = 0; k < 12; k++) {
        int interval = (noteNum - rootChroma);
        if (interval < 0)
          interval += 12;
        if ((mask >> interval) & 1)
          break;
        noteNum--;
        if (noteNum < 0) {
          noteNum = 11;
          octave--;
        }
      }
      return 60 + (octave * 12) + noteNum;
    }
    return 60 + (int32_t)(pitch * 24.0f + (pitch >= 0.0f ? 0.5f : -0.5f));
  }

  // =========================================================================
  // QuantizeSeqStepToNote — Maps 0..127 amplitude to scaled pitch
  // =========================================================================
  static inline uint8_t QuantizeSeqStepToNote(uint8_t rawStep, uint8_t scaleMode, uint8_t rootId, uint8_t oct, uint8_t octRange) {
    if (scaleMode >= 5) return rawStep; // 5=voltage, 6=trigger bypass quantizer

    int baseOffset = (oct + 1) * 12 + rootId;
    int sOctRange = octRange * 12;
    int absNote = baseOffset + ((rawStep * sOctRange + 63) / 127);

    if (absNote < 0) absNote = 0;
    if (absNote > 127) absNote = 127;

    if (scaleMode != 0) { // Not Chromatic
      uint16_t mask = 0;
      switch (scaleMode) {
      case 1: mask = 0xAB5; break; // Major
      case 2: mask = 0x5AD; break; // Minor
      case 3: mask = 0x295; break; // Penta Maj
      case 4: mask = 0x4A9; break; // Penta Min
      }

      int noteNum = absNote % 12;
      int octave = absNote / 12;
      int rootChroma = rootId % 12;

      for (int k = 0; k < 12; k++) {
        int interval = (noteNum - rootChroma);
        if (interval < 0) interval += 12;
        if ((mask >> interval) & 1) break;
        noteNum--;
        if (noteNum < 0) {
          noteNum = 11;
          octave--;
        }
      }
      absNote = (octave * 12) + noteNum;
      if (absNote < 0) absNote = 0;
      if (absNote > 127) absNote = 127;
    }
    return (uint8_t)absNote;
  }

  // =========================================================================
  // ApplySynthParams — assemble SynthParams and send to Core 1
  // =========================================================================

  void __not_in_flash_func(ApplySynthParams)(int32_t unifiedX, int32_t unifiedY,
                                             int32_t rawMain) {
    SynthParams p = {0};

    // Gate from pulse inputs — delayed by 1 control tick so that the CV
    // output (updated in HandleOutputs, which runs first) has time to
    // settle through the analog path before the synth triggers.  Without
    // this delay, Pulse-In-driven sequencer steps produce a 1-tick pitch
    // glitch because the AudioIn ADC still holds the previous CV value
    // at the instant of gate rise.
    static bool delayedP1 = false, delayedP2 = false;
    bool curP1 = Connected(Pulse1) ? PulseIn(0) : false;
    bool curP2 = Connected(Pulse2) ? PulseIn(1) : false;
    p.gate = delayedP1; // Pulse 2 exclusively used for mod/clock now per user
                        // request
    delayedP1 = curP1;
    delayedP2 = curP2;

    // Drum mode: pulse/audio inputs → per-drum triggers
    if (selectedSynthMode == SYNTH_MODE_DRUMS) {
      static bool lastP1 = false, lastP2 = false, lastA1 = false,
                  lastA2 = false;
                  
      // Calculate kit offset based on Y knob (synthParameterTimbre)
      uint32_t sampleCount = SynthCore_GetCustomSampleCount();
      int availableSamples = (int)sampleCount - selectedSampleIdx;
      int maxKits = availableSamples / 4;
      if (maxKits < 1) maxKits = 1;
      int kitIndex = (int)(synthParameterTimbre * (maxKits - 0.001f));
      int newBase = selectedSampleIdx + (kitIndex * 4);
      
      static int lastDrumBase = -1;
      if (newBase != lastDrumBase) {
          SynthCore_SetDrumBase(newBase);
          lastDrumBase = newBase;
      }

      bool p1 = delayedP1;
      bool p2 = delayedP2;
      if (p1 && !lastP1)
        SynthCore_TriggerNote(36, 127); // Sample 0 (BD)
      lastP1 = p1;
      if (p2 && !lastP2)
        SynthCore_TriggerNote(39, 127); // Sample 3 (OH)
      lastP2 = p2;

      bool t1 = lastA1;
      if (fastTrigA1)
        t1 = true;
      else if (AudioIn(0) < 200)
        t1 = false;
      if (t1 && !lastA1) {
        int vel = (peakTrigA1 * 127) / 2047;
        if (vel < 1) vel = 1;
        if (vel > 127) vel = 127;
        SynthCore_TriggerNote(37, vel); // Sample 1 (SD)
        peakTrigA1 = 0;
      }
      lastA1 = t1;
      if (!t1) peakTrigA1 = 0;
      fastTrigA1 = false;

      bool t2 = lastA2;
      if (fastTrigA2)
        t2 = true;
      else if (AudioIn(1) < 200)
        t2 = false;
      if (t2 && !lastA2) {
        int vel = (peakTrigA2 * 127) / 2047;
        if (vel < 1) vel = 1;
        if (vel > 127) vel = 127;
        SynthCore_TriggerNote(38, vel); // Sample 2 (CH)
        peakTrigA2 = 0;
      }
      lastA2 = t2;
      if (!t2) peakTrigA2 = 0;
      fastTrigA2 = false;

      p.gate = false; // drums handle their own triggers
    }

    // Drum Synth mode: synthesized BD/SD/OH/CH, same input routing as Drums
    if (selectedSynthMode == SYNTH_MODE_DRUM_SYNTH) {
      static bool lastP1ds = false, lastP2ds = false, lastA1ds = false,
                  lastA2ds = false;

      bool p1 = delayedP1;
      bool p2 = delayedP2;
      if (p1 && !lastP1ds)
        SynthCore_TriggerNote(36, 127); // BD
      lastP1ds = p1;
      if (p2 && !lastP2ds)
        SynthCore_TriggerNote(46, 127); // OH
      lastP2ds = p2;

      // Audio 1 → Snare Drum (SD), velocity from peak
      bool t1 = lastA1ds;
      if (fastTrigA1)
        t1 = true;
      else if (AudioIn(0) < 200)
        t1 = false;
      if (t1 && !lastA1ds) {
        int vel = (peakTrigA1 * 127) / 2047;
        if (vel < 1) vel = 1;
        if (vel > 127) vel = 127;
        SynthCore_TriggerNote(38, vel); // SD
        peakTrigA1 = 0;
      }
      lastA1ds = t1;
      if (!t1) peakTrigA1 = 0;
      fastTrigA1 = false;

      // Audio 2 → Closed Hat (CH), velocity from peak
      bool t2 = lastA2ds;
      if (fastTrigA2)
        t2 = true;
      else if (AudioIn(1) < 200)
        t2 = false;
      if (t2 && !lastA2ds) {
        int vel = (peakTrigA2 * 127) / 2047;
        if (vel < 1) vel = 1;
        if (vel > 127) vel = 127;
        SynthCore_TriggerNote(42, vel); // CH
        peakTrigA2 = 0;
      }
      lastA2ds = t2;
      if (!t2) peakTrigA2 = 0;
      fastTrigA2 = false;

      p.gate = false; // drum synth handles its own triggers
    }

    static bool lastGateState = false;
    bool gateRise = p.gate && !lastGateState;
    lastGateState = p.gate;

    // Calculate polyphony first so we know whether to suppress mono static
    // triggers
    bool isPolyphonic = false;

    // Unquantized mode is legacy fallback when audioInScale == 0 and audio2Mode
    // != 2 If scale is 0, we only enable polyphony if audio2Mode specifically
    // requests Pitch 2
    if (settings.audioInScale > 0)
      isPolyphonic = true;
    if (settings.audio2Mode == 2)
      isPolyphonic = true;

    // Trigger sampler on gate rise
    if (gateRise && !isPolyphonic &&
        (selectedSynthMode == SYNTH_MODE_SAMPLER_ONESHOT ||
         selectedSynthMode == SYNTH_MODE_SAMPLER_LOOP ||
         selectedSynthMode == SYNTH_MODE_GRANULAR)) {
      SynthCore_TriggerNote(60, 127);
    }

    if (currentSource == SOURCE_SYNTH) {
      // CV/Audio modulation
      int32_t rawL = Connected(Audio1) ? AudioIn(0) : 0;
      int32_t rawR = Connected(Audio2) ? AudioIn(1) : 0;
      float rawModPitch = (float)rawL / 2048.0f;
      float modPitch = rawModPitch;
      float modTimbre = (float)rawR / 2048.0f;
      float cvOffset = modPitch * 3.0f;
      float cvOffset2 = modTimbre * 3.0f; // For secondary Pitch 2 mode

      static bool lastPulse1 = false;
      static bool lastPulse2 = false;
      static uint8_t activeNote1 = 0;
      static uint8_t activeNote2 = 0;
      bool curP1 = delayedP1;
      bool curP2 = delayedP2;

      if (selectedSynthMode == SYNTH_MODE_SAMPLER_PLAYER) {
        p.pitch = synthParameterPitch + cvOffset;
        p.timbre = synthParameterTimbre + modTimbre;
        p.envelope = synthParameterEnv;
      } else if (selectedSynthMode == SYNTH_MODE_DRUMS ||
                 selectedSynthMode == SYNTH_MODE_DRUM_SYNTH) {
        p.pitch = synthParameterPitch;
        p.timbre = synthParameterTimbre;
        p.envelope = synthParameterEnv;
      } else {
        float sp = synthParameterPitch;
        if (sp < 0.05f)
          sp = 0.0f;
        if (sp > 0.95f)
          sp = 1.0f;
        synthParameterPitch = sp;

        float rangeFactor = (float)settings.pitchRangeSemis / 12.0f;
        float knobOffset = (sp - 0.5f) * rangeFactor;
        float rootOffset = (float)((int)settings.rootNote - 60) / 24.0f;

        float totalPitch1 = knobOffset + rootOffset + cvOffset;
        float totalPitch2 = knobOffset + rootOffset + cvOffset2;

        // Unquantized mode is legacy fallback when audioInScale == 0 and
        // audio2Mode != 2 If scale is 0, we only enable polyphony if audio2Mode
        // specifically requests Pitch 2
        // This logic was moved up before the gateRise check.
        // if (settings.audioInScale > 0)
        //   isPolyphonic = true;
        // if (settings.audio2Mode == 2)
        //   isPolyphonic = true;

        if (isPolyphonic) {
          uint8_t a1Note = QuantizePitchToNote(
              totalPitch1, settings.audioInScale, settings.rootNote);
          uint8_t a2Note = QuantizePitchToNote(
              totalPitch2, settings.audioInScale, settings.rootNote);

          p.pitch = 0.5f; // Neutralize global sweep so SynthCore doesn't bend
                          // the discrete notes
          p.gate = false; // Disable global gate trigger

          // Pulse -> Note Tracker (Skip in DRUMS mode as it uses independent
          // triggers)
          if (selectedSynthMode != SYNTH_MODE_DRUMS &&
              selectedSynthMode != SYNTH_MODE_DRUM_SYNTH) {
            // Pulse 1 -> Note 1 Tracker
            if (curP1 && !lastPulse1) {
              SynthCore_TriggerNote(a1Note, 127);
              activeNote1 = a1Note;
            } else if (!curP1 && lastPulse1) {
              SynthCore_ReleaseNote(activeNote1);
            }

            // Pulse 2 -> Note 2 Tracker (Only if Audio2Mode is Polyphony)
            if (settings.audio2Mode == 2) {
              if (curP2 && !lastPulse2) {
                SynthCore_TriggerNote(a2Note, 127);
                activeNote2 = a2Note;
              } else if (!curP2 && lastPulse2) {
                SynthCore_ReleaseNote(activeNote2);
              }
            }
          }
          // In quantized mode, gate is handled exclusively via
          // TriggerNote/ReleaseNote. Do NOT set p.gate here — it would cause a
          // second on/off drone voice.
        } else {
          // Unquantized Monophonic Legacy
          p.pitch = 0.5f + totalPitch1;
          // Skip global gate for modes that handle their own triggers
          if (selectedSynthMode != SYNTH_MODE_DRUMS &&
              selectedSynthMode != SYNTH_MODE_DRUM_SYNTH &&
              selectedSynthMode != SYNTH_MODE_SAMPLER_ONESHOT &&
              selectedSynthMode != SYNTH_MODE_SAMPLER_LOOP &&
              selectedSynthMode != SYNTH_MODE_GRANULAR) {
            p.gate = curP1;
          } else {
            p.gate = false;
          }
        }

        // S&H on continuous total pitch
        static float latchedTotalPitch = 0.5f;
        if (settings.audioInHold) {
          if (gateRise)
            latchedTotalPitch = p.pitch;
          p.pitch = latchedTotalPitch;
        } else {
          latchedTotalPitch = p.pitch;
        }

        float selectedModTimbre = 0.0f;
        if (settings.audio2Mode == 0)
          selectedModTimbre =
              modTimbre; // Only modulate Timbre if Mode is 0 (Synth Y)
        p.timbre = synthParameterTimbre + (selectedModTimbre * 0.5f);
        p.envelope = synthParameterEnv;
      }
      lastPulse1 = curP1;
      lastPulse2 = curP2;

      p.filterCutoff = synthParameterFilterCutoff;
      p.mode = selectedSynthMode;

      if (p.pitch < -2.0f)
        p.pitch = -2.0f;
      if (p.pitch > 3.0f)
        p.pitch = 3.0f;
      if (p.timbre < 0.0f)
        p.timbre = 0.0f;
      if (p.timbre > 1.0f)
        p.timbre = 1.0f;
      if (p.filterCutoff < 0.0f)
        p.filterCutoff = 0.0f;
      if (p.filterCutoff > 1.0f)
        p.filterCutoff = 1.0f;

      // Sample selection via Timbre parameter (OneShot/Loop)
      {
        bool isSampleSelectMode =
            (selectedSynthMode == SYNTH_MODE_SAMPLER_ONESHOT ||
             selectedSynthMode == SYNTH_MODE_SAMPLER_LOOP ||
             selectedSynthMode == SYNTH_MODE_DRUMS);
        if (isSampleSelectMode) {
          float selectVal = p.timbre;
          if (selectVal < 0)
            selectVal = 0;
          if (selectVal > 0.999f)
            selectVal = 0.999f;
          uint32_t totalSamples = GetActiveSampleCount();
          if (totalSamples > 0) {
            float window = 1.0f / totalSamples;
            float hyst = window * 0.15f;
            int current = selectedSampleIdx;
            int target = (int)(selectVal * totalSamples);
            int newSample = current;
            if (selectVal < (current * window) - hyst)
              newSample = target;
            else if (selectVal > ((current + 1) * window) + hyst)
              newSample = target;
            if (newSample != current) {
              if (newSample < 0)
                newSample = 0;
              if (newSample >= (int)totalSamples)
                newSample = (int)totalSamples - 1;
              SetSampleForCurrentMode(newSample);
            }
          }
        }
      }
    } else {
      // External source: pass through synth params unchanged
      p.pitch = synthParameterPitch;
      p.timbre = synthParameterTimbre;
      p.envelope = synthParameterEnv;
      p.filterCutoff = synthParameterFilterCutoff;
      p.mode = selectedSynthMode;
      p.volume = 1.0f;
    }

    p.volume = (float)sampleVolumeQ16 / 65536.0f;
    SynthCore_UpdateParams(&p);
  }

  // =========================================================================
  // HandleOutputs — Pulse & CV outputs (runs at control rate ~750 Hz)
  //
  // CV Modes:    0=MIDI Pitch, 1=MIDI Vel, 2=MIDI CC, 3=Synth Env,
  //              4=Random S&H, 5=Step Seq, 6=Voice Audio, 7=Internal EG
  // Pulse Modes: 0=MIDI Gate, 1=MIDI Trigger, 2=Clock Out,
  //              3=Probabilistic, 4=Pass-through, 5=Voice Audio
  // Clock Sources: 0=MIDI, 1=Internal BPM, 2=Pulse1In, 3=Pulse2In
  // =========================================================================
  void __not_in_flash_func(HandleOutputs)() {
    VoiceInfo v0 = {0};
    SynthCore_GetVoiceInfo(0, &v0);

    // =========================================================================
    // UNIFIED CLOCK / PULSE SUBSYSTEM
    // =========================================================================
    //
    // Clock sources:  0=MIDI, 1=Internal BPM, 2=Pulse1In, 3=Pulse2In
    // Clock dividers: 0=/1(quarter,default) 1=/2 2=/4 3=×2 4=×3 5=×4 6=×6 7=×8
    // 8=/8
    //
    // Design: compute shared beatTick[4] + beatRise[4] ONCE per control loop.
    // All consumers (handlePulse, Seq A/B, EG) read these instead of
    // re-doing PPQN math independently — eliminates inconsistencies.
    // =========================================================================

    // ---- Persistent state ----
    static uint8_t seqStep[2] = {0, 0};
    static uint8_t genStep[2] = {0, 0};
    static uint16_t randomVal[2] = {0, 0};
    static int16_t p1TrigTimer = 0;
    static int16_t p2TrigTimer = 0;
    const int16_t TRIG_LEN = 8; // ~11ms at 750 Hz

    static bool lastP1Out = false;
    static bool lastP2Out = false;

    // ---- Internal BPM clock ----
    // Increment bpmAccum each control loop.
    // Overflow at (1<<20) = 1/24th of a beat.
    // Math: inc = bpm * 24 * (1<<20) / (750Hz * 60s) = bpm * 559.24.
    // We approximate to 559 to eliminate expensive hardware division in RAM.
    static uint32_t bpmAccum = 0;
    static uint32_t internalClockTick = 0;
    {
      // Internal BPM accumulator. Runs unconditionally so any output that
      // selects Internal BPM clock has it available immediately. Formula: inc =
      // bpm * 24 * (1<<20) / (controlHz * 60) Actual control rate ~375 Hz → inc
      // = bpm * 1118  (NOT 559 which assumed 750 Hz)
      uint32_t currentBpm = settings.bpm | (settings.bpmHi << 7);
      if (currentBpm < 20)
        currentBpm = 20;
      bpmAccum +=
          currentBpm * 1118u; // corrected for actual ~375 Hz control rate
      if (bpmAccum >= (1u << 20)) {
        bpmAccum -= (1u << 20);
        internalClockTick++;
      }
    }

    // ---- Pulse inputs: edge detection + tick counters ----
    static bool lastP1InState = false;
    static bool lastP2InState = false;
    static uint32_t p1InTick = 0;
    static uint32_t p2InTick = 0;

    bool p1In = Connected(Pulse1) ? PulseIn(0) : false;
    bool p2In = Connected(Pulse2) ? PulseIn(1) : false;
    bool p1InRise = p1In && !lastP1InState;
    bool p2InRise = p2In && !lastP2InState;
    lastP1InState = p1In;
    lastP2InState = p2In;
    if (p1InRise)
      p1InTick++;
    if (p2InRise)
      p2InTick++;

    // ---- MIDI gate ----
    static bool lastGateMidi = false;
    bool gateRiseMidi = globalMidiGate && !lastGateMidi;
    lastGateMidi = globalMidiGate;

    // =========================================================================
    // 24 PPQN CLOCK GENERATION (Software PLL for Analog Inputs)
    // beatTick24[src] — monotonically increasing 24 PPQN tick counter
    // src: 0=MIDI, 1=BPM, 2=Pulse1In, 3=Pulse2In
    // =========================================================================
    static uint32_t prevBeatTick24[4] = {0, 0, 0, 0};
    uint32_t beatTick24[4];

    // 0: MIDI is already natively 24 PPQN
    beatTick24[0] = midiClockTick;

    // 1: BPM Internal (Internal Clock Tick is now 24 PPQN)
    beatTick24[1] = internalClockTick;

    // 2 & 3: Pulse Inputs (Analog Clock)
    // Directly advance by one beat (24 ticks) on every physical pulse.
    // This perfectly matches the 1:1 hardware trigger logic used for the synth
    // and bypasses the previous debounce/PLL which rejected fast clocks.
    static uint32_t analogTicks[2] = {0, 0};
    static uint32_t pLastRiseTime[2] = {0, 0};
    static uint32_t pInterval[2] = {0, 0};
    static uint32_t p1InLoopsSince = 0;
    static uint32_t p2InLoopsSince = 0;

    uint32_t ticksPerPulse1 =
        24 / (settings.pulse1PPQN > 0 ? settings.pulse1PPQN : 1);
    uint32_t ticksPerPulse2 =
        24 / (settings.pulse2PPQN > 0 ? settings.pulse2PPQN : 1);
    uint32_t nowUs = time_us_32();

    if (p1InRise) {
      if (pLastRiseTime[0] > 0)
        pInterval[0] = nowUs - pLastRiseTime[0];
      pLastRiseTime[0] = nowUs;
      analogTicks[0] += ticksPerPulse1; // absolute anchor
      p1InLoopsSince = 0;
      beatTick24[2] = analogTicks[0];
    } else {
      p1InLoopsSince++;
      beatTick24[2] = analogTicks[0];
      if (pInterval[0] > 0 && ticksPerPulse1 > 1) {
        uint32_t elapsed = nowUs - pLastRiseTime[0];
        if (elapsed < pInterval[0]) {
          beatTick24[2] += (elapsed * ticksPerPulse1) / pInterval[0];
        } else {
          beatTick24[2] += (ticksPerPulse1 - 1);
        }
      }
    }

    if (p2InRise) {
      if (pLastRiseTime[1] > 0)
        pInterval[1] = nowUs - pLastRiseTime[1];
      pLastRiseTime[1] = nowUs;
      analogTicks[1] += ticksPerPulse2;
      p2InLoopsSince = 0;
      beatTick24[3] = analogTicks[1];
    } else {
      p2InLoopsSince++;
      beatTick24[3] = analogTicks[1];
      if (pInterval[1] > 0 && ticksPerPulse2 > 1) {
        uint32_t elapsed = nowUs - pLastRiseTime[1];
        if (elapsed < pInterval[1]) {
          beatTick24[3] += (elapsed * ticksPerPulse2) / pInterval[1];
        } else {
          beatTick24[3] += (ticksPerPulse2 - 1);
        }
      }
    }

    bool running[4];
    running[0] = midiClockRunning;
    running[1] = (settings.bpmEnabled != 0);

    // running[2/3] uses a timeout: if no rising edge for ~2s (~1500 loops),
    // mark Pulse In as not running so stale clock state doesn't persist.
    running[2] = (p1InTick > 0) && (p1InLoopsSince < 1500);
    running[3] = (p2InTick > 0) && (p2InLoopsSince < 1500);

    bool beatRise[4]; // Standard Quarter Note Beat Rise (for EG/Legacy)
    for (int s = 0; s < 4; s++) {
      // A standard "beat" is every 24 PPQN ticks
      uint32_t currentBeat = beatTick24[s] / 24;
      uint32_t prevBeat = prevBeatTick24[s] / 24;
      beatRise[s] = running[s] && (currentBeat > prevBeat);
      prevBeatTick24[s] = beatTick24[s];
    }

    // =========================================================================
    // CLOCK DIVIDER / MULTIPLIER (from 24 PPQN)
    // div: 0=/1  1=/2  2=/4  3=×2  4=×3  5=×4  6=×6  7=×8  8=/8 (2 bars)
    // Returns the number of physical output triggers that should have occurred
    // by this 24 PPQN tick.
    // =========================================================================
    auto applyDiv = [](uint32_t tick24, uint8_t div) -> uint32_t {
      static const uint8_t divs[9] = {24, 48, 96, 12, 8, 6, 4, 3, 192};
      if (div < 9)
        return tick24 / divs[div];
      return tick24 / 24;
    };

    // =========================================================================
    // CLOCK PULSE STATE — per-consumer trigger queue
    //
    // Fix for multiplier bug: when applyDiv returns tick*N, the virtual tick
    // can advance by N in a single control loop. The old while-loop set
    // clkTrig=TRIG_LEN N times — each overwriting the previous — so only ONE
    // pulse fired. The fix: enqueue up to 8 advances and drain one per loop,
    // separated by a short gap so each pulse is physically distinct.
    //
    // ClkState is kept per output (P1, P2). Consumers: mode 2 (Clock Out) and
    // mode 3 (Probabilistic) both use this pattern.
    // =========================================================================
    struct ClkState {
      uint32_t prev = 0;     // last processed virtual tick
      uint8_t queue = 0;     // pending triggers waiting to fire
      int16_t timer = 0;     // active pulse countdown (output HIGH while > 0)
      int16_t gap = 0;       // inter-pulse dead-time countdown
      uint8_t lastSrc = 255; // for change detection
      uint8_t lastDiv = 255;
    };
    static ClkState clkSt[2];  // [0]=P1out, [1]=P2out  — mode 2
    static ClkState probSt[2]; // [0]=P1out, [1]=P2out  — mode 3 (prev tracking)

    // Helper: advance a ClkState against divBeat, enqueue any new ticks
    auto advanceClk = [](ClkState &cs, uint32_t divBeat, bool rnng, uint8_t src,
                         uint8_t div) {
      if (!rnng) {
        cs.lastSrc = src;
        cs.lastDiv = div;
        return;
      }
      // Configuration change: reset tracker to prevent huge catch-up lag
      if (src != cs.lastSrc || div != cs.lastDiv) {
        cs.prev = divBeat;
        cs.queue = 0; // Clear pending pulses from old source
        cs.lastSrc = src;
        cs.lastDiv = div;
      }
      // Snap forward if massively behind or backward if source restarted
      if (divBeat > cs.prev + 32 || divBeat < cs.prev)
        cs.prev = divBeat - 1;

      while (divBeat > cs.prev && cs.queue < 8) {
        cs.prev++;
        cs.queue++;
      }
      if (divBeat > cs.prev)
        cs.prev = divBeat; // discard excess
    };

    // Helper: drain one queued trigger; returns true if pulse is active
    auto drainClk = [](ClkState &cs, int16_t trigLen, int16_t gapLen) -> bool {
      if (cs.gap > 0) {
        cs.gap--;
        return false;
      }
      if (cs.queue > 0 && cs.timer <= 0) {
        cs.timer = trigLen;
        cs.gap = trigLen + gapLen;
        cs.queue--;
      }
      if (cs.timer > 0) {
        cs.timer--;
        return true;
      }
      return false;
    };

    // Independent generative sequencer step rises.
    static uint8_t prevGenStepA = 255, prevGenStepB = 255;
    bool genSeqStepRiseA = (genStep[0] != prevGenStepA);
    bool genSeqStepRiseB = (genStep[1] != prevGenStepB);
    prevGenStepA = genStep[0];
    prevGenStepB = genStep[1];

    // =========================================================================
    // PULSE OUTPUT HANDLER
    // Modes: 0=MIDI Gate  1=MIDI Trigger  2=Clock Out  3=Probabilistic
    //        4=Pass-through  5=Voice Audio (skip)
    //        6=Seq A Gate   7=Seq B Gate
    //        8=Bernoulli A  9=Bernoulli A Inverse
    // =========================================================================
    // =========================================================================
    // GRIDS DRUM MACHINE MODE (MI GRIDS PORT)
    // =========================================================================
    if (settings.pulse1Mode == 10) {
      uint8_t clockSrc = settings.pulse1ClockSrc & 3;
      uint32_t t24 = beatTick24[clockSrc];
      static uint32_t lastT24Grids = 0xFFFFFFFF;

      // Handle running state reset
      if (!running[clockSrc]) {
        lastT24Grids = 0xFFFFFFFF;
      } else if (t24 != lastT24Grids) {
        // Clock CV2 at 16th notes (every 6 ticks)
        if ((t24 % 6) == 0 &&
            (t24 > lastT24Grids || lastT24Grids == 0xFFFFFFFF)) {
          gridsCv2Trig = TRIG_LEN;
        }

        // Advance Grids Step every 6 ticks (16th notes) so 32-step pattern = 2
        // bars MI Grids standard: 32 steps × 16th note = 8 beats = 2 bars at
        // 4/4
        if ((t24 % 6) == 0 &&
            (t24 > lastT24Grids || lastT24Grids == 0xFFFFFFFF)) {
          gridsStep = (gridsStep + 1) % 32;

          // Random perturbation on step 0
          if (gridsStep == 0) {
            for (int i = 0; i < 3; i++) {
              gridsPerturbation[i] =
                  (uint8_t)(rand() & 0x0F); // Subtle 0-15 humanize
            }
          }

          // Evaluate Grids triggers
          uint8_t gx = settings.cv1Arg;
          uint8_t gy = settings.cv2Arg;
          // Densities: hardcoded to 100 as per user request (was reusing pulse1Arg, pulse2Arg, pulse1Ch)
          uint8_t dens[3] = {100, 100, 100};

          for (int i = 0; i < 3; i++) {
            uint8_t level = Grids::ReadDrumMap(gridsStep, i, gx, gy);
            // Add perturbation
            if (level < 255 - gridsPerturbation[i])
              level += gridsPerturbation[i];
            else
              level = 255;

            uint8_t threshold = 255 - ((uint32_t)dens[i] << 1);
            if (level > threshold) {
              if (i == 0)
                gridsP1Trig = TRIG_LEN;
              if (i == 1)
                gridsP2Trig = TRIG_LEN;
              if (i == 2)
                gridsCv1Trig = TRIG_LEN;
            }
          }
        }
        lastT24Grids = t24;
      }

      // Update trigger timers and drive GPIO
      bool p1 = (gridsP1Trig > 0);
      if (gridsP1Trig > 0)
        gridsP1Trig--;
      bool p2 = (gridsP2Trig > 0);
      if (gridsP2Trig > 0)
        gridsP2Trig--;
      bool c1 = (gridsCv1Trig > 0);
      if (gridsCv1Trig > 0)
        gridsCv1Trig--;
      bool c2 = (gridsCv2Trig > 0);
      if (gridsCv2Trig > 0)
        gridsCv2Trig--;

      gpio_put(PULSE_1_RAW_OUT, !p1);
      gpio_put(PULSE_2_RAW_OUT, !p2);
      CVOut(0, c1 ? 2047 : 0); // CV1 = HH:       HIGH=+6V, LOW=0V (0 = DAC
                               // midpoint in signed CVOut space)
      CVOut(1, c2 ? 2047 : 0); // CV2 = 16th Clk: HIGH=+6V, LOW=0V

      return; // Skip regular handlers
    }

    auto handlePulse = [&](int idx, uint8_t mode, uint8_t arg, uint8_t clockDiv,
                           uint8_t clockSrc, int16_t &trigTimer,
                           bool &lastOut) {
      if (mode == 5)
        return; // Voice Audio handled in audio path

      bool out = false;
      uint8_t src = clockSrc & 3;

      switch (mode) {

      // ---- 0: MIDI Gate (level-follow) ----
      case 0:
        out = globalMidiGate;
        break;

      // ---- 1: MIDI Trigger (10ms pulse on note-on edge) ----
      case 1:
        if (gateRiseMidi)
          trigTimer = TRIG_LEN;
        if (trigTimer > 0) {
          trigTimer--;
          out = true;
        }
        break;

      // ---- 2: Clock Out — one trigger per divided beat ----
      // Uses ClkState queue so multipliers (×2…×8) fire N distinct pulses.
      case 2: {
        ClkState &cs = clkSt[idx];
        advanceClk(cs, applyDiv(beatTick24[src], clockDiv), running[src], src,
                   clockDiv);
        out = drainClk(cs, TRIG_LEN, 4);
        break;
      }

      // ---- 3: Probabilistic Trigger ----
      // Each clock tick independently rolls the dice at p=arg/127.
      // For multiplier sources, N ticks are processed per loop — each
      // independently can produce a trigger. Uses shared trigTimer for output.
      case 3: {
        ClkState &ps = probSt[idx];
        advanceClk(ps, applyDiv(beatTick24[src], clockDiv), running[src], src,
                   clockDiv);
        // For each queued tick, roll dice. Successful roll arms trigTimer.
        while (ps.queue > 0) {
          ps.queue--;
          uint32_t pArg = (idx == 0) ? settings.pulse1Arg : settings.pulse2Arg;
          uint32_t threshold = (pArg * 4096u) / 127u;
          if ((uint32_t)(rand() & 0xFFF) < threshold)
            trigTimer = TRIG_LEN;
        }
        if (trigTimer > 0) {
          trigTimer--;
          out = true;
        }
        break;
      }

      // ---- 4: Inverse Pulse 1 (for Pulse 2) / Pass-through (for Pulse 1) ----
      case 4:
        if (idx == 1) { // Pulse 2
          out = !lastP1Out;
        } else { // Pulse 1
          out = p1In;
        }
        break;

      // ---- 6: Sequencer A Gate — high on steps marked active ----
      case 6: {
        if (idx != 0) {
          out = false;
          break;
        }
        // Gate mask stored as two 7-bit fields (SysEx protocol: 7 steps per
        // byte) byte0: steps 0-6,  byte1: steps 7-13 Reconstruct: shift Hi by 7
        // so step7 → bit7, step13 → bit13
        uint16_t mask = (uint16_t)(settings.seqAGateMask & 0x7F) |
                        ((uint16_t)(settings.seqAGateMaskHi & 0x7F) << 7) |
                        ((uint16_t)(settings.seqAGateMaskExtra & 0x03) << 14);
        out = (mask >> seqStep[0]) & 1;
        break;
      }

      // ---- 7: Sequencer B Gate — high on steps marked active ----
      case 7: {
        if (idx != 1) {
          out = false;
          break;
        }
        uint16_t mask = (uint16_t)(settings.seqBGateMask & 0x7F) |
                        ((uint16_t)(settings.seqBGateMaskHi & 0x7F) << 7) |
                        ((uint16_t)(settings.seqBGateMaskExtra & 0x03) << 14);
        out = (mask >> seqStep[1]) & 1;
        break;
      }

      // ---- 8: Gen Seq Pulse A ----
      // Fires from independent Gen Seq A step rises with probability = arg/127.
      case 8: {
        if (genSeqStepRiseA) {
          uint32_t threshold = ((uint32_t)arg * 4096u) / 127u;
          if ((uint32_t)(rand() & 0xFFF) < threshold)
            trigTimer = TRIG_LEN;
        }
        if (trigTimer > 0) {
          trigTimer--;
          out = true;
        }
        break;
      }

      // ---- 9: Gen Seq Pulse B ----
      // Fires from independent Gen Seq B step rises with probability = arg/127.
      case 9: {
        if (genSeqStepRiseB) {
          uint32_t threshold = ((uint32_t)arg * 4096u) / 127u;
          if ((uint32_t)(rand() & 0xFFF) < threshold)
            trigTimer = TRIG_LEN;
        }
        if (trigTimer > 0) {
          trigTimer--;
          out = true;
        }
        break;
      }

      default:
        break;
      }

      // Apply variable pulse width
      uint8_t width = (idx == 0) ? settings.pulse1Width : settings.pulse2Width;
      static bool lastRawArr[2] = {false, false};
      static int32_t variableTrigTimer[2] = {0, 0};
      static uint32_t lastRiseLoop[2] = {0, 0};
      static uint32_t currentPeriod[2] = {100, 100};
      static uint8_t lastSeqStep[2] = {255, 255};
      static uint32_t localLoopCnt[2] = {0, 0};
      localLoopCnt[idx]++;

      bool rawOut = out;
      bool forceRetrig = false;
      if (mode == 6) {
        if (seqStep[0] != lastSeqStep[idx]) {
          forceRetrig = true;
          lastSeqStep[idx] = seqStep[0];
        }
      } else if (mode == 7) {
        if (seqStep[1] != lastSeqStep[idx]) {
          forceRetrig = true;
          lastSeqStep[idx] = seqStep[1];
        }
      }

      // MIDI Gate (0), Pass-through (4), and Probabilistic (3) bypass the %
      // width logic. Probabilistic manages its own trigTimer directly; wrapping
      // it causes double-fire. MIDI Trigger (1) treats width as milliseconds.
      // Others (2, 6, 7) use width as a percentage (0..99%). 100% is always
      // pass-through.
      if (width < 100 && mode != 0 && mode != 3 && mode != 4) {
        bool risingEdge = (rawOut && (!lastRawArr[idx] || forceRetrig));
        if (risingEdge) {
          uint32_t period = localLoopCnt[idx] - lastRiseLoop[idx];
          if (period > 10 && period < 2000) {
            currentPeriod[idx] = period;
          }
          lastRiseLoop[idx] = localLoopCnt[idx];

          if (mode == 1) { // MIDI Trigger ignores previous period, treats width
                           // directly as ms
            uint32_t ms = (width == 0) ? 5 : width;
            uint32_t loops = (ms * 750) / 1000; // 750Hz control rate
            if (loops < 1)
              loops = 1;
            variableTrigTimer[idx] = loops;
          } else if (width == 0) {
            variableTrigTimer[idx] = 4; // ~5ms trigger
          } else {
            variableTrigTimer[idx] = (currentPeriod[idx] * width) / 100;
            if (variableTrigTimer[idx] < 4)
              variableTrigTimer[idx] = 4;
            // Avoid fully merging consecutive pulses unless width is 100%
            if ((uint32_t)variableTrigTimer[idx] >= currentPeriod[idx] &&
                currentPeriod[idx] > 2) {
              variableTrigTimer[idx] = currentPeriod[idx] - 2;
            }
          }
        }

        if (variableTrigTimer[idx] > 0) {
          variableTrigTimer[idx]--;
          out = true;
        } else {
          out = false;
        }
      }
      lastRawArr[idx] = rawOut;

      // Drive GPIO (active-low hardware)
      uint32_t gpio = (idx == 0) ? PULSE_1_RAW_OUT : PULSE_2_RAW_OUT;
      gpio_put(gpio, !out);
      lastOut = out;
    };

    handlePulse(0, settings.pulse1Mode, settings.pulse1Arg,
                settings.pulse1ClockDiv, settings.pulse1ClockSrc, p1TrigTimer,
                lastP1Out);
    handlePulse(1, settings.pulse2Mode, settings.pulse2Arg,
                settings.pulse2ClockDiv, settings.pulse2ClockSrc, p2TrigTimer,
                lastP2Out);

    // =========================================================================
    // SEQUENCER / S&H ADVANCE
    // Each column tracks its own divided beat. Uses while-loop so ×N
    // multipliers advance N steps per beat.
    // =========================================================================
    static uint32_t seqPrevA = 0, seqPrevB = 0;
    static uint8_t seqLastSrcA = 255, seqLastDivA = 255;
    static uint8_t seqLastSrcB = 255, seqLastDivB = 255;

    // Seq A / CV1 S&H
    {
      uint8_t src = settings.cv1ClockSrc & 3;
      if (running[src]) {
        uint32_t divBeat = applyDiv(beatTick24[src], settings.cv1ClockDiv);

        if (src != seqLastSrcA || settings.cv1ClockDiv != seqLastDivA) {
          seqPrevA = divBeat;
          seqLastSrcA = src;
          seqLastDivA = settings.cv1ClockDiv;
        }

        // Snap forward if massively behind or backward
        if (divBeat > seqPrevA + 32 || divBeat < seqPrevA)
          seqPrevA = divBeat - 1;
        // Process up to 8 advances per control loop (covers ×8=32nd notes).
        uint8_t maxAdv = 8;
        while (divBeat > seqPrevA && maxAdv-- > 0) {
          seqPrevA++;
          uint8_t len = settings.sequencerLength;
          if (len < 1)
            len = 1;
          if (len > 16)
            len = 16;
          seqStep[0] = (seqStep[0] + 1) % len;
          randomVal[0] = (uint16_t)(rand() & 0xFFF);
        }
      }
    }

    // Seq B / CV2 S&H
    {
      uint8_t src = settings.cv2ClockSrc & 3;
      if (running[src]) {
        uint32_t divBeat = applyDiv(beatTick24[src], settings.cv2ClockDiv);

        if (src != seqLastSrcB || settings.cv2ClockDiv != seqLastDivB) {
          seqPrevB = divBeat;
          seqLastSrcB = src;
          seqLastDivB = settings.cv2ClockDiv;
        }

        if (divBeat > seqPrevB + 32 || divBeat < seqPrevB)
          seqPrevB = divBeat - 1;
        uint8_t maxAdv = 8;
        while (divBeat > seqPrevB && maxAdv-- > 0) {
          seqPrevB++;
          uint8_t len = settings.sequencerLength2;
          if (len < 1)
            len = 1;
          if (len > 16)
            len = 16;
          seqStep[1] = (seqStep[1] + 1) % len;
          randomVal[1] = (uint16_t)(rand() & 0xFFF);
        }
      }
    }

    // Independent Generative Seq A/B step clocks (separate from step sequencer).
    {
      static uint32_t genPrevA = 0, genPrevB = 0;
      static uint8_t genLastSrcA = 255, genLastDivA = 255;
      static uint8_t genLastSrcB = 255, genLastDivB = 255;

      uint8_t srcA = settings.cv1ClockSrc & 3;
      if (running[srcA]) {
        uint32_t divBeatA = applyDiv(beatTick24[srcA], settings.cv1ClockDiv);
        if (srcA != genLastSrcA || settings.cv1ClockDiv != genLastDivA) {
          genPrevA = divBeatA;
          genLastSrcA = srcA;
          genLastDivA = settings.cv1ClockDiv;
        }
        if (divBeatA > genPrevA + 32 || divBeatA < genPrevA)
          genPrevA = divBeatA - 1;
        uint8_t maxAdvA = 8;
        while (divBeatA > genPrevA && maxAdvA-- > 0) {
          genPrevA++;
          genStep[0] = (genStep[0] + 1) & 31; // 32-step generative timeline
        }
      }

      uint8_t srcB = settings.cv2ClockSrc & 3;
      if (running[srcB]) {
        uint32_t divBeatB = applyDiv(beatTick24[srcB], settings.cv2ClockDiv);
        if (srcB != genLastSrcB || settings.cv2ClockDiv != genLastDivB) {
          genPrevB = divBeatB;
          genLastSrcB = srcB;
          genLastDivB = settings.cv2ClockDiv;
        }
        if (divBeatB > genPrevB + 32 || divBeatB < genPrevB)
          genPrevB = divBeatB - 1;
        uint8_t maxAdvB = 8;
        while (divBeatB > genPrevB && maxAdvB-- > 0) {
          genPrevB++;
          genStep[1] = (genStep[1] + 1) & 31; // 32-step generative timeline
        }
      }
    }

    // =========================================================================
    // INTERNAL EG (ADSR) — Q20 fixed-point, output always 0-6V
    // Trigger sources: 0=MIDI Gate, 1=Pulse1In, 2=Pulse2In, 3=Any Clock Beat
    // Modes 1/2/3 are TRIGGER (retrigger on edge, sustain until next trigger).
    // Mode 0 is GATE (sustain while gate high, release on gate fall).
    // =========================================================================
    static uint32_t egVal[2] = {0, 0};
    static uint8_t egStage[2] = {0, 0}; // 0=idle 1=attack 2=decay 3=sustain 4=release
    static bool egLastGate[2] = {false, false};
    {
      uint8_t trig = settings.egTrigSrc;
      bool wantsTrigger = false;
      bool g = false;
      bool usesGate = false;

      switch (trig) {
      case 1: // Pulse1In rising edge → retrigger
        if (p1InRise) wantsTrigger = true;
        break;
      case 2: // Pulse2In rising edge → retrigger
        if (p2InRise) wantsTrigger = true;
        break;
      case 3: // Internal BPM clock
        if (beatRise[1]) wantsTrigger = true;
        break;
      default: // MIDI Gate
        g = globalMidiGate;
        usesGate = true;
        break;
      }

      auto egRate = [](uint8_t t) -> uint32_t {
        uint32_t frames = 1 + ((uint32_t)t * t * 3750u) / (127u * 127u);
        return (1u << 20) / frames;
      };

      for (int i = 0; i < 2; i++) {
        if (usesGate) {
          if (g && !egLastGate[i])
            egStage[i] = 1; // gate rise → Attack
          if (!g && egLastGate[i] && egStage[i] < 4)
            egStage[i] = 4; // gate fall → Release
          egLastGate[i] = g;
        } else {
          if (wantsTrigger) egStage[i] = 1;
          egLastGate[i] = false;
        }

        uint8_t envKnob = (i == 0) ? settings.eg1Env : settings.eg2Env;
        uint8_t a, d, s, r;
        if (envKnob < 64) {
          a = 1;
          d = 10 + envKnob;
          s = 0;
          r = 10 + envKnob;
        } else {
          a = (envKnob - 64) * 2;
          d = 74 + (envKnob - 64) / 2;
          s = (envKnob - 64) * 2;
          r = 74 + (envKnob - 64);
        }

        uint32_t sustainQ20 = ((uint32_t)s << 20) / 127u;

        switch (egStage[i]) {
        case 1:
          egVal[i] += egRate(a);
          if (egVal[i] >= (1u << 20)) {
            egVal[i] = (1u << 20);
            egStage[i] = 2;
          }
          break;
        case 2:
          if (egVal[i] > sustainQ20) {
            uint32_t d_rate = egRate(d);
            egVal[i] = (egVal[i] > d_rate) ? egVal[i] - d_rate : sustainQ20;
            if (egVal[i] <= sustainQ20) egStage[i] = 3;
          } else {
            egStage[i] = 3;
          }
          break;
        case 3:
          egVal[i] = sustainQ20;
          if (!usesGate) egStage[i] = 4; // Auto-release if triggered mode
          break;
        case 4:
          if (egVal[i] > 0) {
            uint32_t r_rate = egRate(r);
            egVal[i] = (egVal[i] > r_rate) ? egVal[i] - r_rate : 0;
          } else {
            egStage[i] = 0;
          }
          break;
        default:
          break;
        }
      }
    }

    // ---- CV output handler ----

    auto handleCV = [&](int idx, uint8_t mode, uint8_t arg) {
      uint8_t range = (idx == 0) ? settings.cv1Range : settings.cv2Range;
      int col = idx; // 0 = left (Seq A), 1 = right (Seq B)

      // Lambda to scale a normalized 0..4095 internal value into the
      // actual DAC bounds based on the cvRange setting from the web UI.
      // CVOut maps inputs [-2048 to 2047] to actual hardware voltages.
      auto applyRange = [](int32_t val4095, uint8_t rng) -> int16_t {
        int32_t scaled = 0;
        switch (rng) {
        case 0:
          scaled = val4095 - 2048;
          break; // ±6V
        case 1:
          scaled = (val4095 / 2) - 1024;
          break; // ±3V
        case 2:
          scaled = (val4095 / 3) - 682;
          break; // ±2V
        case 3:
          scaled = (val4095 / 6) - 341;
          break; // ±1V
        case 4:
          scaled = val4095 / 2;
          break; // 0-6V (4095 -> 2047)
        case 5:
          scaled = val4095 / 4;
          break; // 0-3V
        case 6:
          scaled = val4095 / 12;
          break; // 0-1V
        case 7:
          scaled = val4095 / 2;
          break; // 1V/Oct (Fallback if forced through generic path)
        default:
          scaled = val4095 / 2;
          break;
        }
        return (int16_t)scaled;
      };

      switch (mode) {
      case 0: { // MIDI Pitch — always 1V/Oct, ignore range setting
        CVOutMIDINote(idx, globalMidiNote);
        break;
      }
      case 1: { // MIDI Velocity — always 0-6V (velocity is 0..127,
                // unipolar)
        int32_t val = (int32_t)globalMidiVel * 32; // 0..4064
        CVOut(idx, applyRange(val, 4));            // 4 = 0-6V unipolar
        break;
      }
      case 2: { // MIDI CC — always 0-6V (CC is 0..127, unipolar)
        uint8_t ccVal = (idx == 0) ? lastCv1CC : lastCv2CC;
        int32_t val = (int32_t)ccVal * 32; // 0..4064
        CVOut(idx, applyRange(val, 4));    // 4 = 0-6V unipolar
        break;
      }
      case 3: { // Synth Envelope — always 0-6V (envelope is always
                // positive)
        int32_t val = (int32_t)(v0.envVal >> 17);
        if (val > 4095)
          val = 4095;
        if (val < 0)
          val = 0;
        CVOut(idx, applyRange(val, 4)); // 4 = 0-6V unipolar
        break;
      }
      case 4: { // Random S&H (independent per column, updated by column's
                // clock)
        CVOut(idx, applyRange(randomVal[col], range));
        break;
      }
      case 5: { // Step Sequencer (column-specific bank)
        uint8_t len =
            (col == 0) ? settings.sequencerLength : settings.sequencerLength2;
        if (len < 1)
          len = 1;
        else if (len > 16)
          len = 16;
        uint8_t sIdx = seqStep[col] % len;
        uint8_t seqScale = (col == 0) ? settings.seqAScale : settings.seqBScale;
        if (seqScale == 6) { // Trigger mode: fader = amplitude (fires every step)
          static int32_t seqTrigTimer[2] = {0, 0};
          static int32_t seqTrigAmp[2] = {4095, 4095}; // amplitude for current pulse
          // Track raw seqStep so length==1 still detects every advance.
          static uint8_t prevRawSeqStep5[2] = {255, 255};
          if (seqStep[col] != prevRawSeqStep5[col]) {
            prevRawSeqStep5[col] = seqStep[col];
            // Use step value (0-127) as trigger amplitude
            uint8_t amp = (col == 0) ? settings.sequencerSteps[sIdx]
                                     : settings.sequencerSteps2[sIdx];
            seqTrigAmp[col] = (int32_t)amp * 32; // 0..127 → 0..4064
            seqTrigTimer[col] = 8; // ~10ms at 750 Hz
          }
          if (seqTrigTimer[col] > 0) {
            seqTrigTimer[col]--;
            CVOut(idx, applyRange(seqTrigAmp[col], range));
          } else {
            CVOut(idx, 0); // LOW between pulses
          }
          break;
        }
        uint8_t rawNote = (col == 0) ? settings.sequencerSteps[sIdx]
                                     : settings.sequencerSteps2[sIdx];
        
        uint8_t seqRoot = (col == 0) ? settings.seqARoot : settings.seqBRoot;
        uint8_t seqOctave = (col == 0) ? settings.seqAOctave : settings.seqBOctave;
        uint8_t seqOctRange = (col == 0) ? settings.seqAOctRange : settings.seqBOctRange;
        
        if (range == 7) { // 1V/Octave Pitch Mode triggers calibrated output
          uint8_t note = QuantizeSeqStepToNote(rawNote, seqScale, seqRoot, seqOctave, seqOctRange);
          CVOutMIDINote(idx, note);
        } else {
          int32_t val = (int32_t)rawNote * 32; // Map 0..127 to 0..4064
          CVOut(idx, applyRange(val, range));
        }
        break;
      }
      case 8: { // Utility LFO
        static uint32_t lfoPhase[2] = {0, 0};
        static uint32_t lastLfoPhase[2] = {0, 0};
        static int32_t smoothTarget[2] = {0, 0};
        static int32_t smoothPrevTarget[2] = {0, 0};
        static uint32_t rseed8[2] = {0x12345678, 0x87654321};

        // Control rate = AUDIO_BASE_RATE / 32 = 750 Hz.
        // Phase increments are scaled for 750 Hz: baseInc = 572640 gives ~0.1
        // Hz, varInc at max (127^2 * 8800 ≈ 141M) gives ~25 Hz.
        uint32_t baseInc = 572640u; // ~0.1 Hz at 750 Hz control rate
        uint32_t speedArg = arg;
        uint32_t varInc = speedArg * speedArg * 8800u; // scales to ~25 Hz max
        lfoPhase[idx] += baseInc + varInc;

        uint8_t shape = (idx == 0) ? settings.cv1Ch : settings.cv2Ch;
        int32_t lfoVal = 0; // 0..4095
        uint32_t p = lfoPhase[idx];

        if (p < lastLfoPhase[idx]) {
          smoothPrevTarget[idx] = smoothTarget[idx];
          rseed8[idx] ^= rseed8[idx] << 13;
          rseed8[idx] ^= rseed8[idx] >> 17;
          rseed8[idx] ^= rseed8[idx] << 5;
          smoothTarget[idx] = rseed8[idx] & 0xFFF; // 0..4095
        }
        lastLfoPhase[idx] = p;

        if (shape == 0) {                           // Sine
          float angle = (float)p * 1.462918079e-9f; // p * (2*PI / 2^32)
          lfoVal = (int32_t)((sinf(angle) * 2047.5f) + 2047.5f);
        } else if (shape == 1) { // Triangle
          uint32_t tri = (p < 0x80000000) ? p : ~p;
          lfoVal = tri >> 19;
        } else if (shape == 2) { // Saw Up
          lfoVal = p >> 20;
        } else if (shape == 3) { // Saw Down
          lfoVal = (~p) >> 20;
        } else if (shape == 4) { // Square
          lfoVal = (p < 0x80000000) ? 4095 : 0;
        } else {                  // Smooth Random (shape = 5)
          int32_t frac = p >> 16; // 0..65535
          lfoVal = smoothPrevTarget[idx] +
                   ((smoothTarget[idx] - smoothPrevTarget[idx]) * frac) / 65536;
        }
        CVOut(idx, applyRange(lfoVal, range));
        break;
      }
      case 9: { // Generative Sequencer (recurrence->evolution->random continuum)
        static uint32_t genPattern[2] = {0x5A5A5A5Au, 0x3C3C3C3Cu};
        static uint8_t lastGenStep9[2] = {255, 255};
        static uint32_t rseed9[2] = {0x98765432, 0x23456789};

        uint8_t sIdx = genStep[col] & 31;

        // Trigger quantization: fire a short +6V pulse on step advance.
        uint8_t seqScale = (col == 0) ? settings.seqAScale : settings.seqBScale;
        if (seqScale == 6) { // Trigger mode
          static int32_t genTrigTimer[2] = {0, 0};
          if (sIdx != lastGenStep9[col]) {
            lastGenStep9[col] = sIdx;
            genTrigTimer[col] = 8; // ~10ms at 750 Hz
          }
          if (genTrigTimer[col] > 0) {
            genTrigTimer[col]--;
            CVOut(idx, applyRange(4095, 4)); // 0-6V positive
          } else {
            CVOut(idx, 0); // LOW = 0V
          }
          break;
        }

        if (sIdx != lastGenStep9[col]) {
          lastGenStep9[col] = sIdx;

          uint8_t patternLen = 32;
          uint8_t mutateChance = 0;
          if (arg < 43) {
            // Left: short recurring loops, very stable.
            patternLen = 4 + ((uint32_t)arg * 12u) / 42u; // 4..16
            mutateChance = 1 + (arg / 6);                 // ~1..8
          } else if (arg < 96) {
            // Middle: longer loops with slow evolution.
            uint8_t t = arg - 43;
            patternLen = 16 + ((uint32_t)t * 16u) / 52u; // 16..32
            mutateChance = 4 + ((uint32_t)t * 20u) / 52u; // ~4..24
          } else {
            // Right: mostly random.
            patternLen = 32;
            mutateChance = 96 + (arg - 96); // 96..127
          }
          if (patternLen < 1)
            patternLen = 1;
          if (patternLen > 32)
            patternLen = 32;

          uint32_t mask = (patternLen == 32) ? 0xFFFFFFFFu : ((1u << patternLen) - 1u);
          uint32_t tailBit = (genPattern[col] >> (patternLen - 1)) & 1u;

          rseed9[col] ^= rseed9[col] << 13;
          rseed9[col] ^= rseed9[col] >> 17;
          rseed9[col] ^= rseed9[col] << 5;

          uint32_t newBit = tailBit;
          if ((rseed9[col] & 0x7F) < mutateChance)
            newBit = (rseed9[col] >> 8) & 1u;

          genPattern[col] = ((genPattern[col] << 1) | newBit) & mask;
        }

        uint8_t rawSemi = (uint8_t)((genPattern[col] ^ (genPattern[col] >> 7) ^
                                     (uint32_t)(sIdx * 17u)) &
                                    0x7F);
        uint8_t seqRoot = (col == 0) ? settings.seqARoot : settings.seqBRoot;
        uint8_t seqOctave = (col == 0) ? settings.seqAOctave : settings.seqBOctave;
        uint8_t seqOctRange = (col == 0) ? settings.seqAOctRange : settings.seqBOctRange;

        if (range == 7) {
          if (seqScale == 5) { // voltage: raw note, no scale quantize
            CVOutMIDINote(idx, rawSemi); // Unquantized pseudo-note
          } else {
            uint8_t note = QuantizeSeqStepToNote(rawSemi, seqScale, seqRoot, seqOctave, seqOctRange);
            CVOutMIDINote(idx, note);
          }
        } else {
          int32_t val = (int32_t)rawSemi * 32;
          CVOut(idx, applyRange(val, range));
        }
        break;
      }
      case 10: { // CV Clock Out — click-style trigger to match pulse outputs.
        static ClkState cvClkSt[2];
        ClkState &cs = cvClkSt[idx];
        uint8_t src = (idx == 0) ? settings.cv1ClockSrc : settings.cv2ClockSrc;
        uint8_t clockDiv = (idx == 0) ? settings.cv1ClockDiv : settings.cv2ClockDiv;
        
        advanceClk(cs, applyDiv(beatTick24[src], clockDiv), running[src], src, clockDiv);
        bool out = drainClk(cs, TRIG_LEN, 4);
        CVOut(idx, out ? applyRange(4095, 4) : 0);
        break;
      }
      case 6: // Voice Audio — handled in audio path (pwm from voice data)
        break;
      case 7: { // Internal EG — always 0-6V (ADSR is inherently unipolar)
        int32_t val = (int32_t)(egVal[idx] >> 8); // Q20 -> 0..4095
        if (val > 4095)
          val = 4095;
        CVOut(idx, applyRange(val, 4)); // 4 = 0-6V unipolar
        break;
      }
      default:
        break;
      }
    };

    handleCV(0, settings.cv1Mode, settings.cv1Arg);
    handleCV(1, settings.cv2Mode, settings.cv2Arg);
  }

private:
  // Filter state for external input
  int32_t mix1 = 0, mix2 = 0, mixf1 = 0, mixf2 = 0;
  int32_t mix1R = 0, mix2R = 0, mixf1R = 0, mixf2R = 0;
  int32_t gateEnvL = 0, gateEnvR = 0;
  int32_t lpL = 0, lpR = 0;
  int32_t noiseFloorL = 0, noiseFloorR = 0;
  int32_t gateThreshL = 800, gateThreshR = 800;

  // rawX/rawY needed in HandleEffectSelect/HandleSynthSelect (set in
  // decimator) We access them via the static locals in ProcessSample — but
  // since these handlers are called from within ProcessSample's scope,
  // rawX/rawY are accessible as member variables. Let's store them as
  // members updated each decimator tick.
  int32_t rawX = 0;
  int32_t rawY = 0;

  void __attribute__((used)) GetLiveState(uint8_t *outBlob) {
    // Pack current state into 32 bytes for SysEx CMD 0x16
    // [0] effectIndex  [1..6] fxParam0/1/2 raw knob (14-bit)  [7] source  [8] synthMode
    // [9..10] volume  [11..12] pitch  [13..14] timbre  [15..16] env
    // [17..18] filterCutoff  [19] isCardLocked
    // [20] cv1Arg  [21] cv2Arg  [22] seqLength  [23] seqLength2
    // [24] pulse1Arg  [25] pulse2Arg  [26] cv1Ch  [27] cv2Ch
    // [28] bpm lo  [29] bpm hi
    // [30] cv1 modulation hint (signed -2048..2047 mapped to 0..127, center=64)
    // [31] cv2 modulation hint

    memset(outBlob, 0, 32);
    outBlob[0] = currentEffectIndex & 0x7F;

    auto s14 = [](uint8_t *tgt, uint16_t val) {
      tgt[0] = (val >> 7) & 0x7F;
      tgt[1] = val & 0x7F;
    };

    uint16_t fp0 = 0, fp1 = 0, fp2 = 0;
    if (currentEffect) {
      if (currentEffect->nParameters > 0)
        fp0 = currentEffect->parameters[0].rawValue;
      if (currentEffect->nParameters > 1)
        fp1 = currentEffect->parameters[1].rawValue;
      if (currentEffect->nParameters > 2)
        fp2 = currentEffect->parameters[2].rawValue;
    }
    s14(&outBlob[1], fp0);
    s14(&outBlob[3], fp1);
    s14(&outBlob[5], fp2);

    outBlob[7] = currentSource & 0x7F;
    outBlob[8] = selectedSynthMode & 0x7F;

    uint16_t vol14 = (sampleVolumeQ16 > 65535) ? 16383 : (sampleVolumeQ16 >> 2);
    s14(&outBlob[9], vol14);

    s14(&outBlob[11], (uint16_t)(synthParameterPitch * 16383.0f));
    s14(&outBlob[13], (uint16_t)(synthParameterTimbre * 16383.0f));
    s14(&outBlob[15], (uint16_t)(synthParameterEnv * 16383.0f));
    s14(&outBlob[17], (uint16_t)(synthParameterFilterCutoff * 16383.0f));

    // [19] isCardLocked
    outBlob[19] = settings.isCardLocked;
    // [20-27] Performance-controlled values for UI sync after EXTRA page edits
    outBlob[20] = settings.cv1Arg; // Gen Seq spice / LFO speed (0-127)
    outBlob[21] = settings.cv2Arg;
    outBlob[22] = settings.sequencerLength; // 1-16
    outBlob[23] = settings.sequencerLength2;
    outBlob[24] = settings.pulse1Arg; // probability (pulse mode 3) or gridsBD
    outBlob[25] = settings.pulse2Arg; // gridsSD
    outBlob[26] = settings.cv1Ch;     // gridsHH (or MIDI ch)
    outBlob[27] = settings.cv2Ch;     // for completeness
    // [28..29] BPM as 14-bit MIDI (matches UI: lsb | (msb << 7))
    {
      uint16_t tb = (settings.bpm & 0x7F) |
                    ((uint16_t)(settings.bpmHi & 0x7F) << 7);
      if (settings.bpmHi == 0 && settings.bpm > 127)
        tb = settings.bpm;
      if (tb < 20)
        tb = 20;
      if (tb > 256)
        tb = 256;
      outBlob[28] = (uint8_t)(tb & 0x7F);
      outBlob[29] = (uint8_t)((tb >> 7) & 0x7F);
    }

    // CV modulation hints: encode signed -2048..2047 as 0..127 (center=64).
    // UI uses these to draw a small arc/indicator showing how much CV is
    // shifting the Fx Param 1 and Fx Param 2 knob positions.
    auto cvHint = [](int32_t cvVal) -> uint8_t {
      // cv[] is -2048..2047; map to 0..127 with 64 = 0V
      int32_t h = 64 + ((cvVal * 63) >> 11); // ÷2048 * 63
      if (h < 0) h = 0;
      if (h > 127) h = 127;
      return (uint8_t)h;
    };
    outBlob[30] = cvHint(cv[0]); // CV1 → Fx Param 1 hint
    outBlob[31] = cvHint(cv[1]); // CV2 → Fx Param 2 hint
  }
};

#include "hardware/structs/bus_ctrl.h"
#include "hardware/vreg.h"

MultiFx_Computer *MultiFx_Computer::instance = nullptr;

// =========================================================================
// Settings Persistence
// =========================================================================
void SaveSettingsToFlash() {
  multicore_reset_core1();
  uint32_t interrupts = save_and_disable_interrupts();

  settings.magic = FLASH_MAGIC;
  settings.selectedSynthMode = selectedSynthMode;

  // Persist settings into a dedicated flash sector so we don't erase
  // sampler PCM/sample metadata.
  uint32_t hdr = FLASH_MAGIC;

  if (MultiFx_Computer::instance) {
    int32_t vol = MultiFx_Computer::instance->sampleVolumeQ16;
    settings.globalVolume = (vol > 65535) ? 127 : (vol >> 9);

    // Capture runtime effects and parameters
    settings.currentEffectIndex = currentEffectIndex;
    settings.currentSource = currentSource;
    settings.synthParamPitch =
        (uint16_t)(MultiFx_Computer::instance->synthParameterPitch * 16383.0f);
    settings.synthParamTimbre =
        (uint16_t)(MultiFx_Computer::instance->synthParameterTimbre * 16383.0f);
    settings.synthParamEnv =
        (uint16_t)(MultiFx_Computer::instance->synthParameterEnv * 16383.0f);
    settings.synthParamFilter =
        (uint16_t)(MultiFx_Computer::instance->synthParameterFilterCutoff *
                   16383.0f);

    if (currentEffect) {
      if (currentEffect->nParameters > 0)
        settings.fxParam0 = currentEffect->parameters[0].rawValue;
      if (currentEffect->nParameters > 1)
        settings.fxParam1 = currentEffect->parameters[1].rawValue;
      if (currentEffect->nParameters > 2)
        settings.fxParam2 = currentEffect->parameters[2].rawValue;
    }

  }

  uint32_t flashOffset = FLASH_SETTINGS_BASE - XIP_BASE;
  flash_range_erase(flashOffset, FLASH_SECTOR_SIZE);

  // RP2040 flash programming requires 256-byte aligned page writes.
  // Pack header + settings into one page at sector start.
  uint8_t page[FLASH_PAGE_SIZE];
  memset(page, 0xFF, sizeof(page));
  memcpy(page, &hdr, sizeof(hdr)); // +0
  memcpy(page + 8, &settings, sizeof(DeviceSettings)); // +8
  flash_range_program(flashOffset, page, FLASH_PAGE_SIZE);

  restore_interrupts(interrupts);
  SynthCore_Start();
}

int main() {
  vreg_set_voltage(VREG_VOLTAGE_1_25);
  sleep_ms(10);
  set_sys_clock_khz(240000, true);

  bus_ctrl_hw->priority = 1;

  tusb_init();

  MultiFx_Computer computer;
  computer.EnableNormalisationProbe();
  computer.Run();
}
