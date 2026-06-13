#ifndef _FX_PROGRAM_H_
#define _FX_PROGRAM_H_
#include <stdint.h>

// ============================================================================
// Sample Rate Configuration (Must match SynthCore.h)
// ============================================================================
#ifndef AUDIO_SAMPLE_RATE_DIV
#define AUDIO_SAMPLE_RATE_DIV 2
#endif

#ifndef AUDIO_BASE_RATE
#define AUDIO_BASE_RATE (48000 / AUDIO_SAMPLE_RATE_DIV)
#endif
#include "audio/waveShaper.h"
#include "audio/secondOrderIirFilter.h"
#include "audio/firstOrderIirFilter.h"
#include "audio/firFilter.h"
#include "audio/simpleChorus.h"
#include "audio/sineChorus.h"
#include "audio/oversamplingWaveshaper.h"
#include "audio/gainstage.h"
#include "audio/bitcrusher.h"
#include "audio/delay.h"
#include "audio/compressor.h"
#include "audio/reverb.h"
#include "audio/reverb2.h"
#include "audio/reverb3.h"
#include "audio/threebandeq.h"
#include "audio/pitchshifter.h"
#include "audio/tremolo.h"
#include "pipicofx/picofxCore.h"




#define FXPROGRAM6_DELAY_TIME_LOWPASS_T 2

#ifndef FLOAT_AUDIO
typedef struct {
    int16_t highpassCutoff;
    uint8_t nWaveshapers;
    int16_t highpass_out,highpass_old_out,highpass_old_in;
    WaveShaperDataType waveshaper1;
    FirFilterType filter3;
    SecondOrderIirFilterType filter1;
    DelayDataType delay;
    FirstOrderIirType feedbackFilter;
    GainStageDataType presetVolume;
} FxProgram1DataType;
#else
typedef struct {
    float highpassCutoff;
    uint8_t nWaveshapers;
    float highpass_out,highpass_old_out,highpass_old_in;
    WaveShaperDataType waveshaper1;
    FirFilterType filter3;
    SecondOrderIirFilterType filter1;
    DelayDataType * delay;
} FxProgram1DataType;
#endif


#ifndef FLOAT_AUDIO
typedef struct {
    // Parameters
    int16_t mix;        // 0: General Wet/Dry Mix
    int16_t time;       // 1: Delay Time
    int16_t warmth;     // 2: Feedback + Reverb Wash + Saturation
    int16_t freeze;     // Freeze state
    
    // Echo State
    DelayDataType delayL;
    DelayDataType delayR;
    int32_t delaySmoothedTime; // For smooth time changes
    int16_t lastTimeKnob;
    int32_t lpStateL;
    int32_t lpStateR;
    
    // Verb Tank (Diffusion)
    AllpassType apL1;
    AllpassType apL2;
    AllpassType apR1;
    AllpassType apR2;
    
    GainStageDataType presetVolume;
} FxProgram9DataType;
#else
typedef struct {
    float mix, time, warmth, freeze;
    GainStageDataType presetVolume;
} FxProgram9DataType;
#endif

typedef struct {
    SimpleChorusType chorusData;
    GainStageDataType presetVolume;
} FxProgram2DataType;


typedef struct {
    GainStageDataType presetVolume;
} FxProgram3DataType;

#ifndef FLOAT_AUDIO
typedef struct {
    GainStageDataType gainStage;
    uint8_t cabSimType;
    uint8_t nWaveshapers;
    uint8_t waveshaperType;
    int16_t highpass_out,highpass_old_out,highpass_old_in;
    const char cabNames[6][24];
    const char waveShaperNames[4][24];
    FirFilterType hiwattFir;
    OversamplingWaveshaperDataType waveshaper1;
    SecondOrderIirFilterType hiwattIir1;
    SecondOrderIirFilterType hiwattIir2;
    SecondOrderIirFilterType hiwattIir3;
    
    FirFilterType frontmanFir;
    SecondOrderIirFilterType frontmanIir1;
    SecondOrderIirFilterType frontmanIir2;
    SecondOrderIirFilterType frontmanIir3;

    FirFilterType voxAC15Fir;
    SecondOrderIirFilterType voxAC15Iir1;
    SecondOrderIirFilterType voxAC15Iir2;
    SecondOrderIirFilterType voxAC15Iir3;
    

    //uint8_t updateLock;
} FxProgram4DataType;
#else
typedef struct {
    gainStageData gainStage;
    uint8_t cabSimType;
    uint8_t nWaveshapers;
    uint8_t waveshaperType;
    float highpass_out,highpass_old_out,highpass_old_in;
    const char cabNames[6][24];
    const char waveShaperNames[4][24];
    FirFilterType hiwattFir;
    MultiWaveShaperDataType waveshaper1;
    SecondOrderIirFilterType hiwattIir1;
    SecondOrderIirFilterType hiwattIir2;
    SecondOrderIirFilterType hiwattIir3;
    
    FirFilterType frontmanFir;
    SecondOrderIirFilterType frontmanIir1;
    SecondOrderIirFilterType frontmanIir2;
    SecondOrderIirFilterType frontmanIir3;

    FirFilterType voxAC15Fir;
    SecondOrderIirFilterType voxAC15Iir1;
    SecondOrderIirFilterType voxAC15Iir2;
    SecondOrderIirFilterType voxAC15Iir3;
    

    //uint8_t updateLock;
} FxProgram4DataType;
#endif


