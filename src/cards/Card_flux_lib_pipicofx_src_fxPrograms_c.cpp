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
/* stripped system include */
#include "pipicofx/fxPrograms.h"
#include "pipicofx/picofxCore.h"
#include "stringFunctions.h"

extern FxProgramType fxProgramHighGain;

FxProgramType* fxPrograms[N_FX_PROGRAMS]={
    &fxProgramOff, // 0: Off (Bypass)

    // --- Dynamic / Utility ---
    &fxProgramCompressor, // 1: Compressor
    &fxProgramEq, // 2: EQ


    // --- Overdrive / Distortion ---
    &fxProgramAmpModel, // 3: Amp Model (Crunch)
    &fxProgramEchoVerb, // 4: Echo Verb
    &fxProgramMonsterCrusher, // 5: Monster Crusher

    // --- Modulation ---
    &fxProgramTremolo, // 6: Tremolo
    &fxProgramSineChorus, // 7: Sine Chorus
    &fxProgramVibChorus, // 8: VibChorus
    &fxProgramPitchShifter, // 9: Pitch Shifter
    &fxProgramCwo, // 10: CWO (Bode Frequency Shifter)

    // --- Delay ---
    &fxProgramDelay, // 11: Digital Delay
    &fxProgramTape, // 12: Tape Delay
    &fxProgramPingPong, // 13: Ping Pong Delay
    &fxProgramLofiDelay, // 14: Tape Loop

    // --- Reverb ---
    &fxProgramPlateReverb, // 15: Plate Reverb
    &fxProgramSpringReverb, // 16: Spring Reverb
    &fxProgramFreeVerb, // 17: FreeVerb (Standard Room/Hall)
    &fxProgramShimmerVerb, // 18: ShimmerVerb
    &fxProgramCathedral, // 19: Deep Cathedral
    &fxProgramGranular, // 20: Clouds
    &fxProgramMicroLoop, // 21: Micro Looper
    &fxProgramGenLoss, // 22: Generation Loss
    &fxProgramLossy, // 23: Lossy
    &fxProgramSpectral, // 24: Space Resonator

    // --- Legacy / Experimental ---
    &fxProgramReverb, // 25: Basic Reverb
    &fxProgramReverb2, // 26: Allpass Reverb
    &fxProgramReverb3, // 27: Hadamard Reverb
    &fxProgramOilCan, // 28: Oil Can Echo
    &fxProgramStrings, // 29: Resonator
    &fxProgramWind, // 30: Wind
    
    // --- Experimental ---
    &fxProgramSpeech // 31: RoboTalk
    };
} // namespace Card_Flux
