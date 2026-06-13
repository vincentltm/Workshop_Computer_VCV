#include <stdint.h>
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