typedef struct 
{
    BitCrusherDataType bitcrusher;
    uint8_t resolution;
    GainStageDataType presetVolume;
} FxProgram5DataType;

typedef struct 
{
    DelayDataType delay;
    GainStageDataType presetVolume;
} FxProgram6DataType;


typedef struct
{
    WaveShaperDataType waveshaper1;
    WaveShaperDataType waveshaper2;
    WaveShaperDataType waveshaper3;
    GainStageDataType gainStage;
    CompressorDataType compressor;
    int16_t highpass_out,highpass_old_out,highpass_old_in;
    const char cabNames[4][24];
    FirFilterType hiwattFir;
    FirFilterType frontmanFir;
    FirFilterType voxAC15Fir;
    SecondOrderIirFilterType cabF1;
    SecondOrderIirFilterType cabF2;
    SecondOrderIirFilterType cabF3;
    SecondOrderIirFilterType cabF4;    
    DelayDataType* delay;
    uint8_t cabSimType;
    
} FxProgram7DataType;


typedef struct 
{
    uint8_t compressorType;
    CompressorDataType compressor;
    GainStageDataType presetVolume;
}  FxProgram8DataType;


typedef struct
{
    ReverbType reverb;
    int16_t reverbTime;
    GainStageDataType presetVolume;
    int16_t freeze;
} FxProgram10DataType;

typedef struct 
{
    SineChorusType sineChorus;
    GainStageDataType presetVolume;
} FxProgram11DataType;


typedef struct 
{
    Reverb2Type reverb;
    GainStageDataType presetVolume;
} FxProgram12DataType;

typedef struct 
{
    int16_t mix;
    Reverb3Type reverb;
    GainStageDataType presetVolume;
    int16_t freeze;
} FxProgram13DataType;

typedef struct
{
    ThreeBandEQType eq;
    GainStageDataType presetVolume;
} FxProgram14DataType;

typedef struct 
{
    int16_t reverbTime;
    ThreeBandEQType eq;
    CompressorDataType comp;
    GainStageDataType postGain;
    ReverbType reverb;
    GainStageDataType presetVolume;
} FxProgram15DataType;


typedef struct 
{
    Pitchshifter2DataType pitchShifter;
    int16_t mix;
    GainStageDataType presetVolume;
} FxProgram16DataType;


typedef struct 
{

    Pitchshifter2DataType pitchShifter;
    FirstOrderIirType glitterTamer;
    DelayDataType delays[4];
    AllpassType allpasses[2];
    int16_t oldVal;
    int16_t feedback;
    int16_t mix;
    GainStageDataType presetVolume;
} FxProgram17DataType;

typedef struct 
{
    TremoloType tremolo;
    GainStageDataType presetVolume;
} FxProgram18DataType;


typedef struct 
{
    DelayDataType delays[8];
    FirstOrderIirType feedbackFilters[8];
    AllpassType allpasses[4];
    GainStageDataType presetVolume;
    int16_t mix;
    int16_t freeze; // Frozen State
} FxProgram19DataType;

typedef struct {
    int16_t* buffer;
    int32_t writePtr;
    int32_t readPtrQ8; // Fixed point for interpolation
    
    // Parameters
    int16_t mix;
    int16_t wowDepth;    // Amount of pitch wobble
    int16_t flutterDepth; // Faster random wobble
    int16_t genLossAmount; // Controls Sample Rate Redux / Bit Depth
    int16_t filterCutoff; // LP/HP combo or just LP
    int16_t noiseSat;     // Noise level + Saturation
    
    GainStageDataType presetVolume;
    
    // State
    int32_t lfoPhaseWow;
    int32_t lfoPhaseFlutter;
    int32_t lfoPhaseDrift;

    // Dropouts
    uint16_t dropoutTimer;
    int16_t dropoutTargetGain; 
    int16_t dropoutTargetCutoff;
    int32_t dropoutCurrentGain;
    int32_t dropoutCurrentCutoff;

    // Filters (Q16 state)
    int32_t lpStateL, lpStateR;   // Gap Loss
    int32_t lp2StateL, lp2StateR; // Gap Loss 2nd stage
    int32_t hpStateL, hpStateR;   // Head Bump
    int32_t hp2StateL, hp2StateR; // Head Bump 2nd stage
    
    // Hysteresis State (Magnetic Flux)
    int32_t hysterStateL, hysterStateR; 

    // Jitter / Scrape
    uint32_t rngState;

} FxProgram26DataType;

typedef struct {
    int16_t* buffer;
    int32_t writePtr;
    int32_t freezePtr;
    
    // Parameters
    int16_t lossAmount;      // Compression/degradation amount
    int16_t speed;           // Update rate for loss algorithm
    int16_t packetMode;      // Packet loss/repeat mode
    int16_t mix;
    
    GainStageDataType presetVolume;
    
    // State
    int16_t freezeBufferL[256]; // Spectral freeze buffer
    int16_t freezeBufferR[256];
    uint8_t freezeActive;
    int32_t speedCounter;
    int32_t packetCounter;
    uint8_t packetLossActive;
    
    // Compression state
    int16_t lastSampleL;
    int16_t lastSampleR;
    int16_t compressedL;
    int16_t compressedR;
    
    // Bit depth state
    uint8_t sampleHoldCounter;
    
} FxProgram27DataType;

#define NUM_SPECTRAL_BANDS 16

typedef struct {
    int16_t* buffer;
    int32_t writePtr;
    
    // Resynthesis Bank
    int32_t bpfState1[NUM_SPECTRAL_BANDS];
    int32_t bpfState2[NUM_SPECTRAL_BANDS];
    
    int16_t bpfCoeff[NUM_SPECTRAL_BANDS]; // Q14
    int16_t bpfR2[NUM_SPECTRAL_BANDS];    // Q14
    int16_t bpfGain[NUM_SPECTRAL_BANDS];  // Q15
    
    int32_t envState[NUM_SPECTRAL_BANDS]; // Envelope energy
    uint32_t oscPhase[NUM_SPECTRAL_BANDS]; // Sine phase
    uint32_t oscInc[NUM_SPECTRAL_BANDS];   // Phase increment
    
    // Parameters  
    int16_t mix;          // Dry/Wet
    int16_t blur;         // Reverb Smear size (Envelope decay)
    int16_t warp;         // Tape wow/flutter (Sine detune LFO)
    int16_t freeze;       // Locks the envelope followers
    
    GainStageDataType presetVolume;
    
    // Modulation LFO
    uint32_t lfoPhase;
    
} FxProgram28DataType;

typedef struct {
    int16_t* buffer;
    int32_t writePtr;
    
    // Motor Physics
    uint32_t motorPhase; // L and R (with offset)
    
    // Oil Physics (Filtering)
    int32_t lpStateL;
    int32_t lpStateR;
    
    // Parameters
    int16_t mix;
    int16_t timeVal;    // Motor Speed (inv delay time)
    int16_t smudge;     // Feedback + Darkening (Oil Viscosity)
    int16_t wobble;     // Disk irregularity
    int16_t freeze;     // Freeze State
    
    GainStageDataType presetVolume;
    
    // Jitter for degradation
    uint32_t rng;
} FxProgram29DataType;

extern FxProgramType fxProgramStrings;
extern FxProgramType fxProgramWind;
extern FxProgramType fxProgramSpeech;
#define N_FX_PROGRAMS 32

typedef struct {
} FxProgramTypeEmpty;

// ... (existing defines)

extern FxProgramType fxProgramAmpModel;
extern FxProgramType fxProgramVibChorus;
extern FxProgramType fxProgramOff;
extern FxProgramType fxProgramAmpModel2;
extern FxProgramType fxProgramMonsterCrusher;
extern FxProgramType fxProgramDelay;
extern FxProgramType fxProgramPingPong; 
extern FxProgramType fxProgramTape;     
extern FxProgramType fxProgramLofiDelay; 
extern FxProgramType fxProgram7;
extern FxProgramType fxProgramCompressor;
extern FxProgramType fxProgramEchoVerb;
extern FxProgramType fxProgramReverb;
extern FxProgramType fxProgramSineChorus;
extern FxProgramType fxProgramReverb2;
extern FxProgramType fxProgramReverb3;
extern FxProgramType fxProgramEq;
extern FxProgramType fxProgramPitchShifter;
extern FxProgramType fxProgramShimmerVerb;
extern FxProgramType fxProgramTremolo;
extern FxProgramType fxProgramFreeVerb;
extern FxProgramType fxProgramSpringReverb;
extern FxProgramType fxProgramPlateReverb; 
extern FxProgramType fxProgramCwo; 
extern FxProgramType fxProgramGranular; 
extern FxProgramType fxProgramCathedral; 
extern FxProgramType fxProgramMicroLoop; 
extern FxProgramType fxProgramGenLoss; 
extern FxProgramType fxProgramLossy;   
extern FxProgramType fxProgramSpectral; 
extern FxProgramType fxProgramOilCan;
extern FxProgramType* fxPrograms[N_FX_PROGRAMS];

#endif